#pragma once

#include <Arduino.h>
#include <LoRa.h>

#include <stdint.h>

#include "constants.h"
#include "json.h"
#include "log.h"
#include "payload.h"
#include "types.h"
#include "user.h"
#include "util.h"

namespace beacon {

// LoRa transport for a single SX1276 (the LoRa module on the MKR WAN 1310).
//
// The receive path is split in two because of how the arduino-LoRa library
// signals a packet. `LoRa.onReceive` is invoked from the DIO0 interrupt
// handler (see `onDio0Rise` -> `handleDio0Rise` in the library) with the FIFO
// already positioned at the start of the packet. The bytes therefore *must* be
// read inside the callback, but parsing JSON and writing to Serial must not be
// done from an interrupt. So the callback copies the raw bytes into a buffer
// and raises a flag; `poll()` does the rest and must be called regularly from
// `loop()`.
//
// Packets are capped by the hardware: the SX1276 FIFO holds 255 bytes and three
// UUIDs spend 108 of them, which leaves a little over a hundred characters of
// text. `transmit()` enforces that and reports the real byte count rather than
// letting the radio silently send a truncated frame.
class Radio {
public:
  // One byte beyond the FIFO limit so the buffer can hold a null terminator.
  static const int buffer_size = beacon::constants::lora_max_packet_size + 1;

  Radio(User& user, Log& log) :
    m_user(user),
    m_log(log)
  {}

  // Non-copyable: this holds references to the user and the log, so a copy
  // would keep operating on the originals. A node owns one radio.
  Radio(const Radio&) = delete;
  Radio& operator=(const Radio&) = delete;

  // Bring up the radio and apply the air-interface settings from
  // `constants.h`. Returns false and leaves a reason in `get_error()`.
  bool setup();

  // Process at most one received packet. Call from `loop()`.
  void poll();

  // Build the on-air form of `payload`. Returns by value: the previous
  // implementation handed back a pointer into a destroyed local (BUG-03).
  string_t encode(const Payload& payload) const;

  // Put `packet` on the air exactly as given. On failure returns false and
  // fills `error` with a message suitable for showing to the user.
  bool transmit(const string_t& packet, string_t& error);

  // Bytes left for message text after the JSON envelope. Measured once at
  // setup by encoding an empty message, so it accounts for the real key names
  // and UUID lengths rather than a guess.
  size_t max_text_bytes() const;

  const string_t& get_error() const { return m_error; }
  int32_t last_rssi() const { return m_last_rssi; }
  uint32_t get_sent() const { return m_sent; }
  uint32_t get_received() const { return m_received; }
  uint32_t get_accepted() const { return m_accepted; }
  uint32_t get_ignored() const { return m_ignored; }
  // Dropped packets live in a static counter because the DIO0 handler is a
  // static function with no instance to write to. One radio per module makes
  // that sound; the counter is reset by setup().
  uint32_t get_dropped() const { return s_dropped; }
  uint32_t get_malformed() const { return m_malformed; }

  // Record a failure from `setup()` and report it.
  bool fail(const string_t& reason) {
    m_error = reason;
    return false;
  }

private:
  // Runs in the DIO0 interrupt handler.
  static void on_receive_isr(int packet_size);

  // Discard anything left in the FIFO so the next packet is read from a clean
  // position. Called from interrupt context.
  static void drain_fifo();

  // Full packet processing. Runs in loop() context, not an interrupt.
  void process(int size, int rssi);

  // Print a decoded incoming message according to the user's DisplayMode.
  void print_received(const Payload& payload, int rssi) const;

  User& m_user;
  Log& m_log;

  string_t m_error;
  size_t m_overhead = 0;
  int32_t m_last_rssi = 0;
  uint32_t m_sent = 0;
  uint32_t m_received = 0;
  uint32_t m_accepted = 0;
  uint32_t m_ignored = 0;
  uint32_t m_malformed = 0;

  // Shared with the interrupt handler; defined in radio.cpp because C++11 has
  // no inline variables and the SAMD core is built with -std=gnu++11.
  static char s_buffer[buffer_size];
  static volatile bool s_pending;
  static volatile bool s_processing;
  static volatile uint32_t s_dropped;
  static int s_size;
  static int s_rssi;
};

}  // namespace beacon
