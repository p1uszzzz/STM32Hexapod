#ifndef __PCA9685_H
#define __PCA9685_H

#include "main.h"

/* PCA9685 寄存器地址 */
#define PCA9685_MODE1       0x00
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06

/* 两块 PCA9685 的 I2C 写地址（7位地址左移一位） */
#define PCA9685_ADDR_1      0x80    // 第一块，A0 未短接，7位地址 0x40
#define PCA9685_ADDR_2      0xC0    // 第二块，A5 短接，7位地址 0x60

/* 函数声明 */
void PCA9685_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2);
void PCA9685_Set_Servo_Angle(uint8_t channel, uint8_t angle);

#endif