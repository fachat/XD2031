#ifndef NVCONFIG_H
#define NVCONFIG_H

#include <inttypes.h>

int8_t	nv_valid_crc (void);		// returns nonzero if crc is valid
void	nv_save_config (void);
uint8_t	nv_restore_config (void);	// returns 0 on success

struct nv_config_data 
{
  /*********************************************************************
   ***   Do not remove or change data! You may add data at the end.  ***
   *********************************************************************/
  uint8_t version_major, version_minor, version_patchlevel;
  uint8_t device_address;
  /* ---> insert new data here <--- */
} __attribute__((packed)) ;

struct nv_struct {
  uint16_t crc;
  union {
    struct nv_config_data config;
    uint8_t bytes [ sizeof(struct nv_config_data) ];
  };
} __attribute__((packed)) ;

void debug_nvconfig(void); 

#endif
