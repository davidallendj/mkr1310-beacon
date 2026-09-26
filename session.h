#pragma once

#include <string>

#include "cli.h"
#include "constants.h"
#include "log.h"
#include "message.h"
#include "payload.h"
#include "radio.h"
#include "types.h"
#include "user.h"
#include "util.h"

namespace beacon {

// One node: its identity, its radio, and the commands the terminal exposes.
//
// This is deliberately separate from the sketch. The sketch only owns the
// serial port and calls `setup()`, `poll()` and `feed()`; everything a user can
// actually observe lives here, which is what makes the whole
// type-a-line -> transmit -> receive -> print path testable off-device.
class Session {
public:
  Session();

  // Non-copyable, and non-movable on purpose.
  //
  // The command handlers are lambdas that capture `this`, and `Radio` holds
  // references to the user and the log. A copy would therefore be a different
  // object whose commands still ran against the original -- silently, and with
  // no visible symptom until output went to the wrong place. A node owns one
  // session for its whole life, so forbidding copies costs nothing.
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;

  ~Session() = default;

  // Bring up the radio and register commands. Returns false if the radio could
  // not be started; the reason is in `get_radio().get_error()` and the session
  // stays usable for the commands that do not transmit.
  bool setup();

  // Service the radio. Call from `loop()`.
  void poll();

  // Run one line of user input, as if it had been typed and submitted.
  void feed(const string_t& line);

  Cli& get_cli() { return m_cli; }
  const Cli& get_cli() const { return m_cli; }
  Radio& get_radio() { return m_radio; }
  const Radio& get_radio() const { return m_radio; }
  Log& get_log() { return m_log; }
  User& get_user() { return m_user; }
  const User& get_user() const { return m_user; }
  const string_t& get_error() const { return m_error; }

  // Print identity, radio settings and counters. Also used to recover the
  // banner if the terminal attached after boot.
  void print_status();
  void print_user();

private:
  void register_commands();
  void execute(const string_t& line);
  void send_message(const string_t& text, bool quiet);

  void cmd_message(const strings_t& args);
  void cmd_user(const strings_t& args);
  void cmd_log_level(const strings_t& args);
  void cmd_help(const strings_t& args);
  void cmd_stats(const strings_t& args);
  void cmd_flush(const strings_t& args);

  User m_user;
  User m_everyone;
  Log m_log;
  Radio m_radio;
  Cli m_cli;

  string_t m_error;
};

}  // namespace beacon
