#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ik_check.py — 腿长 / 工作空间 / 舵机角度自查工具

算法与固件完全一致，可直接对照：
    Core/Src/leg_ik.c   inverseKinematics()      （几何解算）
    Core/Src/gait.c     hip/femur/tibia_angle()  （IK 角 → 舵机 0~180）

用法（不需要任何第三方库）：
    python tools/ik_check.py                       # 用固件当前的腿长和 home 表
    python tools/ik_check.py --l2 57.9 --l3 110
    python tools/ik_check.py --stride 30 --lift 20 # 顺便算步态包络
    python tools/ik_check.py --min 15 --max 165

输出：
    1. 工作空间范围（|L2-L3| ~ L2+L3）
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
DIR_FEMUR = [1, 1, 1, 1, 1, 1]
DIR_TIBIA = [1, 1, 1, 1, 1, 1]

HIP_MID = [90.0, 90.0, 90.0, -90.0, -90.0, -90.0]   # servo 90° 时的 hip_ik
SERVO_MID = 90.0
FEMUR_MID = 0.0
TIBIA_MID = 90.0

# LEG_HOME（x 前 / y 左 / z 上，足在下方为负）
HOME = [
    (50.0, 60.0, -110.0),
    (0.0, 60.0, -110.0),
    (-50.0, 60.0, -110.0),
    (50.0, -60.0, -110.0),
    (0.0, -60.0, -110.0),
    (-50.0, -60.0, -110.0),
]

DMAX_SCALE = 0.999    # 与 leg_ik.c 的 IK_DMAX_SCALE 对应
DMIN_SCALE = 1.001    # 与 leg_ik.c 的 IK_DMIN_SCALE 对应
D_EPS = 1.0
RAD2DEG = 57.29578


def ik(l1, l2, l3, x, y, z):
    """返回 (hip_deg, femur_deg, tibia_deg, D, K_deg, clamped)。"""
    hip = math.atan2(y, x)
    h = math.hypot(x, y) - l1
    D = math.hypot(h, z)
    if D < D_EPS:
        D = D_EPS

    d_max = (l2 + l3) * DMAX_SCALE
    d_min = abs(l2 - l3) * DMIN_SCALE
    clamped = False
    if D > d_max or D < d_min:
        target = d_max if D > d_max else d_min
        scale = target / D
        h *= scale
        z *= scale
        D = target
        clamped = True

    cos_k = (l2 * l2 + l3 * l3 - D * D) / (2.0 * l2 * l3)
    cos_k = max(-1.0, min(1.0, cos_k))
    K = math.acos(cos_k)
    tibia = math.pi - K

    alpha = math.atan2(z, h)
    cos_b = (l2 * l2 + D * D - l3 * l3) / (2.0 * l2 * D)
    cos_b = max(-1.0, min(1.0, cos_b))
    beta = math.acos(cos_b)
    femur = alpha + beta

    return (math.degrees(hip), math.degrees(femur), math.degrees(tibia),
            D, math.degrees(K), clamped)


def map_joint(ik_deg, mid_deg, direction):
    """gait.c joint_angle() 的映射：servo = 90 + DIR*(ik - mid)"""
    return SERVO_MID + direction * (ik_deg - mid_deg)


def servo_of(leg, hip_deg, femur_deg, tibia_deg, lo, hi):
    """返回 (舵机角三元组, 是否被限幅)"""
    raw = [
        map_joint(hip_deg, HIP_MID[leg], DIR_HIP[leg]),
        map_joint(femur_deg, FEMUR_MID, DIR_FEMUR[leg]),
        map_joint(tibia_deg, TIBIA_MID, DIR_TIBIA[leg]),
    ]
    out = [min(hi, max(lo, v)) for v in raw]
    cut = [abs(a - b) > 1e-9 for a, b in zip(raw, out)]
    return out, cut


def main():
    ap = argparse.ArgumentParser(description="腿长 / 工作空间 / 舵机角度自查")
    ap.add_argument("--l1", type=float, default=22.3, help="髋水平偏移 mm（默认取固件值）")
    ap.add_argument("--l2", type=float, default=57.9, help="大腿长度 mm")
    ap.add_argument("--l3", type=float, default=110.0, help="小腿长度 mm")
    ap.add_argument("--stride", type=float, default=30.0,
                    help="步态扫掠用的步幅 mm（<=0 跳过扫掠）")
    ap.add_argument("--lift", type=float, default=20.0, help="抬腿高度 mm")
    ap.add_argument("--min", type=float, default=15.0, help="软限位下限（度）")
    ap.add_argument("--max", type=float, default=165.0, help="软限位上限（度）")
    args = ap.parse_args()

    l1, l2, l3 = args.l1, args.l2, args.l3
    lo, hi = args.min, args.max

    print("=" * 78)
    print("腿长：LEG_L1 = %.2f  LEG_L2 = %.2f  LEG_L3 = %.2f mm" % (l1, l2, l3))
    print("工作空间（肩轴到足端直线距离 D）：%.2f ~ %.2f mm"
          % (abs(l2 - l3), l2 + l3))
    print("软限位：%.1f ~ %.1f 度" % (lo, hi))
    print("=" * 78)

    # ------------------------------------------------ home 姿态
    print("\n[1] home 姿态（LEG_HOME 六条腿）")
    print("-" * 78)
    print("%-8s %-18s %8s %9s %9s %9s %9s   %s"
          % ("腿", "足端 (x,y,z)", "D", "hip_ik", "fem_ik", "tib_ik",
             "舵机 髋/股/膝", "备注"))
    bad = 0
    for i in range(6):
        x, y, z = HOME[i]
        hip, fem, tib, D, K, clamped = ik(l1, l2, l3, x, y, z)
        sv, cut = servo_of(i, hip, fem, tib, lo, hi)
        note = "D 越界已投影" if clamped else ""
        if any(cut):
            note = (note + " 舵机限幅!").strip()
            bad += 1
        print("%-8s (%5.1f,%5.1f,%5.1f) %8.2f %9.2f %9.2f %9.2f   %5.1f/%5.1f/%5.1f   %s"
              % (LEG_NAMES[i], x, y, z, D, hip, fem, tib, sv[0], sv[1], sv[2], note))
    print("-" * 78)
    print("说明：servo 90° = 装配中位（髋指正侧方 / 大腿水平 / 小腿垂直）")

    # ------------------------------------------------ 步态包络
    if args.stride > 0.0:
        env = [[1e9, -1e9] for _ in range(3)]
        d_lo, d_hi = 1e9, -1e9
        hits = []
        phases = 20
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
                    hip, fem, tib, D, K, _ = ik(l1, l2, l3, x, y, z)
                    sv, cut = servo_of(i, hip, fem, tib, lo, hi)
                    for j in range(3):
                        env[j][0] = min(env[j][0], sv[j])
                        env[j][1] = max(env[j][1], sv[j])
                        if cut[j]:
                            hits.append((stride, k, LEG_NAMES[i], j, sv[j]))
                    d_lo, d_hi = min(d_lo, D), max(d_hi, D)

        print("\n[2] 步态包络（stride = ±%.1f mm，lift = %.1f mm，%d 帧）"
              % (args.stride, args.lift, phases))
        print("-" * 78)
        names = ["髋", "股", "膝"]
        for j in range(3):
            print("  %s servo：%7.2f ~ %7.2f 度" % (names[j], env[j][0], env[j][1]))
        print("  D 范围：%7.2f ~ %7.2f mm（工作空间 %.2f ~ %.2f）"
              % (d_lo, d_hi, abs(l2 - l3), l2 + l3))
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
