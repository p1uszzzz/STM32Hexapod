#include "pca9685.h"

static I2C_HandleTypeDef *pca_i2c;

/* 向 PCA9685 寄存器写一个字节 */
static void PCA9685_Write(uint8_t reg, uint8_t data)
{
    HAL_I2C_Mem_Write(pca_i2c, PCA9685_ADDR, reg, 1, &data, 1, 100);
}

/* 初始化：设频率 50Hz，退出睡眠 */
void PCA9685_Init(I2C_HandleTypeDef *hi2c)
{
    pca_i2c = hi2c;

    PCA9685_Write(PCA9685_MODE1, 0x10);      // 进入睡眠
    PCA9685_Write(PCA9685_PRESCALE, 121);    // 50Hz
    PCA9685_Write(PCA9685_MODE1, 0x20);      // 退出睡眠，自动递增

    HAL_Delay(1);
}

/* 设置某通道舵机角度：0~180 */
void PCA9685_Set_Servo_Angle(uint8_t channel, uint8_t angle)
{
    if (angle > 180) angle = 180;

    /* 0.5ms(0°) ≈ 102，2.5ms(180°) ≈ 512 */
    uint16_t off = 102 + (angle * (512 - 102) / 180);

    /* 每个通道占 4 个寄存器 */
    uint8_t reg = PCA9685_LED0_ON_L + 4 * channel;

    PCA9685_Write(reg,     0);              // ON_L
    PCA9685_Write(reg + 1, 0);              // ON_H
    PCA9685_Write(reg + 2, off & 0xFF);     // OFF_L
    PCA9685_Write(reg + 3, off >> 8);       // OFF_H
}