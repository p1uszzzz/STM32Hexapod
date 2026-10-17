#include "pca9685.h"

static I2C_HandleTypeDef *pca_i2c_1;
static I2C_HandleTypeDef *pca_i2c_2;

/* 根据地址选择 I2C 句柄，再写寄存器 */
static void PCA9685_Write(uint8_t addr, uint8_t reg, uint8_t data)
{
    I2C_HandleTypeDef *hi2c = (addr == PCA9685_ADDR_1) ? pca_i2c_1 : pca_i2c_2;
    HAL_I2C_Mem_Write(hi2c, addr, reg, 1, &data, 1, 100);
}

/* 初始化单块 PCA9685 */
static void PCA9685_Init_One(uint8_t addr)
{
    PCA9685_Write(addr, PCA9685_MODE1, 0x10);      // 进入睡眠
    PCA9685_Write(addr, PCA9685_PRESCALE, 121);    // 50Hz
    PCA9685_Write(addr, PCA9685_MODE1, 0x20);      // 退出睡眠，自动递增
    HAL_Delay(1);
}

/* 初始化两块 PCA9685 */
void PCA9685_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2)
{
    pca_i2c_1 = hi2c1;
    pca_i2c_2 = hi2c2;
    PCA9685_Init_One(PCA9685_ADDR_1);
    PCA9685_Init_One(PCA9685_ADDR_2);
}

/* 设置舵机角度：channel 0~17，angle 0~180 */
void PCA9685_Set_Servo_Angle(uint8_t channel, uint8_t angle)
{
    if (angle > 180) angle = 180;

    uint8_t addr;
    uint8_t ch;
    if (channel < 9) {
        addr = PCA9685_ADDR_1;
        ch = channel;
    } else {
        addr = PCA9685_ADDR_2;
        ch = channel - 9;       //将channel 9~17 映射为 0~8     
    }

    uint16_t off = 102 + (angle * (512 - 102) / 180);
    uint8_t reg = PCA9685_LED0_ON_L + 4 * ch;

    PCA9685_Write(addr, reg,     0);
    PCA9685_Write(addr, reg + 1, 0);
    PCA9685_Write(addr, reg + 2, off & 0xFF);
    PCA9685_Write(addr, reg + 3, off >> 8);
}