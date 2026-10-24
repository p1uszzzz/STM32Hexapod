#include "leg_ik.h"
#include <math.h>

/* 若编译器报 M_PI 未定义，取消下面这行注释 */
/* #define M_PI 3.14159265358979323846f */

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
 *     膝内角：cos(θ) = (L2² + L3² - D²) / (2·L2·L3)
 *     肩角  ：先求直线与水平的夹角 alpha，再求三角形内 beta
 *             femur = alpha + beta
 *
 * 注意：femur 的 +beta 以及 tibia 的 M_PI - 内角，
 *       取决于舵机安装方向，若实机反向，改成 -beta 或直接取内角。
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

    /* 工作空间检查：腿伸直都够不着，或太近折叠不下，都返回 0 */
    if (D > (LEG_L2 + LEG_L3) || D < fabsf(LEG_L2 - LEG_L3)) {
        return angles;
    }

    /* ---------- 第 3 步：膝关节 ---------- */
    /* 余弦定理：cos(膝内角) = (L2² + L3² - D²) / (2·L2·L3) */
    float cosTibia = (LEG_L2*LEG_L2 + LEG_L3*LEG_L3 - D*D)
                     / (2.0f * LEG_L2 * LEG_L3);

    /* 浮点误差可能让 cos 略超 ±1，acos 会返回 NaN，必须限幅 */
    if (cosTibia >  1.0f) cosTibia =  1.0f;
    if (cosTibia < -1.0f) cosTibia = -1.0f;

    float tibiaInternalAngle = acosf(cosTibia);  /* 膝的夹角 */
    /* 舵机角度通常是 180° - 内角，取决于安装，反向就改成内角本身 */
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

    /* 肩角 = 直线角 + 三角形内角；若实机反向改成 alpha - beta */
    angles.femur = alpha + beta;

    return angles;
}