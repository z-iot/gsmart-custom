/* REX 1 MiB: 32 KiB LittleFS, existing EEPROM/RF/Wi-Fi addresses preserved.
 * Sketch + compressed OTA staging must fit below 0x402F3000.
 * Derived from Arduino ESP8266 eagle.flash.1m64.ld, with a smaller FS. */
MEMORY
{
  dport0_0_seg : org = 0x3FF00000, len = 0x10
  dram0_0_seg : org = 0x3FFE8000, len = 0x14000
  irom0_0_seg : org = 0x40201010, len = 0xf1ff0
}
PROVIDE ( _FS_start = 0x402F3000 );
PROVIDE ( _FS_end = 0x402FB000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x1000 );
PROVIDE ( _EEPROM_start = 0x402fb000 );
PROVIDE ( _SPIFFS_start = 0x402F3000 );
PROVIDE ( _SPIFFS_end = 0x402FB000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x1000 );
INCLUDE "local.eagle.app.v6.common.ld"
