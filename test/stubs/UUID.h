#pragma once

#include <cstdio>

// Stand-in for the UUID library, whose real header inherits from Arduino's
// Printable. Produces deterministic, distinct-looking v4 strings so tests can
// assert on exact payloads.
class UUID {
public:
  UUID() : m_value(next_value()) {}

  char* toCharArray() {
    // 8-4-4-4-12 hex digits, like the real thing.
    std::snprintf(m_buffer, sizeof(m_buffer),
                  "%08x-%04x-4%03x-8%03x-%012lx",
                  static_cast<unsigned>(m_value),
                  static_cast<unsigned>((m_value >> 4) & 0xffff),
                  static_cast<unsigned>(m_value & 0xfff),
                  static_cast<unsigned>((m_value >> 8) & 0xfff),
                  m_value);
    return m_buffer;
  }

  static void reset() { next_counter() = 0x01020304; }

private:
  static unsigned long& next_counter() {
    static unsigned long counter = 0x01020304;
    return counter;
  }

  static unsigned long next_value() { return next_counter()++; }

  unsigned long m_value;
  char m_buffer[64];
};
