/*-------------------------------------------------*/
/* I2C bus protocol - bit banging version          */

#include "delay.h"
#include "integer.h"
#include "config.h"	// I2C speed
#include "fatfshw.h"	// I2C ports

#define iic_delay() delayus(I2C_BB_DELAY_US)

/* Generate start condition on the IIC bus */
void iic_start (void)
{
    SDA_HIGH();
    iic_delay();
    SCL_HIGH();
    iic_delay();
    SDA_LOW();
    iic_delay();
    SCL_LOW();
    iic_delay();
}


/* Generate stop condition on the IIC bus */
void iic_stop (void)
{
    SDA_LOW();
    iic_delay();
    SCL_HIGH();
    iic_delay();
    SDA_HIGH();
    iic_delay();
}


/* Send a byte to the IIC bus */
int iic_send (BYTE dat)
{
    BYTE b = 0x80;
    int ack;

    do {
        if (dat & b)     {  /* SDA = Z/L */
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        iic_delay();
        SCL_HIGH();
        iic_delay();
        SCL_LOW();
        iic_delay();
    } while (b >>= 1);
    SDA_HIGH();
    iic_delay();
    SCL_HIGH();
    ack = SDA_VAL ? 0 : 1;  /* Sample ACK */
    iic_delay();
    SCL_LOW();
    iic_delay();
    return ack;
}


/* Receive a byte from the IIC bus */
BYTE iic_rcvr (int ack)
{
    UINT d = 1;


    do {
        d <<= 1;
        SCL_HIGH();
        if (SDA_VAL) d++;
        iic_delay();
        SCL_LOW();
        iic_delay();
    } while (d < 0x100);
    if (ack) {      /* SDA = ACK */
        SDA_LOW();
    } else {
        SDA_HIGH();
    }
    iic_delay();
    SCL_HIGH();
    iic_delay();
    SCL_LOW();
    SDA_HIGH();
    iic_delay();

    return (BYTE)d;
}


