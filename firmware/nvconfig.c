#include <stdio.h>
#include <inttypes.h>
#include <util/crc16.h>		// AVR Libc
#include <avr/eeprom.h>

#include "nvconfig.h"
#include "version.h"
#include "rtconfig.h"
#include "debug.h"

struct EEMEM nv_struct nv;

/* ----- Private prototypes ----------------------------------------------- */
static uint16_t nv_calc_crc(void);

/* ----- Public functions ------------------------------------------------- */
int8_t nv_valid_crc(void) {
  return (eeprom_read_word(&nv.crc) == nv_calc_crc() );
}

void nv_save_config(rtconfig_t *rtc) {
  eeprom_update_word(&nv.size, sizeof(nv.config));

  eeprom_update_dword(&nv.config.version, VERSION_U32);
  eeprom_update_byte(&nv.config.device_address, rtc->device_address);
  eeprom_update_byte(&nv.config.last_used_drive, rtc->last_used_drive);

  eeprom_update_word(&nv.crc, nv_calc_crc());
}

int8_t nv_restore_config(rtconfig_t *rtc) {
  /* Restore saved config. Return
  	-1 if a valid old configuration with less data was restored
	 0 on success
	 1 if crc check of saved config failed
  */
	
  if(!nv_valid_crc()) return 1;

  rtc->device_address =  eeprom_read_byte(&nv.config.device_address);
  rtc->last_used_drive = eeprom_read_byte(&nv.config.last_used_drive);

  if(eeprom_read_dword(&nv.config.version) >= VER32(2,6,11))
  {
    // read value introduced with version 2.6.1
  }

  if(eeprom_read_word(&nv.size) != sizeof(nv.config)) return -1;
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

#if 0
#define EE2RAM_BYTE(x) eeprom_read_byte(&x)
#define EE2RAM_WORD(x) eeprom_read_word(&x)

void data_dump(void) {
  uint16_t i;

  debug_puts("Stored data:\n");
  debug_printf("crc = %u, valid: %u\n", EE2RAM_WORD(nv.crc), nv_valid_crc());
  debug_printf("device_address = %u\n", EE2RAM_BYTE(nv.config.device_address));
  debug_printf("Version %u.%u.%u\n", 
  	EE2RAM_BYTE(nv.config.version_major), 
	EE2RAM_BYTE(nv.config.version_minor), 
	EE2RAM_BYTE(nv.config.version_patchlevel));
  debug_puts("Data via bytes:\n");
  for(i=0; i < sizeof(nv.bytes); i++) {
    debug_printf("Byte #%u: %u\n", i, EE2RAM_BYTE(nv.bytes[i]));
  }
  debug_putcrlf();
}

void debug_nvconfig(void) {
  debug_printf("Size with crc: %d\n", (int) sizeof(nv));
  debug_printf("Size without crc via config: %d\n", (int) sizeof(nv.config));
  debug_printf("Size without crc via bytes: %d\n\n", (int) sizeof(nv.bytes));

  // Init data
  nv_save_config();
  
  data_dump();

  eeprom_update_byte(&nv.config.device_address, 9);	// smashes valid crc
  data_dump();

  eeprom_update_byte(&nv.config.device_address, 8);
  data_dump();

  for(;;); 
}
#endif

#if 0
// The following is the equivalent functionality 
// of AVR Libc's util/crc16.h crc16_update written in C:
uint16_t _crc16_update(uint16_t crc, uint8_t a)
{
    int i;

    crc ^= a;
    for (i = 0; i < 8; ++i)
    {
	if (crc & 1)
	    crc = (crc >> 1) ^ 0xA001;
	else
	    crc = (crc >> 1);
    }

    return crc;
}
#endif
