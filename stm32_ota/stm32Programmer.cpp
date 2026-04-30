#include "stm32Programmer.h"

Stm32Programmer::Stm32Programmer(HardwareSerial *serial)
{
  _serial = serial;
  _lastChar = 0;
  _error = "";
}

void Stm32Programmer::pushError(String error)
{
  if (_error == "")
    _error = error;
  else
    _error = error + " -> " + _error;
}

void Stm32Programmer::sendChar(uint8_t c)
{
  _serial->write(c);
}

bool Stm32Programmer::receiveChars(uint8_t *data, uint8_t len)
{
  for (int i = 0; i <= len; i++)
  {
    int count = 10;
    while (count > 0 && !_serial->available())
    {
      delay(10);
      count--;
    }
    if (!_serial->available())
    {
      pushError("RECEIVE_CHAR_TIMEOUT");
      return false;
    }
    _lastChar = _serial->read();
    data[i] = _lastChar;
  }
  return true;
}

bool Stm32Programmer::receiveAck()
{
  uint8_t data[1];
  if (!receiveChars(data, 1))
  {
    pushError("RECEIVE_ACK");
    return false;
  }
  if (data[0] != STM32ACK)
  {
    pushError("RECEIVE_ACK (" + String(data[0], HEX) + ")");
    return false;
  }
  return true;
}

bool Stm32Programmer::receiveData(uint8_t *data, uint8_t len)
{
  if (!sendCommand(len))
  {
    pushError("RECEIVE_DATA");
    return false;
  }
  uint8_t buf[1];
  if (!receiveChars(buf, 1))
  {
    pushError("RECEIVE_DATA");
    return false;
  }
  uint8_t receiverChecksum = buf[0];
  if (!receiveChars(data, len))
  {
    pushError("RECEIVE_DATA");
    return false;
  }
  if (calculateChecksum(data, len) != receiverChecksum)
  {
    pushError("RECEIVE_DATA");
    return false;
  }
  return true;
}

bool Stm32Programmer::sendCommand(uint8_t cmd)
{
  sendChar(cmd);
  sendChar(~cmd);
  if (!receiveAck())
  {
    pushError("SEND_COMMAND");
    return false;
  }
  return true;
}

bool Stm32Programmer::sendAddress(uint32_t addr)
{
  uint8_t sendaddr[4];
  uint8_t addcheck = 0;
  sendaddr[0] = addr >> 24;
  sendaddr[1] = (addr >> 16) & 0xFF;
  sendaddr[2] = (addr >> 8) & 0xFF;
  sendaddr[3] = addr & 0xFF;
  for (int i = 0; i <= 3; i++)
  {
    sendChar(sendaddr[i]);
    addcheck ^= sendaddr[i];
  }
  sendChar(addcheck);
  if (!receiveAck())
  {
    pushError("SEND_ADDRESS");
    return false;
  }
  return true;
}

bool Stm32Programmer::sendData(uint8_t *data, uint8_t len)
{
  sendChar(len);
  for (int i = 0; i <= len; i++)
  {
    sendChar(data[i]);
  }
  sendChar(calculateChecksum(data, len));
  if (!receiveAck())
  {
    pushError("SEND_DATA");
    return false;
  }
  return true;
}

bool Stm32Programmer::sendInit()
{
  sendChar(STM32INIT);
  if (!receiveAck())
  {
    pushError("SEND_INIT");
    return false;
  }
  return true;
}

bool Stm32Programmer::sendRun()
{
  if (!sendCommand(STM32RUN))
  {
    pushError("SEND_RUN");
    return false;
  }
  if (!sendAddress(STM32STADDR))
  {
    pushError("SEND_RUN");
    return false;
  }
  return true;
}

uint8_t Stm32Programmer::calculateChecksum(uint8_t *data, uint8_t len)
{
  uint8_t sum = len;
  for (int i = 0; i <= len; i++)
    sum ^= data[i];
  return sum;
}

bool Stm32Programmer::flashWrite(uint32_t addr, uint8_t *data, uint8_t len)
{
  if (!sendCommand(STM32WR))
  {
    pushError("FLASH_WRITE");
    return false;
  }
  if (!sendAddress(addr))
  {
    pushError("FLASH_WRITE");
    return false;
  }
  if (!sendData(data, len))
  {
    pushError("FLASH_WRITE");
    return false;
  }
  return true;
}

bool Stm32Programmer::flashRead(uint32_t addr, uint8_t *data, uint8_t len)
{
  if (!sendCommand(STM32RD))
  {
    pushError("FLASH_READ");
    return false;
  }
  if (!sendAddress(addr))
  {
    pushError("FLASH_READ");
    return false;
  }
  if (!receiveData(data, len))
  {
    pushError("FLASH_READ");
    return false;
  }
  return true;
}

bool Stm32Programmer::flashErase()
{
  if (!sendCommand(STM32ERASE))
  {
    pushError("FLASH_ERASE");
    return false;
  }
  sendChar(0xFF);
  sendChar(0x00);
  if (!receiveAck())
  {
    pushError("FLASH_ERASE");
    return false;
  }
  return true;
}

bool Stm32Programmer::flashEraseN()
{
  if (!sendCommand(STM32ERASEN))
  {
    pushError("FLASH_ERASEN");
    return false;
  }
  sendChar(0xFF);
  sendChar(0xFF);
  sendChar(0x00);
  if (!receiveAck())
  {
    pushError("FLASH_ERASEN");
    return false;
  }
  return true;
}

bool Stm32Programmer::chipVersion(uint8_t *data) // len=14
{
  if (!sendCommand(STM32GET))
  {
    pushError("CHIP_VERSION");
    return false;
  }
  if (!receiveChars(data, 14))
  {
    pushError("CHIP_VERSION");
    return false;
  }
  return true;
}

bool Stm32Programmer::chipId(uint8_t *data) // len=3
{
  if (!sendCommand(STM32ID))
  {
    pushError("CHIP_ID");
    return false;
  }
  if (!receiveChars(data, 3))
  {
    pushError("CHIP_ID");
    return false;
  }
  return true;
  // uint16_t id = data[0];
  // id = (id << 8) + data[1];
}
