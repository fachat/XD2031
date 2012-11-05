#ifndef I2C_H_DEFINED
#define I2C_H_DEFINED

#include "integer.h"

int8_t i2c_write (BYTE, UINT, UINT, const void*);  /* Write to I2C device */
int8_t i2c_read (BYTE, UINT, UINT, void*);         /* Read from I2C device */
void i2c_start (void);      /* Generate start condition on the IIC bus */
int8_t i2c_send (BYTE dat);    /* Send a byte to the IIC bus */
BYTE i2c_rcvr (int ack);    /* Receive a byte from the IIC bus */
void i2c_stop (void);       /* Generate stop condition on the IIC bus */

#endif
