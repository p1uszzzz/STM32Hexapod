#include "leg_ik.h"
#include <math.h>

/* 若编译器报 M_PI 未定义，取消下面这行注释 */
/* #define M_PI 3.14159265358979323846f */

/* ========================================================================
 * 工作空间保护参数
 * ------------------------------------------------------------------------
 * 边界再留 0.1% 余量：目标点正好压在 L2+L3 或 hypot(L2,L3) 上时，浮点误差
 * 会让 acos 的参数掠过 ±1；下面虽有 cos 限幅兜底，留点余量更稳。
 * ======================================================================== */
#define IK_DMAX_SCALE  0.999f
#define IK_DMIN_SCALE  1.001f
#define IK_D_EPS       1.0f    /* d < 1 mm 属病态输入（足端几乎压在髋轴上） */

/* ========================================================================
 * 函数：inverseKinematics
 * 作用：已知足端坐标，反解三个关节角度（髋 / 股 / 膝）
 * ------------------------------------------------------------------------
 * 模型（现场实测口径）：舵机 90°/90° 时整条腿与“中性姿态轴”重合、竖直向上，
 * 股连杆 + 胫连杆 + 足端构成竖直面内的两自由度平面臂：
 *
 *   第 1 步：髋关节（水平面方位角）
 *       hip = atan2(y, x)
 *     用 atan2 自动处理四个象限；0 = 腿朝 +x（正前方），+90° = 腿朝 +y（正左）。
 *
 *   第 2 步：转到腿所在的竖直平面，只剩两连杆
 *       r = sqrt(x² + y²)   —— 足端到髋轴的径向距离（≥ 0）
 *       u = 沿中性姿态轴的分量 —— 中性轴朝上时 u = z（足在下方时 u < 0）
 *       d = sqrt(r² + u²)   —— 髋关节旋转中心到足端的直线距离
 *
 *   第 3 步：余弦定理求膝关节 ∠2
 *       cos∠2 = (d² − L2² − L3²) / (2·L2·L3)
 *     取 ∠2 = +acos(…) 这一支（“正向弯曲”）；反向弯曲的腿靠 gait.c 的
 *     DIR_TIBIA 现场翻符号，不在公式里分叉。
 *
 *   第 4 步：余弦定理（或三角形内角）求股关节 ∠1
 *       φ = atan2(r, u)                      —— 视线相对中性轴的角度
 *       β = atan2(L3·sin∠2, L2 + L3·cos∠2)   —— 视线与股连杆的夹角
 *       ∠1 = φ − β
 *
 *   第 5 步：限幅
 *       两关节机械行程都是 ±90°，超出就夹住，保证不会顶到机构死点。
 *
 * 舵机方向 / 中位换算不在这里做，见 gait.c 的 hip_angle() 等包装。
 * ======================================================================== */
LegAngles inverseKinematics(float x, float y, float z) {
    LegAngles angles = {0.0f, 0.0f, 0.0f};

    /* ---------- 第 1 步：髋关节 ---------- */
    angles.hip = atan2f(y, x);

    /* ---------- 第 2 步：转到腿所在的竖直平面 ---------- */
    float radial = sqrtf(x*x + y*y);   /* 足端到髋轴的径向距离 */
#if LEG_IK_NEUTRAL_UP
    float axis = z;                    /* 中性姿态轴朝上：足在下方 ⇒ axis < 0 */
#else
    float axis = -z;                   /* 中性姿态轴朝下：足在下方 ⇒ axis > 0 */
#endif

    float d = sqrtf(radial*radial + axis*axis);
    if (d < IK_D_EPS) {
        /* 足端压在髋轴上：方向未定义，取“正下方”当一个安全方向 */
        radial = 0.0f;
        axis   = -1.0f;
        d      = 1.0f;
    }

    /* ---------- 第 3 步：工作空间保护 ----------
     * 旧代码：越界直接 return 全 0 → 该腿三轴同时甩到 0°，非常危险。
     * 手册 M6 建议“保持上次有效解”，但那是跨 6 条腿共用的 static，会把
     * 1 号腿的解送给 3 号腿，串腿。
     * 这里改为：把目标点沿视线（方向不变）缩放到最近的可达边界点。
     * 方向不变 ⇒ φ 不变，只有 d 变化 ⇒ 输出连续、无跳变、无状态。 */
    {
        const float dMax = LEG_REACH_MAX * IK_DMAX_SCALE;
        const float dMin = LEG_REACH_MIN * IK_DMIN_SCALE;

        if (d > dMax || d < dMin) {
            const float target = (d > dMax) ? dMax : dMin;
            const float scale  = target / d;
            radial *= scale;
            axis   *= scale;
            d       = target;
        }
    }

    /* ---------- 第 4 步：膝关节 ∠2 ---------- */
    float cosTibia = (d*d - LEG_L2*LEG_L2 - LEG_L3*LEG_L3)
                     / (2.0f * LEG_L2 * LEG_L3);

    /* 浮点误差可能让 cos 略超 ±1，acos 会返回 NaN，必须限幅 */
    if (cosTibia >  1.0f) cosTibia =  1.0f;
    if (cosTibia < -1.0f) cosTibia = -1.0f;

    float tibia = acosf(cosTibia);      /* 0 ~ π/2，正向弯曲这一支 */

    /* ---------- 第 5 步：股关节 ∠1 ---------- */
    float phi  = atan2f(radial, axis);  /* 视线相对中性姿态轴的角度 */
    float beta = atan2f(LEG_L3 * sinf(tibia),
                        LEG_L2 + LEG_L3 * cosf(tibia));
    float femur = phi - beta;

    /* ---------- 第 6 步：关节限位 ±90° ---------- */
    if (femur >  LEG_JOINT_MAX_RAD) femur =  LEG_JOINT_MAX_RAD;
    if (femur < -LEG_JOINT_MAX_RAD) femur = -LEG_JOINT_MAX_RAD;
    if (tibia >  LEG_JOINT_MAX_RAD) tibia =  LEG_JOINT_MAX_RAD;
    if (tibia < -LEG_JOINT_MAX_RAD) tibia = -LEG_JOINT_MAX_RAD;

    angles.femur = femur;
    angles.tibia = tibia;

    return angles;
}
