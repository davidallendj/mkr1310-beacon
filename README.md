# BEACON for Arduino MKR 1310 WAN

Turn an [Arduino MKR WAN 1310](https://docs.arduino.cc/hardware/mkr-wan-1310/)
into a pocket chat terminal. Type a message, press enter, and every node in
radio range sees it — no server, no app, no configuration.

> Experimental, and not for production or commercial use. Messages go out in the
> clear, so anyone in range can read them.

## Contents

- [Features](#features)
- [How it works](#how-it-works)
- [Hardware](#hardware)
- [Build and flash](#build-and-flash)
- [Set an identity](#set-an-identity)
- [Using the device](#using-the-device)
  - [From a serial monitor](#from-a-serial-monitor)
  - [From a TTY or terminal](#from-a-tty-or-terminal)
  - [Talking to two nodes](#talking-to-two-nodes)
- [Command reference](#command-reference)
- [Message format and limits](#message-format-and-limits)
- [Troubleshooting](#troubleshooting)
- [Running the tests](#running-the-tests)
- [Repository layout](#repository-layout)
- [Limitations](#limitations)

## Features

- **Send and receive over LoRa** at 915 MHz — type a line, it goes out; traffic
  in range is printed as it arrives, with signal strength.
- **Chat-like terminal.** Anything you type that is not a command is a message.
  No syntax to learn.
- **Compact on-air format.** One JSON packet per message, carrying sender and
  receiver identity plus a message UUID.
- **Honest size limit.** The 255-byte radio buffer only leaves room for 80
  characters, and the firmware tells you the exact figure instead of silently
  truncating.
- **Persistent counters** — sent, received, accepted, ignored, malformed,
  dropped, last RSSI — so you can tell a quiet channel from a broken one.
- **Tunable node identity and modes.** Set a name, choose whether to
  transmit/receive, and pick how much detail incoming messages show.
- **Bounded memory.** The message history is capped, so a long conversation
  cannot exhaust the SAMD21's 32 KB of RAM.
- **Tested without hardware.** 427 host-side checks cover the wire format, the
  CLI, the LoRa framing and the whole terminal experience.

## How it works

Each node has an identity: a UUID and a display name. A message is wrapped in a
JSON envelope and sent as a single LoRa packet.

```json
{"sender":{"id":"642e9e60-b9ee-4d3b-9903-190c91e250ae","name":"alice"},
 "receiver":{"id":"","name":"all"},
 "message":{"id":"b1f0…","contents":"hello there"}}
```

An empty `receiver.id` marks a **broadcast**, which every node accepts. A
non-empty one is a directed message, delivered only to the matching node.

Received packets are announced by the radio's DIO0 interrupt. Because that
callback runs in interrupt context, the bytes are copied there and the JSON is
parsed and printed from the main loop — the interrupt stays short, and nothing
expensive happens where it could corrupt timing.

## Hardware

Any board with an SX1276 LoRa radio, which in practice means the MKR WAN 1310.
The sketch targets `arduino:samd:mkrwan1310`.

To talk to another node you need two of them. A single node will happily send,
but it suppresses its own transmissions rather than echoing them back.

## Build and flash

Install `arduino-cli` and the SAMD board core:

```bash
arduino-cli core update-index
arduino-cli core install arduino:samd
arduino-cli lib install ArduinoJson
```

Libraries required by the sketch:

| Library | Version | Why |
|---|---|---|
| [ArduinoJson](https://arduinojson.org) | 7.x | the wire format |
| [arduino-LoRa](https://github.com/sandeepmistry/arduino-LoRa) | 0.8.0 | the radio |
| UUID | any | message and node identifiers |

`SPI` ships with the board core. **`MKRWAN` is not used**, and
`Arduino_AVRSTL` is not required — the toolchain's own `libstdc++` covers the
standard library use here.

Then build and flash:

```bash
arduino-cli compile --fqbn arduino:samd:mkrwan1310 mkr1310-beacon.ino

# Linux
arduino-cli compile --upload -p /dev/ttyACM0 --fqbn arduino:samd:mkrwan1310 mkr1310-beacon.ino

# macOS
arduino-cli compile --upload -p /dev/cu.usbmodem* --fqbn arduino:samd:mkrwan1310 mkr1310-beacon.ino

# Windows
arduino-cli compile --upload -p COM3 --fqbn arduino:samd:mkrwan1310 mkr1310-beacon.ino
```

Building from the Arduino IDE 2 works too — open `mkr1310-beacon.ino`, pick
**Arduino MKR WAN 1310**, and upload.

## Set an identity

Out of the box a node has a name and a **freshly generated UUID on every
boot**, so two nodes cannot be told apart and neither can address the other.
The board tells you what to do:

```
BEACON for Arduino MKR 1310 WAN - experimental thin client

warning: constants::user_id is empty, so this node gets a new random
         ID on every reboot and cannot be told apart from another
         node. Set user_id in constants.h to:
         "9f1c2a44-7b6e-4d21-8a3f-51c0d9e2b7a1"


You are lora (9f1c2a44-7b6e-4d21-8a3f-51c0d9e2b7a1)
Radio: 915 MHz, SF7, 125 kHz
Sent: 0   Received: 0   Accepted: 0
Type /help for commands, or just type a message.
```

Copy that UUID into `constants.h` and give the board a name. Do this
separately on each board, with a different value:

```cpp
// constants.h
constexpr const char* user_id   = "9f1c2a44-7b6e-4d21-8a3f-51c0d9e2b7a1";
constexpr const char* user_name = "alice";
```

The identity lives in flash as a constant, not in the board's own storage —
the SAMD21 has no EEPROM and no `FlashStorage` library is installed. That means
it survives power cycles but is only changed by editing and reflashing.

## Using the device

Connect the board over USB and open a serial monitor at **9600 baud**. On
Linux the port is `/dev/ttyACM0`; on macOS `/dev/cu.usbmodem*`; on Windows
`COM3` or similar.

> The MKR 1310 talks to the host over USB CDC, so the baud rate is not
> physically meaningful — there is no real serial line. The firmware still needs
> `Serial.begin(9600)` to open the port, and your terminal should be set to
> match so nothing else is confused. A mismatched rate will not garble
> anything.

### From a serial monitor

**Arduino IDE 2.** Open the Serial Monitor pane and set:

- **Port** — `/dev/ttyACM0` (or the Windows COM port)
- **Baud rate** — `9600`
- **Line ending** — `Both NL & CR`. The firmware accepts a bare CR, a bare LF or
  CRLF, so any of the three settings works; `Both NL & CR` is the safest.
- **Local echo** — on, unless your terminal already echoes for you

#### Why local echo matters

**The firmware does not echo characters as you type.** It only responds once
you press Enter. Since the board sends bare `CR` + `LF` and never repeats your
keystrokes, something on the host has to display them — otherwise you are typing
blind. The good news is that a terminal in its normal cooked mode echoes for you
already, which is why `cat` works with no extra flags.

| Symptom | Cause | Fix |
|---|---|---|
| Nothing appears while typing, but Enter works | the tool put the port in raw mode **and** local echo is off | enable echo — see the notes below |
| Characters appear twice | echo on in both the tool and the terminal | turn the tool's local echo off |
| Output steps onto a blank line each time | the terminal is adding its own newline to the board's `CR`+`LF` | see the `--imap` note under picocom |
| No banner, no response to anything | the port was opened without DTR asserted, so the board never reset | press the **RESET** button once |

### From a TTY or terminal

Any program that opens the port works. What matters is that echo is enabled
somewhere, and that the port is not left in a mode that mangles line endings.

**`cat`** — the quickest check that the board is alive, and it needs nothing but
a working tty:

```bash
stty -F /dev/ttyACM0 9600          # Linux
stty -f /dev/ttyACM0 9600          # macOS
cat /dev/ttyACM0
```

`stty` leaves the port in canonical mode with echo enabled, so you see what you
type and Enter submits the line. Press `Ctrl-C` to exit. This is the fastest way
to rule out a hardware or firmware problem before reaching for anything more
elaborate.

**`arduino-cli`** — configures the port itself, so no `stty` is needed:

```bash
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600
```

Add `--timestamp` to prefix each line with a clock time, and `--describe` to see
every available port setting. If nothing appears as you type, run
`stty -F /dev/ttyACM0 sane` in another shell to restore the terminal's echo.

**`screen`** — useful when you want the session to survive a dropped connection.
It leaves the port cooked, so echo works with no extra flags:

```bash
screen /dev/ttyACM0 9600
# detach: Ctrl-A then D      reattach: screen -r
```

**`picocom`** — the most capable option, and the one that needs the most care,
because its defaults work against you here:

```bash
picocom -b 9600 --echo --imap ignlf,crlf /dev/ttyACM0
# exit: Ctrl-A then Ctrl-X
```

- **`--echo`** — picocom sets the port to **raw** mode by default and leaves
  local echo **disabled**, so without this flag you type blind. With it, picocom
  echoes what you type and handles backspace for you.
- **`--imap ignlf,crlf`** — the board sends `CR` + `LF` for every line. In raw
  mode the terminal adds no newline of its own, so without the mapping you get
  either a doubled line advance or a staircase down the screen. This turns the
  board's `CR`+`LF` into a single clean line break, and maps only what is
  *read from the port*, so it never alters what you send.

Inside picocom, `Ctrl-A` then `Ctrl-H` lists its command keys — including
`Ctrl-T` to toggle DTR, a convenient way to reset the board.

#### Two nodes at once

A serial port has a single owner, so you need one terminal per board. Two
`screen` sessions is the least fiddly:

```bash
screen /dev/ttyACM0 9600   # board 1
screen /dev/ttyACM1 9600   # board 2, in a second terminal
```

The second board enumerates as `ttyACM1` only if the first is already attached;
plugging them in one at a time can give you `ttyACM0` both times. Check with
`ls -l /dev/ttyACM*` if a session comes up blank.

#### Notes on serial ports

- **Permissions.** If `cat: /dev/ttyACM0: Permission denied`, add yourself to
  the `dialout` group (Debian/Ubuntu/Arch) or `uucp` (some others), then log
  out and back in. A udev rule avoids that entirely:
  `SUBSYSTEM=="tty", ATTRS{idVendor}=="2341", MODE="0660", GROUP="dialout"`.
- **The port disappears on reset.** The SAMD core re-enumerates on every reset,
  so the device node can briefly vanish. Re-run `ls /dev/ttyACM*` if a tool
  cannot find it.
- **Only one program at a time.** A serial port has a single owner. Close your
  monitor before uploading.
- **The baud rate is nominal.** USB CDC has no real serial line, so 9600 is
  only a convention the firmware and your terminal agree on. Setting it
  correctly keeps the tools happy; a mismatch will not garble anything.
- **Non-interactive use** — `screen` or `picocom` are better than `cat` if you
  want the session to keep running after your terminal command returns.

### Talking to two nodes

With two boards in range, a conversation looks like this. On **alice**:

```
You are alice (642e9e60-b9ee-4d3b-9903-190c91e250ae)
Radio: 915 MHz, SF7, 125 kHz
Sent: 0   Received: 0   Accepted: 0
Type /help for commands, or just type a message.
```

Type a message and press enter:

```
hello from alice
alice -> hello from alice
```

`alice -> …` is the local echo: it confirms what was actually transmitted.
Now look at **bob**'s terminal, where it appears as it arrives:

```
bob (-87 dBm) <- 642e9e60 alice: hello from alice
```

Read that as: *you are bob, the signal was −87 dBm, the packet came from
alice whose short ID is `642e9e60`.*

Because the channel is shared, every node hears everything. `addressed_to` in
the payload already supports directed messages; the CLI sends broadcasts.

## Command reference

Commands start with `/`. The `beacon` prefix is optional, so `/help` and
`/beacon help` are equivalent.

`/help`:

```
----------------------------------------------------------------------
beacon: Exchange short JSON messages with other nodes over LoRa.

Usage:
  beacon <command> [args]...
  /help beacon

Commands:
  message    Send a message to every node in range.
  user       Show or change the local user profile.
  log-level Show or set the logging level.
  stats      Show radio counters and message history.
  flush      Clear the stored message history.
  help       Show help, or help for one command.
----------------------------------------------------------------------
```

`/help <command>` explains one command, for example `/help message`:

```
----------------------------------------------------------------------
message: Send a message to every node in range.

Usage:
  message [args]... [flags]
  /help message

Flags:
  -q, --quiet send without echoing the message locally
----------------------------------------------------------------------
```

| Command | Description |
|---|---|
| `/message <text>` | Send `<text>` to every node in range |
| `/message -q <text>` | Send without echoing locally (`--quiet` also works) |
| `/user` | Show the local profile |
| `/user name <name>` | Change the display name |
| `/user id` | Show the current UUID |
| `/user id <uuid>` | Override the UUID **for this session only** |
| `/user radio` | Show the current radio mode |
| `/user radio <mode>` | `send`, `receive`, or `both` |
| `/user display` | Show the current display mode |
| `/user display <mode>` | `simple`, or `full` |
| `/log-level` | Show the level and the valid values |
| `/log-level <level>` | `fatal`…`trace`, or `0`–`5` |
| `/stats` | Radio counters and message history |
| `/flush` | Clear the message history |
| `/help` | Show help |
| `/help <command>` | Show help for one command |
| *(anything else)* | Sent as a message, as long as it is under 80 characters |

### `/user`

```
/user
```

```
  id:            642e9e60-b9ee-4d3b-9903-190c91e250ae
  name:          alice
  radio mode:    both
  display mode:  full
```

**Radio modes.** `both` (the default) sends and receives. `send` transmits and
ignores incoming traffic — useful on a node that is only a beacon, and it makes
`/stats` still count what it discards. `receive` listens and refuses to send:

```
/user radio receive
radio mode set to receive

the cat sat on the mat
error: radio mode is 'receive', so sending is disabled
```

**Display modes.** `full` shows the sender's short ID, which is how you tell
two nodes with the same name apart:

```
alice (-87 dBm) <- 642e9e60 alice: hello from alice     <- full
alice (-87 dBm) <- alice: hello from alice              <- simple
```

### `/log-level`

The level gates diagnostic output, not message traffic. At the default `info`
your input is not echoed back, so the terminal stays quiet. Turn on `debug` to
see exactly what the firmware parsed — the best first step when a message seems
to be going nowhere:

```
/log-level debug
log level set to debug

a debug message
[debug] a debug message
alice -> a debug message
```

### `/stats`

```
/stats
```

```
Radio:
  sent:         1
  received:     2
  accepted:     2
  ignored:      0
  malformed:    0
  dropped:      0
  last rssi:    -87 dBm
  text budget:  80 bytes
Messages:
  held:         2
    0102030e  morning bob
    0102030e  morning bob
```

How to read it:

- `sent` / `accepted` — the two that matter. Both should rise together.
- `received` vs `accepted` — the gap is `ignored` (someone else's directed
  message) or `malformed` (a corrupt packet).
- `malformed` above zero means the channel is noisy, or another device is
  transmitting something that is not this protocol.
- `dropped` counts packets that arrived while the previous one was still being
  processed. Normally zero; a run of non-zero means traffic is arriving faster
  than it can be printed.
- `text budget` is measured for *this* node, so it reflects your actual name and
  ID lengths.

### Errors

A mistyped command is reported, never broadcast:

```
/mesage oops
```

```
error: 'mesage' is not a known command.
```

The leading `/` is what makes this safe: a line that does not start with `/` and
does not name a command is treated as a message, so ordinary typing goes out
without ceremony, while a typo you marked as a command is caught locally.

An over-long message reports the real numbers:

```
error: message too long: 295 of 255 bytes (envelope uses 175, leaving 80 for text)
```

Bad arguments name what was expected:

```
/user radio sideways
error: unknown radio mode 'sideways' (expected send, receive, or both)
```

## Message format and limits

```json
{
  "sender":   { "id": "<uuid>", "name": "alice" },
  "receiver": { "id": "<uuid>", "name": "all" },
  "message":  { "id": "<uuid>", "contents": "hello there" }
}
```

- **`receiver.id` empty** means broadcast. Any node accepts it.
- **`receiver.id` set** means that node only, matched by UUID, or by name if the
  UUID is unknown.
- **`message.id`** is generated per message, which is what a deduplication
  layer would key on. Not yet used for that.
- **`sent` / `received` timestamps** are part of the format but are omitted,
  because the node has no clock.

**The 80-character limit.** A LoRa packet is capped at 255 bytes by the SX1276.
Three UUIDs plus the JSON keys and structure spend 175 of them, leaving exactly
**80 characters** of text. The limit is measured at startup for the running
node, not hard-coded, so a node with a long name reports a smaller budget.

The firmware refuses an over-long message rather than sending it truncated. The
usual fix is to send a shorter message, or to shorten your node name. Sending
short IDs on the wire instead of full UUIDs would raise the ceiling
substantially — that is on the list in `TODO.md`.

## Troubleshooting

| Symptom | Likely cause | What to do |
|---|---|---|
| No banner when the monitor opens | the port was opened without DTR, so the board did not reset | press **RESET** once with the monitor open |
| Nothing appears while typing | local echo off in the monitor *and* the terminal | enable echo in one of them |
| Characters appear twice | echo on in both the monitor and the terminal | turn the monitor's local echo off |
| Enter does nothing | the monitor is sending a line ending the firmware is not seeing | set line ending to `Both NL & CR`; the firmware accepts CR, LF or CRLF |
| `Sent:` rises, `Accepted:` stays 0 on the other node | different frequency, or the other node is in `send` mode | check `/user radio` on both; both must be on 915 MHz |
| `error: message too long` | text over 80 characters | shorten it, or send in two messages |
| `malformed:` climbing | noise, or a foreign device on this frequency | move the nodes closer, or change `lora_frequency` |
| `dropped:` climbing | more traffic arriving than can be printed | expected under heavy traffic; not an error |
| Node shows a different ID after reboot | `constants::user_id` is empty | set it, as described above |
| `failed to start LoRa` | radio did not respond, or SPI is not connected | check the board; try a different one |

`/log-level debug` is the first thing to reach for when a message seems to go
nowhere — it shows the line exactly as the firmware parsed it.

## Running the tests

The logic is covered by a host-side suite, so changes can be verified without
hardware:

```bash
./test/run.sh              # 427 checks
./test/run.sh --verbose    # keep the per-test output
```

The sketch headers are compiled for a workstation with `Arduino`, `UUID` and
`LoRa` replaced by stubs in `test/stubs/`. `ArduinoJson` is the real library,
because the wire format is what is under test. The build uses `-std=gnu++11`
to match the SAMD core, so a test cannot pass here using a language feature the
board does not have.

## Repository layout

| File | Role |
|---|---|
| `mkr1310-beacon.ino` | Sketch entry: owns the serial port, nothing else |
| `session.h` / `.cpp` | The node: identity, radio, commands, message handling |
| `radio.h` / `.cpp` | LoRa transport; the ISR receives, `poll()` decodes |
| `line_reader.h` | Serial input: line endings, backspace, control bytes |
| `cli.h` | Command framework: `Cli`, `Command`, `Flag` |
| `payload.h` | The on-air JSON envelope |
| `user.h` | Node identity and local preferences |
| `message.h` | A message and its UUID |
| `json.h` | `serialize` / `validate_json` helpers |
| `log.h` | Log threshold and bounded message history |
| `util.h` | `split`, `join`, `copy`, `trim`, number formatting |
| `types.h` | Type aliases and forward declarations |
| `constants.h` | Radio settings, byte ceiling, identity |
| `test/` | Host-side tests and stubs |
| `TODO.md` | What is planned next |
| `BUGS.md` | Every bug found, with reproduction and fix |
| `PROGRESS.md` | Running log of the work |

## Limitations

- **Single packet per message.** No fragmentation, so text over 80 characters is
  refused. There is no way to send more.
- **Transmit blocks.** `endPacket()` waits for the packet to leave the radio —
  up to about a second at SF7. Serial input is not read during that time, so a
  long message can delay a command you are typing.
- **One receive buffer.** A packet arriving while the previous one is being
  processed is dropped and counted in `/stats` as `dropped`, not queued.
- **No clock.** The `sent` / `received` fields are always zero and omitted.
- **Identity is not stored in the board.** It is a constant in
  `constants.h`; `/user id` lasts until reboot.
- **No encryption, acks, retries, or deduplication.** Anything sent is sent
  once, and a duplicate that arrives is shown again.
- **Broadcast only from the CLI.** Directed messages are supported by the
  payload format but there is no `/message <to> <text>` command yet.

## License

© 2026 David J. Allen. All rights reserved.
