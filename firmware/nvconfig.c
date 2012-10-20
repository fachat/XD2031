#include <stdio.h>
#include <inttypes.h>

#include "nvconfig.h"

uint16_t crc16_update(uint16_t crc, uint8_t a)
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

struct nv_struct nv;

void data_dump(void) {
  uint16_t i;

  printf("Stored data: \n");
  printf("crc = %d\n", nv.crc);
  printf("device_address = %d\n", nv.config.device_address);
  printf("Version %d.%d.%d\n", nv.config.version_major, nv.config.version_minor, nv.config.version_patchlevel);
  printf("Data via bytes: \n");
  for(i=0; i < sizeof(nv.bytes); i++) {
    printf("%d. Wert: %d\n", i, nv.bytes[i]);
  }
  printf("\n");
}

void calc_and_store_crc(void) {
  uint16_t i, crc = 0xffff;

  for (i=0; i < sizeof(nv.bytes); i++) crc = crc16_update(crc, nv.bytes[i]);
  nv.crc = crc;
}

int main(void) {
  printf("Size with crc: %d\n", (int) sizeof(nv));
  printf("Size without crc via config: %d\n", (int) sizeof(nv.config));
  printf("Size without crc via bytes: %d\n\n", (int) sizeof(nv.bytes));

  // Init data
  nv.config.device_address = 8;
  nv.config.version_major = 1;
  nv.config.version_minor = 2;
  nv.config.version_patchlevel = 3;
  
  calc_and_store_crc();
  data_dump();

  nv.config.device_address = 9;
  calc_and_store_crc();
  data_dump();

  nv.config.device_address = 8;
  calc_and_store_crc();
  data_dump();

  return 0;
}


