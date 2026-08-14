// MKR 1310 sender
#include <SPI.h>
#include <LoRa.h>
#include <MKRWAN.h>
#include <ArduinoJson.h>

#include <vector>

#include "constants.h"
#include "payload.h"
#include "log.h"
#include "types.h"


char source = 0xBB;
char destination = 0xFF;
beacon::User user{"lora"};          // create a new default user profile (**MUST BE UPDATED AFTER POWER ON!!**)
beacon::Log logger;

enum class CommandType {
  None = 0,
  Local,
  Remote,
  Both
};

void setup(){

  // initialize Serial
  Serial.begin(SERIAL_BAUD_RATE);
  while(!Serial) {
    Serial.println(std::string(constants::intro_message + "\n\n"
      + "User: " + "\n"
      + "  ID:   " + user.get_id() + "\n"
      + "  Name: " + user.get_name() + "\n").data());

    Serial.println(R"(For help getting started, try '/help' and press enter.)");
  }

  // initialize LoRa
  if(!LoRa.begin(LORA_FREQUENCY)){
    Serial.println("failed to start LoRa");
    while(1);
  }

  // Generate a one-time, unique local address to use for device
  char source = LoRa.random();
}

void loop(){
  String line;
  while(Serial.available() > 0){
    delay(0);  // delay to allow buffer to fill
    char ch = Serial.read();
    line += ch;
    if(ch == '\n')
      break; 
  }

  if(line.length() > 0){
    // Handle input locally first
    std::string sline(line.c_str(), line.length());
    if (!sline.empty()) {
      sline.pop_back();
    }
    auto command_type = handle_local_command(sline);
    if (command_type == CommandType::Local) {
      return;
    } else if (command_type == CommandType::Remote || command_type == CommandType::Both) {
      // Create payload with message
      beacon::Payload payload(user, beacon::User{"nobody"}, beacon::Message(sline));

      // Print message locally in serial
      std::string prompt = user.get_name() + " -> ";
      Serial.print(String(prompt.data()));
      Serial.println(payload.serialize());


      // Send packet via LoRa
      LoRa.beginPacket();
      LoRa.println(payload.serialize());
      LoRa.endPacket();
    }
  }
  // in milliseconds
  delay(10);
}

// handle receiving LoRa packets
void on_receive_packet(int packet_size){

  if (user.get_radio_mode() != beacon::RadioMode::Receive
    || user.get_radio_mode() != beacon::RadioMode::Both)
    return;

  if (packet_size == 0)
    return;
  int recipient = LoRa.read();
  String incoming = "";
  while(LoRa.available()){
    incoming += (char)LoRa.read();      
  }

  if(recipient != source && recipient != 0xFF){
    Serial.println("This message is not for me.");
    return;
  }

  std::string prompt = user.get_name() + " (";
  Serial.print(prompt.data());
  Serial.print(LoRa.packetRssi());  
  Serial.print(" dBm) <- ");
  Serial.print(incoming);
  Serial.println();
}

CommandType handle_local_command(const string_t& prompt) {
  if (prompt.empty()) {
    return CommandType::None;
  } else if (prompt == "/help" || prompt[0] == '/') {
    show_local_help(tokenize(prompt));
    return CommandType::Local;
  } else {
    return CommandType::Remote;
  }
}

void show_local_help(const strings_t& args) {
  // show base help available
  string_t prompt = args[0];
  if (args.empty() || prompt == "/help") {
    Serial.println(R"(
----------------------------------------------------------------------
Run commands locally to update user profile or send messages remotely.

Usage:
  /<command> <args>... [options]

Commands:
  user                Update local user profile.
  message             Send a message.
----------------------------------------------------------------------
)");
    return;
  }

  // show user-related help message
  if (prompt == "/user") {
    Serial.println(R"(
----------------------------------------------------------------------
Update the local user profile information.

Usage:
  /user <command> <args>... [options]

Commands:
  name               Set the user's name.
  get                Get the user's name.
----------------------------------------------------------------------
)");
  }

  // show actions-related help message
  if (prompt == "/message") {
    Serial.println(R"(
----------------------------------------------------------------------
Update the local user profile information.

Usage:
  /message <content>
----------------------------------------------------------------------
)");
  }
}

strings_t tokenize(const std::string& line) {
  std::vector<string_t> v;

  // initializing variables
  int start, end;
  start = end = 0;

  // defining the delimitation character
  char dl = ' ';
  while ((start = line.find_first_not_of(dl, end))
           != string_t::npos) {
    // line.find(dl, start) will return the index of dl
    // from start index
    end = line.find(dl, start);
    // substr function return the substring of the
    // original string from the given starting index
    // to the given end index
    v.push_back(line.substr(start, end - start));
  }
  return v;
}
