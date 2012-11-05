/*-------------------------------------------------*/
/* I2C block read/write controls                   */

#include <avr/io.h>
#include <string.h>
#include "i2c.h"
#include "device.h"

int8_t i2c_read (
    BYTE dev,           /* Device address */
    UINT adr,           /* Read start address */
    UINT cnt,           /* Read byte count */
    void* buff          /* Read data buffer */
)

{
    BYTE *rbuff = buff;
    int n;


    if (!cnt) return 0;

    n = 10;
    do {                                /* Select device */
        i2c_start();
    } while (!i2c_send(dev) && --n);
    if (n) {
        if (i2c_send((BYTE)adr)) {      /* Set start address */
            i2c_start();                /* Reselect device in read mode */
            if (i2c_send(dev | 1)) {
                do {                    /* Receive data */
                    cnt--;
                    *rbuff++ = i2c_rcvr(cnt ? 1 : 0);
                } while (cnt);
            }
        }
    }

    i2c_stop();                         /* Deselect device */

    return cnt ? 0 : 1;
}



int8_t i2c_write (
    BYTE dev,           /* Device address */
    UINT adr,           /* Write start address */
    UINT cnt,           /* Write byte count */
    const void* buff    /* Data to be written */
)
{
    const BYTE *wbuff = buff;
    int n;


    if (!cnt) return 0;

    n = 10;
    do {                                /* Select device */
        i2c_start();
    } while (!i2c_send(dev) && --n);
    if (n) {
        if (i2c_send((BYTE)adr)) {      /* Set start address */
            do {                        /* Send data */
                if (!i2c_send(*wbuff++)) break;
            } while (--cnt);
        }
    }

    i2c_stop();                         /* Deselect device */

    return cnt ? 0 : 1;
}

