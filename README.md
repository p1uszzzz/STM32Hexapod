# STM32 六足机器人（Hexapod）

基于 STM32F103 + 双 PCA9685 + 18 路 MG90S 舵机的六足机器人项目。
开发环境：STM32CubeMX + VSCode + CMake + ARM GCC。

## 硬件清单

| 部件 | 规格 | 数量 |
|---|---|---|
| 主控 | STM32F103xB | 1 |
| 舵机驱动 | PCA9685（16 路 PWM，I2C） | 2 |
| 舵机 | MG90S 金属齿轮 | 18 |
| 电源 | 5V 10A 开关电源 | 1 |
| 滤波 | 3300uF 电解 + 104 陶瓷电容 | 2 × 1 + 2 × 3 |
| 结构 | 开源六足 3D 打印模型（3 舵机/腿） | 1 套 |

## 软件架构

- STM32CubeMX 生成初始化代码（`.ioc` 已纳入版本管理）
- CMake 构建，VSCode + Cortex-Debug 调试
- HAL 库，I2C1 / I2C2 各接一块 PCA9685

### 目录结构

```text
.
├── Core/
│   └── Inc/                 # CubeMX 生成的头文件
│       ├── pca9685.h        # 寄存器定义、地址、函数声明
│   └── Src/
│       ├── main.c           # 主程序 + I2C 扫描测试
│       ├── pca9685.c        # PCA9685 驱动
│       
├── Drivers/                 # HAL 库
├── cmake/                   # CMake 构建脚本
├── docs/
│   └── 函数使用教程.md       # 每个函数的参数/单位/示例/调参速查（先看这个）
├── tools/
│   ├── ik_check.py          # 纯 Python 的腿长/工作空间/角度自检
│   └── lab/                 # 实验室便携构建 + 烧录脚本
└── STM32Hexapod.ioc         # CubeMX 工程文件
```

> 📖 **第一次上手 / 要改代码，先读 [docs/函数使用教程.md](docs/函数使用教程.md)**：
> 三层结构、坐标系约定、`inverseKinematics()` / `Gait_*` / `PCA9685_*` 的参数与单位、
> 可抄的实战例子、调参速查表、报错排查表。

## PCA9685 地址说明（重要）

| 板子 | I2C 总线 | 7 位地址 | 8 位写地址 | 说明 |
|---|---|---|---|---|
| 板 1 | I2C1 | `0x40` | `0x80` | A0~A5 全接地，标准地址 |
| 板 2 | I2C2 | `0x60` | `0xC0` | **A0 误焊至 A5**，地址非标准 |

> ⚠️ 注意：第二块板因焊接时 A0 误接到 A5，地址变为 `0x60`（而非预期的 `0x41`）。
> 代码中 `PCA9685_ADDR_2` 定义为 `0xC0`。
> 如需恢复标准地址，将 A0 改焊回 A0 拉高焊盘，并把宏改回 `0x82`。

## 通道分配

18 个舵机分两条 I2C 总线控制：

| 逻辑通道 | 板子 | 板内通道 | 关节 |
|---|---|---|---|
| 0~8 | 板 1 | 0~8 | 腿 1~3 的髋/股/膝 |
| 9~17 | 板 2 | 0~8 | 腿 4~6 的髋/股/膝 |

具体映射（规律：`逻辑通道 = (腿号-1)*3 + 关节`，关节 0=髋 1=股 2=膝；见 `Core/Src/gait.c` 的 `CH_HIP/CH_FEMUR/CH_TIBIA`）：

| 腿号 | 位置 | 髋 | 股 | 膝 |
|---|---|---|---|---|
| 1 | 前左 | 0 | 1 | 2 |
| 2 | 中左 | 3 | 4 | 5 |
| 3 | 后左 | 6 | 7 | 8 |
| 4 | 前右 | 9 | 10 | 11 |
| 5 | 中右 | 12 | 13 | 14 |
| 6 | 后右 | 15 | 16 | 17 |

## 构建与烧录

```bash
# 配置（首次）
cmake -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake

# 编译
cmake --build build/Debug

# 生成 .hex / .bin
arm-none-eabi-objcopy -O ihex   build/Debug/STM32Hexapod.elf build/Debug/STM32Hexapod.hex
arm-none-eabi-objcopy -O binary build/Debug/STM32Hexapod.elf build/Debug/STM32Hexapod.bin
```

一键脚本（Windows，会自动找 STM32 扩展自带的 gcc / ninja / cmake）：

```bat
tools\lab\build.bat     :: 编译 + 生成 hex/bin
tools\lab\flash.bat     :: SWD + ST-LINK 烧录
tools\lab\flash.bat COM4  :: 串口烧录（BOOT0=1）
tools\lab\flash.bat DFU   :: USB DFU 烧录（BOOT0=1）
```

实验室便携环境（U 盘免安装）见 [tools/lab/README-lab.md](tools/lab/README-lab.md)：
`prepare_usb.bat` 打包工程+工具链+固件，`prepare_vscode_usb.bat` 把 VSCode 做成 U 盘便携版。

### 在 VSCode 里编译 / 烧录

`.vscode/tasks.json` 已配好，**不依赖 CMake Tools 或 STM32 扩展**（它们依赖本机 shim，换电脑容易坏）：

| 操作 | 快捷键 |
|---|---|
| 编译固件 | `Ctrl+Shift+B` |
| 烧录 / 清理 / 制作 U 盘 / 自检 | `Ctrl+Shift+P` → `Tasks: Run Task` |

## 自查工具

```bash
python tools/ik_check.py     # 腿长 / 工作空间 / 舵机角度 / 步态包络，全部算一遍
```

改了 `LEG_L1/L2/L3` 或 `LEG_HOME` 之后先跑它，能在不上机的情况下看出哪一路会打到软限位。

