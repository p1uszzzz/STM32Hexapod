#include "gait.h"
#include "leg_ik.h"
#include "pca9685.h"
#include "main.h"      /* 用到 HAL_Delay */
#include <math.h>

/* 若编译器报 M_PI 未定义，取消下面这行注释 */
/* #define M_PI 3.14159265358979323846f */

/* ========================================================================
 * 6 条腿的 home 位置（相对身体中心，单位 mm）
 * ------------------------------------------------------------------------
 * 坐标系：x 向前，y 向左，z 向上（足端在下方，所以 z 为负）
 * 必须按你的 3D 打印件实测修改，左右对称很重要。
 * 腿编号：1=前左, 2=中左, 3=后左, 4=前右, 5=中右, 6=后右
 * ======================================================================== */
static const FootPos LEG_HOME[LEG_NUM] = {
    {-60.0f,  50.0f, -80.0f},  /* 1 前左 */
    {-60.0f,   0.0f, -80.0f},  /* 2 中左 */
    {-60.0f, -50.0f, -80.0f},  /* 3 后左 */
    { 60.0f,  50.0f, -80.0f},  /* 4 前右 */
    { 60.0f,   0.0f, -80.0f},  /* 5 中右 */
    { 60.0f, -50.0f, -80.0f},  /* 6 后右 */
};

/* ========================================================================
 * 三角步态分组
 * ------------------------------------------------------------------------
 * A 组（1）：腿 1、3、5；B 组（0）：腿 2、4、6
 * 数组下标 0~5 对应腿 1~6
 * ======================================================================== */
static const uint8_t GROUP[LEG_NUM] = {1, 0, 1, 0, 1, 0};

/* ========================================================================
 * 改动点 1：每条腿三个关节对应的“逻辑通道号”（0~17）
 * ------------------------------------------------------------------------
 * 你的 PCA9685_Set_Servo_Angle 接收的是 0~17：
 *   0~8  → 板1 的 0~8 通道（腿 1~3）
 *   9~17 → 板2 的 0~8 通道（腿 4~6）
 * 每腿 3 通道依次为 hip / femur / tibia。
 * 若你实际接线不同，只改这三张表即可。
 * ======================================================================== */
static const uint8_t CH_HIP[LEG_NUM]   = {0, 3, 6,  9, 12, 15};
static const uint8_t CH_FEMUR[LEG_NUM] = {1, 4, 7, 10, 13, 16};
static const uint8_t CH_TIBIA[LEG_NUM] = {2, 5, 8, 11, 14, 17};

/* ========================================================================
 * 改动点 2：弧度 → 0~180 角度，喂给 PCA9685_Set_Servo_Angle
 * ------------------------------------------------------------------------
 * IK 输出是以 0 为中位的弧度，这里平移到 0~180：
 *   -90° → 0，0° → 90，+90° → 180
 * 如果你的舵机中位不是 90，改这里的 90.0f 偏移即可。
 * ======================================================================== */
static uint8_t rad_to_servo_angle(float rad) {
    float deg = rad * 57.29578f + 90.0f;   /* 弧度→角度，再平移到 0~180 */
    if (deg < 0.0f)   deg = 0.0f;
    if (deg > 180.0f) deg = 180.0f;
    return (uint8_t)(deg + 0.5f);
}

/* ========================================================================
 * 函数：Gait_Init
 * 作用：上电初始化，让 6 条腿全部回到 home 位置
 * ======================================================================== */
void Gait_Init(void) {
    for (int i = 0; i < LEG_NUM; i++) {
        FootPos p = LEG_HOME[i];
        LegAngles a = inverseKinematics(p.x, p.y, p.z);

        /* 改动点 3：直接传逻辑通道号 + 0~180 角度 */
        PCA9685_Set_Servo_Angle(CH_HIP[i],   rad_to_servo_angle(a.hip));
        PCA9685_Set_Servo_Angle(CH_FEMUR[i], rad_to_servo_angle(a.femur));
        PCA9685_Set_Servo_Angle(CH_TIBIA[i], rad_to_servo_angle(a.tibia));
    }
}

/* ========================================================================
 * 函数：Gait_TripodStep
 * 作用：三角步态的一帧
 * ------------------------------------------------------------------------
 * stride 步幅 mm，lift 抬腿高度 mm，phase 周期进度 0.0~1.0
 *   0~0.5：A 组摆动、B 组支撑
 *   0.5~1：B 组摆动、A 组支撑
 * ======================================================================== */
void Gait_TripodStep(float stride, float lift, float phase) {
    if (phase < 0.0f) phase = 0.0f;
    if (phase >= 1.0f) phase = 0.999f;

    for (int i = 0; i < LEG_NUM; i++) {
        FootPos home = LEG_HOME[i];
        int isGroupA = GROUP[i];

        int swinging;
        float localPhase;

        if (phase < 0.5f) {
            swinging   = isGroupA;
            localPhase = phase / 0.5f;
        } else {
            swinging   = !isGroupA;
            localPhase = (phase - 0.5f) / 0.5f;
        }

        float x = home.x;
        float y = home.y;
        float z = home.z;

        if (swinging) {
            /* 摆动相：抬腿向前 */
            x += (-stride * 0.5f) + stride * localPhase;
            z += lift * sinf(M_PI * localPhase);
        } else {
            /* 支撑相：贴地向后推 */
            x += ( stride * 0.5f) - stride * localPhase;
        }

        LegAngles a = inverseKinematics(x, y, z);

        /* 改动点 4：写舵机也用逻辑通道号 + 0~180 角度 */
        PCA9685_Set_Servo_Angle(CH_HIP[i],   rad_to_servo_angle(a.hip));
        PCA9685_Set_Servo_Angle(CH_FEMUR[i], rad_to_servo_angle(a.femur));
        PCA9685_Set_Servo_Angle(CH_TIBIA[i], rad_to_servo_angle(a.tibia));
    }
}

/* ========================================================================
 * 函数：Gait_WalkSteps
 * 作用：阻塞式走 n 步（调试用）
 * ======================================================================== */
void Gait_WalkSteps(int n, float stride, float lift) {
    const int SUB = 20;   /* 每周期细分 20 帧 */

    for (int s = 0; s < n; s++) {
        for (int k = 0; k < SUB; k++) {
            Gait_TripodStep(stride, lift, (float)k / SUB);
            HAL_Delay(10);   /* 10ms × 20 = 200ms 一周期 */
        }
    }
}