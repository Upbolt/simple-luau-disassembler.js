#include <optional>
#include <string>

namespace sld
{
  std::optional<std::string> deserialize(const char *data, size_t len);
}