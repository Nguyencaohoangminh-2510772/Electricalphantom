#include "i2c.h"
#include "clock.h"

#define GPIOA_ADDR      0x40020000
#define GPIOB_ADDR      0x40020400
#define I2C3_ADDR       0x40005C00

static volatile uint32_t *GPIOA_MODER   = (volatile uint32_t *)(GPIOA_ADDR + 0x00);
static volatile uint32_t *GPIOA_OTYPER  = (volatile uint32_t *)(GPIOA_ADDR + 0x04);
static volatile uint32_t *GPIOA_OSPEEDR = (volatile uint32_t *)(GPIOA_ADDR + 0x08);
static volatile uint32_t *GPIOA_PUPDR   = (volatile uint32_t *)(GPIOA_ADDR + 0x0C);
static volatile uint32_t *GPIOA_AFRH    = (volatile uint32_t *)(GPIOA_ADDR + 0x24);

static volatile uint32_t *GPIOB_MODER   = (volatile uint32_t *)(GPIOB_ADDR + 0x00);
static volatile uint32_t *GPIOB_OTYPER  = (volatile uint32_t *)(GPIOB_ADDR + 0x04);
static volatile uint32_t *GPIOB_OSPEEDR = (volatile uint32_t *)(GPIOB_ADDR + 0x08);
static volatile uint32_t *GPIOB_PUPDR   = (volatile uint32_t *)(GPIOB_ADDR + 0x0C);
static volatile uint32_t *GPIOB_AFRL    = (volatile uint32_t *)(GPIOB_ADDR + 0x20);

static volatile uint32_t *I2C3_CR1      = (volatile uint32_t *)(I2C3_ADDR + 0x00);
static volatile uint32_t *I2C3_CR2      = (volatile uint32_t *)(I2C3_ADDR + 0x04);
static volatile uint32_t *I2C3_OAR1     = (volatile uint32_t *)(I2C3_ADDR + 0x08);
static volatile uint32_t *I2C3_DR       = (volatile uint32_t *)(I2C3_ADDR + 0x10);
static volatile uint32_t *I2C3_SR1      = (volatile uint32_t *)(I2C3_ADDR + 0x14);
static volatile uint32_t *I2C3_SR2      = (volatile uint32_t *)(I2C3_ADDR + 0x18);
static volatile uint32_t *I2C3_CCR      = (volatile uint32_t *)(I2C3_ADDR + 0x1C);
static volatile uint32_t *I2C3_TRISE    = (volatile uint32_t *)(I2C3_ADDR + 0x20);

#define I2C_CR1_PE      (1 << 0)
#define I2C_CR1_START   (1 << 8)
#define I2C_CR1_STOP    (1 << 9)
#define I2C_CR1_ACK     (1 << 10)
#define I2C_CR1_POS     (1 << 11)
#define I2C_CR1_SWRST   (1 << 15)

#define I2C_SR1_SB      (1 << 0)
#define I2C_SR1_ADDR    (1 << 1)
#define I2C_SR1_BTF     (1 << 2)
#define I2C_SR1_RXNE    (1 << 6)
#define I2C_SR1_TXE     (1 << 7)
#define I2C_SR1_AF      (1 << 10)

#define I2C_TIMEOUT_COUNT   100000
#define I2C_APB1_MHZ        50
#define I2C_STANDARD_CCR    250
#define I2C_STANDARD_TRISE  51

static I2C_Status_t i2c_wait_flag_set(volatile uint32_t *reg, uint32_t flag);
static I2C_Status_t i2c_send_address(uint8_t dev_addr, uint8_t read);
static void i2c_clear_addr_flag(void);
I2C_Status_t i2c_start(void);
void i2c_stop(void);
I2C_Status_t i2c_send_byte(uint8_t data);
I2C_Status_t i2c_read_byte(uint8_t *data, uint8_t ack);
I2C_Status_t i2c_master_transmit(uint8_t dev_addr, const uint8_t *data, uint16_t len);
I2C_Status_t i2c_master_receive(uint8_t dev_addr, uint8_t *data, uint16_t len);

I2C_Status_t i2c_start(void)
{
    *I2C3_CR1 |= I2C_CR1_START;
    return i2c_wait_flag_set(I2C3_SR1, I2C_SR1_SB);
}

void i2c_stop(void)
{
    *I2C3_CR1 |= I2C_CR1_STOP;
}

static I2C_Status_t i2c_wait_flag_set(volatile uint32_t *reg, uint32_t flag)
{
    uint32_t timeout = I2C_TIMEOUT_COUNT;
    while (((*reg) & flag) == 0)
    {
        if (timeout-- == 0)
        {
            return I2C_TIMEOUT;
        }
    }
    return I2C_OK;
}

static void i2c_clear_addr_flag(void)
{
    volatile uint32_t temp;
    temp = *I2C3_SR1;
    temp = *I2C3_SR2;
    (void)temp;
}

static I2C_Status_t i2c_send_address(uint8_t dev_addr, uint8_t read)
{
    uint32_t timeout = I2C_TIMEOUT_COUNT;
    *I2C3_DR = (uint32_t)((dev_addr << 1) | (read & 0x01));
    while ((*I2C3_SR1 & I2C_SR1_ADDR) == 0)
    {
        if ((*I2C3_SR1 & I2C_SR1_AF) != 0)
        {
            *I2C3_SR1 &= ~I2C_SR1_AF;
            i2c_stop();
            return I2C_ERROR;
        }

        if (timeout-- == 0)
        {
            i2c_stop();
            return I2C_TIMEOUT;
        }
    }
    return I2C_OK;
}

void i2c_init(void)
{
    clock_enable_AHB1(GPIOA_peripheral);
    clock_enable_AHB1(GPIOB_peripheral);
    clock_enable_APB1(I2C3_peripheral);

    // PA8 = I2C3_SCL (AF4), PB4 = I2C3_SDA (AF9), open-drain, pull-up.
    *GPIOA_MODER &= ~(0b11 << 16);
    *GPIOA_MODER |=  (0b10 << 16);
    *GPIOA_OTYPER |= (1 << 8);
    *GPIOA_OSPEEDR &= ~(0b11 << 16);
    *GPIOA_OSPEEDR |=  (0b10 << 16);
    *GPIOA_PUPDR &= ~(0b11 << 16);
    *GPIOA_PUPDR |=  (0b01 << 16);
    *GPIOA_AFRH &= ~(0xF << 0);
    *GPIOA_AFRH |=  (4 << 0);

    *GPIOB_MODER &= ~(0b11 << 8);
    *GPIOB_MODER |=  (0b10 << 8);
    *GPIOB_OTYPER |= (1 << 4);
    *GPIOB_OSPEEDR &= ~(0b11 << 8);
    *GPIOB_OSPEEDR |=  (0b10 << 8);
    *GPIOB_PUPDR &= ~(0b11 << 8);
    *GPIOB_PUPDR |=  (0b01 << 8);
    *GPIOB_AFRL &= ~(0xF << 16);
    *GPIOB_AFRL |=  (9 << 16);

    *I2C3_CR1 |= I2C_CR1_SWRST;
    *I2C3_CR1 &= ~I2C_CR1_SWRST;

    *I2C3_CR1 &= ~I2C_CR1_PE;
    *I2C3_CR2 = I2C_APB1_MHZ; // 50
    *I2C3_CCR = I2C_STANDARD_CCR; // 250
    *I2C3_TRISE = I2C_STANDARD_TRISE; // 51
    *I2C3_OAR1 = (1 << 14); // Should always be kept at 1 by software
    *I2C3_CR1 |= I2C_CR1_PE;
}

I2C_Status_t i2c_send_byte(uint8_t data)
{
    if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_TXE) != I2C_OK)
    {
        return I2C_TIMEOUT;
    }

    *I2C3_DR = data;

    if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_TXE) != I2C_OK)
    {
        return I2C_TIMEOUT;
    }

    if ((*I2C3_SR1 & I2C_SR1_AF) != 0)
    {
        *I2C3_SR1 &= ~I2C_SR1_AF;
        return I2C_ERROR;
    }

    return I2C_OK;
}

I2C_Status_t i2c_read_byte(uint8_t *data, uint8_t ack)
{
    if (data == 0)
    {
        return I2C_ERROR;
    }

    if (ack != 0)
    {
        *I2C3_CR1 |= I2C_CR1_ACK;
    }
    else
    {
        *I2C3_CR1 &= ~I2C_CR1_ACK;
    }

    if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_RXNE) != I2C_OK)
    {
        return I2C_TIMEOUT;
    }

    *data = (uint8_t)*I2C3_DR;
    return I2C_OK;
}

I2C_Status_t i2c_master_transmit(uint8_t dev_addr, const uint8_t *data, uint16_t len)
{
    if ((data == 0) && (len > 0))
    {
        return I2C_ERROR;
    }

    if (i2c_start() != I2C_OK)
    {
        return I2C_TIMEOUT;
    }

    if (i2c_send_address(dev_addr, 0) != I2C_OK)
    {
        return I2C_ERROR;
    }
    i2c_clear_addr_flag();

    for (uint16_t i = 0; i < len; i++)
    {
        if (i2c_send_byte(data[i]) != I2C_OK)
        {
            i2c_stop();
            return I2C_ERROR;
        }
    }

    if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_BTF) != I2C_OK)
    {
        i2c_stop();
        return I2C_TIMEOUT;
    }

    i2c_stop();
    return I2C_OK;
}

I2C_Status_t i2c_master_receive(uint8_t dev_addr, uint8_t *data, uint16_t len)
{
    if ((data == 0) || (len == 0))
    {
        return I2C_ERROR;
    }

    if (i2c_start() != I2C_OK)
    {
        return I2C_TIMEOUT;
    }

    if (len == 1)
    {
        *I2C3_CR1 &= ~I2C_CR1_POS;
        *I2C3_CR1 &= ~I2C_CR1_ACK;
    }
    else if (len == 2)
    {
        *I2C3_CR1 |= I2C_CR1_ACK;
        *I2C3_CR1 |= I2C_CR1_POS;
    }
    else
    {
        *I2C3_CR1 &= ~I2C_CR1_POS;
        *I2C3_CR1 |= I2C_CR1_ACK;
    }

    if (i2c_send_address(dev_addr, 1) != I2C_OK)
    {
        return I2C_ERROR;
    }

    i2c_clear_addr_flag();

    if (len == 1)
    {
        i2c_stop();

        if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_RXNE) != I2C_OK)
        {
            return I2C_TIMEOUT;
        }

        data[0] = (uint8_t)*I2C3_DR;
    }
    else if (len == 2)
    {
        *I2C3_CR1 &= ~I2C_CR1_ACK;

        if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_BTF) != I2C_OK)
        {
            return I2C_TIMEOUT;
        }

        i2c_stop();
        data[0] = (uint8_t)*I2C3_DR;
        data[1] = (uint8_t)*I2C3_DR;
    }
    else
    {
        for (uint16_t i = 0; i < len; i++)
        {
            if (i == (uint16_t)(len - 3))
            {
                if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_BTF) != I2C_OK)
                {
                    return I2C_TIMEOUT;
                }

                *I2C3_CR1 &= ~I2C_CR1_ACK;
                data[i++] = (uint8_t)*I2C3_DR;

                if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_BTF) != I2C_OK)
                {
                    return I2C_TIMEOUT;
                }

                i2c_stop();
                data[i++] = (uint8_t)*I2C3_DR;
                data[i] = (uint8_t)*I2C3_DR;
                break;
            }

            if (i2c_wait_flag_set(I2C3_SR1, I2C_SR1_RXNE) != I2C_OK)
            {
                return I2C_TIMEOUT;
            }

            data[i] = (uint8_t)*I2C3_DR;
        }
    }

    *I2C3_CR1 &= ~I2C_CR1_POS;
    *I2C3_CR1 |= I2C_CR1_ACK;
    return I2C_OK;
}