#include "mpu6050.h"
#include "delay.h"
#include "i2c.h"

static uint8_t mpu_i2c_addr = MPU_I2C_ADDR ; 
static float mpu_accel_scale = 16384.0f;
static float mpu_gyro_scale = 131.0f;

static MPU_STATUS MPU_WRITEREG( uint8_t reg, uint8_t data);
static MPU_STATUS MPU_READREG(uint8_t reg, uint8_t *data);
static MPU_STATUS MPU_READREGS(uint8_t reg, uint8_t *data, uint16_t len);
static MPU_STATUS MPU_CHECK_ID(void);
static MPU_STATUS MPU_SetAccelRange(MPU_AccelRange range);
static MPU_STATUS MPU_SetGyroRange(MPU_GyroRange range);


static MPU_STATUS MPU_WRITEREG( uint8_t reg, uint8_t data)
{
    uint8_t buffer[2];
    buffer[0]= reg ;
    buffer[1]= data ;
    if (i2c_master_transmit(mpu_i2c_addr, buffer, 2) != I2C_OK)
    {
        return MPU_ERROR;
    }
    return MPU_OK;
}

static MPU_STATUS MPU_READREG(uint8_t reg, uint8_t *data)
{
    if (data == 0)
    {
        return MPU_ERROR;
    }

    if (i2c_master_transmit(mpu_i2c_addr, &reg, 1) != I2C_OK)
    {
        return MPU_ERROR;
    }

    if (i2c_master_receive(mpu_i2c_addr, data, 1) != I2C_OK)
    {
        return MPU_ERROR;
    }

    return MPU_OK;
}

static MPU_STATUS MPU_READREGS(uint8_t reg, uint8_t *data, uint16_t len)
{
    if ((data == 0) || (len == 0))
    {
        return MPU_ERROR;
    }

    if (i2c_master_transmit(mpu_i2c_addr, &reg, 1) != I2C_OK)
    {
        return MPU_ERROR;
    }

    if (i2c_master_receive(mpu_i2c_addr, data, len) != I2C_OK)
    {
        return MPU_ERROR;
    }

    return MPU_OK;
}

static MPU_STATUS MPU_CHECK_ID(void) 
{
    uint8_t id;

    if (MPU_READREG(MPU_WHO_AM_I, &id) != MPU_OK)
    {
        return MPU_ERROR;
    }

    if (id != MPU_WHO_AM_I_VALUE)
    {
        return MPU_ERROR;
    }

    return MPU_OK;
}

static MPU_STATUS MPU_SetAccelRange(MPU_AccelRange range)
{
    if (range > MPU_ACCEL_16G)
    {
        return MPU_ERROR;
    }

    if (MPU_WRITEREG(MPU_ACCEL_CONFIG, (uint8_t)(range << 3)) != MPU_OK) // set accel range
    {
        return MPU_ERROR;
    }

    switch (range)
    {
        case MPU_ACCEL_2G:
            mpu_accel_scale = 16384.0f;
            break;
        case MPU_ACCEL_4G:
            mpu_accel_scale = 8192.0f;
            break;
        case MPU_ACCEL_8G:
            mpu_accel_scale = 4096.0f;
            break;
        case MPU_ACCEL_16G:
            mpu_accel_scale = 2048.0f;
            break;
        default:
            return MPU_ERROR;
    }

    return MPU_OK;
}

static MPU_STATUS MPU_SetGyroRange(MPU_GyroRange range)
{
    if (range > MPU_GYRO_2000DPS)
    {
        return MPU_ERROR;
    }

    if (MPU_WRITEREG(MPU_GYRO_CONFIG, (uint8_t)(range << 3)) != MPU_OK) // set gyro range
    {
        return MPU_ERROR;
    }

    switch (range)
    {
        case MPU_GYRO_250DPS:
            mpu_gyro_scale = 131.0f;
            break;
        case MPU_GYRO_500DPS:
            mpu_gyro_scale = 65.5f;
            break;
        case MPU_GYRO_1000DPS:
            mpu_gyro_scale = 32.8f;
            break;
        case MPU_GYRO_2000DPS:
            mpu_gyro_scale = 16.4f;
            break;
        default:
            return MPU_ERROR;
    }

    return MPU_OK;
}

MPU_STATUS MPU_Init(void)
{
    delay_init();
    i2c_init();

    if (MPU_CHECK_ID() != MPU_OK)
    {
        return MPU_ERROR;
    }

    if (MPU_WRITEREG(MPU_PWR_MGMT_1, MPU_DEVICE_RESET) != MPU_OK) // reset sensor
    {
        return MPU_ERROR;
    }

    delay_ms(100);

    if (MPU_WRITEREG(MPU_PWR_MGMT_1, MPU_CLK_PLL_XGYRO) != MPU_OK) // xgyro lam nguon tham chieu pll
    {
        return MPU_ERROR;
    }

    if (MPU_WRITEREG(MPU_SMPLRT_DIV, 0x07) != MPU_OK) // sample rate = Gyroscope Output Rate / (1 + SMPLRT_DIV) = 1000 / (1+ 7) = 125hz
    {
        return MPU_ERROR;
    }

    if (MPU_WRITEREG(MPU_CONFIG, 0x03) != MPU_OK) // Gyroscope Output Rate =1khz
    {
        return MPU_ERROR;
    }

    if (MPU_SetAccelRange(MPU_ACCEL_4G) != MPU_OK) // accel range
    {
        return MPU_ERROR;
    }

    if (MPU_SetGyroRange(MPU_GYRO_500DPS) != MPU_OK) // gyro range
    {
        return MPU_ERROR;
    }

    return MPU_OK;
}

MPU_STATUS MPU_ReadData(MPU6050_Data *data)
{
    uint8_t buffer[14];
    int16_t temp_raw;
    MPU_RawData accel_raw;
    MPU_RawData gyro_raw;

    if (data == 0)
    {
        return MPU_ERROR;
    }

    if (MPU_READREGS(MPU_ACCEL_XOUT_H, buffer, 14) != MPU_OK)
    {
        return MPU_ERROR;
    }

    accel_raw.x = (int16_t)((buffer[0] << 8) | buffer[1]);
    accel_raw.y = (int16_t)((buffer[2] << 8) | buffer[3]);
    accel_raw.z = (int16_t)((buffer[4] << 8) | buffer[5]);
    temp_raw = (int16_t)((buffer[6] << 8) | buffer[7]);
    gyro_raw.x = (int16_t)((buffer[8] << 8) | buffer[9]);
    gyro_raw.y = (int16_t)((buffer[10] << 8) | buffer[11]);
    gyro_raw.z = (int16_t)((buffer[12] << 8) | buffer[13]);

    data->accel_g.x = (float)accel_raw.x / mpu_accel_scale;
    data->accel_g.y = (float)accel_raw.y / mpu_accel_scale;
    data->accel_g.z = (float)accel_raw.z / mpu_accel_scale;

    data->gyro_dps.x = (float)gyro_raw.x / mpu_gyro_scale;
    data->gyro_dps.y = (float)gyro_raw.y / mpu_gyro_scale;
    data->gyro_dps.z = (float)gyro_raw.z / mpu_gyro_scale;

    data->temp_c = ((float)temp_raw / 340.0f) + 36.53f;

    return MPU_OK;
}
