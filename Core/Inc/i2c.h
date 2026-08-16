#ifndef INC_I2C_H_
#define INC_I2C_H_

#include <stdint.h>

typedef enum
{
    I2C_OK = 0,
    I2C_ERROR,
    I2C_TIMEOUT
} I2C_Status_t;

void i2c_init(void);
I2C_Status_t i2c_send_byte(uint8_t data);
I2C_Status_t i2c_read_byte(uint8_t *data, uint8_t ack);
I2C_Status_t i2c_master_transmit(uint8_t dev_addr, const uint8_t *data, uint16_t len);
I2C_Status_t i2c_master_receive(uint8_t dev_addr, uint8_t *data, uint16_t len);

#endif /* INC_I2C_H_ */