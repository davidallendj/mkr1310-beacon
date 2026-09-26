#include "radio.h"

#include <stdio.h>

// Definitions for Radio's interrupt-shared state. These live in a .cpp rather
// than inline in the header because the SAMD core is compiled with
// -std=gnu++11, which has no inline variables.
//
// s_buffer / s_size / s_rssi / s_pending are written only by the DIO0
// interrupt handler and read only by poll(). The ISR writes the payload first
// and raises s_pending last, and poll() refuses to let a new ISR overwrite the
// buffer while a packet is being processed, so a reader always sees a
// self-consistent packet.

namespace beacon {

char Radio::s_buffer[Radio::buffer_size];
volatile bool Radio::s_pending = false;
volatile bool Radio::s_processing = false;
volatile uint32_t Radio::s_dropped = 0;
int Radio::s_size = 0;
int Radio::s_rssi = 0;


bool Radio::setup() {
  const long frequency = constants::lora_frequency;

  if (!LoRa.begin(frequency)) {
    return fail("failed to start LoRa at " + to_str(frequency) + " Hz");
  }

  // Applied explicitly rather than inherited from the SX1276 register defaults
  // so that every node in a network is guaranteed to agree. See the note in
  // constants.h: arduino-LoRa 0.8.0's setters return void, so a rejected value
  // cannot be reported here.
  LoRa.setSignalBandwidth(constants::lora_bandwidth);
  LoRa.setSpreadingFactor(constants::lora_spreading_factor);
  LoRa.setCodingRate4(constants::lora_coding_rate);
  LoRa.setSyncWord(constants::lora_sync_word);
  LoRa.setTxPower(constants::lora_tx_power);

  LoRa.onReceive(on_receive_isr);

  // Fresh start for the interrupt-shared state, in case setup() is called
  // again after a radio restart.
  s_pending = false;
  s_processing = false;
  s_dropped = 0;
  s_size = 0;
  s_rssi = 0;

  // Measure the real cost of the envelope by encoding an empty message, so
  // max_text_bytes() reflects the actual key names and UUID lengths.
  const Payload probe(m_user, User::broadcast(), Message(string_t()));
  m_overhead = probe.serialize().size();

  return true;
}


size_t Radio::max_text_bytes() const {
  const size_t limit = static_cast<size_t>(constants::lora_max_packet_size);
  if (m_overhead == 0 || limit <= m_overhead) {
    return 0;
  }
  return limit - m_overhead;
}


string_t Radio::encode(const Payload& payload) const {
  return payload.serialize();
}


bool Radio::transmit(const string_t& packet, string_t& error) {
  if (!m_user.can_send()) {
    error = "radio mode is '" + string_t(to_string(m_user.get_radio_mode())) +
            "', so sending is disabled";
    return false;
  }

  if (packet.empty()) {
    error = "refusing to send an empty packet";
    return false;
  }

  const size_t limit = static_cast<size_t>(constants::lora_max_packet_size);
  if (packet.size() > limit) {
    // Report the real numbers: the limit, the envelope overhead, and how much
    // room is actually left for text. JSON escaping can make the encoded form
    // longer than the text, so this is measured rather than predicted.
    error = "message too long: " + to_str(static_cast<long>(packet.size())) +
            " of " + to_str(static_cast<long>(limit)) +
            " bytes (envelope uses " + to_str(static_cast<long>(m_overhead)) +
            ", leaving " + to_str(static_cast<long>(max_text_bytes())) +
            " for text)";
    return false;
  }

  // beginPacket() fails when the radio is still busy with a previous
  // transmission; endPacket() returns 0 if the packet never left.
  if (!LoRa.beginPacket()) {
    error = "radio is busy, message not sent";
    return false;
  }

  // print(), not println(): println() would append CRLF, and the receiver reads
  // exactly packet_size bytes rather than trying to guess where JSON ends.
  LoRa.print(packet.c_str());

  if (!LoRa.endPacket()) {
    error = "radio did not finish sending, message not delivered";
    return false;
  }

  ++m_sent;
  return true;
}


void Radio::poll() {
  if (!s_pending) {
    return;
  }

  // Claim the packet with interrupts briefly disabled. Setting s_processing
  // first is what stops the next interrupt from starting to overwrite the
  // buffer while it is being parsed; the buffer itself is not copied, so this
  // critical section stays short.
  noInterrupts();
  s_processing = true;
  s_pending = false;
  const int size = s_size;
  const int rssi = s_rssi;
  interrupts();

  process(size, rssi);

  s_processing = false;
}


void Radio::process(int size, int rssi) {
  if (size <= 0) {
    return;
  }

  ++m_received;

  // Checked here rather than in the interrupt handler so the ISR stays small
  // and the check uses the current mode rather than a snapshot. See BUG-04: the
  // original condition was `!= Receive || != Both`, which is always true.
  if (!m_user.can_receive()) {
    ++m_ignored;
    return;
  }

  const string_t raw(s_buffer, static_cast<size_t>(size));

  if (!validate_json(raw)) {
    ++m_malformed;
    // Reported but not added to the message history: the log holds messages
    // the user actually received, not diagnostics.
    Serial.println(("error: received " + to_str(size) +
                    " bytes that are not valid JSON").c_str());
    return;
  }

  Payload payload;
  if (!payload.deserialize(raw)) {
    ++m_malformed;
    Serial.println("error: received valid JSON in an unexpected shape");
    return;
  }

  // Our own broadcast looping back is not a new message.
  if (payload.get_sender().get_id() == m_user.get_id()) {
    return;
  }

  // Addressed to somebody else.
  if (!payload.addressed_to(m_user)) {
    ++m_ignored;
    return;
  }

  ++m_accepted;
  m_last_rssi = rssi;
  m_log.add_message(payload.get_message());
  print_received(payload, rssi);
}


void Radio::print_received(const Payload& payload, int rssi) const {
  const User& sender = payload.get_sender();
  const Message& message = payload.get_message();

  // "self (-92 dBm) <- alice: hello"
  const string_t prompt = m_user.get_name() + " (" + to_str(rssi) + " dBm) <- ";

  Serial.print(prompt.c_str());
  if (m_user.get_display_mode() == DisplayMode::Full) {
    Serial.print(sender.get_id_short().c_str());
    Serial.print(" ");
  }
  Serial.print(sender.get_name().c_str());
  Serial.print(": ");
  Serial.println(message.get_contents().c_str());
}


// --- interrupt context --------------------------------------------------

void Radio::on_receive_isr(int packet_size) {
  // The FIFO is already positioned at the start of the packet, so the bytes
  // have to be read here. Everything expensive happens later in poll().
  if (packet_size <= 0) {
    drain_fifo();
    return;
  }

  if (packet_size >= buffer_size || s_processing) {
    // Either longer than the buffer can hold, or the previous packet has not
    // been consumed yet. Either way the bytes are dropped -- but the FIFO still
    // has to be emptied so the next packet is read from a clean position.
    drain_fifo();
    ++s_dropped;
    return;
  }

  for (int i = 0; i < packet_size; ++i) {
    s_buffer[i] = static_cast<char>(LoRa.read());
  }
  s_buffer[packet_size] = '\0';

  s_size = packet_size;
  s_rssi = LoRa.packetRssi();

  // Raised last, so a reader that sees s_pending also sees the payload.
  s_pending = true;
}


void Radio::drain_fifo() {
  while (LoRa.available() > 0) {
    LoRa.read();
  }
}

}  // namespace beacon
