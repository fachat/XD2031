#ifndef I2C_H_DEFINED
#define I2C_H_DEFINED

#include "integer.h"

int iic_write (BYTE, UINT, UINT, const void*);  /* Write to I2C device */
int iic_read (BYTE, UINT, UINT, void*);         /* Read from I2C device */
void iic_start (void);      /* Generate start condition on the IIC bus */
int iic_send (BYTE dat);    /* Send a byte to the IIC bus */
BYTE iic_rcvr (int ack);    /* Receive a byte from the IIC bus */
void iic_stop (void);       /* Generate stop condition on the IIC bus */

#endif
