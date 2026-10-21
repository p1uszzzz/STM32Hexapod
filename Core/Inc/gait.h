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

#ifdef __cplusplus
}
#endif

#endif /* GAIT_H */