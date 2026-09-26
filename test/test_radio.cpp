// End-to-end tests for the LoRa transport in radio.h / radio.cpp.
//
// These drive the same paths the firmware uses: a packet handed to the
// receive callback (as the DIO0 interrupt would), then poll() called from
// loop(). The stub radio keeps transmit and receive separate, so malformed,
// foreign and over-long packets can be injected.

#include "testing.h"

#include "Arduino.h"

#include "log.h"
#include "payload.h"
#include "radio.h"
#include "user.h"

using beacon::Log;
using beacon::LogLevel;
using beacon::Message;
using beacon::Payload;
using beacon::Radio;
using beacon::User;

namespace {

struct Fixture {
  User alice = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");
  User bob = make_user("bob", "bbbbbbbb-0000-4000-8000-000000000002");
  Log log;
  Radio radio;

  Fixture() : radio(alice, log) {
    LoRa.reset();
    LoRa.set_rssi(-87);
    arduino_stub::reset();
  }

  static User make_user(const string_t& name, const string_t& id) {
    User user(name);
    user.set_id(id);
    return user;
  }

  // Serialise a message as if it had come from `sender`.
  string_t wire_from(User& sender, const string_t& text) {
    const Payload payload(sender, User::broadcast(), Message(text));
    return payload.serialize();
  }
};

}  // namespace


void test_radio() {
  UUID::reset();

  TEST("radio: setup applies the air interface and hooks the interrupt");
  {
    Fixture f;
    CHECK(f.radio.setup());
    CHECK_EQ(f.radio.get_error(), string_t(""));

    // Without onReceive the receive path would silently never run; that was
    // the original BUG-04 companion problem.
    CHECK(LoRa.has_callback());
    CHECK_EQ(LoRa.frequency(), beacon::constants::lora_frequency);
    CHECK_EQ(LoRa.bandwidth(), beacon::constants::lora_bandwidth);
    CHECK_EQ(LoRa.spreading_factor(), beacon::constants::lora_spreading_factor);
    CHECK_EQ(LoRa.coding_rate(), beacon::constants::lora_coding_rate);
    CHECK_EQ(LoRa.sync_word(), beacon::constants::lora_sync_word);
    CHECK_EQ(LoRa.tx_power(), beacon::constants::lora_tx_power);
  }

  TEST("radio: setup reports a radio that will not start");
  {
    Fixture f;
    LoRa.set_begin_result(0);
    CHECK(!f.radio.setup());
    CHECK(f.radio.get_error().find("failed to start LoRa") != std::string::npos);
    LoRa.set_begin_result(1);
  }

  TEST("radio: BUG-05 framing is exact on transmit");
  {
    Fixture f;
    f.radio.setup();

    const string_t text = "hello there";
    const Payload payload(f.alice, User::broadcast(), Message(text));
    string_t error;
    CHECK(f.radio.transmit(f.radio.encode(payload), error));
    CHECK_EQ(error, string_t(""));

    // BUG-05: println() would have appended "\r\n", and the receiver used to
    // throw away the first byte as a "recipient" id, eating the leading '{'.
    CHECK_EQ(LoRa.last_tx(), f.radio.encode(payload));
    CHECK(LoRa.last_tx().find('\r') == std::string::npos);
    CHECK(LoRa.last_tx().find('\n') == std::string::npos);
    CHECK_EQ(LoRa.last_tx()[0], '{');
    CHECK_EQ(f.radio.get_sent(), 1u);
  }

  TEST("radio: BUG-04 receive mode gate admits Both and Receive");
  {
    // The original condition was `!= Receive || != Both`, which is always
    // true, so every packet was dropped.
    Fixture f;
    f.radio.setup();

    f.alice.set_radio_mode(beacon::RadioMode::Both);
    f.radio.poll();
    LoRa.inject(f.wire_from(f.bob, "gate test both"));
    f.radio.poll();
    CHECK_EQ(f.radio.get_accepted(), 1u);

    f.alice.set_radio_mode(beacon::RadioMode::Receive);
    f.radio.poll();
    LoRa.inject(f.wire_from(f.bob, "gate test receive"));
    f.radio.poll();
    CHECK_EQ(f.radio.get_accepted(), 2u);
  }

  TEST("radio: receive mode ignores traffic but still counts it");
  {
    Fixture f;
    f.radio.setup();
    f.alice.set_radio_mode(beacon::RadioMode::Send);

    LoRa.inject(f.wire_from(f.bob, "should be ignored"));
    f.radio.poll();
    CHECK_EQ(f.radio.get_ignored(), 1u);
    CHECK_EQ(f.radio.get_accepted(), 0u);
  }

  TEST("radio: an incoming packet is decoded and printed");
  {
    Fixture f;
    f.radio.setup();
    arduino_stub::reset();

    LoRa.set_rssi(-92);
    const string_t wire = f.wire_from(f.bob, "hi alice");
    CHECK(LoRa.inject(wire));
    f.radio.poll();

    CHECK_EQ(f.radio.get_accepted(), 1u);
    CHECK_EQ(f.radio.get_received(), 1u);
    CHECK_EQ(f.radio.last_rssi(), -92);
    CHECK(arduino_stub::out.find("hi alice") != std::string::npos);
    CHECK(arduino_stub::out.find("bob") != std::string::npos);
    CHECK(arduino_stub::out.find("-92 dBm") != std::string::npos);
    // The message is retained for /stats.
    CHECK_EQ(f.log.size(), 1u);
  }

  TEST("radio: poll does nothing until a packet has been received");
  {
    Fixture f;
    f.radio.setup();
    arduino_stub::reset();

    f.radio.poll();
    f.radio.poll();
    CHECK(arduino_stub::out.empty());
    CHECK_EQ(f.radio.get_received(), 0u);
  }

  TEST("radio: BUG-05 round trip is lossless");
  {
    // The exact failure that used to occur: send a payload, then feed the
    // transmitted bytes back in as if they had arrived, and check the text
    // survives.
    Fixture f;
    f.radio.setup();

    const string_t text = "round trip works";
    const Payload payload(f.alice, User::broadcast(), Message(text));
    string_t error;
    CHECK(f.radio.transmit(f.radio.encode(payload), error));

    // Our own packet coming back is suppressed, so re-inject it as bob's.
    f.bob.set_display_mode(beacon::DisplayMode::Full);
    const Payload from_bob(f.bob, User::broadcast(), Message(text));
    arduino_stub::reset();

    CHECK(LoRa.inject(from_bob.serialize()));
    f.radio.poll();
    CHECK_EQ(f.radio.get_accepted(), 1u);
    CHECK(arduino_stub::out.find(text) != std::string::npos);
  }

  TEST("radio: our own broadcast is not reported back to us");
  {
    Fixture f;
    f.radio.setup();
    arduino_stub::reset();

    const Payload mine(f.alice, User::broadcast(), Message("talking to myself"));
    CHECK(LoRa.inject(mine.serialize()));
    f.radio.poll();

    CHECK_EQ(f.radio.get_received(), 1u);
    CHECK_EQ(f.radio.get_accepted(), 0u);
    CHECK(arduino_stub::out.find("talking to myself") == std::string::npos);
  }

  TEST("radio: a packet meant for someone else is ignored");
  {
    Fixture f;
    f.radio.setup();
    arduino_stub::reset();

    const Payload to_bob(f.bob, f.bob, Message("private"));
    CHECK(LoRa.inject(to_bob.serialize()));
    f.radio.poll();

    CHECK_EQ(f.radio.get_ignored(), 1u);
    CHECK_EQ(f.radio.get_accepted(), 0u);
    CHECK(arduino_stub::out.find("private") == std::string::npos);
  }

  TEST("radio: malformed payloads are reported, not fatal");
  {
    Fixture f;
    f.radio.setup();

    // Not JSON.
    arduino_stub::reset();
    LoRa.inject("this is not json at all");
    f.radio.poll();
    CHECK_EQ(f.radio.get_malformed(), 1u);
    CHECK(arduino_stub::out.find("not valid JSON") != std::string::npos);

    // Valid JSON of the wrong shape.
    arduino_stub::reset();
    LoRa.inject("{\"totally\":\"unrelated\"}");
    f.radio.poll();
    CHECK_EQ(f.radio.get_malformed(), 2u);
    CHECK(arduino_stub::out.find("unexpected shape") != std::string::npos);

    // Truncated JSON.
    arduino_stub::reset();
    const string_t wire = f.wire_from(f.bob, "a somewhat longer message here");
    LoRa.inject(wire.substr(0, wire.size() / 2));
    f.radio.poll();
    CHECK_EQ(f.radio.get_malformed(), 3u);

    // The node keeps working afterwards.
    arduino_stub::reset();
    LoRa.inject(f.wire_from(f.bob, "still alive"));
    f.radio.poll();
    CHECK_EQ(f.radio.get_accepted(), 1u);

    // Diagnostics must not appear in the message history: that store is meant
    // to hold messages the user actually received, so /stats does not list
    // parse errors alongside real traffic.
    CHECK_EQ(f.log.size(), 1u);
    if (const beacon::Message* held = f.log.get_message(0)) {
      CHECK_EQ(held->get_contents(), string_t("still alive"));
    }
  }

  TEST("radio: an empty packet is discarded");
  {
    Fixture f;
    f.radio.setup();
    arduino_stub::reset();

    LoRa.inject("");
    f.radio.poll();
    CHECK_EQ(f.radio.get_received(), 0u);
    CHECK_EQ(f.radio.get_malformed(), 0u);
  }

  TEST("radio: a packet longer than the buffer is dropped, not overflowed");
  {
    Fixture f;
    f.radio.setup();
    const uint32_t before = f.radio.get_dropped();

    // The FIFO can deliver at most 255 bytes; inject a pathological size to
    // prove the guard works without relying on the hardware limit.
    LoRa.push(std::string(600, 'x'));
    LoRa.inject(std::string(600, 'x'));

    f.radio.poll();
    CHECK(f.radio.get_dropped() > before);
    CHECK_EQ(f.radio.get_accepted(), 0u);

    // The FIFO was drained, so a good packet still works.
    arduino_stub::reset();
    LoRa.inject(f.wire_from(f.bob, "after the flood"));
    f.radio.poll();
    CHECK_EQ(f.radio.get_accepted(), 1u);
  }

  TEST("radio: an over-long message is refused with a real byte count");
  {
    Fixture f;
    f.radio.setup();

    // 255 bytes is the hard ceiling, and the envelope already spends ~150.
    const string_t too_long(400, 'a');
    const Payload payload(f.alice, User::broadcast(), Message(too_long));
    const string_t packet = f.radio.encode(payload);
    CHECK(packet.size() > 255u);

    string_t error;
    CHECK(!f.radio.transmit(packet, error));
    CHECK(error.find("too long") != std::string::npos);
    // The message must name the actual sizes, not just say "too long".
    CHECK(error.find("255") != std::string::npos);
    CHECK(error.find(beacon::to_str(static_cast<long>(packet.size()))) != std::string::npos);
    // Nothing was put on the air.
    CHECK_EQ(LoRa.packets_sent(), 0);
    CHECK_EQ(f.radio.get_sent(), 0u);

    // A message that fits is accepted, and the reported budget is truthful.
    const size_t budget = f.radio.max_text_bytes();
    CHECK(budget > 0u);
    const string_t fits(budget, 'b');
    const Payload ok(f.alice, User::broadcast(), Message(fits));
    string_t ok_error;
    CHECK(f.radio.transmit(f.radio.encode(ok), ok_error));
    CHECK_EQ(ok_error, string_t(""));
  }

  TEST("radio: the size guard is exact at the boundary");
  {
    // Three UUIDs plus the JSON keys spend most of the 255-byte FIFO. The
    // reported budget must be exact, not an estimate: one character too many
    // has to be refused rather than silently truncated on the air.
    Fixture f;
    f.radio.setup();
    const size_t budget = f.radio.max_text_bytes();
    CHECK_EQ(budget, static_cast<size_t>(80));

    string_t error;
    const Payload at_limit(f.alice, User::broadcast(),
                           Message(string_t(budget, 'x')));
    CHECK_EQ(f.radio.encode(at_limit).size(), static_cast<size_t>(255));
    CHECK(f.radio.transmit(f.radio.encode(at_limit), error));

    const Payload over(f.alice, User::broadcast(),
                       Message(string_t(budget + 1, 'x')));
    CHECK_EQ(f.radio.encode(over).size(), static_cast<size_t>(256));
    CHECK(!f.radio.transmit(f.radio.encode(over), error));
    CHECK(error.find("too long") != std::string::npos);

    // One character over must not have put anything on the air.
    CHECK_EQ(f.radio.get_sent(), 1u);
  }

  TEST("radio: sending is refused when the radio mode says so");
  {
    Fixture f;
    f.radio.setup();
    f.alice.set_radio_mode(beacon::RadioMode::Receive);

    const Payload payload(f.alice, User::broadcast(), Message("nope"));
    string_t error;
    CHECK(!f.radio.transmit(f.radio.encode(payload), error));
    CHECK(error.find("sending is disabled") != std::string::npos);
    CHECK_EQ(f.radio.get_sent(), 0u);
  }

  TEST("radio: a busy radio is reported instead of silently dropped");
  {
    Fixture f;
    f.radio.setup();

    const Payload payload(f.alice, User::broadcast(), Message("busy"));
    string_t error;

    LoRa.set_begin_packet_result(0);
    CHECK(!f.radio.transmit(f.radio.encode(payload), error));
    CHECK(error.find("busy") != std::string::npos);

    LoRa.set_begin_packet_result(1);
    LoRa.set_end_packet_result(0);
    CHECK(!f.radio.transmit(f.radio.encode(payload), error));
    CHECK(error.find("did not finish") != std::string::npos);

    LoRa.set_end_packet_result(1);
    CHECK(f.radio.transmit(f.radio.encode(payload), error));
  }

  TEST("radio: an empty packet is never transmitted");
  {
    Fixture f;
    f.radio.setup();
    string_t error;
    CHECK(!f.radio.transmit(string_t(), error));
    CHECK(error.find("empty") != std::string::npos);
    CHECK_EQ(LoRa.packets_sent(), 0);
  }

  TEST("radio: display mode changes how much is shown");
  {
    Fixture f;
    f.radio.setup();
    f.alice.set_display_mode(beacon::DisplayMode::Full);
    arduino_stub::reset();
    LoRa.inject(f.wire_from(f.bob, "detailed"));
    f.radio.poll();
    CHECK(arduino_stub::out.find("bbbbbbbb") != std::string::npos);

    f.alice.set_display_mode(beacon::DisplayMode::Simple);
    arduino_stub::reset();
    LoRa.inject(f.wire_from(f.bob, "terse"));
    f.radio.poll();
    CHECK(arduino_stub::out.find("bbbbbbbb") == std::string::npos);
    CHECK(arduino_stub::out.find("terse") != std::string::npos);
  }

  TEST("radio: back-to-back packets are all delivered");
  {
    Fixture f;
    f.radio.setup();
    arduino_stub::reset();

    for (int i = 0; i < 5; ++i) {
      LoRa.inject(f.wire_from(f.bob, "packet " + beacon::to_str(i)));
      // poll() from loop() runs between arrivals, as it would on the device.
      f.radio.poll();
    }
    CHECK_EQ(f.radio.get_accepted(), 5u);
    CHECK_EQ(f.log.size(), 5u);
    for (int i = 0; i < 5; ++i) {
      const string_t expected = "packet " + beacon::to_str(i);
      CHECK(arduino_stub::out.find(expected) != std::string::npos);
    }
  }

  TEST("radio: interrupt bookkeeping");
  {
    Fixture f;
    f.radio.setup();

    // The receive path must not leave interrupts disabled: poll() brackets its
    // state swap with noInterrupts()/interrupts(), and if that leaked the USB
    // stack would stop responding.
    LoRa.inject(f.wire_from(f.bob, "check interrupts"));
    CHECK_EQ(arduino_stub::interrupts_enabled, 1);
    f.radio.poll();
    CHECK_EQ(arduino_stub::interrupts_enabled, 1);
  }
}
