#pragma once

#include <functional>

// #include "message.h"
// #include "log.h"
// #include "cli.h"

// typedef std::string string_t;
// typedef std::vector<std::string> strings_t;
// typedef std::vector<beacon::Message> messages_t;
// typedef std::function<void(const strings_t&)> entry_func_t;

// forward declarations
namespace beacon {
class Error;
class Flag;
class Command;
class Message;
class Log;
}

// type aliases
using log_t = beacon::Log;
using message_t = beacon::Message;
using flag_t = beacon::Flag;
using command_t = beacon::Command;
using cli_error_t = beacon::Error;
using string_t = std::string;

using strings_t = std::vector<string_t>;
using messages_t = std::vector<message_t>;
using flags_t = std::vector<flag_t>;
using commands_t = std::vector<command_t>;
using entry_func_t = std::function<void(const strings_t&)>;