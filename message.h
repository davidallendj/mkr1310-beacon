#pragma once

#include <UUID.h>

namespace beacon {

class Message {
public:
  Message(const std::string& contents) :
    m_contents(contents) 
  {
    // generate a new message ID
    UUID uuid;
    m_id = uuid.toCharArray();
  }

  const char* to_json() const {
    JsonDocument doc;
    std::string stream;

    doc["id"] = m_id;
    doc["contents"] = m_contents;

    serializeJson(doc, stream);
    return stream.data();
  }

  void from_json(const std::string& stream) {
    JsonDocument doc;
    deserializeJson(doc, stream);

    m_id = doc["id"].as<std::string>();
    m_contents = doc["contents"].as<std::string>();
  }

  const std::string& get_id() const { return m_id; }
  const std::string& get_contents() const { return m_contents; }

private:
  std::string m_id;
  std::string m_contents;
};

}