#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ik_check.py — 腿长 / 工作空间 / 舵机角度自查工具

算法与固件完全一致，可直接对照：
    Core/Src/leg_ik.c   inverseKinematics()      （竖直面两自由度几何解算）
    Core/Src/gait.c     hip/femur/tibia_angle()  （IK 角 → 舵机 0~180）

模型（现场实测口径）：所有舵机 90° 时整条腿竖直向上，股 + 胫构成竖直面内的
两自由度平面臂，股关节 ∠1、膝关节 ∠2 均限 ±90°。

用法（不需要任何第三方库）：
    python tools/ik_check.py                        # 用固件当前的腿长和 home 表
    python tools/ik_check.py --l2 57.9 --l3 110
    python tools/ik_check.py --stride 30 --lift 10  # 顺便算步态包络
    python tools/ik_check.py --hip-min 15 --hip-max 165 --leg-min 5 --leg-max 175

输出：
    1. 工作空间范围（hypot(L2,L3) ~ L2+L3）
    2. home 姿态每腿的 IK 角 → 舵机角，标出哪一路被软限位截断
    3. 步态扫掠下每个关节的舵机角最小/最大值，以及是否越界
"""

import argparse
import math

# ---------------------------------------------------------------- 固件常量
# 腿序 1~6 = 前左 / 中左 / 后左 / 前右 / 中右 / 后右
LEG_NAMES = ["1 前左", "2 中左", "3 后左", "4 前右", "5 中右", "6 后右"]

CH_HIP = [0, 3, 6, 9, 12, 15]
CH_FEMUR = [1, 4, 7, 10, 13, 16]
CH_TIBIA = [2, 5, 8, 11, 14, 17]

GROUP = [1, 0, 1, 0, 1, 0]              # 三角步态 A/B 组

DIR_HIP = [1, 1, 1, -1, -1, -1]         # 右腿髋镜像
DIR_FEMUR = [-1, -1, -1, -1, -1, -1]    # 实测装法：股关节方向与几何角相反
DIR_TIBIA = [1, 1, 1, 1, 1, 1]

HIP_MID = [90.0, 90.0, 90.0, -90.0, -90.0, -90.0]   # servo 90° 时的 hip_ik
SERVO_MID = 90.0
FEMUR_MID = 0.0                          # servo 90° 时 ∠1 = 0（中性姿态轴）
TIBIA_MID = 0.0                          # servo 90° 时 ∠2 = 0（与股连杆共线）

NEUTRAL_UP = 1                           # 与 leg_ik.h 的 LEG_IK_NEUTRAL_UP 对应
JOINT_MAX_DEG = 90.0                     # 两关节机械行程 ±90°

# LEG_HOME（x 前 / y 左 / z 上，足在下方为负）
HOME = [
    (75.3, 89.7, -75.2),
    (0.0, 117.2, -75.2),
    (-75.3, 89.7, -75.2),
    (75.3, -89.7, -75.2),
    (0.0, -117.2, -75.2),
    (-75.3, -89.7, -75.2),
]

DMAX_SCALE = 0.999    # 与 leg_ik.c 的 IK_DMAX_SCALE 对应
DMIN_SCALE = 1.001    # 与 leg_ik.c 的 IK_DMIN_SCALE 对应
D_EPS = 1.0
RAD2DEG = 57.29578


def reach_limits(l2, l3):
    """工作空间：|∠2| ≤ 90° ⇒ cos∠2 ≥ 0 ⇒ d ≥ hypot(l2, l3)"""
    return math.hypot(l2, l3), l2 + l3


def ik(l2, l3, x, y, z):
    """返回 (hip_deg, femur_deg, tibia_deg, d, projected, limited)。"""
    hip = math.degrees(math.atan2(y, x))

    radial = math.hypot(x, y)
    axis = z if NEUTRAL_UP else -z

    d = math.hypot(radial, axis)
    if d < D_EPS:
        radial, axis, d = 0.0, -1.0, 1.0

    r_min, r_max = reach_limits(l2, l3)
    d_max, d_min = r_max * DMAX_SCALE, r_min * DMIN_SCALE
    projected = False
    if d > d_max or d < d_min:
        target = d_max if d > d_max else d_min
        scale = target / d
        radial *= scale
        axis *= scale
        d = target
        projected = True

    cos_t = (d * d - l2 * l2 - l3 * l3) / (2.0 * l2 * l3)
    cos_t = max(-1.0, min(1.0, cos_t))
    tibia = math.degrees(math.acos(cos_t))          # ∠2，0~90°

    phi = math.degrees(math.atan2(radial, axis))
    beta = math.degrees(math.atan2(l3 * math.sin(math.radians(tibia)),
                                   l2 + l3 * math.cos(math.radians(tibia))))
    femur = phi - beta                              # ∠1

    limited = False
    if abs(femur) > JOINT_MAX_DEG:
        femur = math.copysign(JOINT_MAX_DEG, femur)
        limited = True
    if abs(tibia) > JOINT_MAX_DEG:
        tibia = math.copysign(JOINT_MAX_DEG, tibia)
        limited = True

    return hip, femur, tibia, d, projected, limited


def map_joint(ik_deg, mid_deg, direction):
    """gait.c joint_angle() 的映射：servo = 90 + DIR*(ik - mid)"""
    return SERVO_MID + direction * (ik_deg - mid_deg)


def servo_of(leg, hip_deg, femur_deg, tibia_deg, hip_lim, leg_lim):
    """返回 (舵机角三元组, 是否被限幅)"""
    raw = [
        map_joint(hip_deg, HIP_MID[leg], DIR_HIP[leg]),
        map_joint(femur_deg, FEMUR_MID, DIR_FEMUR[leg]),
        map_joint(tibia_deg, TIBIA_MID, DIR_TIBIA[leg]),
    ]
    lims = [hip_lim, leg_lim, leg_lim]
    out = [min(lims[i][1], max(lims[i][0], v)) for i, v in enumerate(raw)]
    cut = [abs(a - b) > 1e-9 for a, b in zip(raw, out)]
    return out, cut


def main():
    ap = argparse.ArgumentParser(description="腿长 / 工作空间 / 舵机角度自查")
    ap.add_argument("--l2", type=float, default=57.9, help="股连杆长度 mm")
    ap.add_argument("--l3", type=float, default=110.0, help="胫连杆长度 mm")
    ap.add_argument("--stride", type=float, default=30.0,
                    help="步态扫掠用的步幅 mm（<=0 跳过扫掠）")
    ap.add_argument("--lift", type=float, default=10.0, help="抬腿高度 mm")
    ap.add_argument("--hip-min", type=float, default=15.0, help="髋软限位下限（度）")
    ap.add_argument("--hip-max", type=float, default=165.0, help="髋软限位上限（度）")
    ap.add_argument("--leg-min", type=float, default=5.0, help="股/膝软限位下限（度）")
    ap.add_argument("--leg-max", type=float, default=175.0, help="股/膝软限位上限（度）")
    args = ap.parse_args()

    l2, l3 = args.l2, args.l3
    hip_lim = (args.hip_min, args.hip_max)
    leg_lim = (args.leg_min, args.leg_max)
    r_min, r_max = reach_limits(l2, l3)

    print("=" * 78)
    print("腿长：股连杆 LEG_L2 = %.2f  胫连杆 LEG_L3 = %.2f mm" % (l2, l3))
    print("中性姿态：舵机 90°/90° → 整条腿竖直%s" % ("向上" if NEUTRAL_UP else "向下"))
    print("工作空间（髋 → 足端 d）：%.2f ~ %.2f mm（因 ∠2 限 ±90°）" % (r_min, r_max))
    print("软限位：髋 %.1f~%.1f   股/膝 %.1f~%.1f 度"
          % (hip_lim[0], hip_lim[1], leg_lim[0], leg_lim[1]))
    print("=" * 78)

    # ------------------------------------------------ home 姿态
    print("\n[1] home 姿态（LEG_HOME 六条腿）")
    print("-" * 78)
    print("%-8s %-20s %8s %8s %8s %8s   %-16s %s"
          % ("腿", "足端 (x,y,z)", "d", "∠1 股", "∠2 膝", "髋_ik",
             "舵机 髋/股/膝", "备注"))
    bad = 0
    for i in range(6):
        x, y, z = HOME[i]
        hip, fem, tib, d, projected, limited = ik(l2, l3, x, y, z)
        sv, cut = servo_of(i, hip, fem, tib, hip_lim, leg_lim)
        notes = []
        if projected:
            notes.append("d 越界已投影")
        if limited:
            notes.append("关节撞 ±90°")
        if any(cut):
            notes.append("舵机限幅!")
            bad += 1
        print("%-8s (%5.1f,%5.1f,%5.1f) %8.2f %8.2f %8.2f %8.2f   %5.1f/%5.1f/%5.1f  %s"
              % (LEG_NAMES[i], x, y, z, d, fem, tib, hip,
                 sv[0], sv[1], sv[2], " ".join(notes)))
    print("-" * 78)
    print("说明：servo 90° = 装配中位（三个关节都与中性姿态轴对齐）")

    # ------------------------------------------------ 步态包络
    if args.stride > 0.0:
        env = [[1e9, -1e9] for _ in range(3)]
        d_lo, d_hi = 1e9, -1e9
        hits = []
        phases = 40
        for sgn in (-1.0, 1.0):
            stride = sgn * args.stride
            for k in range(phases):
                phase = k / phases
                for i in range(6):
                    hx, hy, hz = HOME[i]
                    x, y, z = hx, hy, hz
                    if phase < 0.5:
                        swinging = GROUP[i] == 1
                        local = phase / 0.5
                    else:
                        swinging = GROUP[i] == 0
                        local = (phase - 0.5) / 0.5
                    if swinging:
                        x += (-stride * 0.5) + stride * local
                        z += args.lift * math.sin(math.pi * local)
                    else:
                        x += (stride * 0.5) - stride * local
                    hip, fem, tib, d, _p, _l = ik(l2, l3, x, y, z)
                    sv, cut = servo_of(i, hip, fem, tib, hip_lim, leg_lim)
                    for j in range(3):
                        env[j][0] = min(env[j][0], sv[j])
                        env[j][1] = max(env[j][1], sv[j])
                        if cut[j]:
                            hits.append((stride, k, LEG_NAMES[i], j, sv[j]))
                    d_lo, d_hi = min(d_lo, d), max(d_hi, d)

        print("\n[2] 步态包络（stride = ±%.1f mm，lift = %.1f mm，%d 帧）"
              % (args.stride, args.lift, phases))
        print("-" * 78)
        names = ["髋", "股", "膝"]
        for j in range(3):
            print("  %s servo：%7.2f ~ %7.2f 度" % (names[j], env[j][0], env[j][1]))
        print("  d 范围：%7.2f ~ %7.2f mm（工作空间 %.2f ~ %.2f）"
              % (d_lo, d_hi, r_min, r_max))
        print("-" * 78)
        if hits:
            print("  [警告] %d 处打到软限位，需要减小 stride / lift：" % len(hits))
            for h in hits[:10]:
                print("      stride=%.1f 帧%d %s 关节%d → %.2f 度" % h)
            bad += len(hits)
        else:
            print("  全部在软限位内，没有截断。")

    print("\n结论：%s" % ("全部通过 ✅" if bad == 0 else "存在 %d 处问题 ❌" % bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
