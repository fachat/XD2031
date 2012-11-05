/*-------------------------------------------------*/
/* I2C bus protocol - bit banging version          */

#include "delay.h"
#include "integer.h"
#include "config.h"	// I2C speed
#include "device.h"	// I2C ports

#define i2c_delay() delayus(I2C_BB_DELAY_US)

/* Generate start condition on the IIC bus */
void i2c_start (void)
{
    SDA_HIGH();
    i2c_delay();
    SCL_HIGH();
    i2c_delay();
    SDA_LOW();
    i2c_delay();
    SCL_LOW();
    i2c_delay();
}


/* Generate stop condition on the IIC bus */
void i2c_stop (void)
{
    SDA_LOW();
    i2c_delay();
    SCL_HIGH();
    i2c_delay();
    SDA_HIGH();
    i2c_delay();
}


/* Send a byte to the IIC bus */
int i2c_send (BYTE dat)
{
    BYTE b = 0x80;
    int ack;

    do {
        if (dat & b)     {  /* SDA = Z/L */
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        i2c_delay();
        SCL_HIGH();
        i2c_delay();
        SCL_LOW();
        i2c_delay();
    } while (b >>= 1);
    SDA_HIGH();
    i2c_delay();
    SCL_HIGH();
    ack = SDA_VAL ? 0 : 1;  /* Sample ACK */
    i2c_delay();
    SCL_LOW();
    i2c_delay();
    return ack;
}


/* Receive a byte from the IIC bus */
BYTE i2c_rcvr (int ack)
{
    UINT d = 1;


    do {
        d <<= 1;
        SCL_HIGH();
        if (SDA_VAL) d++;
        i2c_delay();
        SCL_LOW();
        i2c_delay();
    } while (d < 0x100);
    if (ack) {      /* SDA = ACK */
        SDA_LOW();
    } else {
        SDA_HIGH();
    }
    i2c_delay();
    SCL_HIGH();
    i2c_delay();
    SCL_LOW();
    SDA_HIGH();
    i2c_delay();

    return (BYTE)d;
}


