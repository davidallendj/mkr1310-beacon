#pragma once

#include <vector>

#include "types.h"
#include "message.h"

namespace beacon {

enum class LogLevel {
  FATAL = 0,
  ERROR,
  WARN,
  INFO,
  DEBUG,
  TRACE
};

class Log {
public:
  Log(LogLevel level = LogLevel::INFO) :
    m_level(level)
  {}

  void add_message(const Message& message) { m_messages.emplace_back(message); }
  void remove_message(const string_t& message_id) {  }
  void remove_message(int index) {  }
  void get_message(int index) { m_messages.at(index); }

private:
  LogLevel m_level;
  messages_t m_messages;
};

}