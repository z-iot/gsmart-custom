#include "stm32Updater.h"
#include "esphome/core/log.h"
#include <HardwareSerial.h>

namespace esphome
{
    namespace stm32
    {

        static const char *const TAG = "stm32Updater";

        static const char *_err2str(uint8_t _error)
        {
            if (_error == UPDATE_ERROR_OK)
            {
                return ("No Error");
            }
            else if (_error == UPDATE_ERROR_WRITE)
            {
                return ("Flash Write Failed");
            }
            else if (_error == UPDATE_ERROR_ERASE)
            {
                return ("Flash Erase Failed");
            }
            else if (_error == UPDATE_ERROR_READ)
            {
                return ("Flash Read Failed");
            }
            else if (_error == UPDATE_ERROR_SPACE)
            {
                return ("Not Enough Space");
            }
            else if (_error == UPDATE_ERROR_SIZE)
            {
                return ("Bad Size Given");
            }
            else if (_error == UPDATE_ERROR_STREAM)
            {
                return ("Stream Read Timeout");
            }
            else if (_error == UPDATE_ERROR_MD5)
            {
                return ("MD5 Check Failed");
            }
            else if (_error == UPDATE_ERROR_MAGIC_BYTE)
            {
                return ("Wrong Magic Byte");
            }
            else if (_error == UPDATE_ERROR_ACTIVATE)
            {
                return ("Could Not Activate The Firmware");
            }
            else if (_error == UPDATE_ERROR_NO_PARTITION)
            {
                return ("Partition Could Not be Found");
            }
            else if (_error == UPDATE_ERROR_BAD_ARGUMENT)
            {
                return ("Bad Argument");
            }
            else if (_error == UPDATE_ERROR_ABORT)
            {
                return ("Aborted");
            }
            else if (_error == UPDATE_ERROR_RESTART)
            {
                return ("Restart Failed");
            }
            return ("UNKNOWN");
        }

        void Stm32UpdaterClass::startBootloader()
        {
            _programmer->_serial->write(";");
            _programmer->_serial->write("33;");
            _programmer->_serial->flush();
            delay(10);
            String flush = "";
            while (_programmer->_serial->available())
            {
                flush += String(_programmer->_serial->read(), HEX) + " ";
                delay(1);
            }
            ESP_LOGD(TAG, "flash bootloader: %s", flush.c_str());
            delay(100);
            // _programmer->_serial->begin(115200, SERIAL_8E1);
        }

        Stm32UpdaterClass::Stm32UpdaterClass()
            : _error(0), _buffer(0), _bufferLen(0), _size(0), _progress_callback(NULL), _progress(0)
        {
        }

        Stm32UpdaterClass &Stm32UpdaterClass::onProgress(THandlerFunction_Progress fn)
        {
            _progress_callback = fn;
            return *this;
        }

        void Stm32UpdaterClass::_reset()
        {
            if (_buffer)
                delete[] _buffer;
            _buffer = 0;
            _bufferLen = 0;
            _progress = 0;
            _size = 0;

            if (_ledPin != -1)
            {
                digitalWrite(_ledPin, !_ledOn); // off
            }
        }

        bool Stm32UpdaterClass::canRollBack()
        {
            if (_buffer)
            { // Update is running
                return false;
            }
            return true;
        }

        bool Stm32UpdaterClass::rollBack()
        {
            if (_buffer)
            { // Update is running
                return false;
            }
            return false;
        }

        bool Stm32UpdaterClass::begin(HardwareSerial *serial, size_t size, int ledPin, uint8_t ledOn)
        {
            ESP_LOGD(TAG, "flash begin");

            if (_size > 0)
            {
                log_w("already running");
                return false;
            }

            _ledPin = ledPin;
            _ledOn = !!ledOn; // 0(LOW) or 1(HIGH)

            _reset();
            _error = 0;
            _target_md5 = emptyString;
            _md5 = MD5Builder();

            if (size == 0)
            {
                _error = UPDATE_ERROR_SIZE;
                return false;
            }

            // if (command == U_FLASH) {
            //     // _partition = esp_ota_get_next_update_partition(NULL);
            //     // if(!_partition){
            //     //     _error = UPDATE_ERROR_NO_PARTITION;
            //     //     return false;
            //     // }
            //     // log_d("OTA Partition: %s", _partition->label);
            // }
            // else {
            //     _error = UPDATE_ERROR_BAD_ARGUMENT;
            //     log_e("bad command %u", command);
            //     return false;
            // }

            // if(size == UPDATE_SIZE_UNKNOWN){
            //     size = _partition->size;
            // } else if(size > _partition->size){
            //     _error = UPDATE_ERROR_SIZE;
            //     log_e("too large %u > %u", size, _partition->size);
            //     return false;
            // }

            // initialize
            _buffer = (uint8_t *)malloc(255);
            if (!_buffer)
            {
                log_e("malloc failed");
                return false;
            }
            _size = size;
            _md5.begin();

            _programmer = new Stm32Programmer(serial);

            startBootloader();

            // int count = 5;
            // while (count > 0 && !_programmer->sendInit())
            // {
            //   delay(50);
            //   count--;
            // }

            // return count > 0;
            if (!_programmer->sendInit())
            {
                _abort(UPDATE_ERROR_ACTIVATE);
                return false;
            }
            return true;
        }

        void Stm32UpdaterClass::_abort(uint8_t err)
        {
            ESP_LOGD(TAG, "flash abort %d", err);
            _reset();
            _error = err;
        }

        void Stm32UpdaterClass::abort()
        {
            _abort(UPDATE_ERROR_ABORT);
        }

        bool Stm32UpdaterClass::_writeBuffer()
        {
            // first bytes of new firmware
            if (!_progress)
            {
                // check magic
                //  if(_buffer[0] != ESP_IMAGE_HEADER_MAGIC){
                //      _abort(UPDATE_ERROR_MAGIC_BYTE);
                //      return false;
                //  }
            }
            if (!_progress && _progress_callback)
            {
                _progress_callback(0, _size);
            }
            // if(!ESP.partitionEraseRange(_partition, _progress, SPI_FLASH_SEC_SIZE)){
            //     _abort(UPDATE_ERROR_ERASE);
            //     return false;
            // }
            // if (!ESP.partitionWrite(_partition, _progress + skip, (uint32_t*)_buffer + skip/sizeof(uint32_t), _bufferLen - skip)) {
            //     _abort(UPDATE_ERROR_WRITE);
            //     return false;
            // }
            // restore magic or md5 will fail
            // if(!_progress && _command == U_FLASH){
            //     _buffer[0] = ESP_IMAGE_HEADER_MAGIC;
            // }

            ESP_LOGD(TAG, "flash write %d (%d)", _progress, _bufferLen);
            if (!_programmer->flashWrite(STM32STADDR + _progress, _buffer, _bufferLen))
            {
                _abort(UPDATE_ERROR_WRITE);
                return false;
            }

            _md5.add(_buffer, _bufferLen);
            _progress += _bufferLen;
            _bufferLen = 0;
            if (_progress_callback)
            {
                _progress_callback(_progress, _size);
            }
            return true;
        }

        bool Stm32UpdaterClass::setMD5(const char *expected_md5)
        {
            if (strlen(expected_md5) != 32)
            {
                return false;
            }
            _target_md5 = expected_md5;
            return true;
        }

        bool Stm32UpdaterClass::end(bool evenIfRemaining)
        {
            if (hasError() || _size == 0)
            {
                return false;
            }

            if (!isFinished() && !evenIfRemaining)
            {
                log_e("premature end: res:%u, pos:%u/%u\n", getError(), progress(), _size);
                _abort(UPDATE_ERROR_ABORT);
                return false;
            }

            if (evenIfRemaining)
            {
                if (_bufferLen > 0)
                {
                    _writeBuffer();
                }
                _size = progress();
            }

            _md5.calculate();
            if (_target_md5.length())
            {
                if (_target_md5 != _md5.toString())
                {
                    _abort(UPDATE_ERROR_MD5);
                    return false;
                }
            }

            // if (!stm32SendCommand(STM32RUN))
            // {
            //     _abort(UPDATE_ERROR_RESTART);
            //     return false;
            // }
            if (!_programmer->sendRun())
            {
                _abort(UPDATE_ERROR_RESTART);
                return false;
            }

            _programmer->_serial->begin(115200, SERIAL_8N1);
            delete _programmer;

            return true;
        }

        size_t Stm32UpdaterClass::write(uint8_t *data, size_t len)
        {
            if (hasError() || !isRunning())
            {
                return 0;
            }

            if (len > remaining())
            {
                _abort(UPDATE_ERROR_SPACE);
                return 0;
            }

            size_t left = len;

            while ((_bufferLen + left) > 255)
            {
                size_t toBuff = 255 - _bufferLen;
                memcpy(_buffer + _bufferLen, data + (len - left), toBuff);
                _bufferLen += toBuff;
                if (!_writeBuffer())
                {
                    return len - left;
                }
                left -= toBuff;
            }
            memcpy(_buffer + _bufferLen, data + (len - left), left);
            _bufferLen += left;
            if (_bufferLen == remaining())
            {
                if (!_writeBuffer())
                {
                    return len - left;
                }
            }
            return len;
        }

        size_t Stm32UpdaterClass::writeStream(Stream &data)
        {
            size_t written = 0;
            size_t toRead = 0;
            int timeout_failures = 0;

            if (hasError() || !isRunning())
                return 0;

            if (_ledPin != -1)
            {
                pinMode(_ledPin, OUTPUT);
            }

            while (remaining())
            {
                if (_ledPin != -1)
                {
                    digitalWrite(_ledPin, _ledOn); // Switch LED on
                }
                size_t bytesToRead = 255 - _bufferLen;
                if (bytesToRead > remaining())
                {
                    bytesToRead = remaining();
                }

                /*
                Init read&timeout counters and try to read, if read failed, increase counter,
                wait 100ms and try to read again. If counter > 300 (30 sec), give up/abort
                */
                toRead = 0;
                timeout_failures = 0;
                while (!toRead)
                {
                    toRead = data.readBytes(_buffer + _bufferLen, bytesToRead);
                    if (toRead == 0)
                    {
                        timeout_failures++;
                        if (timeout_failures >= 300)
                        {
                            _abort(UPDATE_ERROR_STREAM);
                            return written;
                        }
                        delay(100);
                    }
                }

                if (_ledPin != -1)
                {
                    digitalWrite(_ledPin, !_ledOn); // Switch LED off
                }
                _bufferLen += toRead;
                if ((_bufferLen == remaining() || _bufferLen == 255) && !_writeBuffer())
                    return written;
                written += toRead;
            }
            return written;
        }

        void Stm32UpdaterClass::printError(Print &out)
        {
            out.println(_err2str(_error));
        }

        const char *Stm32UpdaterClass::errorString()
        {
            return _err2str(_error);
        }

        Stm32UpdaterClass Stm32Updater;

    }
}