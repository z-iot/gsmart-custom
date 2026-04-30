#ifndef STM32UPDATER_H
#define STM32UPDATER_H

#include <Arduino.h>
#include <MD5Builder.h>
#include <functional>
#include <HardwareSerial.h>
#include "stm32Programmer.h"

#define UPDATE_ERROR_OK                 (0)
#define UPDATE_ERROR_WRITE              (1)
#define UPDATE_ERROR_ERASE              (2)
#define UPDATE_ERROR_READ               (3)
#define UPDATE_ERROR_SPACE              (4)
#define UPDATE_ERROR_SIZE               (5)
#define UPDATE_ERROR_STREAM             (6)
#define UPDATE_ERROR_MD5                (7)
#define UPDATE_ERROR_MAGIC_BYTE         (8)
#define UPDATE_ERROR_ACTIVATE           (9)
#define UPDATE_ERROR_NO_PARTITION       (10)
#define UPDATE_ERROR_BAD_ARGUMENT       (11)
#define UPDATE_ERROR_ABORT              (12)
#define UPDATE_ERROR_RESTART            (13)

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

namespace esphome {
namespace stm32 {

class Stm32UpdaterClass {
  public:
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;

    Stm32UpdaterClass();
    Stm32UpdaterClass& onProgress(THandlerFunction_Progress fn);
    bool begin(HardwareSerial *serial, size_t size=UPDATE_SIZE_UNKNOWN, int ledPin = -1, uint8_t ledOn = LOW);
    size_t write(uint8_t *data, size_t len);
    size_t writeStream(Stream &data);
    bool end(bool evenIfRemaining = false);
    void abort();
    void printError(Print &out);
    const char * errorString();
    bool setMD5(const char * expected_md5);
    String md5String(void){ return _md5.toString(); }
    void md5(uint8_t * result){ return _md5.getBytes(result); }

    //Helpers
    uint8_t getError(){ return _error; }
    void clearError(){ _error = UPDATE_ERROR_OK; }
    bool hasError(){ return _error != UPDATE_ERROR_OK; }
    bool isRunning(){ return _size > 0; }
    bool isFinished(){ return _progress == _size; }
    size_t size(){ return _size; }
    size_t progress(){ return _progress; }
    size_t remaining(){ return _size - _progress; }

    /*
      Template to write from objects that expose
      available() and read(uint8_t*, size_t) methods
      faster than the writeStream method
      writes only what is available
    */
    template<typename T>
    size_t write(T &data){
      size_t written = 0;
      if (hasError() || !isRunning())
        return 0;

      size_t available = data.available();
      while(available) {
        if(_bufferLen + available > remaining()){
          available = remaining() - _bufferLen;
        }
        if(_bufferLen + available > 255) {
          size_t toBuff = 255 - _bufferLen;
          data.read(_buffer + _bufferLen, toBuff);
          _bufferLen += toBuff;
          if(!_writeBuffer())
            return written;
          written += toBuff;
        } else {
          data.read(_buffer + _bufferLen, available);
          _bufferLen += available;
          written += available;
          if(_bufferLen == remaining()) {
            if(!_writeBuffer()) {
              return written;
            }
          }
        }
        if(remaining() == 0)
          return written;
        available = data.available();
      }
      return written;
    }
    bool canRollBack();
    bool rollBack();

    Stm32Programmer *_programmer;

  private:
    void _reset();
    void _abort(uint8_t err);
    bool _writeBuffer();

    void startBootloader();

    uint8_t _error;
    uint8_t *_buffer;
    size_t _bufferLen;
    size_t _size;
    THandlerFunction_Progress _progress_callback;
    uint32_t _progress;

    String _target_md5;
    MD5Builder _md5;

    int _ledPin;
    uint8_t _ledOn;
};

extern Stm32UpdaterClass Stm32Updater;

}
}
#endif
