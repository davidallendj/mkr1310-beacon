#include <functional>
#pragma once

#include <algorithm>
#include <memory>

#include "constants.h"
#include "types.h"
#include "util.h"

// example:
//
//  beacon::Cli cli("example");
//  cli.add_command(beacon::Command("user", "Update the local user profile information.", [](const strings_t& args){
//   Serial.println("error: user profiles not implemented yet.");
//  }))
//  std::vector<std::string> args = beacon::tokenize(line);
//  strings_t::const_iterator begin = args.begin() + 1;
//  strings_t::const_iterator end = args.end();
//  strings_t argv(begin, end);
//  cli.run_command(args[0], argv);

namespace beacon {

class Error {
public:
  Error(const string_t& message, int code = -1) :
    m_message(message),
    m_code(code)
  {}

  const string_t& get_message() const { return m_message; }
  int get_code() const { return m_code; }

private:
  int m_code;
  string_t m_message;
};


class Flag {
public:
  Flag(const string_t& name, const char shortopt = ' ', const string_t& help = "") :
    m_name(name),
    m_short_description(help),
    m_shortopt(shortopt)
  {}

  const string_t& get_name() const { return m_name; }
  const strings_t& get_aliases() const { return m_aliases; }

private:
  string_t m_name;
  string_t m_short_description;
  string_t m_long_description;
  string_t m_longopt;
  strings_t m_aliases;
  char m_shortopt;
};


class Command {
public:
  Command(const string_t& name, const string_t& help = "", entry_func_t func = [](const strings_t&){}) :
    m_name(name),
    m_help(help),
    m_run(func)
  {}

  // base functions
  void run(const strings_t& args) {
    // handle no arguments
    if (args.empty()) {
      print_help();
      return;
    }

    // get next command and process
    Command *next = get_command(args.at(0));
    if (next) {
      if (next->get_name() == args.at(1)) {
        Serial.println(string_t("next -> " + next->get_name()).data());
      }
      return;
    }

    // otherwise, run the given function for this command
    m_run(args); 
  }
  void set_run(const entry_func_t& func) { m_run = func; }
  void set_help(const string_t& help) { m_help = help; }
  const string_t& get_name() const { return m_name; }
  const string_t& get_help() const { return m_help; }
  void print_help() const {
    Serial.println(get_help_message().data());
  }
  string_t get_help_message() const {
    string_t message = "----------------------------------------------------------------------\n";
    message += m_description + "\n\n";
    message += "Usage:\n";
    message += "  /" + m_name + " <args>... [flags]\n";
    message += "  /help " + m_name + "\n";
    message += "\n";

    if (!m_commands.empty()) {
      message += "Commands:\n";
      for(const auto& command : m_commands) {
        const int padding = m_column_size - command.get_name().size();
        string_t px = string_t(" ", padding);
        message += "  " + command.get_name() + px + "            " + command.get_help() + "\n";
      }
      message += "----------------------------------------------------------------------\n";
    }
    return message;
  }

  // flag functions
  void add_flag(const Flag& flag) { m_flags.emplace_back(flag); }
  void remove_flag(const string_t& flag_name) {}
  Flag* get_flag(const string_t& flag_name) {
    const auto iter = std::find_if(
      m_flags.begin(), 
      m_flags.end(), 
      [&flag_name](const Flag& flag){ return flag.get_name() == flag_name; }
    );
    if (iter != m_flags.end()) {
      return &(*iter);
    } else {
      return nullptr;
    }
  }

  // command functions
  Command& add_command(const Command& command) { 
    m_commands.emplace_back(command); 
    return *this;
  }

  Command& add_command(const string_t& name, const string_t& help = "", entry_func_t func = [](const strings_t&){}) {
    return add_command(Command(name, help, func));
  }

  void exec(const strings_t& args, const char prefix = ' ') {
    // requires at least one arg
    if (args.empty()) {

    }

    // check if name is empty and stop if it is
    string_t name = args[0];
    if (name.empty()) {
      return;
    }

    // if prefix is set, check for it and remove from incoming name
    if (name[0] == prefix) {
      name = name.substr(1);
    }

    // find the command by it's name
    Command *command = get_command(name);
    if (command != nullptr) {
      command->run(args);
    } else {
      string_t message = "error: '" + name + "' is an invalid command or not found.";
      Serial.println(message.data());
    }
  }

  Command* get_command(const string_t& name) {
    std::function<bool(const Command& c)> func = [&name](const Command& c) { return c.get_name() == name; };
    return find_if(m_commands, func);
  }

  flags_t parse_flags(const strings_t& args) {
    flags_t flags;
    for (const auto& arg : args) {
      // skip processing if arg is empty
      if (arg.empty()) {
        continue;
      }

      // check if arg matches longopt first
      if (arg.size() > 2) {
        if (arg.substr(0, 2) == "--") {
          // match; add the found flag to collection
          const string_t flag_name = arg.substr(2);
          std::function<bool(const Flag&)> func = [&flag_name](const Flag& f) { return f.get_name() == flag_name; };
          Flag *flag = find_if(m_flags, func);
          if (flag) {
            flags.emplace_back(*flag);
          }
        }
        // process next arg since we're done with this one
        continue;
      }

      // check if arg matches shortopt
      if (arg.size() > 1) {
        if (arg.substr(0, 1) == "-") {
          const string_t flag_name = arg.substr(1);
          std::function<bool(const Flag&)> func = [&flag_name](const Flag& f) { return f.get_name() == flag_name; };
          Flag *flag = find_if(m_flags, func);
          if (flag) {
            flags.emplace_back(*flag);
          }
        }
      }
    }
  }
  
private:
  string_t m_name;
  string_t m_description;
  string_t m_help;
  int m_column_size;
  entry_func_t m_run;
  flags_t m_flags;
  commands_t m_commands;
};


class Cli {
public:
  Cli(const string_t& root = "root", LogLevel level = LogLevel::INFO, const string_t& intro = "", const string_t& help = "") : 
    m_intro(intro),
    m_log(Log(level)),
    m_root(Command(root, help))
  {}

  Command& get_root_ref() { return m_root; }
  Command& add_command(Command&& command) { 
    m_root.add_command(command); 
    return command;
  }
  Command& add_command(const string_t& name, const string_t& help = "", entry_func_t func = [](const strings_t&){}) {
    return add_command(Command(name, help, func));
  }
  Command* get_command(const string_t& name) { return m_root.get_command(name); }
  void run_command(const strings_t& args) {
    // first arg must be root command
    string_t command = args[0];
    if (command == m_root.get_name()) {
      // remove the root and pass in what's left
      auto argv = beacon::copy(args, 1);
      m_root.run(argv);
    } else {
      print_help();
    }
  }

  void print_help(const string_t& command_name = "") {
    if (command_name.empty()) {
      m_root.print_help();
      return;
    }
    Command *command = get_command(command_name);
    command->print_help();
  }

  void print_intro() const {
    Serial.println(m_intro.data());
  }

private:
  string_t m_intro;
  flags_t m_flags;
  log_t m_log;
  Command m_root;
};

} // namespace beacon


