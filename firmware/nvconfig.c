#include <stdio.h>
#include <inttypes.h>
#include <util/crc16.h>		// AVR Libc
#include <avr/eeprom.h>

#include "nvconfig.h"
#include "version.h"
#include "rtconfig.h"
#include "debug.h"

#undef DEBUG_NV_DUMP
#undef DEBUG_NVCONFIG_RAW

struct EEMEM nv_struct nv;

/* ----- Private prototypes ----------------------------------------------- */
static uint16_t nv_calc_crc(void);

/* ----- Public functions ------------------------------------------------- */
int8_t nv_valid_crc(void) {
  if(eeprom_read_word(&nv.crc) == nv_calc_crc() ) {
    return 1;
  }
  debug_puts("INVALID EEPROM CRC\n");
  return 0;
}

void nv_save_config(rtconfig_t *rtc) {
  eeprom_update_word(&nv.size, sizeof(nv.config));

  eeprom_update_dword(&nv.config.version, VERSION_U32);
  eeprom_update_byte(&nv.config.device_address, rtc->device_address);
  eeprom_update_byte(&nv.config.last_used_drive, rtc->last_used_drive);

  eeprom_update_word(&nv.crc, nv_calc_crc());

  debug_puts("Config written to EEPROM\n");
}

int8_t nv_restore_config(rtconfig_t *rtc) {
  /* Restore saved config. Return
  	-1 if a valid old configuration with less data was restored
	 0 on success
	 1 if crc check of saved config failed
  */

  nv_data_dump();
	
  if(!nv_valid_crc()) return 1;

  rtc->device_address =  eeprom_read_byte(&nv.config.device_address);
  rtc->last_used_drive = eeprom_read_byte(&nv.config.last_used_drive);

  if(eeprom_read_dword(&nv.config.version) >= VER32(2,6,11))
  {
    // read value introduced with version 2.6.11
  }

  if(eeprom_read_word(&nv.size) != sizeof(nv.config)) {
    debug_puts("CONFIG WITH LESS DATA READ FROM EEPROM\n");
    return -1;
  }
  debug_puts("Config read from EEPROM\n");
  return 0;
}

/* ----- Private functions ------------------------------------------------ */
static uint16_t nv_calc_crc(void) {
  uint16_t i, crc = 0xffff;

  for (i=0; i < eeprom_read_word(&nv.size); i++) {
    crc = _crc16_update(crc, eeprom_read_byte(&nv.bytes[i]));
  }
  return crc;
}

/* ----- Debug only ------------------------------------------------------- */

void nv_data_dump(void) {
#ifdef DEBUG_NV_DUMP
    debug_puts("EEPROM contents:\n");
    debug_printf("CRC = %u\n", eeprom_read_word(&nv.crc));
    debug_printf("Size: %d\n", eeprom_read_word(&nv.size));
    debug_printf("Ver32: %d\n", eeprom_read_dword(&nv.config.version));
    debug_printf("device_address = %u\n", eeprom_read_byte(&nv.config.device_address));
    debug_printf("last_used_drive = %u\n", eeprom_read_byte(&nv.config.last_used_drive));
    debug_puts("Data via bytes:\n");
#ifdef DEBUG_NVCONFIG_RAW
      for(uint16_t i=0; i < sizeof(nv.bytes); i++) {
        debug_printf("Byte #%u: %u\n", i, eeprom_read_byte(&nv.bytes[i]));
      }
#endif
#endif
}
