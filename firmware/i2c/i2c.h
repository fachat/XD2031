#ifndef I2C_H_DEFINED
#define I2C_H_DEFINED

#include <stdint.h>
void i2c_init(void);
 
// Read from I2C device
    int8_t i2c_read(uint8_t dev,	// Device address
		    uint16_t adr,	// Read start address
		    uint16_t cnt,	// Read byte count
		    void *buff	// Read data buffer
    );
 
// Write to I2C device
    int8_t i2c_write(uint8_t dev,	// Device address
		     uint16_t adr,	// Write start address
		     uint16_t cnt,	// Write byte count
		     const void *buff	// Data to be written
    );
 
// Low level I2C routines
void i2c_start(void);		// Generate start condition on the I2C bus
void i2c_stop(void);		// Generate stop condition on the I2C bus
uint8_t i2c_rcvr(int ack);	// Receive a byte from the I2C bus
int8_t i2c_send(uint8_t dat);	// Send a byte to the I2C bus

#endif	/*  */
