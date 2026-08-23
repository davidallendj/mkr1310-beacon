#pragma once

#include <UUID.h>

#include "types.h"

namespace beacon {

class Message {
public:
  Message(const string_t& contents) :
    m_contents(contents) 
  {
    // generate a new message ID
    UUID uuid;
    m_id = uuid.toCharArray();
  }


  const char* to_json() const {
    JsonDocument doc;
    string_t stream;

    doc["id"] = m_id;
    doc["contents"] = m_contents;

    serializeJson(doc, stream);
    return stream.data();
  }


  void from_json(const string_t& stream) {
    JsonDocument doc;
    deserializeJson(doc, stream);

    m_id = doc["id"].as<string_t>();
    m_contents = doc["contents"].as<string_t>();
  }

  const string_t& get_id() const { return m_id; }
  const string_t& get_id_short() const { return m_id.substr(0, 8); }
  const string_t& get_contents() const { return m_contents; }
  void print() const { Serial.println(m_contents.data()); }

private:
  string_t m_id;
  string_t m_contents;
};

bool validate_json(const char* input) {
  StaticJsonDocument<0> doc, filter;
  return deserializeJson(doc, input, DeserializationOption::Filter(filter)) == DeserializationError::Ok;
}

}