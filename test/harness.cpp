#include "testing.h"

#include "stubs/Arduino.h"
#include "stubs/LoRa.h"

namespace testing {

int g_checks = 0;
int g_failures = 0;

void begin(const char* name) {
  std::printf("\n  %s\n", name);
}

void failure(const char* file, int line, const std::string& message) {
  ++g_failures;
  std::printf("    FAIL %s:%d\n      %s\n", file, line, message.c_str());
}

void summary() {
  std::printf("\n  %d checks, %d failures\n", g_checks, g_failures);
}

}  // namespace testing

namespace arduino_stub {

std::string out;
std::string in;
long millis_value = 0;
int interrupts_enabled = 1;

void reset() {
  out.clear();
  in.clear();
  millis_value = 0;
  interrupts_enabled = 1;
}

void feed(const std::string& text) { in += text; }

}  // namespace arduino_stub

SerialStub Serial;
LoRaStub LoRa;

int main() {
  std::printf("mkr1310-beacon host tests\n");
  run_all_tests();
  testing::summary();
  return testing::g_failures == 0 ? 0 : 1;
}
