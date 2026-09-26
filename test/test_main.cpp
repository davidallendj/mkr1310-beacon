// Entry point for the host-side test run.

#include "testing.h"

void test_util();
void test_cli();
void test_payload();
void test_radio();
void test_session();
void test_line_reader();

void run_all_tests() {
  test_util();
  test_cli();
  test_payload();
  test_radio();
  test_session();
  test_line_reader();
}
