# BEACON for Arduino MKR 1310 WAN

This is a tiny sketch to load software to send/receive messages with the Arduino MKR 1310 WAN board using LoRa frequencies. It is experimental and not meant for production or commericial use. Development is done with Arduino IDE 2.

## Getting Started

1. Clone the repository.

```bash
git clone https://github.com/davidallendj/mkr1310-beacon && cd mkr1310-beacon
```

2. Install and set up Arduino IDE. On ArchLinux, I install the packages via an AUR helper like `yay`.

```bash
yay -S arduino-cli arduino-language-server arduino-ide-bin
```

3. Open the `mkr1310-beacon.ino` sketch.
4. Install board software and libraries:
    - *Boards*
        - Arduino AVR Boards (maybe unnecessary?)
        - Arduino SAMB Boards
    - *Libraries*
        - MKRWAN
        - MKRWAN_v2
        - ArduinoJson
        - LoRa
        - UUID
        - Arduino_AVRSTL

5. Select board, compile sketch, and upload (probably on `/dev/ttyACM0`)
6. While connected to a PC via USB, open the serial monitor. You should see some output:

```bash
Arduino MKR 1310 WAN - Experimental Thin Client
Software written by David J. Allen (davidallendj@gmail.com)
GitHub: https://github.com/davidallendj
© 2026 David J. Allen. All rights reserved

User: 
  ID:   642e9e60-b9ee-4d3b-9903-190c91e250ae
  Name: lora

For help getting started, try '/help' and press enter.
```

