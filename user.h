#pragma once

#include <ArduinoJson.h>
#include <UUID.h>

#include <string>

#include "constants.h"
#include "json.h"
#include "types.h"
#include "util.h"

namespace beacon {

// What the radio should do. `Both` is the default so a node participates in
// conversations immediately.
enum class RadioMode {
  Send = 0,
  Receive,
  Both
};

// How much detail to show when a message is printed.
enum class DisplayMode {
  Simple = 0,
  Full
};

inline const char* to_string(RadioMode mode) {
  switch (mode) {
    case RadioMode::Send:    return "send";
    case RadioMode::Receive: return "receive";
    case RadioMode::Both:    return "both";
  }
  return "both";
}

inline const char* to_string(DisplayMode mode) {
  switch (mode) {
    case DisplayMode::Simple: return "simple";
    case DisplayMode::Full:   return "full";
  }
  return "full";
}

// Parse a mode name, case-insensitively. Returns false for anything else, so
// callers can report bad input instead of silently defaulting.
inline bool radio_mode_from_string(const string_t& text, RadioMode& out) {
  const string_t value = to_lower(trim(text));
  if (value == "send")    { out = RadioMode::Send;    return true; }
  if (value == "receive") { out = RadioMode::Receive; return true; }
  if (value == "both")    { out = RadioMode::Both;    return true; }
  return false;
}

inline bool display_mode_from_string(const string_t& text, DisplayMode& out) {
  const string_t value = to_lower(trim(text));
  if (value == "simple") { out = DisplayMode::Simple; return true; }
  if (value == "full")   { out = DisplayMode::Full;   return true; }
  return false;
}

// A participant in the network: a stable identity plus local preferences.
class User {
public:
  // A user with no name generates a fresh random UUID. A user constructed from
  // a configured ID keeps it, which is what makes the node addressable across
  // reboots.
  User() :
    m_id(generate_id()),
    m_name(""),
    m_radio_mode(RadioMode::Both),
    m_display_mode(DisplayMode::Full)
  {}

  explicit User(const string_t& name) :
    m_id(generate_id()),
    m_name(name),
    m_radio_mode(RadioMode::Both),
    m_display_mode(DisplayMode::Full)
  {}

  // Adopt a configured identity. An empty `id` is ignored so that a partially
  // filled configuration still gets a usable random UUID.
  User(const string_t& name, const string_t& id) :
    m_name(name),
    m_radio_mode(RadioMode::Both),
    m_display_mode(DisplayMode::Full)
  {
    m_id = id.empty() ? generate_id() : id;
  }

  // The pseudo-recipient of a broadcast. No ID, plus the reserved name, so
  // `Payload::addressed_to` treats it as matching every node. Built once and
  // reused rather than generated per message, to avoid burning a UUID on
  // every send.
  static User broadcast() {
    User user;
    user.m_id.clear();
    user.m_name = beacon::constants::broadcast_name;
    return user;
  }

  // {"id": "...", "name": "...", "radio_mode": "both", "display_mode": "full"}
  string_t to_json() const {
    JsonDocument doc;
    doc["id"] = m_id;
    doc["name"] = m_name;
    doc["radio_mode"] = to_string(m_radio_mode);
    doc["display_mode"] = to_string(m_display_mode);
    return beacon::serialize(doc);
  }

  // Read a user from a JSON object. Returns false if `json` is not an object.
  // Absent fields keep their current value.
  bool from_json(JsonVariantConst json) {
    if (!json.is<JsonObjectConst>()) {
      return false;
    }
    const JsonObjectConst obj = json.as<JsonObjectConst>();

    // A key that is present but empty is honoured, because an empty ID is
    // meaningful: it is how `User::broadcast()` marks a message as having no
    // intended recipient. Only an absent or non-string key leaves the current
    // value alone.
    if (obj["id"].is<const char*>()) {
      const char* raw = obj["id"].as<const char*>();
      m_id = (raw == nullptr) ? string_t() : string_t(raw);
    }
    if (obj["name"].is<const char*>()) {
      m_name = obj["name"].as<string_t>();
    }

    RadioMode radio = m_radio_mode;
    if (radio_mode_from_string(obj["radio_mode"].as<string_t>(), radio)) {
      m_radio_mode = radio;
    }
    DisplayMode display = m_display_mode;
    if (display_mode_from_string(obj["display_mode"].as<string_t>(), display)) {
      m_display_mode = display;
    }
    return true;
  }

  // Deliberately a different name: an overload taking `string_t` would be
  // ambiguous with the `JsonVariantConst` one, because ArduinoJson's
  // `doc["key"]` proxy converts to both.
  bool from_json_string(const string_t& stream) {
    JsonDocument doc;
    if (deserializeJson(doc, stream) != DeserializationError::Ok) {
      return false;
    }
    return from_json(doc.as<JsonVariantConst>());
  }

  // Return value, not a reference: `substr` returns a temporary. See BUG-07.
  string_t get_id_short(size_t length = 8) const {
    return m_id.substr(0, length);
  }
  const string_t& get_id() const { return m_id; }
  const string_t& get_name() const { return m_name; }
  RadioMode get_radio_mode() const { return m_radio_mode; }
  DisplayMode get_display_mode() const { return m_display_mode; }

  void set_name(const string_t& name) { m_name = name; }
  void set_id(const string_t& id) { m_id = id; }
  void set_radio_mode(RadioMode mode) { m_radio_mode = mode; }
  void set_display_mode(DisplayMode mode) { m_display_mode = mode; }

  bool can_send() const { return m_radio_mode != RadioMode::Receive; }
  bool can_receive() const {
    return m_radio_mode == RadioMode::Receive || m_radio_mode == RadioMode::Both;
  }

private:
  static string_t generate_id() {
    UUID uuid;
    return string_t(uuid.toCharArray());
  }

  string_t m_id;
  string_t m_name;
  RadioMode m_radio_mode;
  DisplayMode m_display_mode;
};

}  // namespace beacon
