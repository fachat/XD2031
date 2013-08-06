// I2C bus protocol - bit banging version

#include "delayhw.h"
// delay.h does not work with avr-gcc 4.7.2 / avr-libc 1.8.0 because
// the double wrapped function parameter is not recognized as integer
// constant. Couldn't figure out, why this works in iec.c and doesn't here.

#include "config.h"	// I2C speed
#include "hwdefines.h"	// I2C ports

// longest delay in NXP's UM10204 I2C-bus specification
// and user manual (Rev. 5--9 October 2012) is 4.7uS
#ifndef I2C_DELAY_US
#define I2C_DELAY_US 5
#endif

// Get status of SDA line
static inline uint8_t i2c_sda(void) {
    return (INPUT_SDA & _BV(PIN_SDA));
}

// Get status of SCL line
static inline uint8_t i2c_scl(void) {
    return (INPUT_SCL & _BV(PIN_SCL));
}

// External pull-ups are required, internal pull-ups
// are deactivated here

static inline void i2c_set_sda(uint8_t x) {
    if(x) {
        // define as input, external pull-up will pull high
        DDR_SDA &= ~_BV(PIN_SDA);
    } else {
        DDR_SDA |= _BV(PIN_SDA); // define as output, output 0
    }
    delayhw_us(I2C_DELAY_US);
}

static inline void i2c_set_scl(uint8_t x) {
    if(x) {
        // define as input, external pull-up will pull high
        DDR_SCL &= ~_BV(PIN_SCL);
        delayhw_us(I2C_DELAY_US);
        while(!i2c_scl());       // Clock stretching
    } else {
        DDR_SCL |= _BV(PIN_SCL); // define as output, output 0
        delayhw_us(I2C_DELAY_US);
    }
}

// Generate start condition on the I2C bus
void i2c_start (void) {
    i2c_set_sda(1);
    i2c_set_scl(1);
    i2c_set_sda(0);
    i2c_set_scl(0);
}


// Generate stop condition on the I2C bus
void i2c_stop (void) {
    i2c_set_sda(0);
    i2c_set_scl(1);
    i2c_set_sda(1);
}


// Send a byte to the I2C bus
int i2c_send (uint8_t dat) {
    uint8_t b = 0x80;
    int ack;

    do {
        if (dat & b)     {  	// SDA = Z/L
            i2c_set_sda(1);
        } else {
            i2c_set_sda(0);
        }
        i2c_set_scl(1);
        i2c_set_scl(0);
    } while (b >>= 1);
    i2c_set_sda(1);
    i2c_set_scl(1);
    ack = i2c_sda() ? 0 : 1;	// Sample ACK
    i2c_set_scl(0);
    return ack;
}


// Receive a byte from the I2C bus
uint8_t i2c_rcvr (int ack) {
    uint16_t d = 1;

    do {
        d <<= 1;
        i2c_set_scl(1);
        if (i2c_sda()) d++;
        i2c_set_scl(0);
    } while (d < 0x100);
    if (ack) {     		// SDA = ACK
        i2c_set_sda(0);
    } else {
        i2c_set_sda(1);
    }
    i2c_set_scl(1);
    i2c_set_scl(0);
    i2c_set_sda(1);

    return (uint8_t)d;
}
