// End-to-end tests for the terminal experience: type a line, see what happens.
//
// These drive `Session::feed()` exactly as the sketch's serial reader does, and
// watch the LoRa stub, so the whole path
//     keystrokes -> CLI -> payload -> LoRa -> interrupt -> poll -> printed
// is covered without hardware.

#include "testing.h"

#include <type_traits>

#include "Arduino.h"

#include "line_reader.h"
#include "payload.h"
#include "session.h"

using beacon::Payload;
using beacon::Session;

namespace {

// A session with a fixed identity, so payloads are predictable. Each instance
// needs its own id: a node discards anything that appears to come from itself,
// so two sessions sharing an id would silently ignore each other.
class TestSession {
public:
  explicit TestSession(const string_t& name = "alice",
                       const string_t& id = "aaaaaaaa-0000-4000-8000-000000000001") {
    LoRa.reset();
    LoRa.set_rssi(-75);
    arduino_stub::reset();
    m_session.get_user().set_id(id);
    m_session.get_user().set_name(name);
    m_session.setup();
  }

  // Run a line and return everything it printed.
  string_t type(const string_t& line) {
    arduino_stub::reset();
    m_session.feed(line);
    return arduino_stub::out;
  }

  // Deliver `text` to the session as if it had arrived over the air from
  // `sender_id` / `sender_name`, then let the session process it.
  void receive(const string_t& sender_id, const string_t& sender_name,
               const string_t& text) {
    beacon::User sender(sender_name);
    sender.set_id(sender_id);
    const Payload payload(sender, beacon::User::broadcast(),
                          beacon::Message(text));
    LoRa.inject(payload.serialize());
    m_session.poll();
  }

  string_t delivered() const { return LoRa.last_tx(); }

  Session& session() { return m_session; }

private:
  Session m_session;
};

}  // namespace


void test_line_reader() {
  TEST("line reader: a line ends at carriage return");
  {
    beacon::LineReader reader;
    reader.feed("hello");
    CHECK(!reader.has_line());
    CHECK_EQ(reader.buffer(), string_t("hello"));

    reader.feed('\r');
    CHECK(reader.has_line());
    // The partial buffer is cleared, ready for the next line.
    CHECK_EQ(reader.buffer(), string_t(""));

    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("hello"));
    CHECK(!reader.next_line(line));
  }

  TEST("line reader: CRLF is one line, not two");
  {
    // Terminals send "\r\n"; treating the '\n' as a terminator too would run
    // every line twice, the second time empty.
    beacon::LineReader reader;
    reader.feed("hi\r\n");

    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("hi"));
    CHECK(!reader.next_line(line));
  }

  TEST("line reader: a bare newline also terminates a line");
  {
    // The Arduino serial monitor can be set to send "\n" instead of "\r\n".
    // If that did not terminate a line, the user would get no response at all.
    beacon::LineReader reader;
    reader.feed("hi\n");
    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("hi"));
  }

  TEST("line reader: a bare carriage return also terminates a line");
  {
    beacon::LineReader reader;
    reader.feed("hi\r");
    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("hi"));
  }

  TEST("line reader: LFCR is one line, not two");
  {
    beacon::LineReader reader;
    reader.feed("hi\n\r");
    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("hi"));
    CHECK(!reader.next_line(line));
  }

  TEST("line reader: two identical terminators are two lines");
  {
    // Pressing Enter on an empty line has to submit an empty line, otherwise
    // the status cannot be re-displayed.
    beacon::LineReader reader;
    reader.feed("a\n\nb\n");
    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("a"));
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t(""));
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("b"));
    CHECK(!reader.next_line(line));
  }

  TEST("line reader: an empty line is preserved");
  {
    beacon::LineReader reader;
    reader.feed("\r\r");
    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t(""));
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t(""));
    CHECK(!reader.next_line(line));
  }
  TEST("line reader: backspace and delete edit the current line");
  {
    beacon::LineReader reader;
    reader.feed("helXXo");
    reader.feed('\b');
    reader.feed(0x7f);   // delete removes the same way
    reader.feed('!');

    string_t line;
    reader.feed('\r');
    CHECK(reader.next_line(line));
    // "helXXo" minus two characters, plus '!'.
    CHECK_EQ(line, string_t("helX!"));
  }

  TEST("line reader: backspace on an empty line is harmless");
  {
    beacon::LineReader reader;
    reader.feed('\b');
    reader.feed(0x7f);
    reader.feed('\b');
    reader.feed("ok\r");
    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("ok"));
  }

  TEST("line reader: control bytes are dropped, printable ones kept");
  {
    beacon::LineReader reader;
    // A tab and the ESC of an ANSI sequence are control bytes and must not
    // become message text. The rest of the sequence is printable, so it is
    // kept -- the reader drops control bytes, it does not parse escapes.
    reader.feed("a\tb\x1b[0mc");
    reader.feed('\r');

    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("ab[0mc"));
  }

  TEST("line reader: UTF-8 survives intact");
  {
    beacon::LineReader reader;
    reader.feed("ol\xc3\xa1 caf\xc3\xa9\r");
    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("ol\xc3\xa1 caf\xc3\xa9"));
  }

  TEST("line reader: several lines in one burst are all delivered");
  {
    beacon::LineReader reader;
    reader.feed("one\rtwo\rthree\r");

    string_t line;
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("one"));
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("two"));
    CHECK(reader.next_line(line));
    CHECK_EQ(line, string_t("three"));
    CHECK(!reader.next_line(line));
  }

  TEST("line reader: reset discards pending state");
  {
    beacon::LineReader reader;
    reader.feed("partial");
    reader.feed("done\r");
    reader.reset();
    CHECK(!reader.has_line());
    CHECK_EQ(reader.buffer(), string_t(""));
  }

  TEST("session: input read from the terminal behaves end to end");
  {
    // Bytes in, message out -- the same path the sketch's serial pump uses.
    TestSession t;
    beacon::LineReader reader;
    reader.feed("typed from the terminal\r\n");

    string_t line;
    while (reader.next_line(line)) {
      t.session().feed(line);
    }
    CHECK(t.delivered().find("typed from the terminal") != std::string::npos);
  }
}


void test_session() {
  UUID::reset();

  TEST("session: a bare line is sent as a message");
  {
    TestSession t;
    const string_t out = t.type("hello world");

    // The exact text is transmitted, and echoed locally.
    CHECK(out.find("alice -> hello world") != std::string::npos);
    CHECK(LoRa.packets_sent() == 1);

    // What went on the air is the JSON envelope carrying that text.
    const string_t wire = t.delivered();
    CHECK(beacon::validate_json(wire));
    Payload sent;
    CHECK(sent.deserialize(wire));
    CHECK_EQ(sent.get_text(), string_t("hello world"));
    CHECK_EQ(sent.get_sender().get_name(), string_t("alice"));
    CHECK_EQ(sent.get_receiver().get_id(), string_t(""));
    CHECK_EQ(t.session().get_radio().get_sent(), 1u);
  }

  TEST("session: words are joined with spaces, not concatenated");
  {
    TestSession t;
    t.type("the quick brown fox");
    // BUG-10: the old code produced " thequickbrownfox".
    CHECK(t.delivered().find("the quick brown fox") != std::string::npos);
    CHECK(t.delivered().find("quickbrown") == std::string::npos);
  }

  TEST("session: /message sends the same way");
  {
    TestSession t;
    const string_t out = t.type("/message hello there");
    CHECK(out.find("alice -> hello there") != std::string::npos);
    CHECK(t.delivered().find("hello there") != std::string::npos);
  }

  TEST("session: --quiet suppresses the echo but still transmits");
  {
    TestSession t;
    const string_t out = t.type("/message --quiet secret");
    CHECK(out.find("alice ->") == std::string::npos);
    CHECK(LoRa.packets_sent() == 1);
    CHECK(t.delivered().find("secret") != std::string::npos);
  }

  TEST("session: an incoming message is printed and kept");
  {
    TestSession t;
    arduino_stub::reset();

    t.receive("bbbbbbbb-0000-4000-8000-000000000002", "bob", "hi alice");
    const string_t out = arduino_stub::out;

    CHECK(out.find("hi alice") != std::string::npos);
    CHECK(out.find("bob") != std::string::npos);
    CHECK(out.find("-75 dBm") != std::string::npos);
    // Full display mode is the default, so the short sender ID is shown.
    CHECK(out.find("bbbbbbbb") != std::string::npos);

    CHECK_EQ(t.session().get_radio().get_accepted(), 1u);
    CHECK_EQ(t.session().get_log().size(), 1u);

    // /stats reports the message and the counters.
    const string_t stats = t.type("/stats");
    CHECK(stats.find("hi alice") != std::string::npos);
    CHECK(stats.find("accepted:     1") != std::string::npos);
  }

  TEST("session: a round trip between two nodes");
  {
    // The headline feature: one node sends, the other receives and shows it.
    TestSession alice("alice", "aaaaaaaa-0000-4000-8000-000000000001");
    TestSession bob("bob", "bbbbbbbb-0000-4000-8000-000000000002");

    alice.type("ping from alice");
    // Hand the transmitted bytes to bob, as the air would.
    const string_t wire = alice.delivered();
    LoRa.set_rssi(-64);
    CHECK(LoRa.inject(wire));

    arduino_stub::reset();
    bob.session().poll();
    const string_t shown = arduino_stub::out;

    CHECK(shown.find("ping from alice") != std::string::npos);
    CHECK(shown.find("alice") != std::string::npos);
    CHECK(shown.find("-64 dBm") != std::string::npos);
    CHECK_EQ(bob.session().get_radio().get_accepted(), 1u);

    // And the reply in the other direction.
    arduino_stub::reset();
    bob.type("pong from bob");
    CHECK(LoRa.inject(bob.delivered()));
    arduino_stub::reset();
    alice.session().poll();
    CHECK(arduino_stub::out.find("pong from bob") != std::string::npos);
    CHECK_EQ(alice.session().get_radio().get_accepted(), 1u);
  }

  TEST("session: a line that is a command is not sent as a message");
  {
    TestSession t;
    t.type("/user");
    CHECK(LoRa.packets_sent() == 0);

    t.type("/stats");
    CHECK(LoRa.packets_sent() == 0);

    t.type("/help");
    CHECK(LoRa.packets_sent() == 0);
  }

  TEST("session: a mistyped command is reported, not broadcast");
  {
    TestSession t;
    // Explicitly marked with '/', so a typo must not reach the network.
    const string_t out = t.type("/mesage hello");
    CHECK(LoRa.packets_sent() == 0);
    CHECK(out.find("is not a known command") != std::string::npos);
  }

  TEST("session: /user changes the identity used when sending");
  {
    TestSession t;
    const string_t out = t.type("/user name carol");
    CHECK(out.find("name set to carol") != std::string::npos);
    CHECK_EQ(t.session().get_user().get_name(), string_t("carol"));

    t.type("renamed");
    CHECK(t.delivered().find("carol") != std::string::npos);
  }

  TEST("session: /user radio controls transmission");
  {
    TestSession t;
    t.type("/user radio receive");
    const string_t out = t.type("should not go out");
    CHECK(out.find("sending is disabled") != std::string::npos);
    CHECK(LoRa.packets_sent() == 0);

    t.type("/user radio both");
    t.type("now it should");
    CHECK(LoRa.packets_sent() == 1);
  }

  TEST("session: /user display changes incoming output detail");
  {
    TestSession t;
    t.type("/user display simple");
    arduino_stub::reset();
    t.receive("bbbbbbbb-0000-4000-8000-000000000002", "bob", "terse");
    CHECK(arduino_stub::out.find("bbbbbbbb") == std::string::npos);
    CHECK(arduino_stub::out.find("terse") != std::string::npos);
  }

  TEST("session: an over-long message is refused with the real budget");
  {
    TestSession t;
    const string_t huge(400, 'z');
    const string_t out = t.type(huge);

    CHECK(LoRa.packets_sent() == 0);
    CHECK(out.find("too long") != std::string::npos);
    // The message must tell the user how much room they actually have.
    CHECK(out.find("255") != std::string::npos);
  }

  TEST("session: /log-level gates debug echo");
  {
    TestSession t;
    const string_t before = t.type("/log-level");
    CHECK(before.find("log level: info") != std::string::npos);

    // At INFO the input is not echoed back.
    const string_t quiet = t.type("visible message");
    CHECK(quiet.find("[debug]") == std::string::npos);

    t.type("/log-level debug");
    const string_t loud = t.type("visible message");
    CHECK(loud.find("[debug] visible message") != std::string::npos);

    const string_t bad = t.type("/log-level shouting");
    CHECK(bad.find("unknown log level") != std::string::npos);
  }

  TEST("session: /flush clears history");
  {
    TestSession t;
    t.receive("bbbbbbbb-0000-4000-8000-000000000002", "bob", "remember me");
    CHECK_EQ(t.session().get_log().size(), 1u);

    const string_t out = t.type("/flush");
    CHECK(out.find("cleared") != std::string::npos);
    CHECK_EQ(t.session().get_log().size(), 0u);
  }

  TEST("session: blank input re-prints status");
  {
    TestSession t;
    const string_t out = t.type("   ");
    CHECK(out.find("You are alice") != std::string::npos);
    CHECK(out.find("915 MHz") != std::string::npos);
    CHECK(LoRa.packets_sent() == 0);
  }

  TEST("session: every line is handled without crashing");
  {
    TestSession t;
    bool ok = true;
    const char* lines[] = {
      "", " ", "/", "//", "///", "/message", "/user", "/user bogus",
      "/user radio sideways", "/user display loud", "/log-level", "/log-level 99",
      "/help", "/help user", "/help message", "/help nosuch", "/nosuch",
      "/beacon", "/beacon help", "/beacon message hi", "plain message",
      "-", "--", "--quiet", "text with \xc3\xa9 utf8", "a  b   c",
    };
    for (const char* line : lines) {
      try {
        t.type(line);
        t.session().poll();
      } catch (const std::exception&) {
        ok = false;
      }
    }
    CHECK(ok);
  }

  TEST("session: a command word is not swallowed as a message when prefixed");
  {
    // "message hi" with no slash names a command, so it runs the command.
    TestSession t;
    arduino_stub::reset();
    t.type("message hi");
    // It dispatches to /message, so the payload is sent and echoed.
    CHECK(t.delivered().find("hi") != std::string::npos);
    CHECK(arduino_stub::out.find("alice -> hi") != std::string::npos);
  }

  TEST("session: copying is refused at compile time");
  {
    // Command handlers capture `this`, and Radio holds references, so a copied
    // Session would run its commands against the original. That failure is
    // silent, so it is made a compile error instead.
    CHECK(!std::is_copy_constructible<Session>::value);
    CHECK(!std::is_copy_assignable<Session>::value);
    CHECK(!std::is_move_constructible<Session>::value);
    CHECK(!std::is_copy_constructible<beacon::Radio>::value);
    CHECK(!std::is_copy_assignable<beacon::Radio>::value);
  }
}
