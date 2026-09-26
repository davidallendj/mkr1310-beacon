# TODO

Current task list for `mkr1310-beacon`. Check this file before starting work.

Status legend: `[ ]` todo · `[~]` in progress · `[x]` done · `[!]` blocked

---

## Phase 0 — Project tracking & verification harness

- [x] 0.1 Create `TODO.md`, `BUGS.md`, `PROGRESS.md` (required by `AGENTS.md`)
- [x] 0.2 Host-side test harness, because no board is attached
      (`ls /dev/ttyACM*` is empty, so nothing can be flashed from here)
      - [x] `Arduino.h` / `Serial` stub, faithful about `Print`'s overloads
      - [x] `UUID` stub (the real header inherits `Printable`)
      - [x] `LoRa` stub with loopback and a separate inject path
      - [x] Real `ArduinoJson` (the wire format is what is under test)
      - [x] `test/run.sh` builds with `-std=gnu++11` to match the SAMD core
- [x] 0.3 Baseline build recorded: 34,392 B flash / 3,516 B RAM, ~19 warnings

## Phase 1 — Bug fixes

All items below are done; see `BUGS.md` for detail and the covering test.
- [x] 1.1 BUG-01 `Command::run` never invoked the command handler
- [x] 1.2 BUG-02 a bare command name aborted the board
- [x] 1.3 BUG-03 `Payload::serialize()` returned a destroyed local
- [x] 1.4 BUG-04 radio-mode gate always returned
- [x] 1.5 BUG-05 packet framing mismatch
- [x] 1.6 BUG-06 `validate_json()` deprecated + empty filter
- [x] 1.7 BUG-07 `get_id_short()` returned a reference to a temporary
- [x] 1.8 BUG-08 `Payload::deserialize()` could not read objects
- [x] 1.9 BUG-09a `parse_flags()` had no return
- [x] 1.10 BUG-09b `join()` emitted a leading separator
- [x] 1.11 BUG-10 `message` concatenated words with no separator
- [x] 1.12 BUG-11 `user.h` / `message.h` missing `ArduinoJson.h`
- [x] 1.13 BUG-12 uninitialized `m_column_size`, unset `m_description`
- [x] 1.14 BUG-13 `copy()` read out of bounds past the vector end
- [x] 1.15 BUG-14 chained `add_command` nested commands as subcommands
- [x] 1.16 BUG-15 empty receiver ID dropped on deserialize (broadcasts lost)
- [x] 1.17 BUG-16 `split_args` returned the command name as a word
- [x] 1.18 BUG-17 a bare `\n` did not submit a line
- [x] 1.19 BUG-18 header definitions collided across translation units
- [x] 1.20 BUG-19 headers used `Serial` without `Arduino.h`
- [x] 1.21 BUG-20 ambiguous `from_json` overloads

## Phase 2 — CLI

- [x] 2.1 Root name optional: `/message hi` and `/beacon message hi` both work
- [x] 2.2 Unknown commands report an error plus usage; no crash
- [x] 2.3 Subcommand routing; no out-of-range `at()` on any input path
- [x] 2.4 Working `/help` and `/help <command>`, with flags documented
- [x] 2.5 `parse_flags` returns flags; `split_args` separates flags from words
- [x] 2.6 Line editing extracted to `line_reader.h`: CRLF, LF, CR, backspace,
      control bytes, UTF-8
- [x] 2.7 Unused `MKRWAN.h` removed
- [x] 2.8 `Arduino String` dropped from the sketch in favour of `std::string`
- [x] 2.9 Sketch split: `line_reader.h` / `session.*` / `radio.*` vs a 94-line
      `.ino` that only owns the serial port

## Phase 3 — Core feature: send + receive over LoRa

- [x] 3.1 `LoRa.onReceive()` registered (the receive path was never wired up)
- [x] 3.2 Reads exactly `packet_size` bytes; stray first-byte discard removed
- [x] 3.3 Bytes read in the DIO0 ISR, JSON parse and `Serial` output deferred to
      `loop()`; `s_processing` guards against the buffer being overwritten
- [x] 3.4 Explicit air interface: frequency, bandwidth, SF, coding rate, sync
      word, TX power
- [x] 3.5 Packet-size guard with measured overhead, the real byte count, and the
      remaining text budget
- [x] 3.6 Stable identity across reboots via `constants::user_id`, with a
      generated fallback and a warning telling the user what to paste in
- [x] 3.7 `/user` implemented: `name`, `id`, `radio`, `display`
- [x] 3.8 `/log-level` implemented and wired to the log threshold
- [x] 3.9 `Log` store: working `find` / `remove` / bounded capacity
- [x] 3.10 Boot banner printed once, with a blank line re-printing status
- [x] 3.11 Incoming messages decoded and printed per `DisplayMode`
- [x] 3.12 `/stats` and `/flush`
- [x] 3.13 Self-echo suppression and broadcast/directed addressing

## Phase 4 — Verification & docs

- [x] 4.1 Host tests green: **427 checks, 0 failures**
- [x] 4.2 Device build clean — zero warnings from project files
- [x] 4.3 Size check: 64,260 B flash (24%), 4,332 B static RAM (13%)
- [x] 4.4 `README.md` rewritten
- [x] 4.5 On-device test steps recorded in `PROGRESS.md`

---

## Next up

- [!] **Flash two boards and confirm on real hardware.** Everything so far is
      verified against stubs; the ISR path, the SX1276 FIFO reads and the USB
      serial stack have only been reasoned about, not executed. Steps are in
      `PROGRESS.md`.
- [ ] Give each node a real `constants::user_id` so they can be told apart.

## Deferred (not in current scope)

- [ ] D.1 Directed messaging from the CLI (`/message alice hello`)
- [ ] D.2 Message dedup / duplicate suppression
- [ ] D.3 Delivery acks and retries
- [ ] D.4 Off-board queue while the radio is busy
- [ ] D.5 Encryption / authentication — the channel is open to anyone in range
- [ ] D.6 Persist profile + history to flash (needs a `FlashStorage` library,
      which is not installed for the SAMD21)
- [ ] D.7 A real clock, so `sent` / `received` timestamps mean something
- [ ] D.8 Short IDs on the wire — three UUIDs plus JSON keys spend 175 of 255
      bytes, leaving only 80 characters of text
- [ ] D.9 WiFi / TheThingsNetwork uplink via MKRWAN
- [ ] D.10 CLI tab-completion and history

## Known limitations

- **Single packet only.** LoRa here sends one 255-byte packet per message; there
  is no fragmentation, so long text is refused rather than split.
- **Blocking transmit.** `endPacket()` waits for the packet to be sent (up to a
  second at SF7 with a long airtime), during which serial input is not read.
- **Single receive buffer.** A packet arriving while the previous one is being
  processed is dropped and counted in `/stats` as `dropped`, not queued.
- **No clock.** `sent` / `received` fields exist in the format but are always
  zero and are omitted from the wire.
- **Node identity is not persisted.** `/user id` lasts until reboot only.
