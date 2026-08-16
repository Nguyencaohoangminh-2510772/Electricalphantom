#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include <stdint.h>

#define MPU_I2C_ADDR 0x68 // AD0 = 0

#define MPU_SMPLRT_DIV 0x19 // Chia tan so lay mau
#define MPU_CONFIG 0x1A // Low pass filter
#define MPU_GYRO_CONFIG 0x1B // Chon thang do gyro
#define MPU_ACCEL_CONFIG 0x1C // Chon thang do accel

#define MPU_ACCEL_XOUT_H 0x3B // Thanh ghi data dau tien
#define MPU_TEMP_OUT_H 0x41
#define MPU_GYRO_XOUT_H 0x43

#define MPU_PWR_MGMT_1 0x6B // Cau hinh nguon
#define MPU_WHO_AM_I 0x75 // ID cam bien

#define MPU_DEVICE_RESET (1 << 7) // Reset sensor
#define MPU_SLEEP (1 << 6)
#define MPU_CLK_PLL_XGYRO 0x01
#define MPU_WHO_AM_I_VALUE 0x68

typedef enum
{
    MPU_OK = 0,
    MPU_ERROR,
    MPU_TIMEOUT
} MPU_STATUS;

typedef enum
{
    MPU_ACCEL_2G = 0,
    MPU_ACCEL_4G,
    MPU_ACCEL_8G,
    MPU_ACCEL_16G
} MPU_AccelRange;

typedef enum
{
    MPU_GYRO_250DPS = 0,
    MPU_GYRO_500DPS,
    MPU_GYRO_1000DPS,
    MPU_GYRO_2000DPS
} MPU_GyroRange;

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} MPU_RawData;

typedef struct
{
    float x;
    float y;
    float z;
} MPU_Data;

typedef struct
{
    MPU_Data accel_g;
    MPU_Data gyro_dps;
    float temp_c;
} MPU6050_Data;

MPU_STATUS MPU_Init(void);
MPU_STATUS MPU_ReadData(MPU6050_Data *data);

#endif /* INC_MPU6050_H_ */
