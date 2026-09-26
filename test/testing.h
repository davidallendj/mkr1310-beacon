#pragma once

// Tiny assertion framework. The point is a readable failure line, not a
// dependency.

#include <cstdio>
#include <string>

namespace testing {

extern int g_checks;
extern int g_failures;

void begin(const char* name);
void summary();
void failure(const char* file, int line, const std::string& message);

// Defined in test_main.cpp; runs every suite in dependency order.
void run_all_tests();

inline std::string to_text(const std::string& value) {
  return "\"" + value + "\"";
}
inline std::string to_text(const char* value) {
  return value == nullptr ? std::string("(null)") : "\"" + std::string(value) + "\"";
}
inline std::string to_text(bool value) {
  return value ? "true" : "false";
}
template <typename T>
inline std::string to_text(const T& value) {
  return std::to_string(value);
}

inline bool check(bool condition, const char* expr, const char* file, int line) {  ++g_checks;
  if (!condition) {
    failure(file, line, std::string("expected: ") + expr);
  }
  return condition;
}

template <typename A, typename B>
inline bool check_eq(const A& actual, const B& expected, const char* expr,
                     const char* file, int line) {
  ++g_checks;
  if (!(actual == expected)) {
    const std::string message = std::string("expected: ") + expr +
                                "\n      actual: " + to_text(actual) +
                                "\n    expected: " + to_text(expected);
    failure(file, line, message);
    return false;
  }
  return true;
}

}  // namespace testing

// Defined in test_main.cpp; runs every suite in dependency order.
void run_all_tests();

#define TEST(name) testing::begin(name)
#define CHECK(cond) testing::check((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
  testing::check_eq((actual), (expected), #actual, __FILE__, __LINE__)
