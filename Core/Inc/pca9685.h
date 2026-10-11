#ifndef __PCA9685_H
#define __PCA9685_H

#include "main.h"

/* PCA9685 寄存器地址 */
#define PCA9685_MODE1       0x00
#define PCA9685_PRESCALE    0xFE
#define PCA9685_LED0_ON_L   0x06

/* 默认 I2C 写地址（7位地址 0x40 左移一位） */
#define PCA9685_ADDR        0x80

/* 函数声明 */
void PCA9685_Init(I2C_HandleTypeDef *hi2c);
void PCA9685_Set_Servo_Angle(uint8_t channel, uint8_t angle);

#endif