// Tests for the JSON wire format: payload.h, user.h, message.h, json.h.

#include "testing.h"

#include "Arduino.h"

#include "json.h"
#include "log.h"
#include "message.h"
#include "payload.h"
#include "user.h"

using beacon::Message;
using beacon::Payload;
using beacon::User;
using beacon::validate_json;

namespace {

beacon::User make_user(const string_t& name, const string_t& id) {
  User user(name);
  user.set_id(id);
  return user;
}

}  // namespace


void test_payload() {
  TEST("payload: BUG-03 serialize returns a usable string");
  {
    UUID::reset();
    const User alice = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    const Message message("hello there");
    const Payload payload(alice, User::broadcast(), message);

    // The old signature returned `const char*` into a destroyed local, so this
    // pointer was already dangling. Returning by value makes it safe to hold,
    // copy, and read as often as needed.
    const string_t first = payload.serialize();
    const string_t second = payload.serialize();
    CHECK_EQ(first, second);
    CHECK(!first.empty());
    CHECK(validate_json(first));
  }

  TEST("payload: round trip");
  {
    UUID::reset();
    const User alice = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    const User bob = make_user("bob", "bbbbbbbb-0000-4000-8000-000000000002");
    const Message message("the quick brown fox");

    Payload sent(alice, bob, message);
    const string_t wire = sent.serialize();

    Payload received;
    CHECK(received.deserialize(wire));

    CHECK_EQ(received.get_sender().get_id(), string_t("aaaaaaaa-0000-4000-8000-000000000001"));
    CHECK_EQ(received.get_sender().get_name(), string_t("alice"));
    CHECK_EQ(received.get_receiver().get_name(), string_t("bob"));
    CHECK_EQ(received.get_text(), string_t("the quick brown fox"));
    // BUG-08: the message ID used to be lost because the object was read with
    // as<std::string>() and came back empty.
    CHECK_EQ(received.get_message().get_id(), sent.get_message().get_id());
  }

  TEST("payload: BUG-08 malformed input is rejected, not half-applied");
  {
    UUID::reset();
    const User alice = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    Payload target(alice, User::broadcast(), Message("original"));

    // Valid JSON, wrong shape.
    CHECK(!target.deserialize("{\"nonsense\":true}"));
    CHECK_EQ(target.get_text(), string_t("original"));

    // Not JSON at all.
    CHECK(!target.deserialize("this is not json"));
    CHECK(!target.deserialize(""));
    CHECK_EQ(target.get_text(), string_t("original"));

    // Truncated JSON.
    const Payload source(alice, User::broadcast(), Message("fresh"));
    const string_t wire = source.serialize();
    CHECK(!target.deserialize(wire.substr(0, wire.size() / 2)));
    CHECK_EQ(target.get_text(), string_t("original"));
  }

  TEST("payload: BUG-06 validate_json");
  {
    // Before the fix, an empty filter document was passed to
    // DeserializationOption::Filter and StaticJsonDocument<0> was deprecated.
    CHECK(validate_json("{\"a\":1}"));
    CHECK(validate_json("[1,2,3]"));
    CHECK(validate_json("\"text\""));
    CHECK(validate_json("null"));

    CHECK(!validate_json(""));
    CHECK(!validate_json("{"));
    CHECK(!validate_json("{\"a\":}"));
    CHECK(!validate_json("not json"));
    CHECK(!validate_json(static_cast<const char*>(nullptr)));
  }

  TEST("payload: BUG-07 get_id_short returns a value, not a dangling reference");
  {
    const User user = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    // Both getters used to be `const std::string&` bound to a substr() result.
    const string_t short_id = user.get_id_short();
    CHECK_EQ(short_id, string_t("aaaaaaaa"));
    CHECK_EQ(short_id.size(), 8u);
    // Still valid after the User it came from is gone.
    string_t escaped;
    {
      const Message message("gone soon");
      escaped = message.get_id_short();
    }
    CHECK_EQ(escaped.size(), 8u);

    // An ID shorter than the requested length must not throw or read past the
    // end.
    User tiny;
    tiny.set_id("ab");
    CHECK_EQ(tiny.get_id_short(), string_t("ab"));
  }

  TEST("payload: special characters survive the round trip");
  {
    UUID::reset();
    const User alice = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");

    // Quotes, backslashes and newlines all have to be escaped by the encoder
    // and unescaped by the decoder, or the JSON is malformed.
    const string_t tricky = "say \"hi\"\\ then\nnewline";
    Payload sent(alice, User::broadcast(), Message(tricky));
    const string_t wire = sent.serialize();
    CHECK(validate_json(wire));

    Payload received;
    CHECK(received.deserialize(wire));
    CHECK_EQ(received.get_text(), tricky);
  }

  TEST("payload: UTF-8 in names and text");
  {
    UUID::reset();
    const User named = make_user("caf\xc3\xa9", "aaaaaaaa-0000-4000-8000-000000000001");
    Payload sent(named, User::broadcast(), Message("ol\xc3\xa1"));
    const string_t wire = sent.serialize();
    CHECK(validate_json(wire));

    Payload received;
    CHECK(received.deserialize(wire));
    CHECK_EQ(received.get_sender().get_name(), string_t("caf\xc3\xa9"));
    CHECK_EQ(received.get_text(), string_t("ol\xc3\xa1"));
  }

  TEST("payload: addressing");
  {
    const User alice = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    const User bob = make_user("bob", "bbbbbbbb-0000-4000-8000-000000000002");

    // A broadcast reaches everyone. The broadcast receiver is named "all" but
    // carries no ID, and that empty ID is what marks it as a broadcast.
    Payload broadcast(alice, User::broadcast(), Message("hi all"));
    CHECK_EQ(broadcast.get_receiver().get_id(), string_t(""));
    CHECK_EQ(broadcast.get_receiver().get_name(), string_t("all"));
    CHECK(broadcast.addressed_to(alice));
    CHECK(broadcast.addressed_to(bob));
    // Even a node actually named "all" must still receive it.
    const User also_all = make_user("all", "cccccccc-0000-4000-8000-000000000003");
    CHECK(broadcast.addressed_to(also_all));

    // A broadcast must still be a broadcast after a full round trip over the
    // wire. The empty receiver ID is the marker, and a deserialized User starts
    // with a generated UUID that has to be cleared again.
    Payload wire_payload(alice, User::broadcast(), Message("hello everyone"));
    Payload on_the_wire;
    CHECK(on_the_wire.deserialize(wire_payload.serialize()));
    CHECK_EQ(on_the_wire.get_receiver().get_id(), string_t(""));
    CHECK(on_the_wire.addressed_to(alice));
    CHECK(on_the_wire.addressed_to(bob));
    CHECK(on_the_wire.addressed_to(make_user("all", "cccccccc-0000-4000-8000-000000000003")));

    // A directed message reaches its target by ID...
    Payload to_bob(alice, bob, Message("psst"));
    CHECK(!to_bob.addressed_to(alice));
    CHECK(to_bob.addressed_to(bob));

    // ...or by name, so a node can be addressed before IDs are exchanged.
    User bob_by_name("bob");
    Payload to_name(alice, bob_by_name, Message("psst"));
    CHECK(to_name.addressed_to(bob));
    CHECK(!to_name.addressed_to(alice));
  }

  TEST("payload: envelope overhead and the 255-byte ceiling");
  {
    UUID::reset();
    const User alice = make_user("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    const Payload empty(alice, User::broadcast(), Message(""));
    const Payload full(alice, User::broadcast(),
                       Message("0123456789012345678901234567890123456789"));

    const size_t overhead = empty.serialize().size();
    const size_t with_text = full.serialize().size();
    CHECK(overhead > 0u);
    // Three UUIDs plus keys eat most of the budget; the difference must equal
    // the text length.
    CHECK_EQ(with_text - overhead, 40u);
    // This is the real constraint: even an empty envelope is close to the
    // hardware limit, which is why long messages cannot be sent.
    CHECK(overhead < 255u);
    CHECK(with_text < 255u);
  }

  TEST("user: identity, modes, json");
  {
    UUID::reset();

    // A configured ID survives; an empty one falls back to a generated UUID.
    const User configured("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    CHECK_EQ(configured.get_id(), string_t("aaaaaaaa-0000-4000-8000-000000000001"));

    User generated("bob");
    CHECK_EQ(generated.get_id().size(), 36u);
    User another("bob");
    CHECK(generated.get_id() != another.get_id());

    const User broadcast = User::broadcast();
    CHECK_EQ(broadcast.get_id(), string_t(""));
    CHECK_EQ(broadcast.get_name(), string_t("all"));

    // Modes
    CHECK(generated.can_send());
    CHECK(generated.can_receive());
    generated.set_radio_mode(beacon::RadioMode::Send);
    CHECK(generated.can_send());
    CHECK(!generated.can_receive());
    generated.set_radio_mode(beacon::RadioMode::Receive);
    CHECK(!generated.can_send());
    CHECK(generated.can_receive());

    beacon::RadioMode mode = beacon::RadioMode::Send;
    CHECK(beacon::radio_mode_from_string("RECEIVE", mode));
    CHECK_EQ(static_cast<int>(mode), static_cast<int>(beacon::RadioMode::Receive));
    CHECK(beacon::radio_mode_from_string(" both ", mode));
    CHECK_EQ(static_cast<int>(mode), static_cast<int>(beacon::RadioMode::Both));
    CHECK(!beacon::radio_mode_from_string("sideways", mode));

    beacon::DisplayMode display = beacon::DisplayMode::Full;
    CHECK(beacon::display_mode_from_string("simple", display));
    CHECK_EQ(static_cast<int>(display), static_cast<int>(beacon::DisplayMode::Simple));
    CHECK(!beacon::display_mode_from_string("loud", display));

    // JSON round trip preserves everything the wire carries.
    User source("carol", "cccccccc-0000-4000-8000-000000000003");
    source.set_radio_mode(beacon::RadioMode::Receive);
    source.set_display_mode(beacon::DisplayMode::Simple);

    User target;
    CHECK(target.from_json_string(source.to_json()));
    CHECK_EQ(target.get_name(), string_t("carol"));
    CHECK_EQ(target.get_id(), string_t("cccccccc-0000-4000-8000-000000000003"));
    CHECK_EQ(static_cast<int>(target.get_radio_mode()),
             static_cast<int>(beacon::RadioMode::Receive));
    CHECK_EQ(static_cast<int>(target.get_display_mode()),
             static_cast<int>(beacon::DisplayMode::Simple));

    // A partial document keeps the fields it does not mention.
    User partial;
    CHECK(partial.from_json_string("{\"name\":\"dave\"}"));
    CHECK_EQ(partial.get_name(), string_t("dave"));
    CHECK(partial.get_id().size() > 0u);
    CHECK(!partial.from_json_string("not json"));
    CHECK(!partial.from_json_string(""));
  }

  TEST("message: json");
  {
    UUID::reset();
    const Message message("hi");
    const string_t wire = message.to_json();
    CHECK(validate_json(wire));

    Message parsed;
    CHECK(parsed.from_json_string(wire));
    CHECK_EQ(parsed.get_contents(), string_t("hi"));
    CHECK_EQ(parsed.get_id(), message.get_id());

    // Each message gets its own ID, which is what a dedup layer would key on.
    CHECK(Message("a").get_id() != Message("a").get_id());
  }

  TEST("log: level parsing and the bounded store");
  {
    beacon::LogLevel level = beacon::LogLevel::INFO;
    CHECK(beacon::log_level_from_string("debug", level));
    CHECK_EQ(static_cast<int>(level), static_cast<int>(beacon::LogLevel::DEBUG));
    CHECK(beacon::log_level_from_string("  WARN ", level));
    CHECK(beacon::log_level_from_string("5", level));
    CHECK_EQ(static_cast<int>(level), static_cast<int>(beacon::LogLevel::TRACE));
    CHECK(!beacon::log_level_from_string("9", level));
    CHECK(!beacon::log_level_from_string("loud", level));

    beacon::Log log(beacon::LogLevel::INFO);
    CHECK(!log.should_log(beacon::LogLevel::DEBUG));
    CHECK(log.should_log(beacon::LogLevel::ERROR));

    // The store is bounded, so a long conversation cannot exhaust the heap.
    const size_t capacity = beacon::Log::capacity();
    for (size_t i = 0; i < capacity + 10; ++i) {
      log.add_message(Message(beacon::to_str(static_cast<long>(i))));
    }
    CHECK_EQ(log.size(), capacity);

    // The oldest were dropped, the newest kept.
    const beacon::Message* newest = log.get_message(log.size() - 1);
    CHECK(newest != nullptr);
    if (newest != nullptr) {
      CHECK(newest->get_contents().find("41") != std::string::npos);
    }

    // Copy the ID out before erasing: get_message() hands back a pointer into
    // the vector, and removing an earlier element invalidates it.
    string_t newest_id;
    if (newest != nullptr) {
      newest_id = newest->get_id();
    }
    CHECK(log.find_message(newest_id) != nullptr);

    CHECK(log.remove_message("no-such-id") == false);
    CHECK(log.remove_message(static_cast<size_t>(9999)) == false);
    CHECK(log.get_message(static_cast<size_t>(9999)) == nullptr);
    CHECK(log.remove_message(static_cast<size_t>(0)));
    CHECK_EQ(log.size(), capacity - 1);
    CHECK(log.find_message(newest_id) != nullptr);

    log.clear();
    CHECK_EQ(log.size(), 0u);
  }
}
