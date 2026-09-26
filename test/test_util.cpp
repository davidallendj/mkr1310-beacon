// Tests for the string and container helpers in util.h.

#include "testing.h"

#include "util.h"

using beacon::copy;
using beacon::ends_with;
using beacon::join;
using beacon::parse_int;
using beacon::split;
using beacon::starts_with;
using beacon::strip_prefix;
using beacon::to_lower;
using beacon::to_str;
using beacon::trim;

void test_util() {
  TEST("util: split");
  {
    // Regression guard for the original splitter, which produced an empty
    // leading field for a string starting with a delimiter.
    const strings_t fields = split("beacon message hello world");
    CHECK_EQ(fields.size(), 4u);
    CHECK_EQ(fields[0], string_t("beacon"));
    CHECK_EQ(fields[3], string_t("world"));

    const strings_t empty = split("");
    CHECK_EQ(empty.size(), 0u);

    // Runs of delimiters collapse instead of yielding empty fields, so a
    // double space cannot turn into a blank message word.
    const strings_t spaced = split("hello   there");
    CHECK_EQ(spaced.size(), 2u);
    CHECK_EQ(spaced[1], string_t("there"));

    const strings_t leading = split("  hello");
    CHECK_EQ(leading.size(), 1u);
    CHECK_EQ(leading[0], string_t("hello"));
  }

  TEST("util: join");
  {
    // BUG-09b: the old implementation seeded the accumulator with the
    // separator, so the result began with a delimiter.
    const strings_t words = {"hello", "there"};
    CHECK_EQ(join(words), string_t("hello there"));
    CHECK_EQ(join(words, "-"), string_t("hello-there"));
    CHECK_EQ(join(strings_t()), string_t(""));
    CHECK_EQ(join(strings_t{"solo"}), string_t("solo"));

    // BUG-10: the `message` command used a bare std::accumulate, which
    // concatenated words with no separator at all.
    CHECK_EQ(join(split("hello there world")), string_t("hello there world"));
  }

  TEST("util: copy");
  {
    const strings_t source = {"a", "b", "c", "d"};
    CHECK_EQ(join(copy(source, 0)), string_t("a b c d"));
    CHECK_EQ(join(copy(source, 1)), string_t("b c d"));
    CHECK_EQ(join(copy(source, 1, 3)), string_t("b c"));
    CHECK_EQ(join(copy(source, 2, 2)), string_t(""));
    // An end past the end of the vector must not run off it.
    CHECK_EQ(join(copy(source, 2, 99)), string_t("c d"));
    CHECK_EQ(copy(source, 0).size(), 4u);
  }

  TEST("util: trim / to_lower / prefixes");
  {
    CHECK_EQ(trim("  hello  "), string_t("hello"));
    CHECK_EQ(trim("hello"), string_t("hello"));
    CHECK_EQ(trim("   "), string_t(""));
    CHECK_EQ(trim(""), string_t(""));

    CHECK_EQ(to_lower("DeBuG"), string_t("debug"));
    CHECK_EQ(strip_prefix("/help", '/'), string_t("help"));
    CHECK_EQ(strip_prefix("help", '/'), string_t("help"));
    CHECK_EQ(strip_prefix("/", '/'), string_t(""));
    CHECK_EQ(strip_prefix("", '/'), string_t(""));

    CHECK(starts_with("--quiet", "--"));
    CHECK(!starts_with("-q", "--"));
    CHECK(ends_with("packet", "ket"));
    CHECK(!ends_with("packet", "pack"));
  }

  TEST("util: parse_int / to_str");
  {
    long value = 0;
    CHECK(parse_int("42", value));
    CHECK_EQ(value, 42L);
    CHECK(parse_int("-7", value));
    CHECK_EQ(value, -7L);

    // Trailing junk must be rejected rather than silently truncated.
    CHECK(!parse_int("42abc", value));
    CHECK(!parse_int("", value));
    CHECK(!parse_int("abc", value));

    // std::to_string is avoided on device; snprintf is used instead.
    CHECK_EQ(to_str(0), string_t("0"));
    CHECK_EQ(to_str(255), string_t("255"));
    CHECK_EQ(to_str(-100), string_t("-100"));
  }
}
