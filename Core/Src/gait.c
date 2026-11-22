#include "gait.h"
#include "leg_ik.h"
#include "pca9685.h"
#include "main.h"      /* 用到 HAL_Delay */
#include <math.h>

/* 若编译器报 M_PI 未定义，取消下面这行注释 */
/* #define M_PI 3.14159265358979323846f */

#ifndef DEG2RAD
#define DEG2RAD  0.0174532925f   /* 度 → 弧度，单腿角度接口用 */
#endif

/* ========================================================================
 * 6 条腿的 home 位置（相对髋关节旋转中心，单位 mm）
 * ------------------------------------------------------------------------
 * 坐标系：x 向前，y 向左，z 向上（足端在下方，所以 z 为负）
 * 腿编号：1=前左, 2=中左, 3=后左, 4=前右, 5=中右, 6=后右
 *
 * 【怎么来的】极坐标给出：每腿径向 r = 117.2 mm、高 z = −75.2 mm，
 * 腿平面朝向 ψ = 50° / 90° / 130°（左 1~3 腿），右 4~6 腿镜像成 −ψ。
 * 对应 IK 角：股关节 ∠1 = 74°，膝关节 ∠2 = 72°。
 *
 * 【为什么 x/y 这么填】IK 里 hip = atan2(y, x)：hip=0 腿朝正前方、
 * hip=+90° 腿朝正左方。所以 x 必须是“前后”、y 才是“左右”，填反了
 * 髋关节会顶到 0°/180°（旧表就是这个问题，见《代码修改指导手册》M5）。
 *
 * 上电实测（本表 + 下文角度层，股关节 DIR_FEMUR = −1）：servo = 髋 / 股 / 膝
 *   1 前左 ( 75.3, 89.7,-75.2) →  50.0 /  16.1 / 162.1
 *   2 中左 (  0.0,117.2,-75.2) →  90.0 /  16.0 / 162.0
 *   3 后左 (-75.3, 89.7,-75.2) → 130.0 /  16.1 / 162.1
 *   4 前右 ( 75.3,-89.7,-75.2) →  50.0 /  16.1 / 162.1
 *   5 中右 (  0.0,-117.2,-75.2) →  90.0 /  16.0 / 162.0
 *   6 后右 (-75.3,-89.7,-75.2) → 130.0 /  16.1 / 162.1
 * 全部落在软限位内，无一路被限幅。
 * ======================================================================== */
static const FootPos LEG_HOME[LEG_NUM] = {
    { 75.3f,  89.7f, -75.2f},  /* 1 前左 */
    {  0.0f, 117.2f, -75.2f},  /* 2 中左 */
    {-75.3f,  89.7f, -75.2f},  /* 3 后左 */
    { 75.3f, -89.7f, -75.2f},  /* 4 前右 */
    {  0.0f,-117.2f, -75.2f},  /* 5 中右 */
    {-75.3f, -89.7f, -75.2f},  /* 6 后右 */
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
 * PCA9685_Set_Servo_Angle 接收的是 0~17：
 *   0~8  → 板1 的 0~8 通道（腿 1~3）
 *   9~17 → 板2 的 0~8 通道（腿 4~6）
 * 每腿 3 通道依次为 髋 / 股 / 膝。
 * 若实际接线不同，只改这三张表即可。
 * ======================================================================== */
static const uint8_t CH_HIP[LEG_NUM]   = {0, 3, 6,  9, 12, 15};
static const uint8_t CH_FEMUR[LEG_NUM] = {1, 4, 7, 10, 13, 16};
static const uint8_t CH_TIBIA[LEG_NUM] = {2, 5, 8, 11, 14, 17};

/* ========================================================================
 * 改动点 2 / 3 / 4：IK 角度（弧度）→ 舵机 0~180 角度
 * ------------------------------------------------------------------------
 * 现场实测口径（《舵机接线与安装手册》装配说明）：所有舵机 90° 时
 * 整条腿竖直向上，即 舵机 90° = 中性姿态；
 * π/2（即 LEG_JOINT_MAX_RAD）对应舵机 180° 一侧。
 * 所以三个关节统一为：servo = 90 + DIR × IK角，再做 trim 与软限位。
 * 髋关节例外：舵机 90° 时腿指向该腿正外侧，左腿 hip_ik = +90°、
 * 右腿镜像 = −90°，因此中位锚点按腿侧取 ±90°。
 * ======================================================================== */
#define SERVO_MID_DEG    90.0f    /* 舵机机械中位 */

/* 软限位（M3）：MG90S 别顶到机械死点。
 * 股 / 膝要放宽到 5~175，因为按“90° = 腿竖直向上”的口径，
 * 正常站立姿态就落在 160 附近（见下面 home 表），限 15~165 会把腿夹住。 */
#define HIP_MIN_DEG      15.0f
#define HIP_MAX_DEG     165.0f
#define LEG_MIN_DEG       5.0f
#define LEG_MAX_DEG     175.0f

/* servo 90° 时该关节的 IK 角（中位锚点） */
#define FEMUR_MID_DEG     0.0f    /* 股：中性姿态轴 ⇔ ∠1 = 0 */
#define TIBIA_MID_DEG     0.0f    /* 膝：与股连杆共线 ⇔ ∠2 = 0 */

/* 髋：servo 90° 时腿指向正侧方，左腿 hip_ik = +90°，右腿镜像 = −90° */
static const float HIP_MID_DEG[LEG_NUM] = {
     90.0f,  90.0f,  90.0f,   /* 1~3 左 */
    -90.0f, -90.0f, -90.0f,   /* 4~6 右 */
};

/* 每腿每关节方向：+1 正常，−1 反向（左右镜像安装，见 M4）。
 * 哪条腿转反了就把对应格子改成 -1，不要改 IK 公式里的符号。 */
static const int8_t DIR_HIP[LEG_NUM]   = { 1, 1, 1, -1, -1, -1};
static const int8_t DIR_FEMUR[LEG_NUM] = { -1, -1, -1,  -1,  -1,  -1};
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
 *   servo = 90 + DIR × (ikDeg − midDeg) + trim，然后限幅到 [minDeg, maxDeg]
 * ------------------------------------------------------------------------ */
static uint8_t joint_angle(uint8_t ch, float ikDeg, int8_t dir, float midDeg,
                           float minDeg, float maxDeg) {
    float servo = SERVO_MID_DEG + (float)dir * (ikDeg - midDeg);
    if (ch < 18) {
        servo += (float)SERVO_TRIM[ch];
    }
    if (servo < minDeg) servo = minDeg;
    if (servo > maxDeg) servo = maxDeg;
    return (uint8_t)(servo + 0.5f);
}

/* 髋：hip_ik 0 = 腿朝前，+90° = 腿朝左；右腿镜像（DIR = -1，mid = -90°）
 *     左腿 servo = hip_deg；右腿 servo = -hip_deg */
static uint8_t hip_angle(int leg, float rad) {
    return joint_angle(CH_HIP[leg], rad * 57.29578f,
                       DIR_HIP[leg], HIP_MID_DEG[leg],
                       HIP_MIN_DEG, HIP_MAX_DEG);
}

/* 股：∠1 = 0 ⇔ servo 90°；实测装法 DIR_FEMUR = −1，所以 servo = 90 − ∠1（度） */
static uint8_t femur_angle(int leg, float rad) {
    return joint_angle(CH_FEMUR[leg], rad * 57.29578f,
                       DIR_FEMUR[leg], FEMUR_MID_DEG,
                       LEG_MIN_DEG, LEG_MAX_DEG);
}

/* 膝：∠2 = 0 ⇔ servo 90°，所以 servo = 90 + ∠2（度） */
static uint8_t tibia_angle(int leg, float rad) {
    return joint_angle(CH_TIBIA[leg], rad * 57.29578f,
                       DIR_TIBIA[leg], TIBIA_MID_DEG,
                       LEG_MIN_DEG, LEG_MAX_DEG);
}

/* ------------------------------------------------------------------------
 * 把一条腿的三个 IK 角写进对应的三个舵机
 *   i：腿下标 0~5（对外接口的腿号 1~6 减 1）
 * 单腿控制、Gait_Init、Gait_TripodStep 都走这里，保证换算只写一份。
 * ------------------------------------------------------------------------ */
static void write_leg(uint8_t i, LegAngles a) {
    PCA9685_Set_Servo_Angle(CH_HIP[i],   hip_angle(i,   a.hip));
    PCA9685_Set_Servo_Angle(CH_FEMUR[i], femur_angle(i, a.femur));
    PCA9685_Set_Servo_Angle(CH_TIBIA[i], tibia_angle(i, a.tibia));
}

/* ========================================================================
 * 函数：Gait_Init
 * 作用：上电初始化，让 6 条腿全部回到 home 位置
 * ======================================================================== */
void Gait_Init(void) {
    Gait_Home();
}

/* ========================================================================
 * 函数：Gait_Home
 * 作用：6 条腿全部回到 LEG_HOME（运行中也可以随时调用）
 * 非阻塞、无延时：只写 18 路舵机目标角度，转动时间由舵机自己决定。
 * ======================================================================== */
void Gait_Home(void) {
    for (uint8_t i = 0; i < LEG_NUM; i++) {
        FootPos p = LEG_HOME[i];
        write_leg(i, inverseKinematics(p.x, p.y, p.z));
    }
}

/* ========================================================================
 * 函数：Gait_SetFoot
 * 作用：单腿按足端坐标摆位（IK + 角度层，最常用）
 * ------------------------------------------------------------------------
 * leg  ：腿号 1~6；越界直接返回
 * x/y/z：足端相对该腿髋关节旋转中心的坐标，mm（x 前、y 左、z 上）
 * 例：Gait_SetFoot(2, 0.0f, 117.2f, -75.2f);   // 2 号腿摆到 home
 * ======================================================================== */
void Gait_SetFoot(uint8_t leg, float x, float y, float z) {
    if (leg < 1 || leg > LEG_NUM) return;
    write_leg((uint8_t)(leg - 1), inverseKinematics(x, y, z));
}

/* ========================================================================
 * 函数：Gait_SetJointAngles
 * 作用：单腿直接给关节角（跳过 IK，单位：度），标定单个舵机最直观
 * ------------------------------------------------------------------------
 * hip_deg  ：髋方位角，0 = 正前，+90 = 正左
 * femur_deg：股关节 ∠1（相对中性轴，±90）
 * tibia_deg：膝关节 ∠2（相对股连杆延长线，±90）
 * 例：Gait_SetJointAngles(1, 50.0f, 74.0f, 72.0f);
 *     → 舵机 髋 50 / 股 164 / 膝 162（1 号腿的 home）
 * 注意：这里不做工作空间检查，只靠 ±90° 与软限位兜底。
 * ======================================================================== */
void Gait_SetJointAngles(uint8_t leg, float hip_deg, float femur_deg, float tibia_deg) {
    if (leg < 1 || leg > LEG_NUM) return;

    LegAngles a;
    a.hip   = hip_deg   * DEG2RAD;
    a.femur = femur_deg * DEG2RAD;
    a.tibia = tibia_deg * DEG2RAD;

    write_leg((uint8_t)(leg - 1), a);
}

/* ========================================================================
 * 函数：Gait_TripodStep
 * 作用：三角步态的一帧
 * ------------------------------------------------------------------------
 * stride 步幅 mm，lift 抬腿高度 mm，phase 周期进度 0.0~1.0
 *   0~0.5：A 组摆动、B 组支撑
 *   0.5~1：B 组摆动、A 组支撑
 *
 * 实测包络（stride=±30、lift=10、本 home 表、DIR_FEMUR=−1，见 tools/ik_check.py）：
 *   髋 servo 44.8 ~ 135.2，股 servo 9.5 ~ 25.2，膝 servo 149.8 ~ 171.9，
 *   全部在软限位内，无截断。
 * 注意：本几何下“抬腿”会把足端送向工作空间内边界（d 变短，腿被迫伸直），
 *       lift 不宜超过 10~15 mm，否则膝关节会先被 ±90° 限位夹住。
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
        write_leg((uint8_t)i, a);
    }
}

/* ========================================================================
 * 函数：Gait_WalkSteps
 * 作用：阻塞式走 n 步（调试用）
 * ------------------------------------------------------------------------
 * 建议 stride 20~30 mm、lift 差异见 Gait_TripodStep 的说明（≤10~15 mm）。
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
