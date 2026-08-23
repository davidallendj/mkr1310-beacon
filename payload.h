#pragma once

#include <ArduinoJson.h>

#include "user.h"
#include "message.h"

namespace beacon {

class Payload {
public:
  Payload(const User& sender, const User& receiver, const Message& message, time_t sent = 0, time_t received = 0) :
    m_sender(sender),
    m_receiver(receiver),
    m_message(message),
    m_sent(sent),
    m_received(received)
  {}


  // convert payload into 1-dimensional bytes of JSON
  const char* serialize() const {
    JsonDocument doc;
    std::string stream;

    doc["sender"]["id"] = m_sender.get_id();
    doc["sender"]["name"] = m_sender.get_name();
    // doc["receiver"]["id"] = m_receiver.get_id();
    doc["receiver"]["name"] = m_receiver.get_name();
    doc["message"]["id"] = m_message.get_id();
    doc["message"]["contents"] = m_message.get_contents();
    
    serializeJson(doc, stream);
    return stream.data();
  }


  void deserialize(const char* stream) {
    JsonDocument doc;
    deserializeJson(doc, stream);

    m_sender.from_json(doc["sender"].as<std::string>());
    m_receiver.from_json(doc["receiver"].as<std::string>());
    m_message.from_json(doc["message"].as<std::string>());
  }


private:
  User m_sender;
  User m_receiver;
  Message m_message;
  time_t m_sent;
  time_t m_received;
};

}