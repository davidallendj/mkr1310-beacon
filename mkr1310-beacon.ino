// MKR 1310 sender
#include <SPI.h>
#include <LoRa.h>
#include <MKRWAN.h>
#include <ArduinoJson.h>

#include <vector>
#include <numeric>

#include "constants.h"
#include "payload.h"
#include "log.h"
#include "types.h"
#include "cli.h"
#include "util.h"


beacon::User user{"lora"};          // create a new default user profile (**MUST BE UPDATED AFTER POWER ON!!**)
beacon::Log logger;
beacon::Cli cli(
"beacon",
beacon::LogLevel::DEBUG,
R"(
BEACON for Arduino MKR 1310 WAN
Software written by David J. Allen (davidallendj@gmail.com)
Repository: https://git.towk2.me/towk/mkr1310-beacon
GitHub Mirror: https://github.com/davidallendj/mkr1310-beacon
Copyright© 2026 David J. Allen. All rights reserved.)",
"Send structured JSON messages with LoRa to other devices."
);

void setup(){

  // initialize Serial
  Serial.begin(beacon::constants::serial_baud_rate);
  while(!Serial) {
    cli.print_intro();
    Serial.println("\n");
    Serial.println(std::string(
      "User: \n"
      "  ID:   " + user.get_id() + "\n"
      "  Name: " + user.get_name() + "\n"
      "\n"
      "For help getting started, try '/help' and press enter.").data());
  }

  // initialize LoRa
  if(!LoRa.begin(beacon::constants::lora_frequency)){
    Serial.println("error: failed to start LoRa");
    while(1);
  }

  // add all of the CLI commands here
  cli
    .add_command("user", "Update the local user profile information.", [](const strings_t& args){
      Serial.println("error: user profiles not implemented yet.");
    })
    .add_command("message", "Send a message through LoRa.", [&cli](const strings_t& args){
      // show the help message if no args provided
      if (args.size() == 1) {
        auto command = cli.get_command("message");
        if (command) {

        }
        return;
      }

      // create payload with message
      beacon::Payload payload(
        user,
        beacon::User{"nobody"}, 
        beacon::Message(std::accumulate(args.begin()+1, args.end(), string_t(" ")))
      );

      // print message locally in serial
      string_t prompt = user.get_name() + " -> ";
      Serial.print(String(prompt.data()));
      Serial.println(payload.serialize());

      // send packet via LoRa
      LoRa.beginPacket();
      LoRa.println(payload.serialize());
      LoRa.endPacket();
    })
    .add_command("log-level", "Set logging level.", [&cli](const strings_t& args){})
    .add_command("help", "Show help message.", [&cli](const strings_t& args){
      // show base help available
      if (args.size() == 1) {
        cli.print_help();
      }

      if (args.size() == 2) {
        string_t arg1 = args[1];

        // show user-related help when 'user' arg provided
        if (arg1 == "user") {
          auto command = cli.get_command("user");
          command->print_help();
        }

        // show message-related help when 'message' arg provided
        if (arg1 == "message") {
          auto command = cli.get_command("message");
          command->print_help();
        }
      }
    });
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
    // add a debug line showing the args
    string_t debug_line = "[debug]: " + string_t(line.c_str());
    Serial.println(debug_line.data());

    // handle input locally first
    string_t sline(line.c_str(), line.length());
    if (!sline.empty()) {
      sline.pop_back();
    }

    strings_t args = beacon::split(sline);
    cli.run_command(args);
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

  // TODO: determine if received message was intended for user
  bool valid_json = beacon::validate_json(incoming.c_str());
  if (valid_json) {
    
  }

  string_t prompt = user.get_name() + " (";
  Serial.print(prompt.data());
  Serial.print(LoRa.packetRssi());  
  Serial.print(" dBm) <- "); 

  Serial.print(incoming);
  Serial.println();
}
