#pragma once

#include <ArduinoJson.h>

#include <ctime>
#include <string>

#include "json.h"
#include "message.h"
#include "types.h"
#include "user.h"

namespace beacon {

// The on-air envelope. This is what one LoRa packet carries:
// {"sender":{...},"receiver":{...},"message":{...}}
//
// Cost budget: the SX1276 FIFO caps a packet at 255 bytes and three UUIDs
// spend 108 of them, so there is room for only a short sentence. `radio.h`
// enforces the limit and reports it rather than truncating silently.
class Payload {
public:
  Payload() = default;

  Payload(const User& sender, const User& receiver, const Message& message,
          time_t sent = 0, time_t received = 0) :
    m_sender(sender),
    m_receiver(receiver),
    m_message(message),
    m_sent(sent),
    m_received(received)
  {}

  // Serialize to a JSON string, returned by value. The previous version
  // returned `const char*` into a function-local `std::string` that was
  // destroyed on return; see BUG-03.
  string_t serialize() const {
    JsonDocument doc;
    doc["sender"]["id"] = m_sender.get_id();
    doc["sender"]["name"] = m_sender.get_name();

    doc["receiver"]["id"] = m_receiver.get_id();
    doc["receiver"]["name"] = m_receiver.get_name();

    doc["message"]["id"] = m_message.get_id();
    doc["message"]["contents"] = m_message.get_contents();

    // Only meaningful once a clock source exists; omitted while zero so the
    // common case does not spend scarce packet bytes.
    if (m_sent != 0) {
      doc["sent"] = static_cast<long long>(m_sent);
    }
    if (m_received != 0) {
      doc["received"] = static_cast<long long>(m_received);
    }

    return beacon::serialize(doc);
  }

  // Parse a received envelope. Returns false and leaves this payload
  // untouched when the document is malformed or the fields have the wrong
  // types, so a corrupt packet cannot half-populate a message.
  bool deserialize(const string_t& stream) {
    JsonDocument doc;
    if (deserializeJson(doc, stream) != DeserializationError::Ok) {
      return false;
    }

    // Build into temporaries so a mid-way failure does not corrupt the
    // current state.
    User sender;
    User receiver;
    Message message;
    if (!sender.from_json(doc["sender"])) {
      return false;
    }
    if (!receiver.from_json(doc["receiver"])) {
      return false;
    }
    if (!message.from_json(doc["message"])) {
      return false;
    }

    m_sender = sender;
    m_receiver = receiver;
    m_message = message;
    m_sent = doc["sent"].is<long long>() ? static_cast<time_t>(doc["sent"].as<long long>()) : 0;
    m_received = doc["received"].is<long long>() ? static_cast<time_t>(doc["received"].as<long long>()) : 0;

    // A receiver with no ID means "every node". `User` starts life with a
    // generated UUID, so without this a received broadcast would carry a
    // random receiver ID and `addressed_to` would report it as directed at a
    // node that does not exist -- silently dropping every broadcast.
    if (doc["receiver"]["id"].is<const char*>()) {
      const char* raw = doc["receiver"]["id"].as<const char*>();
      if (raw == nullptr || raw[0] == '\0') {
        m_receiver.set_id(string_t());
      }
    }

    return true;
  }

  const User& get_sender() const { return m_sender; }
  const User& get_receiver() const { return m_receiver; }
  const Message& get_message() const { return m_message; }
  const string_t& get_text() const { return m_message.get_contents(); }
  time_t get_sent() const { return m_sent; }
  time_t get_received() const { return m_received; }

  // True when this payload is addressed to `user`, either by ID or by name.
  //
  // An empty receiver ID marks a broadcast (see `User::broadcast()`), which is
  // why that is checked before the name: the broadcast receiver is called "all",
  // and matching on the name alone would deliver it only to a node literally
  // named "all".
  bool addressed_to(const User& user) const {
    if (m_receiver.get_id().empty()) {
      return true;
    }
    if (m_receiver.get_id() == user.get_id()) {
      return true;
    }
    if (!m_receiver.get_name().empty() && m_receiver.get_name() == user.get_name()) {
      return true;
    }
    return false;
  }

private:
  User m_sender;
  User m_receiver;
  Message m_message;
  time_t m_sent = 0;
  time_t m_received = 0;
};

}  // namespace beacon
