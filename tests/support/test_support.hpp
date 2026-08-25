#ifndef RPCMP_TEST_SUPPORT_HPP
#define RPCMP_TEST_SUPPORT_HPP

#include <iostream>
#include <string>

namespace rpcmp::test {

class Suite {
public:
  void check(const bool condition, const char* expression, const char* file, const int line) {
    if (!condition) {
      ++failures_;
      std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
    }
  }

  int finish(const std::string& name) const {
    if (failures_ == 0) {
      std::cout << name << ": PASS\n";
      return 0;
    }
    std::cerr << name << ": " << failures_ << " failure(s)\n";
    return 1;
  }

private:
  int failures_{};
};

} // namespace rpcmp::test

#define RPCMP_CHECK(suite, expression) (suite).check((expression), #expression, __FILE__, __LINE__)

#endif // RPCMP_TEST_SUPPORT_HPP
