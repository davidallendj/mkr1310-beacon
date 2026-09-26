#pragma once

#include <stdint.h>

// Nested namespace definitions rather than `namespace beacon::constants`,
// which is C++17: the SAMD core is built with -std=gnu++11.
//
// Constants are `constexpr` so they have internal linkage and can appear in
// more than one translation unit without duplicate symbols.
namespace beacon {
namespace constants {

// --- serial -------------------------------------------------------------
constexpr unsigned long serial_baud_rate = 9600;

// --- LoRa radio ---------------------------------------------------------
// arduino-LoRa 0.8.0 applies frequency and TX power inside begin(); bandwidth,
// spreading factor, coding rate and sync word are left at the SX1276 register
// defaults. They are set explicitly here so that every node agrees on the air
// interface, and so the values live in one place instead of being implicit in
// the library.
//
// Note: in 0.8.0 these setters return void, so a rejected value cannot be
// detected here. The values below match the SX1276 defaults that begin() leaves
// in place, so setting them is effectively a no-op that documents intent.
constexpr long lora_frequency = 915E6;      // 915 MHz (US ISM)
constexpr long lora_bandwidth = 125000;      // 125 kHz
constexpr int lora_spreading_factor = 7;     // SF7
constexpr int lora_coding_rate = 5;          // 4/5 (the library wants the denominator)
constexpr int lora_sync_word = 0x12;         // private network
constexpr int lora_tx_power = 17;            // dBm (library default)

// Payload CRC is on by default in the SX1276 and arduino-LoRa 0.8.0 exposes no
// setter for it. The library already discards CRC-failed packets before
// invoking the receive callback, so nothing is needed on our side.

// The SX1276 receive FIFO holds at most 255 bytes, which is the hard ceiling on
// a single message. See `radio.h` for how the JSON envelope is budgeted.
constexpr int lora_max_packet_size = 255;

// Receiver name used for a broadcast, so a message with no intended recipient
// is still legible on the wire.
constexpr const char* broadcast_name = "all";

// --- local identity -----------------------------------------------------
// Paste a UUID (v4) here to keep the same identity across reboots. While this
// is empty a random UUID is generated at boot and printed once, and the
// terminal shows the line to paste in. Two nodes cannot be told apart reliably
// until this is set.
constexpr const char* user_id = "";
constexpr const char* user_name = "lora";

}  // namespace constants
}  // namespace beacon
