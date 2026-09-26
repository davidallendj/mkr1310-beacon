// Tests for the command framework in cli.h.
//
// The first two cases are direct regressions for BUG-01 and BUG-02. Note that
// the SAMD core is built with -fno-exceptions, so the `args.at(1)` that used to
// appear on this path did not throw: it aborted the program. On a workstation
// the same code throws std::out_of_range, which is what these tests observe.

#include "testing.h"

#include "Arduino.h"

#include "cli.h"

using beacon::Cli;
using beacon::Command;
using beacon::Flag;
using beacon::split;

// Records what the handlers were called with so dispatch can be asserted on.
struct Recorder {
  int message_calls = 0;
  int help_calls = 0;
  int user_calls = 0;
  strings_t last_message_args;
};

namespace {

Recorder g_recorder;

void handle_message(const strings_t& args) {
  ++g_recorder.message_calls;
  g_recorder.last_message_args = args;
}

void handle_help(const strings_t& args) {
  (void)args;
  ++g_recorder.help_calls;
}

void handle_user(const strings_t& args) {
  (void)args;
  ++g_recorder.user_calls;
}

// A CLI shared by the cases below, mirroring the set of commands the sketch
// registers.
Cli& fixture() {
  static Cli cli("beacon", beacon::LogLevel::INFO, "intro", "root help");
  static bool built = false;
  if (!built) {
    cli.add_command("message", "Send a message to every node in range.", handle_message);
    cli.add_command("user", "Show or change the local user profile.", handle_user);
    cli.add_command("help", "Show help, or help for one command.", handle_help);
    cli.get_command("message")->add_flag(
      Flag("quiet", 'q', "send without echoing the message locally"));
    built = true;
  }
  return cli;
}

}  // namespace


void test_cli() {
  arduino_stub::reset();
  g_recorder = Recorder();
  Cli& cli = fixture();

  TEST("cli: BUG-01 a registered handler actually runs");
  {
    cli.run_command(split("beacon message hello world"));
    // Before the fix this was 0: run() returned before reaching m_run(args).
    CHECK_EQ(g_recorder.message_calls, 1);

    // args[0] must be the command name so handlers can skip it, and the root
    // name must have been stripped on the way.
    CHECK_EQ(g_recorder.last_message_args.size(), 3u);
    CHECK_EQ(g_recorder.last_message_args[0], string_t("message"));
    CHECK_EQ(g_recorder.last_message_args[1], string_t("hello"));
    CHECK_EQ(g_recorder.last_message_args[2], string_t("world"));
  }

  TEST("cli: BUG-02 a bare command name does not crash");
  {
    // Before the fix: args.at(1) on a one-element vector. Uncatchable on the
    // device (aborts); here it surfaced as std::out_of_range.
    bool ok = true;
    try {
      cli.run_command(split("beacon help"));
    } catch (const std::exception&) {
      ok = false;
    }
    CHECK(ok);
    CHECK_EQ(g_recorder.help_calls, 1);

    try {
      cli.run_command(split("beacon message"));
    } catch (const std::exception&) {
      ok = false;
    }
    CHECK(ok);
    CHECK_EQ(g_recorder.message_calls, 2);
  }

  TEST("cli: the root name is optional");
  {
    // The boot banner suggests "/help", so a bare /command has to work as well
    // as /beacon command.
    arduino_stub::reset();
    g_recorder = Recorder();

    cli.run_command("/message hello");
    CHECK_EQ(g_recorder.message_calls, 1);

    cli.run_command("/help");
    CHECK_EQ(g_recorder.help_calls, 1);

    cli.run_command("message hello");
    CHECK_EQ(g_recorder.message_calls, 2);

    // The root on its own shows usage rather than running something.
    cli.run_command("/beacon");
    CHECK_EQ(g_recorder.message_calls, 2);
    CHECK(arduino_stub::out.find("Usage:") != std::string::npos);
  }

  TEST("cli: unknown commands are reported, not fatal");
  {
    arduino_stub::reset();

    bool ok = true;
    try {
      cli.run_command("/nope");
    } catch (const std::exception&) {
      ok = false;
    }
    CHECK(ok);
    CHECK(arduino_stub::out.find("is not a known command") != std::string::npos);
    // Usage follows, so the user can see what is available.
    CHECK(arduino_stub::out.find("Usage:") != std::string::npos);
  }

  TEST("cli: empty and malformed input is safe");
  {
    arduino_stub::reset();

    bool ok = true;
    try {
      cli.run_command(strings_t());
      cli.run_command(split(""));
      cli.run_command(split("/"));
      cli.run_command(split("   "));
      cli.print_help("does-not-exist");
    } catch (const std::exception&) {
      ok = false;
    }
    CHECK(ok);
  }

  TEST("cli: help text is complete");
  {
    arduino_stub::reset();

    // BUG-12: the description column used to print empty, and the help
    // message ended without its rule for leaf commands.
    const Command* message = cli.get_command("message");
    CHECK(message != nullptr);
    if (message != nullptr) {
      const string_t help = message->get_help_message();
      CHECK(help.find("message") != std::string::npos);
      CHECK(help.find("Send a message") != std::string::npos);
      CHECK(help.find("Usage:") != std::string::npos);
      CHECK(help.find("-----") != std::string::npos);
      // Flags must be documented.
      CHECK(help.find("Flags:") != std::string::npos);
      CHECK(help.find("--quiet") != std::string::npos);
      // The trailing rule must be present for a command with no subcommands.
      CHECK(help.size() > 70);
      CHECK_EQ(help[help.size() - 2], '-');
    }

    const string_t root_help = cli.get_root_ref().get_help_message();
    CHECK(root_help.find("Commands:") != std::string::npos);
    CHECK(root_help.find("message") != std::string::npos);
    CHECK(root_help.find("user") != std::string::npos);
    // Column padding must be consistent, which needs the width to be derived
    // rather than read from an uninitialized int. "message" is the longest
    // name, so it gets the minimum padding of two spaces.
    CHECK(root_help.find("message   Send") != std::string::npos);
    CHECK(root_help.find("user      Show") != std::string::npos);
  }

  TEST("cli: /help <command> shows that command's help");
  {
    arduino_stub::reset();
    cli.print_help("user");
    CHECK(arduino_stub::out.find("user") != std::string::npos);
    CHECK(arduino_stub::out.find("local user profile") != std::string::npos);
  }

  TEST("cli: subcommands route to the child");
  {
    arduino_stub::reset();

    Cli nested("root", beacon::LogLevel::INFO, "", "help");
    int child_calls = 0;
    nested.add_command("parent", "A parent", [](const strings_t& args) {
      (void)args;
    });
    nested.get_command("parent")->add_command("child", "A child",
                                              [&child_calls](const strings_t& args) {
      (void)args;
      ++child_calls;
    });

    nested.run_command("/parent child");
    CHECK_EQ(child_calls, 1);

    nested.run_command("/parent");
    // No subcommand given: usage, and no crash.
    CHECK_EQ(child_calls, 1);
    CHECK(arduino_stub::out.find("child") != std::string::npos);

    nested.run_command("/parent bogus");
    CHECK_EQ(child_calls, 1);
    CHECK(arduino_stub::out.find("is not a command of") != std::string::npos);
  }

  TEST("cli: flags");
  {
    arduino_stub::reset();

    Cli flagged("root", beacon::LogLevel::INFO, "", "help");
    flagged.add_command("message", "Send.", [](const strings_t& args) { (void)args; });
    Command* command = flagged.get_command("message");
    CHECK(command != nullptr);
    if (command == nullptr) {
      return;
    }
    command->add_flag(Flag("quiet", 'q', "suppress the echo"));

    const strings_t args = {"message", "--quiet", "hello", "there"};
    flags_t found = command->parse_flags(args);
    CHECK_EQ(found.size(), 1u);
    CHECK_EQ(found[0].get_name(), string_t("quiet"));

    // BUG-09a: parse_flags() used to fall off the end without returning.
    CHECK(command->has_flag(args, "quiet"));
    CHECK(!command->has_flag(args, "verbose"));

    // Short form and bare alias both match.
    CHECK(Flag("quiet", 'q', "").matches("-q"));
    CHECK(Flag("quiet", 'q', "").matches("--quiet"));
    CHECK(!Flag("quiet", 'q', "").matches("--loud"));
    CHECK(!Flag("quiet", 'q', "").matches("quiet"));

    Flag aliased("quiet", 'q', "");
    aliased.add_alias("silent");
    CHECK(aliased.matches("silent"));
    CHECK(!aliased.matches("loud"));

    flags_t flags;
    strings_t words;
    command->split_args(args, flags, words);
    CHECK_EQ(flags.size(), 1u);
    // The command name is not part of the message text.
    CHECK_EQ(words.size(), 2u);
    CHECK_EQ(words[0], string_t("hello"));
    CHECK_EQ(words[1], string_t("there"));

    // An unrecognised switch is dropped rather than becoming message text.
    const strings_t junk = {"message", "--bogus", "hi"};
    flags_t junk_flags;
    strings_t junk_words;
    command->split_args(junk, junk_flags, junk_words);
    CHECK_EQ(junk_flags.size(), 0u);
    CHECK_EQ(junk_words.size(), 1u);
    CHECK_EQ(junk_words[0], string_t("hi"));
  }

  TEST("cli: chained add_command registers siblings, not nested children");
  {
    // add_command() returns *this, not the new child. When it returned the
    // child, a chain like the one below made "user" a subcommand of "message",
    // so `/message hi` reported "hi is not a command of message" and every
    // other command vanished.
    Cli chained("beacon", beacon::LogLevel::INFO, "", "root help");
    int a_calls = 0;
    int b_calls = 0;
    chained
      .add_command("a", "first", [&a_calls](const strings_t& args) {
        (void)args;
        ++a_calls;
      })
      .add_command("b", "second", [&b_calls](const strings_t& args) {
        (void)args;
        ++b_calls;
      });

    // Both are direct children of the root.
    CHECK(chained.get_command("a") != nullptr);
    CHECK(chained.get_command("b") != nullptr);
    CHECK(!chained.get_command("a")->has_subcommands());
    CHECK(!chained.get_command("b")->has_subcommands());

    chained.run_command("/a");
    chained.run_command("/b");
    CHECK_EQ(a_calls, 1);
    CHECK_EQ(b_calls, 1);
  }

  TEST("cli: log level plumbing");
  {
    arduino_stub::reset();

    Cli cli("root", beacon::LogLevel::WARN, "", "help");
    CHECK_EQ(static_cast<int>(cli.get_level()), static_cast<int>(beacon::LogLevel::WARN));
    cli.set_level(beacon::LogLevel::TRACE);
    CHECK(cli.get_log().should_log(beacon::LogLevel::DEBUG));
    CHECK(cli.get_log().should_log(beacon::LogLevel::FATAL));
    cli.set_level(beacon::LogLevel::ERROR);
    CHECK(!cli.get_log().should_log(beacon::LogLevel::WARN));
    CHECK(cli.get_log().should_log(beacon::LogLevel::ERROR));
  }
}
