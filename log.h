#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "message.h"
#include "types.h"
#include "util.h"

namespace beacon {

enum class LogLevel {
  FATAL = 0,
  ERROR,
  WARN,
  INFO,
  DEBUG,
  TRACE
};

inline const char* to_string(LogLevel level) {
  switch (level) {
    case LogLevel::FATAL: return "fatal";
    case LogLevel::ERROR: return "error";
    case LogLevel::WARN:  return "warn";
    case LogLevel::INFO:  return "info";
    case LogLevel::DEBUG: return "debug";
    case LogLevel::TRACE: return "trace";
  }
  return "info";
}

// Parse a level name ("debug"), or a bare number ("4"). Returns false for
// anything else so the CLI can report bad input.
inline bool log_level_from_string(const string_t& text, LogLevel& out) {
  const string_t value = to_lower(trim(text));
  if (value == "fatal") { out = LogLevel::FATAL; return true; }
  if (value == "error") { out = LogLevel::ERROR; return true; }
  if (value == "warn")  { out = LogLevel::WARN;  return true; }
  if (value == "info")  { out = LogLevel::INFO;  return true; }
  if (value == "debug") { out = LogLevel::DEBUG; return true; }
  if (value == "trace") { out = LogLevel::TRACE; return true; }

  long value_as_number = 0;
  if (parse_int(value, value_as_number) &&
      value_as_number >= static_cast<long>(LogLevel::FATAL) &&
      value_as_number <= static_cast<long>(LogLevel::TRACE)) {
    out = static_cast<LogLevel>(value_as_number);
    return true;
  }
  return false;
}

// Bounded FIFO of recent messages, plus the threshold that decides which
// internal events are worth printing. Messages are capped because the SAMD21
// has 32 KB of RAM and this is an always-on buffer.
class Log {
public:
  // A function rather than a `static const size_t` member: C++11 has no inline
  // variables, and a static data member would need an out-of-class definition
  // the moment anything bound it to a reference.
  static size_t capacity() { return 32; }

  explicit Log(LogLevel level = LogLevel::INFO) :
    m_level(level)
  {}

  // Append a message, dropping the oldest once capacity() is reached.
  void add_message(const Message& message) {
    while (m_messages.size() >= capacity()) {
      m_messages.erase(m_messages.begin());
    }
    m_messages.emplace_back(message);
  }

  // Remove by message ID. Returns false when no such message is held.
  bool remove_message(const string_t& message_id) {
    const auto iter = std::find_if(
      m_messages.begin(), m_messages.end(),
      [&message_id](const Message& m) { return m.get_id() == message_id; });

    if (iter == m_messages.end()) {
      return false;
    }
    m_messages.erase(iter);
    return true;
  }

  // Remove by position. Returns false when out of range.
  bool remove_message(size_t index) {
    if (index >= m_messages.size()) {
      return false;
    }
    m_messages.erase(m_messages.begin() + static_cast<long>(index));
    return true;
  }

  // Look up by ID. Returns nullptr when not held, so callers must check.
  const Message* find_message(const string_t& message_id) const {
    const auto iter = std::find_if(
      m_messages.begin(), m_messages.end(),
      [&message_id](const Message& m) { return m.get_id() == message_id; });

    if (iter == m_messages.end()) {
      return nullptr;
    }
    return &(*iter);
  }

  const Message* get_message(size_t index) const {
    if (index >= m_messages.size()) {
      return nullptr;
    }
    return &m_messages[index];
  }

  const messages_t& get_messages() const { return m_messages; }
  size_t size() const { return m_messages.size(); }
  void clear() { m_messages.clear(); }

  LogLevel get_level() const { return m_level; }
  void set_level(LogLevel level) { m_level = level; }

  // True when an event of `level` should be printed under the current
  // threshold. Lower enum values are more severe, so a message passes when it
  // is at least as severe as the threshold.
  bool should_log(LogLevel level) const {
    return static_cast<int>(level) <= static_cast<int>(m_level);
  }

private:
  LogLevel m_level;
  messages_t m_messages;
};

}  // namespace beacon
