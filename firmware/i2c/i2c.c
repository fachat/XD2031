// I2C block read/write controls

#include "i2c.h"

int8_t i2c_read (
    uint8_t dev,        // Device address
    uint16_t adr,       // Read start address
    uint16_t cnt,       // Read byte count
    void* buff          // Read data buffer
)

{
    uint8_t *rbuff = buff;
    int n;


    if (!cnt) return 0;

    n = 10;
    do {                                // Select device
        i2c_start();
    } while (!i2c_send(dev) && --n);
    if (n) {
        if (i2c_send((uint8_t)adr)) {   // Set start address
            i2c_start();                // Reselect device in read mode
            if (i2c_send(dev | 1)) {
                do {                    // Receive data
                    cnt--;
                    *rbuff++ = i2c_rcvr(cnt ? 1 : 0);
                } while (cnt);
            }
        }
    }

    i2c_stop();                         // Deselect device

    return cnt;
}



int8_t i2c_write (
    uint8_t dev,        // Device address
    uint16_t adr,       // Write start address
    uint16_t cnt,       // Write byte count
    const void* buff    // Data to be written
)
{
    const uint8_t *wbuff = buff;
    int n;


    if (!cnt) return 0;

    n = 10;
    do {                                // Select device
        i2c_start();
    } while (!i2c_send(dev) && --n);
    if (n) {
        if (i2c_send((uint8_t)adr)) {   // Set start address
            do {                        // Send data
                if (!i2c_send(*wbuff++)) break;
            } while (--cnt);
        }
    }

    i2c_stop();                         // Deselect device

    return cnt;
}

