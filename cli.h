#pragma once

// Arduino.h is included here rather than relying on the sketch to have pulled
// it in first: the command framework writes to Serial, so it must be
// self-contained.
#include <Arduino.h>

#include <algorithm>
#include <string>
#include <vector>

#include "constants.h"
#include "log.h"
#include "types.h"
#include "util.h"

namespace beacon {

// A command or a flag that was not found, or input that could not be used.
// Handlers can throw or return one of these to report a problem uniformly.
class Error {
public:
  Error(const string_t& message, int code = -1) :
    m_code(code),
    m_message(message)
  {}

  const string_t& get_message() const { return m_message; }
  int get_code() const { return m_code; }

private:
  int m_code;
  string_t m_message;
};


// A boolean command-line switch, e.g. `--verbose` or `-v`.
class Flag {
public:
  Flag(const string_t& name, const char shortopt = '\0', const string_t& help = "") :
    m_name(name),
    m_short_description(help),
    m_shortopt(shortopt)
  {}

  const string_t& get_name() const { return m_name; }
  char get_shortopt() const { return m_shortopt; }
  const string_t& get_help() const { return m_short_description; }
  const strings_t& get_aliases() const { return m_aliases; }
  void add_alias(const string_t& alias) { m_aliases.emplace_back(alias); }

  // True when `token` is this flag written any of the accepted ways: "--name",
  // "-s", or a bare alias.
  bool matches(const string_t& token) const {
    if (token.size() > 2 && token.compare(0, 2, "--") == 0 &&
        token.substr(2) == m_name) {
      return true;
    }
    if (m_shortopt != '\0' && token.size() == 2 && token[0] == '-' &&
        token[1] == m_shortopt) {
      return true;
    }
    for (const auto& alias : m_aliases) {
      if (token == alias) {
        return true;
      }
    }
    return false;
  }

  // How the flag is displayed in help output, e.g. "-v, --verbose".
  string_t get_usage() const {
    string_t usage;
    if (m_shortopt != '\0') {
      usage += string_t("-") + m_shortopt + ", ";
    }
    usage += "--" + m_name;
    return usage;
  }

private:
  string_t m_name;
  string_t m_short_description;
  char m_shortopt;
  strings_t m_aliases;
};


class Command {
public:
  Command(const string_t& name, const string_t& help = "",
          entry_func_t func = [](const strings_t&){}) :
    m_name(name),
    m_help(help),
    m_run(func)
  {}

  const string_t& get_name() const { return m_name; }
  const string_t& get_help() const { return m_help; }
  const flags_t& get_flags() const { return m_flags; }
  const commands_t& get_commands() const { return m_commands; }
  bool has_subcommands() const { return !m_commands.empty(); }

  void set_run(const entry_func_t& func) { m_run = func; }
  void set_help(const string_t& help) { m_help = help; }

  // Run this command.
  //
  // `args[0]` is this command's own name; anything after it is user input.
  // A command that has subcommands routes to one of them instead of running
  // its own handler, which is how the root dispatches.
  void run(const strings_t& args) {
    if (!m_commands.empty()) {
      if (args.size() < 2) {
        // No subcommand given: show what is available rather than running a
        // handler with no input.
        print_help();
        return;
      }

      Command* child = get_command(args[1]);
      if (child == nullptr) {
        Serial.print(("error: '" + args[1] + "' is not a command of '" + m_name + "'.").c_str());
        Serial.println();
        print_help();
        return;
      }

      // Drop our own name so the child sees itself at args[0].
      child->run(copy(args, 1));
      return;
    }

    m_run(args);
  }

  // --- flags ---
  void add_flag(const Flag& flag) { m_flags.emplace_back(flag); }

  Flag* get_flag(const string_t& name) {
    const auto iter = std::find_if(
      m_flags.begin(), m_flags.end(),
      [&name](const Flag& f) { return f.get_name() == name; });

    return iter == m_flags.end() ? nullptr : &(*iter);
  }

  // Which of this command's flags appear in `args`.
  flags_t parse_flags(const strings_t& args) const {
    flags_t found;
    for (const auto& arg : args) {
      if (arg.empty()) {
        continue;
      }
      for (const auto& flag : m_flags) {
        if (flag.matches(arg)) {
          found.emplace_back(flag);
          break;
        }
      }
    }
    return found;
  }

  bool has_flag(const strings_t& args, const string_t& name) const {
    for (const auto& arg : args) {
      for (const auto& flag : m_flags) {
        if (flag.get_name() == name && flag.matches(arg)) {
          return true;
        }
      }
    }
    return false;
  }

  // Split `args` into the flags this command understands and the remaining
  // positional words, so a handler can tell `/message --quiet hi there` from
  // `/message hi --quiet there`.
  //
  // `args[0]` is the command name and is never included in `words_out`: it is
  // not user input, and including it would prepend a stray word to whatever the
  // handler is being asked to do.
  void split_args(const strings_t& args, flags_t& flags_out, strings_t& words_out) const {
    for (size_t i = 1; i < args.size(); ++i) {
      const string_t& arg = args[i];
      if (!arg.empty() && arg[0] == '-') {
        bool recognised = false;
        for (const auto& flag : m_flags) {
          if (flag.matches(arg)) {
            flags_out.emplace_back(flag);
            recognised = true;
            break;
          }
        }
        // An unrecognised switch is dropped too: it is never a message word,
        // and passing it on would corrupt the text being sent.
        if (!recognised) {
          continue;
        }
        continue;
      }
      if (!arg.empty()) {
        words_out.emplace_back(arg);
      }
    }
  }

  // --- subcommands ---
  // Returns *this, not the new child, so that a chain of calls registers
  // siblings:
  //     parent.add_command("a", ...).add_command("b", ...);
  // Returning the child here would make "b" a subcommand of "a", which is an
  // easy mistake to make and hard to notice.
  Command& add_command(const Command& command) {
    m_commands.emplace_back(command);
    return *this;
  }

  Command& add_command(const string_t& name, const string_t& help = "",
                       entry_func_t func = [](const strings_t&){}) {
    return add_command(Command(name, help, func));
  }

  Command* get_command(const string_t& name) {
    const auto iter = std::find_if(
      m_commands.begin(), m_commands.end(),
      [&name](const Command& c) { return c.get_name() == name; });

    return iter == m_commands.end() ? nullptr : &(*iter);
  }

  // --- help ---
  void print_help() const {
    Serial.println(get_help_message().c_str());
  }

  string_t get_usage() const {
    string_t usage = m_name;
    if (!m_commands.empty()) {
      usage += " <command>";
    }
    usage += " [args]...";
    if (!m_flags.empty()) {
      usage += " [flags]";
    }
    return usage;
  }

  string_t get_help_message() const {
    static const string_t rule(70, '-');

    string_t message = rule + "\n";
    message += m_name + ": " + m_help + "\n\n";
    message += "Usage:\n";
    message += "  " + get_usage() + "\n";
    message += "  /help " + m_name + "\n";

    if (!m_commands.empty()) {
      message += "\nCommands:\n";
      for (const auto& command : m_commands) {
        message += "  " + pad(command.get_name(), name_column_width()) +
                   command.get_help() + "\n";
      }
    }

    if (!m_flags.empty()) {
      message += "\nFlags:\n";
      for (const auto& flag : m_flags) {
        message += "  " + pad(flag.get_usage(), flag_column_width()) +
                   flag.get_help() + "\n";
      }
    }

    message += rule + "\n";
    return message;
  }

private:
  // Left-justify `text` in a field of `width` characters.
  static string_t pad(const string_t& text, size_t width) {
    if (text.size() >= width) {
      return text + " ";
    }
    return text + string_t(width - text.size() + 2, ' ');
  }

  // Widest entry in the command list, so columns line up, with a sane floor.
  size_t name_column_width() const {
    size_t width = 8;
    for (const auto& command : m_commands) {
      width = std::max(width, command.get_name().size());
    }
    return width;
  }

  size_t flag_column_width() const {
    size_t width = 8;
    for (const auto& flag : m_flags) {
      width = std::max(width, flag.get_usage().size());
    }
    return width;
  }

  string_t m_name;
  string_t m_help;
  entry_func_t m_run;
  flags_t m_flags;
  commands_t m_commands;
};


class Cli {
public:
  Cli(const string_t& root = "root", LogLevel level = LogLevel::INFO,
      const string_t& intro = "", const string_t& help = "") :
    m_intro(intro),
    m_log(Log(level)),
    m_root(Command(root, help))
  {}

  Command& get_root_ref() { return m_root; }
  const Command& get_root_ref() const { return m_root; }

  // Registers a command on the root and returns the Cli itself, so a chain of
  // calls registers siblings rather than nesting them.
  Cli& add_command(const Command& command) {
    m_root.add_command(command);
    return *this;
  }

  Cli& add_command(const string_t& name, const string_t& help = "",
                   entry_func_t func = [](const strings_t&){}) {
    return add_command(Command(name, help, func));
  }

  Command* get_command(const string_t& name) { return m_root.get_command(name); }

  Log& get_log() { return m_log; }
  const Log& get_log() const { return m_log; }

  // Run the command described by tokenized input.
  //
  // The root name is optional, so "/message hi" and "/beacon message hi" are
  // both accepted -- the boot banner suggests "/help", which would otherwise
  // print usage because "help" would be read as the root name.
  void run_command(const strings_t& args) {
    if (args.empty()) {
      m_root.print_help();
      return;
    }

    // Skip a leading root name if one was typed.
    const size_t offset = (args[0] == m_root.get_name()) ? 1 : 0;
    if (offset >= args.size()) {
      m_root.print_help();
      return;
    }

    const string_t name = strip_prefix(args[offset], '/');
    if (name.empty()) {
      m_root.print_help();
      return;
    }

    Command* command = m_root.get_command(name);
    if (command == nullptr) {
      Serial.print(("error: '" + name + "' is not a known command.").c_str());
      Serial.println();
      m_root.print_help();
      return;
    }

    command->run(copy(args, static_cast<int>(offset)));
  }

  void run_command(const string_t& line) {
    run_command(split(trim(line)));
  }

  void print_help() { m_root.print_help(); }

  // Show help for `command_name`, or for the root when it is empty.
  void print_help(const string_t& command_name) {
    if (command_name.empty()) {
      m_root.print_help();
      return;
    }

    Command* command = m_root.get_command(command_name);
    if (command == nullptr) {
      Serial.print(("error: '" + command_name + "' is not a known command.").c_str());
      Serial.println();
      m_root.print_help();
      return;
    }
    command->print_help();
  }

  void print_intro() const {
    Serial.println(m_intro.c_str());
  }

  void set_level(LogLevel level) { m_log.set_level(level); }
  LogLevel get_level() const { return m_log.get_level(); }

private:
  string_t m_intro;
  Log m_log;
  Command m_root;
};

}  // namespace beacon
