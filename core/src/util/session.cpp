#include "st/util/session.h"

#include "st/util/strings.h"

#include <cstdlib>

namespace st::util {

std::string currentUserName() {
  for (const char *var : {"USERNAME", "USER", "LOGNAME"}) {
#ifdef _MSC_VER
    char *value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, var) == 0 && value) {
      std::string out = toLower(value);
      std::free(value);
      if (!out.empty())
        return out;
    }
#else
    if (const char *value = std::getenv(var); value && *value)
      return toLower(value);
#endif
  }
  return "unknown";
}

} // namespace st::util
