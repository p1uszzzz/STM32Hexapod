#ifndef GAIT_H
#define GAIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LEG_NUM 6   /* 六足 */

/* 身体坐标系下的足端位置，单位 mm
 * x 向前，y 向左，z 向上（足端在下方时 z 为负） */
typedef struct {
    float x;
    float y;
    float z;
} FootPos;

/* 上电初始化：6 条腿回到 home 位置，避免突然抽动 */
void Gait_Init(void);

/* 三角步态一帧
 * stride：步幅 mm
 * lift  ：抬腿高度 mm
 * phase ：周期进度 0.0 ~ 1.0
 *         0~0.5 A 组摆、B 组撑；0.5~1 交换 */
void Gait_TripodStep(float stride, float lift, float phase);

/* 阻塞式走 n 步（调试用）
 * n：步数；stride：步幅 mm；lift：抬腿高度 mm */
void Gait_WalkSteps(int n, float stride, float lift);

/* ======================== 单腿控制（调试 / 标定用） ========================
 * 腿号统一用 1~6：1 前左、2 中左、3 后左、4 前右、5 中右、6 后右
 * 腿号越界（<1 或 >6）时函数直接返回，不会写舵机。
 * ======================================================================== */

/* 单腿按“足端坐标”摆位（最常用）
 *   leg  ：腿号 1~6
 *   x/y/z：足端相对“该腿髋关节旋转中心”的坐标，单位 mm
 *          x 向前（+）、y 向左（+）、z 向上（+），脚在身体下方时 z 为负
 * 内部会走 IK + 角度层，越界时自动投到可达边界、关节角夹到 ±90°。
 * 例：把 2 号腿摆到 home → Gait_SetFoot(2, 0.0f, 117.2f, -75.2f); */
void Gait_SetFoot(uint8_t leg, float x, float y, float z);

/* 单腿直接给“关节角”，跳过 IK（标定单个舵机时最直观）
 *   leg       ：腿号 1~6
 *   hip_deg   ：髋关节方位角，0 = 腿朝正前，+90 = 腿朝正左（单位：度）
 *   femur_deg ：股关节 ∠1，相对中性姿态轴，范围 ±90（单位：度）
 *   tibia_deg ：膝关节 ∠2，相对股连杆延长线，范围 ±90（单位：度）
 * 例：1 号腿回到 home → Gait_SetJointAngles(1, 50.0f, 74.0f, 72.0f);
 *     （舵机会读到 髋 50 / 股 164 / 膝 162） */
void Gait_SetJointAngles(uint8_t leg, float hip_deg, float femur_deg, float tibia_deg);

/* 6 条腿一起回到 LEG_HOME（运行中随时可调，不阻塞、无延时） */
void Gait_Home(void);

#ifdef __cplusplus
}
#endif

#endif /* GAIT_H */