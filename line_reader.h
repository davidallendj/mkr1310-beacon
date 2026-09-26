#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace beacon {

// Turns a stream of characters into complete lines.
//
// The terminal side of the sketch is only a byte pump, so all the editing rules
// live here where they can be tested: what counts as a line terminator, what
// backspace does, and which bytes are noise.
class LineReader {
public:
  // Accept one character.
  //
  // Both '\r' and '\n' end a line, because the Arduino serial monitor can be
  // configured to send either, and a user who picked the other one would
  // otherwise get no response at all. A "\r\n" (or "\n\r") pair counts as one
  // terminator rather than two, so Windows-style input does not submit every
  // line twice. Two *identical* terminators in a row are two lines, so
  // pressing Enter on an empty line still submits an empty line.
  //
  // Backspace and delete remove the previous character. Other control bytes are
  // dropped. Everything else, including UTF-8 continuation bytes (all >= 0x80),
  // is kept.
  void feed(char ch) {
    if (ch == '\r' || ch == '\n') {
      const char other = (ch == '\r') ? '\n' : '\r';
      if (m_last == other) {
        // Second half of a CRLF/LFCR pair: already terminated the line.
        m_last = ch;
        return;
      }
      m_lines.push_back(m_buffer);
      m_buffer.clear();
      m_last = ch;
      return;
    }

    m_last = ch;

    if (ch == '\b' || ch == 0x7f) {
      if (!m_buffer.empty()) {
        m_buffer.pop_back();
      }
      return;
    }

    if (static_cast<unsigned char>(ch) < 0x20) {
      return;
    }

    m_buffer.push_back(ch);
  }

  void feed(const string_t& text) {
    for (size_t i = 0; i < text.size(); ++i) {
      feed(text[i]);
    }
  }

  // Pop the next complete line, if one is ready.
  bool next_line(string_t& out) {
    if (m_lines.empty()) {
      return false;
    }
    out = m_lines.front();
    m_lines.erase(m_lines.begin());
    return true;
  }

  bool has_line() const { return !m_lines.empty(); }

  // Characters typed but not yet terminated.
  const string_t& buffer() const { return m_buffer; }

  // Drop the partial line and any complete lines still queued.
  void reset() {
    m_buffer.clear();
    m_lines.clear();
  }

private:
  string_t m_buffer;
  strings_t m_lines;
  // Previous character, used to recognise a CRLF/LFCR pair.
  char m_last = '\0';
};

}  // namespace beacon
