#pragma once
#include <ArduinoJson.h>
#include <cstdlib>

namespace gsmart_glink {
// Exact-size, fallible allocation: std::string's growth can abort on ESP8266
// OOM. Release the JSON pools before the socket allocates its TCP send buffer.
template<class Sender> bool send_serialized(JsonDocument &doc, Sender sender) {
  if (doc.overflowed()) return false;
  const size_t length = measureJson(doc);
  auto *data = static_cast<char *>(std::malloc(length + 1));
  if (!data) return false;
  const size_t written = serializeJson(doc, data, length + 1);
  doc.clear();
  const bool ok = written == length && sender(data, length);
  std::free(data);
  return ok;
}
}  // namespace gsmart_glink
