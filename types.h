#pragma once

#include <functional>
#include <string>
#include <vector>

// Forward declarations. `types.h` deliberately includes no Arduino headers so
// that it stays cheap to include everywhere; the types it aliases are all
// completed by the headers that define them.
namespace beacon {
class Command;
class Error;
class Flag;
class Log;
class Message;
class Payload;
class Radio;
class User;
}  // namespace beacon

// --- type aliases -------------------------------------------------------
using string_t = std::string;
using strings_t = std::vector<string_t>;

using log_t = beacon::Log;
using message_t = beacon::Message;
using payload_t = beacon::Payload;
using user_t = beacon::User;
using radio_t = beacon::Radio;

using flag_t = beacon::Flag;
using command_t = beacon::Command;
using cli_error_t = beacon::Error;

using flags_t = std::vector<flag_t>;
using commands_t = std::vector<command_t>;
using messages_t = std::vector<message_t>;

// Signature every command handler must have. `args[0]` is the command name, so
// user-supplied words start at index 1.
using entry_func_t = std::function<void(const strings_t& args)>;
