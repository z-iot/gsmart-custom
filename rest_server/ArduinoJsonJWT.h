#ifndef ArduinoJsonJWT_H
#define ArduinoJsonJWT_H

#include <Arduino.h>
// #include <ArduinoJson.h>
#include <libb64/cdecode.h>
#include <libb64/cencode.h>
#include "esphome/components/json/json_util.h"

#ifdef ESP32
#include <mbedtls/md.h>
#elif defined(ESP8266)
#include <bearssl/bearssl_hmac.h>
#endif

#define JWT_HEADER "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"

class ArduinoJsonJWT {
 private:
  String _secret;

  String sign(String& value);

  static String encode(const char* cstr, int len);
  static String decode(String value);

 public:
  ArduinoJsonJWT(String secret);

  void setSecret(String secret);
  String getSecret();

  String buildJWT(JsonObject& payload);
  void parseJWT(String jwt, JsonDocument& jsonDocument);
};

#endif
