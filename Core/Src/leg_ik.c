#include "leg_ik.h"
#include <math.h>

/* 若编译器报 M_PI 未定义，取消下面这行注释 */
/* #define M_PI 3.14159265358979323846f */

/* ========================================================================
 * 工作空间保护参数
 * ------------------------------------------------------------------------
 * 边界再留 0.1% 余量：目标点正好压在 L2+L3 或 |L2-L3| 上时，浮点误差会让
 * acos 的参数掠过 ±1，虽然下面有 cos 限幅兜底，但留点余量更稳。
 * ======================================================================== */
#define IK_DMAX_SCALE  0.999f
#define IK_DMIN_SCALE  1.001f
#define IK_D_EPS       1.0f    /* D < 1 mm 属病态输入（足端几乎压在肩轴上） */

/* ========================================================================
 * 函数：inverseKinematics
 * 作用：已知足端坐标，反解三个关节角度（单腿 3 自由度）
 * ------------------------------------------------------------------------
 * 思路（分两步，把 3D 问题拆成 2D）：
 *
 *   第 1 步：髋关节（水平旋转）
 *     足端在 xy 平面的投影角度就是髋关节角度：
 *         hip = atan2(y, x)
 *     用 atan2 而不是 atan，能自动处理四个象限。
 *
 *   第 2 步：把腿转到垂直平面，变成 2D 两连杆问题
 *     水平距离 = sqrt(x² + y²) - LEG_L1   （减去髋偏移）
 *     垂直距离 = z
 *     肩到足端的直线距离 D = sqrt(水平² + 垂直²)
 *
 *   第 3 步：余弦定理求膝、肩
 *     膝内角 K：cos(K) = (L2² + L3² - D²) / (2·L2·L3)
 *     肩角    ：先求直线与水平的夹角 alpha，再求三角形内 beta
 *               femur = alpha + beta
 *
 *   第 4 步：输出约定
 *       tibia = π − K   （0 = 伸直，π/2 = 膝内角 90° = 舵机中位）
 *       femur = alpha + beta（0 = 大腿水平）
 *     舵机方向 / 中位换算不在这里做，见 gait.c 的 hip_angle() 等包装。
 * ======================================================================== */
LegAngles inverseKinematics(float x, float y, float z) {
    LegAngles angles = {0.0f, 0.0f, 0.0f};

    /* ---------- 第 1 步：髋关节 ---------- */
    angles.hip = atan2f(y, x);

    /* ---------- 第 2 步：转到垂直平面 ---------- */
    float horizontalDist = sqrtf(x*x + y*y) - LEG_L1;  /* 水平投影 - 髋偏移 */
    float verticalDist   = z;                           /* 垂直距离 */

    /* 肩到足端的直线距离（余弦定理里的 D） */
    float D = sqrtf(horizontalDist*horizontalDist + verticalDist*verticalDist);
    if (D < IK_D_EPS) {
        D = IK_D_EPS;   /* 防下面做比例缩放时除 0 */
    }

    /* ---------- 工作空间保护 ----------
     * 旧代码：越界直接 return 全 0 → 该腿三轴同时甩到 0°，非常危险。
     * 手册 M6 建议“保持上次有效解”，但那是跨 6 条腿共用的 static，
     * 会把 1 号腿的解送给 3 号腿，串腿。
     * 这里改成：把目标点沿视线（保持方向不变）缩放到最近的可达边界点。
     * 方向不变 ⇒ alpha 不变，只有 D 变化 ⇒ 输出连续、无跳变、无状态。 */
    {
        const float dMax = LEG_REACH_MAX * IK_DMAX_SCALE;
        const float dMin = LEG_REACH_MIN * IK_DMIN_SCALE;

        if (D > dMax || D < dMin) {
            const float target = (D > dMax) ? dMax : dMin;
            const float scale  = target / D;   /* 同一比例缩放水平/垂直分量 */
            horizontalDist *= scale;
            verticalDist   *= scale;
            D = target;
        }
    }

    /* ---------- 第 3 步：膝关节 ---------- */
    /* 余弦定理：cos(膝内角) = (L2² + L3² - D²) / (2·L2·L3) */
    float cosTibia = (LEG_L2*LEG_L2 + LEG_L3*LEG_L3 - D*D)
                     / (2.0f * LEG_L2 * LEG_L3);

    /* 浮点误差可能让 cos 略超 ±1，acos 会返回 NaN，必须限幅 */
    if (cosTibia >  1.0f) cosTibia =  1.0f;
    if (cosTibia < -1.0f) cosTibia = -1.0f;

    float tibiaInternalAngle = acosf(cosTibia);  /* 膝内角 K */
    /* 输出约定：tibia = π − K，按《安装手册》§7，K=90° 时对应舵机中位 */
    angles.tibia = M_PI - tibiaInternalAngle;

    /* ---------- 第 4 步：肩关节 ---------- */
    /* alpha：足端直线与水平面的夹角 */
    float alpha = atan2f(verticalDist, horizontalDist);

    /* beta：三角形内大腿与足端直线的夹角，同样用余弦定理 */
    float cosBeta = (LEG_L2*LEG_L2 + D*D - LEG_L3*LEG_L3)
                    / (2.0f * LEG_L2 * D);
    if (cosBeta >  1.0f) cosBeta =  1.0f;
    if (cosBeta < -1.0f) cosBeta = -1.0f;

    float beta = acosf(cosBeta);

    /* 肩角 = 直线角 + 三角形内角 */
    angles.femur = alpha + beta;

    return angles;
}
