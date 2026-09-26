#pragma once

// Arduino.h is included here rather than relying on the sketch to have pulled
// it in first: Message::print() writes to Serial, so this must be
// self-contained.
#include <Arduino.h>

#include <ArduinoJson.h>
#include <UUID.h>

#include <string>

#include "json.h"
#include "types.h"

namespace beacon {

// A single text message plus the UUID that identifies it. The UUID is what a
// future dedup layer would key on, so it is carried on the wire.
class Message {
public:
  Message() = default;

  explicit Message(const string_t& contents) :
    m_id(generate_id()),
    m_contents(contents)
  {}

  // {"id": "...", "contents": "..."}
  string_t to_json() const {
    JsonDocument doc;
    doc["id"] = m_id;
    doc["contents"] = m_contents;
    return beacon::serialize(doc);
  }

  // Read a message from a JSON object. Returns false if `json` is not an
  // object, leaving this message untouched.
  bool from_json(JsonVariantConst json) {
    if (!json.is<JsonObjectConst>()) {
      return false;
    }
    const JsonObjectConst obj = json.as<JsonObjectConst>();

    // Only overwrite what is actually present, so a partial document does not
    // wipe a field we already have. A present-but-empty value is honoured.
    if (obj["id"].is<const char*>()) {
      const char* raw = obj["id"].as<const char*>();
      m_id = (raw == nullptr) ? string_t() : string_t(raw);
    }
    if (obj["contents"].is<const char*>()) {
      m_contents = obj["contents"].as<string_t>();
    }
    return true;
  }

  // Convenience entry point for callers holding a whole JSON document.
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

  // Return value: `substr` builds a temporary, which must not be bound to a
  // reference. See BUG-07.
  string_t get_id_short(size_t length = 8) const {
    return m_id.substr(0, length);
  }
  const string_t& get_id() const { return m_id; }
  const string_t& get_contents() const { return m_contents; }
  void set_contents(const string_t& contents) { m_contents = contents; }

  void print() const { Serial.println(m_contents.c_str()); }

private:
  static string_t generate_id() {
    UUID uuid;
    return string_t(uuid.toCharArray());
  }

  // Declaration order matters: the constructor initializes m_id first.
  string_t m_id;
  string_t m_contents;
};

}  // namespace beacon
