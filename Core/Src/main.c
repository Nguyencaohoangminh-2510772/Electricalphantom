/* USER CODE BEGIN Header */
/**
  * Dự án Cánh vây tàu - Điều khiển Motor DC phản hồi góc nghiêng
  * STM32F411CEU6 (Tận dụng FPU cho Mahony Filter)
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "arm_math.h"  // CMSIS-DSP hỗ trợ tính toán FPU siêu tốc
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define MPU6050_ADDR (0x68 << 1) // Địa chỉ I2C của MPU6050 dịch trái 1 bit

// --- Hằng số PID ---
#define PID_KP 5.0f
#define PID_KI 0.2f
#define PID_KD 1.5f
#define PWM_MAX 999  
#define PWM_MIN -999

#define PI_F 3.14159265f
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// --- Biến thuật toán Mahony ---
float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
float twoKp = 1.0f; // 2 * Kp
float twoKi = 0.0f; // 2 * Ki
float integralFBx = 0.0f, integralFBy = 0.0f, integralFBz = 0.0f;
float roll = 0.0f, pitch = 0.0f;

// --- Biến đọc cảm biến MPU6050 ---
float ax = 0, ay = 0, az = 0;
float gx = 0, gy = 0, gz = 0;

// --- Biến điều khiển PID ---
float setpoint_roll = 0.0f; 
float integral_err = 0.0f;
int16_t motor_pwm_out = 0;

// --- Biến thời gian ---
uint32_t last_time = 0;
uint32_t display_time = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */
void MPU6050_Init(void);
void MPU6050_Read(void);
void MahonyAHRSupdateIMU(float gx_rad, float gy_rad, float gz_rad, float ax_g, float ay_g, float az_g, float dt);
float PID_Compute(float setpoint, float current_val, float rate_val, float dt);
void Motor_Drive(int16_t speed);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// --- Retarget printf sang UART1 để debug ---
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  // 1. Khởi động Timer 2 cấp xung PWM
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  
  // 2. Cấu hình MPU6050
  MPU6050_Init();
  
  // 3. Khởi tạo mốc thời gian
  last_time = HAL_GetTick();
  display_time = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN 3 */
  while (1)
  {
      // ========================================================
      // LUỒNG ĐIỀU KHIỂN THỜI GIAN THỰC (Chu kỳ 10ms ~ 100Hz)
      // ========================================================
      if (HAL_GetTick() - last_time >= 10) {
          float dt = (HAL_GetTick() - last_time) / 1000.0f;
          last_time = HAL_GetTick();
          
          // 1. Đọc data nguyên bản từ MPU6050
          MPU6050_Read();
          
          // 2. Chạy bộ lọc Mahony (Sensor Fusion)
          // Đổi Gyro sang Rad/s để đúng chuẩn tính toán Quaternion
          float gx_rad = gx * PI_F / 180.0f;
          float gy_rad = gy * PI_F / 180.0f;
          float gz_rad = gz * PI_F / 180.0f;
          
          MahonyAHRSupdateIMU(gx_rad, gy_rad, gz_rad, ax, ay, az, dt);
          
          // 3. Tính toán PID chống trễ
          // Truyền thẳng Gx (deg/s) vào làm khâu D để dập dao động lập tức
          motor_pwm_out = (int16_t)PID_Compute(setpoint_roll, roll, gx, dt);
          
          // 4. Xuất PWM chạy Motor
          Motor_Drive(motor_pwm_out);
      }
      
      // ========================================================
      // LUỒNG HIỂN THỊ (Chu kỳ 500ms) - Chống nghẽn I2C/UART
      // ========================================================
      if (HAL_GetTick() - display_time >= 500) {
          display_time = HAL_GetTick();
          printf("Roll: %5.1f | Pitch: %5.1f | PWM: %4d\r\n", roll, pitch, motor_pwm_out);
          
          // Nếu có dùng thư viện LCD, gọi lcd_send_string() ở đây
      }
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

// =========================================================
// 1. GIAO TIẾP MPU6050 (Không cần thư viện ngoài)
// =========================================================
void MPU6050_Init(void) {
    uint8_t data = 0;
    // Đánh thức MPU6050
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x6B, 1, &data, 1, 100);
    // Cấu hình Gyro (±250 deg/s)
    data = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x1B, 1, &data, 1, 100);
    // Cấu hình Accel (±2g)
    data = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x1C, 1, &data, 1, 100);
}

void MPU6050_Read(void) {
    uint8_t buffer[14];
    int16_t raw_ax, raw_ay, raw_az, raw_gx, raw_gy, raw_gz;
    
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, 0x3B, 1, buffer, 14, 100);
    
    raw_ax = (int16_t)(buffer[0] << 8 | buffer[1]);
    raw_ay = (int16_t)(buffer[2] << 8 | buffer[3]);
    raw_az = (int16_t)(buffer[4] << 8 | buffer[5]);
    raw_gx = (int16_t)(buffer[8] << 8 | buffer[9]);
    raw_gy = (int16_t)(buffer[10] << 8 | buffer[11]);
    raw_gz = (int16_t)(buffer[12] << 8 | buffer[13]);
    
    // Scale dữ liệu
    ax = raw_ax / 16384.0f;
    ay = raw_ay / 16384.0f;
    az = raw_az / 16384.0f;
    gx = raw_gx / 131.0f;
    gy = raw_gy / 131.0f;
    gz = raw_gz / 131.0f;
}

// =========================================================
// 2. THUẬT TOÁN BỘ LỌC MAHONY (Sensor Fusion bằng FPU)
// =========================================================
void MahonyAHRSupdateIMU(float gx_rad, float gy_rad, float gz_rad, float ax_g, float ay_g, float az_g, float dt) {
    float recipNorm;
    float halfvx, halfvy, halfvz;
    float halfex, halfey, halfez;
    float qa, qb, qc;

    if(!((ax_g == 0.0f) && (ay_g == 0.0f) && (az_g == 0.0f))) {
        // arm_sqrt_f32 chạy trên FPU rất nhanh
        arm_sqrt_f32(ax_g * ax_g + ay_g * ay_g + az_g * az_g, &recipNorm);
        recipNorm = 1.0f / recipNorm;
        ax_g *= recipNorm; ay_g *= recipNorm; az_g *= recipNorm;

        halfvx = q1 * q3 - q0 * q2;
        halfvy = q0 * q1 + q2 * q3;
        halfvz = q0 * q0 - 0.5f + q3 * q3;

        halfex = (ay_g * halfvz - az_g * halfvy);
        halfey = (az_g * halfvx - ax_g * halfvz);
        halfez = (ax_g * halfvy - ay_g * halfvx);

        if(twoKi > 0.0f) {
            integralFBx += twoKi * halfex * dt;
            integralFBy += twoKi * halfey * dt;
            integralFBz += twoKi * halfez * dt;
            gx_rad += integralFBx; gy_rad += integralFBy; gz_rad += integralFBz;
        }

        gx_rad += twoKp * halfex;
        gy_rad += twoKp * halfey;
        gz_rad += twoKp * halfez;
    }

    gx_rad *= (0.5f * dt); gy_rad *= (0.5f * dt); gz_rad *= (0.5f * dt);
    qa = q0; qb = q1; qc = q2;
    q0 += (-qb * gx_rad - qc * gy_rad - q3 * gz_rad);
    q1 += (qa * gx_rad + qc * gz_rad - q3 * gy_rad);
    q2 += (qa * gy_rad - qb * gz_rad + q3 * gx_rad);
    q3 += (qa * gz_rad + qb * gy_rad - qc * gx_rad);

    arm_sqrt_f32(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3, &recipNorm);
    recipNorm = 1.0f / recipNorm;
    q0 *= recipNorm; q1 *= recipNorm; q2 *= recipNorm; q3 *= recipNorm;
    
    // Tính góc Euler (Roll, Pitch)
    roll  = atan2f(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.29578f;
    pitch = asinf(-2.0f * (q1*q3 - q0*q2)) * 57.29578f;
}

// =========================================================
// 3. THUẬT TOÁN ĐIỀU KHIỂN PID CÁNH VÂY
// =========================================================
float PID_Compute(float setpoint, float current_val, float rate_val, float dt) {
    float error = setpoint - current_val;
    
    // Khâu I có Anti-windup chặn bão hòa ở mức 200
    integral_err += error * dt;
    if (integral_err > 200.0f) integral_err = 200.0f;
    if (integral_err < -200.0f) integral_err = -200.0f;
    
    // Khâu D lấy thẳng từ Gyro rate (làm mịn, không bị nảy số)
    float derivative = -rate_val; 
    
    float output = (PID_KP * error) + (PID_KI * integral_err) + (PID_KD * derivative);
    
    // Giới hạn trong tầm PWM
    if (output > PWM_MAX) output = PWM_MAX;
    if (output < PWM_MIN) output = PWM_MIN;
    
    return output;
}

// =========================================================
// 4. LỆNH ĐIỀU KHIỂN MOTOR (MẠCH CẦU H L298N/TB6612)
// =========================================================
void Motor_Drive(int16_t speed) {
    // Sửa GPIO_PIN_1, GPIO_PIN_2 theo thực tế nối dây
    if (speed > 0) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    } 
    else if (speed < 0) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, -speed);
    } 
    else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    }
}

/* USER CODE END 4 */
