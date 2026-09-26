// BEACON for Arduino MKR 1310 WAN
//
// A thin serial client that exchanges short JSON messages with other nodes
// over LoRa. Type a message and press enter; anything that is not a command is
// sent to every node in range. Commands start with '/'.
//
// This file owns the serial port and nothing else. The node itself -- its
// identity, radio, commands and message handling -- lives in `session.h`, so
// that all of it can be exercised on a workstation by `test/run.sh`.
//
// See README.md for wiring and usage, BUGS.md for known problems, and
// TODO.md for what is still planned.

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#include "constants.h"
#include "line_reader.h"
#include "radio.h"
#include "session.h"
#include "types.h"

namespace {

beacon::Session session;
beacon::LineReader reader;

void read_serial_line() {
  while (Serial.available() > 0) {
    const int raw = Serial.read();
    if (raw < 0) {
      break;
    }

    // Line editing rules live in LineReader so they can be tested off-device.
    reader.feed(static_cast<char>(raw));

    string_t line;
    while (reader.next_line(line)) {
      session.feed(line);
    }
  }
}

}  // namespace


void setup() {
  Serial.begin(beacon::constants::serial_baud_rate);

  // Give the host a moment to attach so the banner is not missed, but do not
  // block boot indefinitely on a port nobody opened.
  const unsigned long start = millis();
  while (!Serial && (millis() - start) < 1500) {
    delay(10);
  }

  session.get_cli().print_intro();
  Serial.println();

  const bool radio_ok = session.setup();
  if (!radio_ok) {
    Serial.print("error: ");
    Serial.println(session.get_radio().get_error().c_str());
    Serial.println("The radio is not usable. Commands will not transmit or receive.");
  }

  const string_t& problem = session.get_error();
  if (!problem.empty()) {
    // Not fatal, but the node is not really usable as a participant until it is
    // fixed, so say so loudly.
    Serial.println("warning: constants::user_id is empty, so this node gets a new");
    Serial.println("         random ID on every reboot and cannot be told apart from");
    Serial.println("         another node. Set user_id in constants.h to:");
    Serial.print("         \"");
    Serial.println(session.get_user().get_id().c_str());
    Serial.println("\"");
  }

  session.print_status();
}


void loop() {
  // Handle radio traffic first so an incoming message is not stuck behind
  // serial input handling.
  session.poll();

  read_serial_line();

  // A small yield so the watchdog and the DIO0 interrupt are serviced promptly.
  delay(1);
}
