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
 * 腿编号：1=前左, 2=中左, 3=后左, 4=前右, 5=中右, 6=后右
 *
 * 【为什么 x/y 这么填】IK 里 hip = atan2(y, x)，即 hip=0 是腿朝正前方、
 * hip=+90° 是腿朝正左方。所以 x 必须是“前后”、y 才是“左右”，填反了
 * 髋关节会顶到 0°/180°（旧表就是这个问题，见《代码修改指导手册》M5）。
 *
 * 【为什么 z = -110】z 取 −LEG_L3 就是“小腿垂直向下”的近中位姿态：
 * 旧表 z=-80 正好等于旧 LEG_L3=80，同一条规律。按实测 L3=110 取 -110 时，
 * 六个舵机几乎都落在 90°（装配中位）附近，上电最安全、最好标定。
 *
 * 上电实测（本表 + 下文角度层）：servo = 髋/股/膝
 *   1 前左 ( 50, 60,-110) → 50.2 / 90.0 / 91.1     4 前右 ( 50,-60,-110) → 50.2 / 90.0 / 91.1
 *   2 中左 (  0, 60,-110) → 90.0 / 88.2 / 98.7     5 中右 (  0,-60,-110) → 90.0 / 88.2 / 98.7
 *   3 后左 (-50, 60,-110) →129.8 / 90.0 / 91.1     6 后右 (-50,-60,-110) →129.8 / 90.0 / 91.1
 * 全部落在 15°~165° 软限位内，无一路被限幅。
 * ======================================================================== */
static const FootPos LEG_HOME[LEG_NUM] = {
    { 50.0f,  60.0f, -110.0f},  /* 1 前左 */
    {  0.0f,  60.0f, -110.0f},  /* 2 中左 */
    {-50.0f,  60.0f, -110.0f},  /* 3 后左 */
    { 50.0f, -60.0f, -110.0f},  /* 4 前右 */
    {  0.0f, -60.0f, -110.0f},  /* 5 中右 */
    {-50.0f, -60.0f, -110.0f},  /* 6 后右 */
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
 * 改动点 2 / 3 / 4：IK 角度（弧度）→ 舵机 0~180 角度
 * ------------------------------------------------------------------------
 * 装配中位（舵机 90°）的定义见《舵机接线与安装手册》§7：
 *   髋 servo 90° → 摇臂指向该腿正外侧（左腿 hip_ik = +90°，右腿 hip_ik = −90°）
 *   股 servo 90° → 摇臂水平（femur_ik = 0，大腿水平）
 *   膝 servo 90° → 摇臂朝下、小腿垂直（膝内角 K = 90°，IK 输出 tibia = π−K = 90°）
 *
 * 所以绝不能像旧代码那样对三个关节一律 “+90° 平移”，必须先减掉该关节
 * “中位对应的 IK 角”，否则髋/膝会被硬顶到 0°/180°（M2/M4 的根因）。
 * 统一映射：servo = 90 + DIR × (ikDeg − midDeg)，再做 trim 与软限位。
 * ======================================================================== */
#define SERVO_MID_DEG    90.0f    /* 舵机机械中位 */
#define SERVO_MIN_DEG    15.0f    /* 软限位：MG90S 别顶到机械死点（M3） */
#define SERVO_MAX_DEG   165.0f

#define FEMUR_MID_DEG     0.0f    /* servo 90° 时 femur_ik = 0（大腿水平） */
#define TIBIA_MID_DEG    90.0f    /* servo 90° 时 K = 90° ⇒ tibia_ik = 90° */

/* 髋：servo 90° 时腿指向正侧方，左腿 hip_ik = +90°，右腿镜像 = −90° */
static const float HIP_MID_DEG[LEG_NUM] = {
     90.0f,  90.0f,  90.0f,   /* 1~3 左 */
    -90.0f, -90.0f, -90.0f,   /* 4~6 右 */
};

/* 每腿每关节方向：+1 正常，−1 反向（左右镜像安装，见 M4）。
 * 哪条腿转反了就把对应格子改成 -1，不要改 IK 公式里的符号。 */
static const int8_t DIR_HIP[LEG_NUM]   = { 1, 1, 1, -1, -1, -1};
static const int8_t DIR_FEMUR[LEG_NUM] = { 1, 1, 1,  1,  1,  1};
static const int8_t DIR_TIBIA[LEG_NUM] = { 1, 1, 1,  1,  1,  1};

/* 18 路微调（单位：度），下标 = 逻辑通道号 = 腿号*3 + (0 髋 / 1 股 / 2 膝)
 * 摇臂不可能装得完全一致，上机后哪一路偏就改哪一格，一般 ±10 以内够用。
 * 例：1 号腿髋偏 -4° → SERVO_TRIM[0] = -4 */
static const int8_t SERVO_TRIM[18] = {
    0, 0, 0,   0, 0, 0,   0, 0, 0,
    0, 0, 0,   0, 0, 0,   0, 0, 0,
};

/* ------------------------------------------------------------------------
 * 通用换算：以“该关节机械中位对应的 IK 角 = midDeg”为锚点做线性映射
 *   servo = 90 + DIR × (ikDeg − midDeg) + trim，然后限幅到 15~165
 * ------------------------------------------------------------------------ */
static uint8_t joint_angle(uint8_t ch, float ikDeg, int8_t dir, float midDeg) {
    float servo = SERVO_MID_DEG + (float)dir * (ikDeg - midDeg);
    if (ch < 18) {
        servo += (float)SERVO_TRIM[ch];
    }
    if (servo < SERVO_MIN_DEG) servo = SERVO_MIN_DEG;
    if (servo > SERVO_MAX_DEG) servo = SERVO_MAX_DEG;
    return (uint8_t)(servo + 0.5f);
}

/* 髋：hip_ik 0 = 腿朝前，+90° = 腿朝左；右腿镜像（DIR = -1，mid = -90°）
 *     左腿 servo = hip_deg；右腿 servo = -hip_deg */
static uint8_t hip_angle(int leg, float rad) {
    return joint_angle(CH_HIP[leg], rad * 57.29578f,
                       DIR_HIP[leg], HIP_MID_DEG[leg]);
}

/* 股：femur_ik 0 = 大腿水平 → servo = 90 + femur_deg */
static uint8_t femur_angle(int leg, float rad) {
    return joint_angle(CH_FEMUR[leg], rad * 57.29578f,
                       DIR_FEMUR[leg], FEMUR_MID_DEG);
}

/* 膝：tibia_ik 90°（K=90°）↔ servo 90° → servo = tibia_deg = 180° − K */
static uint8_t tibia_angle(int leg, float rad) {
    return joint_angle(CH_TIBIA[leg], rad * 57.29578f,
                       DIR_TIBIA[leg], TIBIA_MID_DEG);
}

/* ========================================================================
 * 函数：Gait_Init
 * 作用：上电初始化，让 6 条腿全部回到 home 位置
 * ======================================================================== */
void Gait_Init(void) {
    for (int i = 0; i < LEG_NUM; i++) {
        FootPos p = LEG_HOME[i];
        LegAngles a = inverseKinematics(p.x, p.y, p.z);

        /* 改动点 5：写舵机统一走 hip/femur/tibia_angle 包装 */
        PCA9685_Set_Servo_Angle(CH_HIP[i],   hip_angle(i,   a.hip));
        PCA9685_Set_Servo_Angle(CH_FEMUR[i], femur_angle(i, a.femur));
        PCA9685_Set_Servo_Angle(CH_TIBIA[i], tibia_angle(i, a.tibia));
    }
}

/* ========================================================================
 * 函数：Gait_TripodStep
 * 作用：三角步态的一帧
 * ------------------------------------------------------------------------
 * stride 步幅 mm，lift 抬腿高度 mm，phase 周期进度 0.0~1.0
 *   0~0.5：A 组摆动、B 组支撑
 *   0.5~1：B 组摆动、A 组支撑
 *
 * 实测包络（stride=±30、lift=20、本 home 表）：
 *   髋 servo 42.7 ~ 137.3，股 servo 88.2 ~ 110.2，膝 servo 85.4 ~ 117.8，
 *   肩到足端 D 97.6 ~ 128.4 mm（工作空间 52.1 ~ 167.9 mm），余量很大。
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

        PCA9685_Set_Servo_Angle(CH_HIP[i],   hip_angle(i,   a.hip));
        PCA9685_Set_Servo_Angle(CH_FEMUR[i], femur_angle(i, a.femur));
        PCA9685_Set_Servo_Angle(CH_TIBIA[i], tibia_angle(i, a.tibia));
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
