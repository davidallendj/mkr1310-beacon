#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <numeric>

#include "types.h"

// Every function here is inline: the sketch is a single translation unit, but
// radio.cpp is a second one, and a non-inline definition in a header would then
// produce duplicate symbols at link time.

namespace beacon {

// Return a pointer to the first element of `c` satisfying `func`, or nullptr.
template <typename Container, typename Element>
inline Element* find_if(Container& c, const std::function<bool(const Element& e)>& func) {
  const auto iter = std::find_if(c.begin(), c.end(), func);
  if (iter != c.end()) {
    return &(*iter);
  }
  return nullptr;
}

// Copy the elements in [begin, end) of `v1` into a new vector. A negative `end`
// means "to the end"; a negative `begin` means 0. Both bounds are clamped to the
// size of `v1`.
template <typename T>
inline std::vector<T> copy(const std::vector<T>& v1, int begin, int end = -1) {
  const int size = static_cast<int>(v1.size());

  if (begin < 0) {
    begin = 0;
  }
  if (end < 0) {
    end = size;
  }

  // Clamp: passing an end past the end of the vector used to produce iterators
  // past the last element and read out of bounds. See BUG-13.
  if (begin > size) {
    begin = size;
  }
  if (end > size) {
    end = size;
  }

  std::vector<T> v2;
  if (begin >= end) {
    return v2;
  }
  v2.reserve(static_cast<size_t>(end - begin));
  std::copy(v1.begin() + begin, v1.begin() + end, std::back_inserter(v2));
  return v2;
}

// Join `args` with `separator`, with no leading or trailing separator.
inline string_t join(const strings_t& args, const string_t& separator = " ") {
  if (args.empty()) {
    return string_t();
  }
  return std::accumulate(
    std::next(args.begin()),
    args.end(),
    args.front(),
    [&separator](const string_t& acc, const string_t& next) {
      return acc + separator + next;
    });
}

// Split `line` on `delimiter`, collapsing runs of delimiters and discarding
// empty fields, so that extra whitespace does not produce empty arguments.
inline strings_t split(const string_t& line, char delimiter = ' ') {
  strings_t fields;

  size_t start = 0;
  while (start <= line.size()) {
    const size_t pos = line.find(delimiter, start);
    const size_t len = (pos == string_t::npos) ? string_t::npos : pos - start;
    const string_t field = line.substr(start, len);
    if (!field.empty()) {
      fields.push_back(field);
    }
    if (pos == string_t::npos) {
      break;
    }
    start = pos + 1;
  }
  return fields;
}

inline bool starts_with(const string_t& s, const string_t& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

inline bool ends_with(const string_t& s, const string_t& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline string_t to_lower(string_t s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(::tolower(c));
  });
  return s;
}

inline string_t trim(const string_t& s) {
  const char* whitespace = " \t\r\n";
  const size_t start = s.find_first_not_of(whitespace);
  if (start == string_t::npos) {
    return string_t();
  }
  const size_t end = s.find_last_not_of(whitespace);
  return s.substr(start, end - start + 1);
}

// Remove a single leading `prefix` character, e.g. the '/' in "/help".
inline string_t strip_prefix(const string_t& s, char prefix) {
  if (!s.empty() && s[0] == prefix) {
    return s.substr(1);
  }
  return s;
}

// Interpret `value` as a number. Returns false and leaves `out` untouched when
// `value` is not a complete, valid number.
inline bool parse_int(const string_t& value, long& out) {
  if (value.empty()) {
    return false;
  }
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') {
    return false;
  }
  out = parsed;
  return true;
}

// Integer to string. `std::to_string` is avoided deliberately: its avr-libc
// implementation is unreliable on some cores, and `snprintf` is always present.
inline string_t to_str(long value) {
  char buffer[24];
  buffer[0] = '\0';
  snprintf(buffer, sizeof(buffer), "%ld", value);
  return string_t(buffer);
}

}  // namespace beacon
