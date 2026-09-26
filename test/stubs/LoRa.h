#pragma once

// Stand-in for the arduino-LoRa library, with a loopback radio so a full
// send -> interrupt -> receive -> decode path can be exercised on a
// workstation.
//
// The transmit and receive sides are deliberately separate, so a test can
// inject a packet that was never sent (a corrupt one, a foreign one, an
// over-long one) and check how Radio handles it.

#include <cstdint>
#include <string>
#include <vector>

class StreamStub {
public:
  // Number of bytes currently in the receive FIFO.
  int available() const { return static_cast<int>(m_rx.size()); }

  int read() {
    if (m_rx.empty()) {
      return -1;
    }
    const int value = static_cast<unsigned char>(m_rx[0]);
    m_rx.erase(0, 1);
    return value;
  }

  size_t write(uint8_t value) {
    m_rx.push_back(static_cast<char>(value));
    return 1;
  }

  void push(const std::string& data) { m_rx += data; }
  void clear() { m_rx.clear(); }

private:
  std::string m_rx;
};

class LoRaStub : public StreamStub {
public:
  // --- configuration ---
  int begin(long frequency) {
    m_frequency = frequency;
    return m_begin_result;
  }

  void setSignalBandwidth(long bandwidth) { m_bandwidth = bandwidth; }
  void setSpreadingFactor(int sf) { m_spreading_factor = sf; }
  void setCodingRate4(int denominator) { m_coding_rate = denominator; }
  void setSyncWord(int word) { m_sync_word = word; }
  void setTxPower(int power) { m_tx_power = power; }
  void onReceive(void (*callback)(int)) { m_on_receive = callback; }

  // --- transmit ---
  int beginPacket() {
    m_tx.clear();
    return m_begin_packet_result;
  }

  // Matches the real core: println() appends CRLF, print() does not.
  int endPacket(bool async = false) {
    (void)async;
    m_last_tx = m_tx;
    ++m_packets_sent;
    return m_end_packet_result;
  }

  void print(const char* s) { m_tx += s; }
  void print(const std::string& s) { m_tx += s; }
  void print(char c) { m_tx += c; }
  void print(int v) { m_tx += std::to_string(v); }

  void println(const char* s) { m_tx += s; m_tx += "\r\n"; }
  void println(const std::string& s) { m_tx += s; m_tx += "\r\n"; }

  int packetRssi() { return m_rssi; }
  int parsePacket(int size = 0) {
    (void)size;
    return available();
  }

  // --- test controls ---

  // Hand `data` to the registered callback as if it had arrived over the air.
  // Returns false when no callback is registered, which mirrors the real
  // library ignoring DIO0 with nothing attached.
  bool inject(const std::string& data) {
    push(data);
    m_rssi = m_next_rssi;
    if (m_on_receive == nullptr) {
      return false;
    }
    m_on_receive(static_cast<int>(data.size()));
    return true;
  }

  bool has_callback() const { return m_on_receive != nullptr; }

  const std::string& last_tx() const { return m_last_tx; }
  long frequency() const { return m_frequency; }
  long bandwidth() const { return m_bandwidth; }
  int spreading_factor() const { return m_spreading_factor; }
  int coding_rate() const { return m_coding_rate; }
  int sync_word() const { return m_sync_word; }
  int tx_power() const { return m_tx_power; }
  int packets_sent() const { return m_packets_sent; }

  void set_begin_result(int result) { m_begin_result = result; }
  void set_begin_packet_result(int result) { m_begin_packet_result = result; }
  void set_end_packet_result(int result) { m_end_packet_result = result; }
  void set_rssi(int rssi) { m_next_rssi = rssi; }

  void reset() {
    m_tx.clear();
    m_last_tx.clear();
    clear();
    m_packets_sent = 0;
  }

private:
  std::string m_tx;
  std::string m_last_tx;
  void (*m_on_receive)(int) = nullptr;

  long m_frequency = 0;
  long m_bandwidth = 0;
  int m_spreading_factor = 0;
  int m_coding_rate = 0;
  int m_sync_word = 0;
  int m_tx_power = 0;
  int m_rssi = 0;
  int m_next_rssi = -100;
  int m_packets_sent = 0;

  int m_begin_result = 1;
  int m_begin_packet_result = 1;
  int m_end_packet_result = 1;
};

extern LoRaStub LoRa;
