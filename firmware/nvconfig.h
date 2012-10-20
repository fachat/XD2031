#ifndef NVCONFIG_H
#define NVCONFIG_H

#include <inttypes.h>
#include "rtconfig.h"

void	nv_data_dump(void);
int8_t	nv_valid_crc (void);		// returns nonzero if crc is valid
void	nv_save_config (rtconfig_t *rtc);
int8_t	nv_restore_config (rtconfig_t *rtc);	
/* Restore saved config. Return
	-1 if a valid old configuration with less data was restored
	 0 on success
	+1 if crc check of saved config failed 
*/

struct nv_config_data 
{
  /********************************************************************
  ***   In order to maintain backwards compatibility, do not remove ***
  ***   or change data. Add new data only at the end.               ***
  *********************************************************************/

  // backwards compatibility
  uint32_t version;

  /* We can *not* use structs here, because that would break
     backwards compatibility when sizeof(struct) changes */
  uint8_t device_address;	// rtconfig 0.9.1
  uint8_t last_used_drive;	// rtconfig 0.9.1

  /* ---> insert new data here <--- */

} __attribute__((packed)) ;

struct nv_struct {
  uint16_t crc;
  uint16_t size; // of all data without crc
  union {
    struct nv_config_data config;
    uint8_t bytes [ sizeof(struct nv_config_data) ];
  };
} __attribute__((packed)) ;

void debug_nvconfig(void); 

#endif
