# BUGS

Currently-known bugs. Each entry has a description, reproduction, and status.
Update this file as bugs are found and fixed.

Legend: `OPEN` · `FIXED` · `WONTFIX` (with reason)

Every bug below is covered by a regression test in `test/`, named in the
"Covered by" line. Run `./test/run.sh` to confirm.

---

## BUG-01 — `Command::run` never invoked the command handler

- **Status:** FIXED
- **Severity:** critical — the primary feature (send) was unreachable
- **File:** `cli.h`

`Command::run()` looked up `args.at(0)` and, finding a child, compared
`next->get_name()` to `args.at(1)` and then `return`ed **before** reaching
`m_run(args)`. `m_run` was only called when no child matched, which never
happened for the root command.

```cpp
Command *next = get_command(args.at(0));
if (next) {
  if (next->get_name() == args.at(1)) {   // out of range when args.size() == 1
    Serial.println(string_t("next -> " + next->get_name()).data());
  }
  return;                                 // <-- m_run(args) is unreachable here
}
m_run(args);
```

`Command::run` now routes to a subcommand when the command has children, and
calls its own handler otherwise. Dispatch from the root is handled by
`Cli::run_command`, which is the only place that decides which command to run.

- **Covered by:** `test_cli.cpp` "BUG-01 a registered handler actually runs"

---

## BUG-02 — `/beacon help` aborted the board

- **Status:** FIXED
- **Severity:** critical
- **File:** `cli.h`

With `args.size() == 1`, `args.at(1)` was out of range. The SAMD core is
compiled with `-fno-exceptions`, so `std::vector::at` did **not** throw — it
called `abort()`. The board rebooted every time. The README told the user to
"try `/help`", so this was the first thing a new user would hit.

No `at()` calls remain on any input path; every lookup is bounds-checked and
returns a pointer or `nullptr`.

- **Covered by:** `test_cli.cpp` "BUG-02 a bare command name does not crash",
  and the 25-line fuzz list in `test_session.cpp` "every line is handled
  without crashing"

---

## BUG-03 — `Payload::serialize()` returned a pointer to a destroyed local

- **Status:** FIXED
- **Severity:** critical — undefined behaviour on every send
- **File:** `payload.h`

```cpp
const char* serialize() const {
  JsonDocument doc;
  std::string stream;        // local
  serializeJson(doc, stream);
  return stream.data();      // dangles the moment serialize() returns
}
```

The caller dereferenced the result twice (`Serial.println(...)` then
`LoRa.println(...)`), so the second read touched freed heap.

`serialize()` now returns `string_t` by value, built by `beacon::serialize()` in
`json.h`.

- **Covered by:** `test_payload.cpp` "BUG-03 serialize returns a usable string"

---

## BUG-04 — Radio-mode gate in the receive handler always returned

- **Status:** FIXED
- **Severity:** critical
- **File:** `mkr1310-beacon.ino` (now `radio.cpp`)

```cpp
if (user.get_radio_mode() != beacon::RadioMode::Receive
  || user.get_radio_mode() != beacon::RadioMode::Both)
  return;
```

A value cannot be two different enum members at once, so `||` made the condition
unconditionally true and every packet was discarded. The left operand alone was
already true, because the default is `Both`.

Replaced by `User::can_receive()`, which is a direct expression of intent. The
check also moved out of interrupt context into `Radio::process()`.

- **Covered by:** `test_radio.cpp` "BUG-04 receive mode gate admits Both and
  Receive" and "receive mode ignores traffic but still counts it"

---

## BUG-05 — Packet framing mismatch corrupted every received message

- **Status:** FIXED
- **Severity:** critical
- **File:** `mkr1310-beacon.ino` (now `radio.cpp`)

Transmit used `LoRa.println(...)`, which appends `\r\n` (Arduino `Print`).
Receive read the **first byte into an unused `int recipient`** and threw it away:

```cpp
int recipient = LoRa.read();   // ate the leading '{' of the JSON
```

So `incoming` could never be valid JSON, and the trailing `\r\n` was never
stripped.

Transmit now uses `LoRa.print()`. Receive reads exactly `packet_size` bytes,
which the library reports, so no terminator is involved in either direction.

- **Covered by:** `test_radio.cpp` "BUG-05 framing is exact on transmit" and
  "BUG-05 round trip is lossless"

---

## BUG-06 — `validate_json()` was built on a deprecated type and an empty filter

- **Status:** FIXED
- **Severity:** high — receive-side validation was unreliable
- **File:** `message.h` (now `json.h`)

```cpp
StaticJsonDocument<0> doc, filter;
return deserializeJson(doc, input, DeserializationOption::Filter(filter)) == DeserializationError::Ok;
```

- `StaticJsonDocument<0>` is deprecated in ArduinoJson 7.4.3.
- The filter document was **empty**, so every key was dropped and any
  syntactically valid JSON returned `Ok`. It never checked anything real.

Now a plain `JsonDocument` with no filter, plus explicit empty/null rejection.

- **Covered by:** `test_payload.cpp` "BUG-06 validate_json"

---

## BUG-07 — `get_id_short()` returned a reference to a temporary

- **Status:** FIXED
- **Severity:** high — dangling reference
- **File:** `user.h`, `message.h`

```cpp
const std::string& get_id_short() const { return m_id.substr(0, 8); }
```

`substr` returns by value; binding a `const&` to it extends nothing. GCC flagged
both sites as `-Wreturn-local-addr`. Both now return `string_t` by value.

- **Covered by:** `test_payload.cpp` "BUG-07 get_id_short returns a value, not a
  dangling reference"

---

## BUG-08 — `Payload::deserialize()` could not read JSON objects

- **Status:** FIXED
- **Severity:** high — inbound messages could never be decoded

```cpp
m_sender.from_json(doc["sender"].as<std::string>());
```

`doc["sender"]` is a JSON **object**; there is no `JsonVariantConst ->
std::string` conversion for an object, so this yielded an empty string and
`deserializeJson` then reported `EmptyInput`. Deserialization was entirely
non-functional.

`User`, `Message` and `Payload` now read `JsonVariantConst` directly.
`Payload::deserialize` builds into temporaries and only commits on success, so a
corrupt packet cannot half-populate a message.

- **Covered by:** `test_payload.cpp` "BUG-08 malformed input is rejected, not
  half-applied"

---

## BUG-09 — `parse_flags()` had no return; `join()` emitted a leading separator

- **Status:** FIXED
- **Severity:** medium
- **File:** `cli.h`, `util.h`

**(a)** `parse_flags` built a `flags_t` and fell off the end without returning
it: `warning: no return statement in function returning non-void`.

**(b)** `join` passed the separator as the `accumulate` *init* value, making it
the first token: `join({"a","b"})` yielded `" a b"`.

Both fixed; `parse_flags` is `const` and returns the flags, and `join`
accumulates from `front()`.

- **Covered by:** `test_util.cpp` "util: join", `test_cli.cpp` "cli: flags"

---

## BUG-10 — `message` command joined words with no separator

- **Status:** FIXED
- **Severity:** medium — message text was mangled
- **File:** `mkr1310-beacon.ino`

```cpp
beacon::Message(std::accumulate(args.begin()+1, args.end(), string_t(" ")))
```

Without a binary operation `std::accumulate` concatenates with **no** separator
and seeds a leading space, so `/message hello there` became `" hellothere"`.
Now uses `beacon::join(words)`.

- **Covered by:** `test_session.cpp` "words are joined with spaces, not
  concatenated"

---

## BUG-11 — `user.h` and `message.h` used `JsonDocument` without including `ArduinoJson.h`

- **Status:** FIXED
- **Severity:** medium — fragile include order

Both headers called `JsonDocument`, `serializeJson` and `deserializeJson` but
included only `<UUID.h>`. They compiled only because `payload.h` included
`ArduinoJson.h` first. Both now include it directly.

The same class of problem recurred for `Arduino.h` (see BUG-19) and is why
`json.h` now exists as a shared, self-contained home for the JSON helpers.

- **Covered by:** implicit — every test includes headers in a different order

---

## BUG-12 — `Command::m_column_size` uninitialized; `m_description` never set

- **Status:** FIXED
- **Severity:** low — garbled help output

`int m_column_size;` had no initializer, so the help column width was whatever
was on the stack. `m_description` was never assigned, so help always printed an
empty description and never printed the command's own `m_help`. The help
message also omitted its trailing rule for leaf commands.

`m_description` is gone; `m_help` is the one-line summary. Column widths are
computed from the actual entries.

- **Covered by:** `test_cli.cpp` "cli: help text is complete"

---

## BUG-13 — `beacon::copy()` read out of bounds when `end` exceeded the vector

- **Status:** FIXED
- **Severity:** high — memory-safety violation
- **File:** `util.h` (inherited from the original helper, not previously called)

`copy(v, 2, 99)` on a 4-element vector formed `v.begin() + 99` and copied out of
bounds. Found by `test_util.cpp`, which segfaulted on the first run.

Both bounds are now clamped to the vector's size.

- **Covered by:** `test_util.cpp` "util: copy"

---

## BUG-14 — Chained `add_command()` nested commands instead of registering siblings

- **Status:** FIXED
- **Severity:** critical — broke the whole command set
- **File:** `cli.h`

`add_command` returned a reference to the **newly added child**, so a chain

```cpp
cli.add_command("message", ...).add_command("user", ...).add_command("help", ...);
```

made `user` and `help` subcommands of `message`. The result: `/message hi`
replied *"hi is not a command of message"*, and `user`, `help`, `stats`,
`log-level` and `flush` were unreachable. This was introduced while fixing
BUG-01 and caught immediately by `test_session.cpp`.

`add_command` now returns `*this` (the parent), so a chain registers siblings.

- **Covered by:** `test_cli.cpp` "chained add_command registers siblings, not
  nested children"

---

## BUG-15 — An empty receiver ID was dropped on deserialize, silently killing broadcasts

- **Status:** FIXED
- **Severity:** critical — every received broadcast was discarded
- **File:** `payload.h`, `user.h`

`User` starts life with a randomly generated UUID, and `User::from_json`
originally ignored an ID that was present but empty. An empty receiver ID is
exactly how a broadcast is marked, so a received broadcast was left with a
random receiver ID; `addressed_to` then compared that ID and the name `"all"`
against the local node, matched neither, and dropped the message.

- `User::from_json` now honours a present-but-empty value.
- `Payload::deserialize` clears the receiver ID when the document says so.
- `Payload::addressed_to` treats an empty receiver ID as a broadcast *before*
  falling back to the name, so a node actually named `all` still receives it.

- **Covered by:** `test_payload.cpp` "payload: addressing", plus every receive
  test in `test_radio.cpp` and `test_session.cpp`

---

## BUG-16 — `split_args()` returned the command name as a message word

- **Status:** FIXED
- **Severity:** medium — would have prefixed every message with "message"
- **File:** `cli.h`

`split_args` iterated from index 0, so the command name landed in the positional
words. It now starts at index 1; `args[0]` is never user input.

- **Covered by:** `test_cli.cpp` "cli: flags"

---

## BUG-17 — A bare `\n` from the serial monitor did not submit a line

- **Status:** FIXED
- **Severity:** medium — no response at all for some terminal settings
- **File:** `line_reader.h`

The first version treated only `\r` as a terminator and ignored `\n` to avoid
doubling CRLF input. The Arduino serial monitor can be configured to send `\n`
alone, and under that setting the user would get **no response to any
command**.

`LineReader` now accepts both, and collapses a `\r\n` (or `\n\r`) pair into one
terminator. Two *identical* terminators remain two lines, so pressing Enter on an
empty line still submits an empty line.

- **Covered by:** `test_session.cpp` "line reader: ..." (9 cases)

---

## BUG-18 — Header definitions collided once a second translation unit existed

- **Status:** FIXED
- **Severity:** high — link failure
- **File:** `util.h`, `constants.h`

`util.h` defined its free functions without `inline`, which was fine while the
sketch was a single translation unit. Adding `radio.cpp` produced duplicate
symbols for `join`, `split`, `trim`, `to_str` and the rest.

`constants.h` also used `namespace beacon::constants { ... }`, a C++17 nested
namespace definition, while the SAMD core builds with `-std=gnu++11`. GCC
accepts it as an extension; it was replaced with nested namespace blocks, and the
constants became `constexpr` so they have internal linkage.

- **Covered by:** the test build links `radio.cpp` and `session.cpp` alongside
  the test files, so any regression re-breaks it immediately

---

## BUG-19 — Headers used `Serial` without including `Arduino.h`

- **Status:** FIXED
- **Severity:** medium — fragile include order
- **File:** `cli.h`, `message.h`

`cli.h` (five call sites) and `message.h` (`Message::print`) referenced `Serial`
while only the sketch guaranteed it was declared. Compiling `session.cpp` on its
own failed. Both headers now include `<Arduino.h>`.

- **Covered by:** implicit — `session.cpp` and `radio.cpp` are separate
  translation units in both the device and host builds

---

## BUG-20 — `from_json()` overloads were ambiguous for ArduinoJson proxies

- **Status:** FIXED
- **Severity:** medium — compile error
- **File:** `user.h`, `message.h`

Offering both `from_json(JsonVariantConst)` and `from_json(const string_t&)`
made every `from_json(doc["key"])` ambiguous, because ArduinoJson's
`MemberProxy` converts to both. The string-taking entry points are now
`from_json_string`.

---

## Open

_(none)_
