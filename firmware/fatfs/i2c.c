/*-------------------------------------------------*/
/* I2C block read/write controls                   */

#include <avr/io.h>
#include <string.h>
#include "i2c.h"
#include "device.h"

int iic_read (
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
        iic_start();
    } while (!iic_send(dev) && --n);
    if (n) {
        if (iic_send((BYTE)adr)) {      /* Set start address */
            iic_start();                /* Reselect device in read mode */
            if (iic_send(dev | 1)) {
                do {                    /* Receive data */
                    cnt--;
                    *rbuff++ = iic_rcvr(cnt ? 1 : 0);
                } while (cnt);
            }
        }
    }

    iic_stop();                         /* Deselect device */

    return cnt ? 0 : 1;
}



int iic_write (
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
        iic_start();
    } while (!iic_send(dev) && --n);
    if (n) {
        if (iic_send((BYTE)adr)) {      /* Set start address */
            do {                        /* Send data */
                if (!iic_send(*wbuff++)) break;
            } while (--cnt);
        }
    }

    iic_stop();                         /* Deselect device */

    return cnt ? 0 : 1;
}

