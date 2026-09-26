#include "session.h"

#include <Arduino.h>

namespace beacon {

// The SAMD core's Print has no std::string overloads, so strings are converted
// once here rather than with a .c_str() at every call site.
namespace {

void say_line(const char* text) { Serial.println(text); }
void say_line(const string_t& text) { Serial.println(text.c_str()); }

}  // namespace


Session::Session() :
  m_user(constants::user_name, constants::user_id),
  m_everyone(User::broadcast()),
  m_log(LogLevel::INFO),
  m_radio(m_user, m_log),
  m_cli("beacon", LogLevel::INFO,
        "BEACON for Arduino MKR 1310 WAN - experimental thin client",
        "Exchange short JSON messages with other nodes over LoRa.")
{}


bool Session::setup() {
  register_commands();

  if (!m_radio.setup()) {
    m_error = m_radio.get_error();
    return false;
  }

  // An unset user_id means this node looks different on every boot, which
  // makes it impossible to tell apart from another node or to address.
  if (constants::user_id[0] == '\0') {
    m_error = string_t("no user_id in constants.h; this node's ID is ") + m_user.get_id();
  }
  return true;
}


void Session::poll() {
  m_radio.poll();
}


void Session::print_user() {
  say_line(string_t("  id:            ") + m_user.get_id());
  say_line(string_t("  name:          ") + m_user.get_name());
  say_line(string_t("  radio mode:    ") + to_string(m_user.get_radio_mode()));
  say_line(string_t("  display mode:  ") + to_string(m_user.get_display_mode()));
}


void Session::print_status() {
  say_line("");
  say_line("You are " + m_user.get_name() + " (" + m_user.get_id() + ")");
  say_line(string_t("Radio: ") +
           to_str(constants::lora_frequency / 1000000) + " MHz, SF" +
           to_str(constants::lora_spreading_factor) + ", " +
           to_str(constants::lora_bandwidth / 1000) + " kHz");
  say_line(string_t("Sent: ") + to_str(static_cast<long>(m_radio.get_sent())) +
           "   Received: " + to_str(static_cast<long>(m_radio.get_received())) +
           "   Accepted: " + to_str(static_cast<long>(m_radio.get_accepted())));
  say_line("Type /help for commands, or just type a message.");
  say_line("");
}


void Session::send_message(const string_t& text, bool quiet) {
  if (text.empty()) {
    say_line("usage: /message <text>");
    return;
  }

  if (!m_user.can_send()) {
    say_line(string_t("error: radio mode is '") + to_string(m_user.get_radio_mode()) +
             "', so sending is disabled");
    return;
  }

  const Message message(text);
  const Payload payload(m_user, m_everyone, message);

  // encode() and transmit() are separate so the JSON is built exactly once.
  const string_t packet = m_radio.encode(payload);

  string_t error;
  if (!m_radio.transmit(packet, error)) {
    say_line("error: " + error);
    return;
  }

  if (quiet) {
    return;
  }

  // Echo locally in the same shape an incoming message arrives in, so a
  // conversation reads consistently in one terminal.
  say_line(m_user.get_name() + " -> " + message.get_contents());
}


// --- command handlers ---------------------------------------------------
// Each receives argv with the command name at index 0.

void Session::cmd_message(const strings_t& args) {
  // Pull out any flags (e.g. --quiet) so what is left is the text to send.
  strings_t words;
  flags_t flags;
  Command* self = m_cli.get_command("message");
  if (self != nullptr) {
    self->split_args(args, flags, words);
  } else {
    words.assign(args.begin() + 1, args.end());
  }

  bool quiet = false;
  for (const auto& flag : flags) {
    if (flag.get_name() == "quiet") {
      quiet = true;
    }
  }

  if (words.empty()) {
    if (self != nullptr) {
      self->print_help();
    } else {
      say_line("usage: /message <text>");
    }
    return;
  }

  // Joined with spaces: words arrive as separate argv entries.
  send_message(join(words), quiet);
}


void Session::cmd_user(const strings_t& args) {
  if (args.size() == 1) {
    print_user();
    return;
  }

  const string_t subcommand = to_lower(args[1]);
  const string_t value = join(copy(args, 2), " ");

  if (subcommand == "name") {
    if (value.empty()) {
      say_line("usage: /user name <name>");
      return;
    }
    m_user.set_name(value);
    say_line("name set to " + value);
    return;
  }

  if (subcommand == "id") {
    // Changing the ID at runtime does not persist; it is lost on reboot. This
    // stays a placeholder until the profile can be written to flash.
    if (value.empty()) {
      say_line("id: " + m_user.get_id());
      return;
    }
    m_user.set_id(value);
    say_line("id set for this session only; set constants::user_id to persist it");
    return;
  }

  if (subcommand == "radio") {
    if (value.empty()) {
      say_line(string_t("radio mode: ") + to_string(m_user.get_radio_mode()));
      return;
    }
    RadioMode mode;
    if (!radio_mode_from_string(value, mode)) {
      say_line("error: unknown radio mode '" + value + "' (expected send, receive, or both)");
      return;
    }
    m_user.set_radio_mode(mode);
    say_line(string_t("radio mode set to ") + to_string(mode));
    return;
  }

  if (subcommand == "display") {
    if (value.empty()) {
      say_line(string_t("display mode: ") + to_string(m_user.get_display_mode()));
      return;
    }
    DisplayMode mode;
    if (!display_mode_from_string(value, mode)) {
      say_line("error: unknown display mode '" + value + "' (expected simple or full)");
      return;
    }
    m_user.set_display_mode(mode);
    say_line(string_t("display mode set to ") + to_string(mode));
    return;
  }

  say_line("error: unknown '/user' subcommand '" + subcommand +
           "' (expected name, id, radio, or display)");
}


void Session::cmd_log_level(const strings_t& args) {
  if (args.size() == 1) {
    say_line(string_t("log level: ") + to_string(m_cli.get_level()));
    say_line("levels: fatal, error, warn, info, debug, trace (or 0-5)");
    return;
  }

  LogLevel level;
  if (!log_level_from_string(args[1], level)) {
    say_line("error: unknown log level '" + args[1] +
             "' (expected fatal, error, warn, info, debug, or trace)");
    return;
  }

  m_cli.set_level(level);
  m_log.set_level(level);
  say_line(string_t("log level set to ") + to_string(level));
}


void Session::cmd_help(const strings_t& args) {
  if (args.size() == 1) {
    m_cli.print_help();
    return;
  }
  m_cli.print_help(args[1]);
}


void Session::cmd_stats(const strings_t& args) {
  (void)args;
  say_line("Radio:");
  say_line("  sent:         " + to_str(static_cast<long>(m_radio.get_sent())));
  say_line("  received:     " + to_str(static_cast<long>(m_radio.get_received())));
  say_line("  accepted:     " + to_str(static_cast<long>(m_radio.get_accepted())));
  say_line("  ignored:      " + to_str(static_cast<long>(m_radio.get_ignored())));
  say_line("  malformed:    " + to_str(static_cast<long>(m_radio.get_malformed())));
  say_line("  dropped:      " + to_str(static_cast<long>(m_radio.get_dropped())));
  say_line("  last rssi:    " + to_str(m_radio.last_rssi()) + " dBm");
  say_line("  text budget:  " + to_str(static_cast<long>(m_radio.max_text_bytes())) + " bytes");

  say_line("Messages:");
  say_line("  held:         " + to_str(static_cast<long>(m_log.size())));
  for (size_t i = 0; i < m_log.size(); ++i) {
    const Message* message = m_log.get_message(i);
    if (message == nullptr) {
      continue;
    }
    say_line("    " + message->get_id_short() + "  " + message->get_contents());
  }
}


void Session::cmd_flush(const strings_t& args) {
  (void)args;
  m_log.clear();
  say_line("message history cleared");
}


void Session::register_commands() {
  // The handlers are member functions, so they are wrapped: an `entry_func_t` is
  // a plain function pointer type and cannot hold a `this` pointer.
  m_cli
    .add_command("message", "Send a message to every node in range.",
                 [this](const strings_t& args) { cmd_message(args); })
    .add_command("user", "Show or change the local user profile.",
                 [this](const strings_t& args) { cmd_user(args); })
    .add_command("log-level", "Show or set the logging level.",
                 [this](const strings_t& args) { cmd_log_level(args); })
    .add_command("stats", "Show radio counters and message history.",
                 [this](const strings_t& args) { cmd_stats(args); })
    .add_command("flush", "Clear the stored message history.",
                 [this](const strings_t& args) { cmd_flush(args); })
    .add_command("help", "Show help, or help for one command.",
                 [this](const strings_t& args) { cmd_help(args); });

  // Flags match by name or short form, so `--quiet` and `-q` both work.
  m_cli.get_command("message")->add_flag(
    Flag("quiet", 'q', "send without echoing the message locally"));
}


// --- input --------------------------------------------------------------

void Session::execute(const string_t& line) {
  const string_t text = trim(line);
  if (text.empty()) {
    // Re-print status: this is also how to recover the banner if the terminal
    // attached after boot.
    print_status();
    return;
  }

  if (m_log.should_log(LogLevel::DEBUG)) {
    say_line("[debug] " + text);
  }

  const strings_t args = split(text);

  // A line that neither starts with '/' nor names a command is treated as a
  // message, so the terminal behaves like a chat client. A leading '/' is
  // explicit, which is how a mistyped command gets reported instead of being
  // sent to the network.
  if (text[0] != '/') {
    if (args.empty() || m_cli.get_command(args[0]) == nullptr) {
      send_message(text, false);
      return;
    }
  }

  m_cli.run_command(args);
}


void Session::feed(const string_t& line) {
  execute(line);
}

}  // namespace beacon
