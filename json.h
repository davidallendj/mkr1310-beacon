#pragma once

#include <ArduinoJson.h>

#include <string>

#include "types.h"

namespace beacon {

// Serialize `doc` to a JSON string. Returning by value (rather than a
// `const char*` into a local buffer) is what keeps callers from reading freed
// memory -- see BUG-03.
inline string_t serialize(const JsonDocument& doc) {
  string_t out;
  serializeJson(doc, out);
  return out;
}

// Report whether `input` is syntactically valid JSON.
//
// Uses a plain `JsonDocument` with no filter: an empty filter document drops
// every key, so the previous `DeserializationOption::Filter(filter)` form was
// both deprecated and meaningless. See BUG-06.
inline bool validate_json(const char* input) {
  if (input == nullptr || input[0] == '\0') {
    return false;
  }
  JsonDocument doc;
  return deserializeJson(doc, input) == DeserializationError::Ok;
}

inline bool validate_json(const string_t& input) {
  return validate_json(input.c_str());
}

}  // namespace beacon
