#pragma once

#include <vector>
#include <functional>

#include "types.h"

namespace beacon {

  template <typename Container, typename Element>
  Element* find_if(Container& c, const std::function<bool(const Element& e)>& func) {
    auto iter = std::find_if(c.begin(), c.end(), func);
    if (iter != c.end()) {
      return &(*iter);
    } else {
      return nullptr;
    }
  }

  string_t join(const strings_t& args, const string_t& separator = " ") {
    return std::accumulate(args.begin(), args.end(), separator);
  }

  template <typename T>
  std::vector<T> copy(const std::vector<T>& v1, int begin, int end = -1) {
    if (begin < 0) {
      begin = 0;
    }
    if (end < 0) {
      end = v1.size();
    }
    std::vector<T> v2;
    std::copy(v1.begin() + begin, v1.begin() + end, back_inserter(v2));
    return v2;
  } 

  strings_t split(const string_t& line, char delimiter = ' ') {
  std::vector<string_t> v;

  int start, end;
  start = end = 0;

  while ((start = line.find_first_not_of(delimiter, end))
           != string_t::npos) {
    // line.find(delimiter, start) will return the index of delimiter
    // from start index
    end = line.find(delimiter, start);
    // substr function return the substring of the
    // original string from the given starting index
    // to the given end index
    v.push_back(line.substr(start, end - start));
  }
  return v;
}
}