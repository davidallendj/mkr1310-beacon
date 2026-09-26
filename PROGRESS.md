# PROGRESS

Running log of what was done, why, and what to verify. Read this to catch up
between commits, branches, and PRs.

---

## Entry 2026-09-25 (1) — Project review and bug triage

### Context

First pass over the repo at `a05c76c` ("refactor: reorganized and modularized
project structure"), working tree clean, `main` only. Asked for an overview of
current capabilities.

### What the project is

A single-sketch Arduino client for the **MKR WAN 1310** that types JSON messages
over a serial monitor and is *meant* to broadcast them over LoRa. Self-described
in the README as experimental. 741 lines across 9 files.

### What I found

Structure and intent are good, but **the sketch compiled while delivering none of
its core feature.** Sending and receiving were both non-functional:

1. `Command::run` never called the command handler, so `message` did nothing.
2. `/beacon help` aborted the board.
3. `Payload::serialize()` returned a pointer to a function-local `std::string`.
4. `on_receive_packet()` was never registered — no `LoRa.onReceive()` call.
5. Its radio-mode gate was `!= Receive || != Both`, which is always true.

Full triage with reproduction steps in `BUGS.md`. Plan of record in `TODO.md`.

### Verification done

- **Build:** `arduino-cli compile --fqbn arduino:samd:mkrwan1310` succeeded —
  34,392 B flash (13%), 3,516 B RAM (10%), ~19 warnings from project headers.
- **CLI dispatch:** reproduced on the host with a stubbed `Serial`.
- **No board attached** (`ls /dev/ttyACM*` empty), so nothing could be flashed.

### Design decisions taken

- **`onReceive` runs in an ISR.** Confirmed in `arduino-LoRa/src/LoRa.cpp`:
  `onDio0Rise` (an interrupt handler) calls `handleDio0Rise`, which calls
  `_onReceive(packetLength)` with the FIFO already at the packet start. The bytes
  therefore *must* be read in the callback; only parsing and `Serial` output can
  be deferred to `loop()`.
- **CRC-failed packets are never delivered** — the library guards on
  `IRQ_PAYLOAD_CRC_ERROR_MASK` before invoking the callback, so no check is
  needed on our side.
- **No `FlashStorage` library** is installed for the SAMD21, so the profile
  cannot be persisted. Identity comes from `constants::user_id` with a generated
  fallback and a warning showing what to paste in.
- **No `MKRWAN.h`** — it was included and never used.

---

## Entry 2026-09-25 (2) — Bugs fixed, CLI rebuilt, LoRa send/receive working

### Summary

Phases 0–4 of `TODO.md` are complete. Sending and receiving now work end to
end, verified against a stubbed radio. 20 bugs fixed (BUG-01 … BUG-20), 427 host
checks green, device build clean with zero project warnings.

### The two constraints that shaped everything

Both found by reading the SAMD core's `platform.txt` rather than assuming:

```
compiler.cpp.flags=... -std=gnu++11 ... -fno-exceptions -fno-rtti ...
compiler.warning_flags=-w          # warnings are OFF by default
```

- **`-std=gnu++11`** — no inline variables, no C++14/17 features. This is why
  `Radio`'s interrupt-shared state lives in `radio.cpp` instead of inline in the
  header, and why `constants.h` uses nested namespace blocks rather than
  `namespace beacon::constants { }`. It is also why the test harness compiles
  with `-std=gnu++11`: a test that used a newer feature would pass here and fail
  on the board.
- **`-fno-exceptions`** — `std::vector::at()` does not throw, it calls
  `abort()`. BUG-02 was therefore a board reboot, not a catchable error. No `at()`
  calls remain on any input path.
- **Warnings are off by default**, which is why the original code showed none.
  All builds here use `--warnings all`.

### Structural changes

| Change | Why |
|---|---|
| `line_reader.h` (new) | Line editing was inline in the `.ino` and untestable. Now handles CRLF/LF/CR, backspace, control bytes, UTF-8 — all tested. |
| `session.h` / `session.cpp` (new) | The node itself (identity, radio, commands, message handling) moved out of the sketch. This is what made the whole type-a-line → transmit → receive → print path testable off-device. |
| `radio.h` / `radio.cpp` (new) | LoRa transport, split from the sketch so the ISR/poll interaction could be tested. |
| `json.h` (new) | `serialize` / `validate_json`, shared by `User`, `Message` and `Payload`, each of which now includes `ArduinoJson.h` itself instead of relying on include order. |
| `mkr1310-beacon.ino` | 167 → 94 lines. Owns the serial port and nothing else. |
| `constants.h` | Radio settings, byte ceiling, and identity all in one place, with a note that arduino-LoRa 0.8.0's setters return `void` so a rejected value cannot be detected. |

### Bugs found *by* the tests, after being introduced

Worth calling out, because these are the ones I wrote and the harness caught:

- **BUG-14** — `add_command` returned the new child, so a chain of registrations
  made `user`, `help`, `stats`, `log-level` and `flush` subcommands of `message`.
  Every command was unreachable. Now returns `*this`.
- **BUG-15** — an empty receiver ID was dropped on deserialize, so every received
  broadcast was silently discarded. `addressed_to` now checks the empty ID first.
- **BUG-13** — `beacon::copy(v, 2, 99)` on a 4-element vector read out of bounds.
  Inherited from the original `util.h`; the first test run segfaulted on it.
- **BUG-17** — the first `LineReader` ignored a bare `\n`, so a user with the
  serial monitor set to "newline" would get no response at all.
- **BUG-18 / BUG-19 / BUG-20** — surfaced by the device build and by adding a
  second translation unit: non-inline header definitions collided at link time,
  `Serial` was used without `Arduino.h`, and `from_json` was ambiguous.

### One stub deliberately made less permissive

`SerialStub` originally had `print(const std::string&)`. The real SAMD core's
`Print` has no such overload, so the stub was accepting code the device cannot
compile. It was removed, and the sketch grew `say_line()` helpers so the
conversion happens in one place. A stub that is more permissive than the thing
it stands in for is worse than no stub.

### Numbers

| | Before | After |
|---|---|---|
| Host checks | none | **427, 0 failures** |
| Project warnings (`--warnings all`) | ~19 | **0** |
| Flash | 34,392 B (13%) | 64,148 B (24%) |
| Static RAM | 3,516 B (10%) | 4,332 B (13%) |
| `.ino` | 167 lines | 94 lines |

Flash roughly doubled because the CLI, payload and radio paths are now real
rather than dead. There is plenty of room.

### Verification status

Everything is verified against stubs plus a real device **compile**. The
following have been reasoned about but never executed:

- the DIO0 interrupt path on real hardware
- `LoRa.read()` against a real SX1276 FIFO
- the USB CDC serial stack under actual traffic
- heap behaviour with real ArduinoJson documents

### On-device test steps for you

Two MKR 1310s are needed for a real end-to-end check.

```bash
# 1. Give each node a distinct identity.
#    Edit constants.h on each board:
#      constexpr const char* user_id = "<paste the UUID the board printed>";
#      constexpr const char* user_name = "alice";   // and "bob"

# 2. Build and upload to the first board.
arduino-cli compile --upload -p /dev/ttyACM0 --fqbn arduino:samd:mkrwan1310 mkr1310-beacon.ino

# 3. Build and upload to the second, after replugging.
arduino-cli compile --upload -p /dev/ttyACM0 --fqbn arduino:samd:mkrwan1310 mkr1310-beacon.ino

# 4. Open two serial monitors.
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600

# 5. In monitor A:  hello from alice
#    In monitor B:  it should print "bob (-NN dBm) <- alice: hello from alice"

# 6. Check the counters agree.
#    /stats    in both monitors: sent=1 on A, accepted=1 on B
```

Things worth watching for, in rough order of likelihood:

1. **Nothing received at all.** Check both boards are on the same frequency and
   that one is not set to `/user radio send`.
2. **A packet or two arrive, then stop.** Look at `dropped` in `/stats` — the
   receive buffer is single-slot by design.
3. **`error: message too long`.** Expected: the envelope spends 175 of 255
   bytes, so text is capped at 80 characters. `/stats` prints the exact budget.
4. **The banner is missing.** Press Enter on an empty line to re-print status.
5. **A reboot on some input.** Please report it — it should be impossible now
   that no `at()` remains, so it would be a new bug.

### Next

Per `TODO.md`: flash two boards and confirm on hardware, then set real
`user_id` values. After that, directed messaging (D.1) is the most valuable
follow-up, since the payload already carries a receiver ID and name.

---

## Entry 2026-09-25 (3) — README rewritten as a usage guide

### Context

Asked for the README to be brought up to date, specifically as a guide covering
the main features and how to drive the device from a serial monitor or a
TTY/terminal.

### What changed in the code

Two small fixes came out of writing the docs, both found by generating real
output rather than recalling it:

- **`Session` and `Radio` are now non-copyable** (`= delete`). The command
  handlers are lambdas capturing `this`, and `Radio` holds references to the
  user and the log, so a copied `Session` was a *different* object whose
  commands still ran against the original. The symptom was silent — output went
  to the wrong place — and it surfaced only because a test helper returned a
  `Session` by value. A node owns one session for its whole life, so forbidding
  copies costs nothing and turns a silent failure into a compile error.
  Covered by `test_session.cpp` "copying is refused at compile time".
- **A parse error no longer pollutes the message history.** `Radio::process`
  was calling `m_log.add_message()` with a diagnostic string, so `/stats` would
  list "rx: malformed JSON in 31 bytes" next to real messages. The log now holds
  only messages actually received; there is a test pinning that.

Neither was found by the device build, and neither would have shown up in
normal use — the copy hazard needs two `Session`s, and the polluted history
needs a malformed packet. Both came from *running* the code and reading the
output carefully.

### The documentation approach

Every terminal transcript in the README is **real output**, produced by a
throwaway generator that drives `Session` against the test stubs
(`/tmp/opencode/gen_readme.cpp`). Nothing was transcribed from memory, so the
boot banner, help text, error strings and counters in the README are exactly
what the firmware prints. The generator is not committed; the tests are.

That approach is what caught the two code fixes above. Writing out a full
session transcript and diffing it against reality surfaced behaviour that the
assertions had not pinned.

### Host-tool guidance that was verified rather than assumed

The serial-monitor section makes claims about third-party tools, so those were
checked against the installed `arduino-cli` (1.5.1) and the `picocom(1)` man
page rather than written from memory:

- `arduino-cli monitor` uses `--config baudrate=9600` (not `-b`), and offers
  `--timestamp` and `--describe`.
- **picocom sets the port to raw mode and leaves local echo DISABLED by
  default** (confirmed in the man page: `--echo` is "Default: Disabled"). My
  first draft wrongly claimed picocom echoes for you. The README now gives
  `picocom -b 9600 --echo --imap ignlf,crlf` and explains why each flag is
  needed: `--echo` because you would otherwise type blind, and `--imap` because
  the board sends bare `CR`+`LF`, which raw mode renders as a doubled line
  advance.

The "Why local echo matters" table is the practical outcome: the firmware never
echoes keystrokes, so something on the host must, and the four ways that goes
wrong are listed with the fix for each.

### README structure now

Features → How it works → Hardware → Build and flash → Set an identity →
**Using the device** (serial monitor / TTY / two nodes) → Command reference →
Message format and limits → Troubleshooting → Tests → Layout → Limitations.

Verified after writing: all internal anchors resolve, all 56 code fences are
balanced, the contents list matches the heading tree, and the two stated
figures (427 checks, 80-character budget) match what the code actually does.

### Counts

427 host checks, 0 failures. Device build unchanged at 64,148 B flash,
4,332 B static RAM, 0 project warnings.
