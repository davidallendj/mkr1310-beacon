#pragma once

#include <UUID.h>

namespace beacon {

enum class RadioMode {
  Send = 0,
  Receive,
  Both
};

enum class DisplayMode {
  Simple = 0,
  Full
};

class User {
public:
  User(const std::string& name) :
    m_name(name),
    m_radio_mode(RadioMode::Both),
    m_display_mode(DisplayMode::Full)
  {
    // generate a new message ID
    UUID uuid;
    m_id = uuid.toCharArray();
  }


  const char* to_json() const {
    JsonDocument doc;
    std::string stream;

    doc["id"] = m_id;
    doc["name"] = m_name;

    serializeJson(doc, stream);
    return stream.data();
  }


  void from_json(const std::string& stream) {
    JsonDocument doc;
    deserializeJson(doc, stream);

    m_id = doc["id"].as<std::string>();
    m_name = doc["name"].as<std::string>();
  }
  

  const std::string& get_id() const { return m_id; }
  const std::string& get_id_short() const { return m_id.substr(0, 8); }
  const std::string& get_name() const { return m_name; }
  const RadioMode& get_radio_mode() const { return m_radio_mode; }

private:
  std::string m_id;
  std::string m_name;
  RadioMode m_radio_mode;
  DisplayMode m_display_mode;
};

}