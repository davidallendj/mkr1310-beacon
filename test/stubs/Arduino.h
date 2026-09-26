// Minimal stand-ins for the Arduino core, so the sketch's headers can be
// compiled and exercised on a workstation.
//
// Only what the project actually uses is provided. The stub is deliberately
// literal: `Serial.println` appends "\r\n" exactly as the real core does, which
// is what surfaced the packet-framing bug (BUG-05).

#pragma once

#include <stdint.h>
#include <string>

namespace arduino_stub {

// Everything written to Serial lands here so tests can assert on it.
extern std::string out;
// Bytes queued for Serial.available()/Serial.read().
extern std::string in;
extern long millis_value;
extern int interrupts_enabled;

void reset();
void feed(const std::string& text);

}  // namespace arduino_stub

class SerialStub {
public:
  void begin(unsigned long) {}
  explicit operator bool() const { return true; }

  int available() {
    return static_cast<int>(arduino_stub::in.size());
  }

  int read() {
    if (arduino_stub::in.empty()) {
      return -1;
    }
    const char ch = arduino_stub::in[0];
    arduino_stub::in.erase(0, 1);
    return static_cast<unsigned char>(ch);
  }

  void flush() {}

  // NOTE: there is deliberately no print/println(const std::string&) overload.
  // The real SAMD core's Print has none either, so accepting one here would
  // hide a class of error that only shows up when compiling for the device.
  // Always pass .c_str().
  void print(const char* s) { arduino_stub::out += s; }
  void print(char c) { arduino_stub::out += c; }
  void print(int v) { arduino_stub::out += std::to_string(v); }
  void print(long v) { arduino_stub::out += std::to_string(v); }
  void print(unsigned int v) { arduino_stub::out += std::to_string(v); }
  void print(unsigned long v) { arduino_stub::out += std::to_string(v); }

  void println() { arduino_stub::out += "\r\n"; }
  void println(const char* s) { arduino_stub::out += s; arduino_stub::out += "\r\n"; }
  void println(char c) { arduino_stub::out += c; arduino_stub::out += "\r\n"; }
  void println(int v) { print(v); println(); }
  void println(long v) { print(v); println(); }
  void println(unsigned int v) { print(v); println(); }
  void println(unsigned long v) { print(v); println(); }
};

extern SerialStub Serial;

inline unsigned long millis() {
  return static_cast<unsigned long>(arduino_stub::millis_value);
}

inline void delay(unsigned long) {}
inline void noInterrupts() { arduino_stub::interrupts_enabled = 0; }
inline void interrupts() { arduino_stub::interrupts_enabled = 1; }
