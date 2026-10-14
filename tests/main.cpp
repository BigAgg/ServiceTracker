#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "st/util/log.h"

int main(int argc, char **argv) {
  // Keep the test output readable; set ST_TEST_LOG=1 to see the log messages.
  if (!std::getenv("ST_TEST_LOG"))
    st::log::setSink([](st::log::Level, std::string_view) {});
  doctest::Context context(argc, argv);
  return context.run();
}
