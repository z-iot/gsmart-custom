#ifndef STM32PROGRAMMER_H
#define STM32PROGRAMMER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>

#define STERR "ERROR"
#define STM32INIT 0x7F    // Send Init command
#define STM32ACK 0x79     // return ACK answer
#define STM32NACK 0x1F    // return NACK answer
#define STM32GET 0        // get version command
#define STM32GVR 0x01     // get read protection status           never used in here
#define STM32ID 0x02      // get chip ID command
#define STM32RUN 0x21     // Restart and Run programm
#define STM32RD 0x11      // Read flash command                   never used in here
#define STM32WR 0x31      // Write flash command
#define STM32ERASE 0x43   // Erase flash command
#define STM32ERASEN 0x44  // Erase extended flash command
#define STM32WP 0x63      // Write protect command                never used in here
#define STM32WP_NS 0x64   // Write protect no-stretch command     never used in here
#define STM32UNPCTWR 0x73 // Unprotect WR command                 never used in here
#define STM32UW_NS 0x74   // Unprotect write no-stretch command   never used in here
#define STM32RP 0x82      // Read protect command                 never used in here
#define STM32RP_NS 0x83   // Read protect no-stretch command      never used in here
#define STM32UR 0x92      // Unprotect read command               never used in here
#define STM32UR_NS 0x93   // Unprotect read no-stretch command    never used in here

#define STM32STADDR 0x8000000 // STM32 codes start address, you can change to other address if use custom bootloader like 0x8002000
#define STM32ERR 0

const String STM32_CHIPNAME[47] = {
    "Unknown Chip",
    "STM32F030x8/05x",
    "STM32F03xx4/6",
    "STM32F030xC",
    "STM32F04xxx/070x6",
    //"STM32F070x6",
    "STM32F070xB/071xx/072xx",
    //"STM32F071xx/072xx",
    "STM32F09xxx",
    "STM32F10xxx-LD",
    "STM32F10xxx-MD",
    "STM32F10xxx-HD",
    "STM32F10xxx-MD-VL",
    "STM32F10xxx-HD-VL",
    "STM32F105/107",
    "STM32F10xxx-XL-D",
    "STM32F20xxxx",
    "STM32F373xx/378xx",
    //"STM32F378xx",
    "STM32F302xB(C)/303xB(C)/358xx",
    //"STM32F358xx",
    "STM32F301xx/302x4(6/8)/318xx",
    //"STM32F318xx",
    "STM32F303x4(6/8)/334xx/328xx",
    "STM32F302xD(E)/303xD(E)/398xx",
    //"STM32F398xx",
    "STM32F40xxx/41xxx",
    "STM32F42xxx/43xxx",
    "STM32F401xB(C)",
    "STM32F401xD(E)",
    "STM32F410xx",
    "STM32F411xx",
    "STM32F412xx",
    "STM32F446xx",
    "STM32F469xx/479xx",
    "STM32F413xx/423xx",
    "STM32F72xxx/73xxx",
    "STM32F74xxx/75xxx",
    "STM32F76xxx/77xxx",
    "STM32H74xxx/75xxx",
    "STM32L01xxx/02xxx",
    "STM32L031xx/041xx",
    "STM32L05xxx/06xxx",
    "STM32L07xxx/08xxx",
    "STM32L1xxx6(8/B)",
    "STM32L1xxx6(8/B)A",
    "STM32L1xxxC",
    "STM32L1xxxD",
    "STM32L1xxxE",
    "STM32L43xxx/44xxx",
    "STM32L45xxx/46xxx",
    "STM32L47xxx/48xxx",
    "STM32L496xx/4A6xx"};

class Stm32Programmer
{
public:
  Stm32Programmer(HardwareSerial *serial);

  void pushError(String error);

  void sendChar(uint8_t c);

  bool receiveChars(uint8_t *data, uint8_t len);
  bool receiveAck();
  bool receiveData(uint8_t *data, uint8_t len);

  bool sendCommand(uint8_t cmd);
  bool sendAddress(uint32_t addr);
  bool sendData(uint8_t *data, uint8_t len);
  bool sendInit();
  bool sendRun();

  uint8_t calculateChecksum(uint8_t *data, uint8_t len);

  bool flashWrite(uint32_t addr, uint8_t *data, uint8_t len);
  bool flashRead(uint32_t addr, uint8_t *data, uint8_t len);
  bool flashErase();
  bool flashEraseN();

  bool chipVersion(uint8_t *data);
  bool chipId(uint8_t *data);

  HardwareSerial *_serial;
  String _error;
  uint8_t _lastChar;

private:
};

#endif
