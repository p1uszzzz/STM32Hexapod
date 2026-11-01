# 实验室便携环境（U 盘）使用说明

目标：**在家里编译好，把 U 盘插到实验室电脑上就能烧录**；如果现场要改参数重编译，U 盘里也带了完整工具链。

> 结论：**可以完全用 U 盘做移动环境**，因为这些工具都是"绿色"的（复制即用，不用安装）：
> `C:\Users\P1uszzzz\AppData\Local\stm32cube\bundles` 里已经有 **gcc 14.3.1 / ninja / cmake / STM32_Programmer_CLI 2.23.0**。
> 唯一可能要管理员权限的是 **USB 驱动**（见第 5 节），工具本身不用装。

---

## 1. U 盘目录结构（照这个摆，脚本会自动找）

```text
U盘\
├─ STM32Hexapod\                     ← 整个仓库（含 build\Debug 的产物）
│   ├─ build\Debug\STM32Hexapod.elf   ← 烧录用（.hex / .bin 也会生成）
│   └─ tools\lab\build.bat  flash.bat
├─ bundles\                          ← 直接复制 AppData 里那个 bundles 文件夹
│   ├─ gnu-tools-for-stm32\14.3.1+st.2\bin\arm-none-eabi-gcc.exe
│   ├─ ninja\1.13.2+st.1\bin\ninja.exe
│   ├─ cmake\<版本>\bin\cmake.exe
│   └─ programmer\2.23.0\bin\STM32_Programmer_CLI.exe
└─ drivers\                          ← 驱动安装包（可选，见第 5 节）
```

`tools\lab\build.bat` 的查找顺序是：**系统 PATH → `U盘\bundles` → `%LOCALAPPDATA%\stm32cube\bundles`**，
所以在家跑和插 U 盘跑用的是同一个脚本，不用改路径。

**复制命令**（在家里的资源管理器地址栏或终端里执行一次）：

```bat
robocopy "C:\Users\P1uszzzz\AppData\Local\stm32cube\bundles" "E:\bundles" /E /NFL /NDL
robocopy "D:\STM32\STM32Hexapod" "E:\STM32Hexapod" /E /XD build\.git
```

（`E:` 换成你的 U 盘盘符；`/XD build\.git` 是排除中间产物和 .git，想连版本记录一起带就删掉 `/XD` 那段。）

---

## 2. 家里先编译（生成要烧的文件）

在仓库根目录双击或命令行运行：

```bat
tools\lab\build.bat
```

它会：配置 CMake → 编译 → 用 objcopy 生成 `.hex` / `.bin` → 打印 flash/RAM 占用。
当前工程占用约 **7.6 KB / 64 KB FLASH**，空间很富余。

---

## 3. 实验室烧录：三种方式，按手里的硬件选一种

| 方式 | 需要的硬件 | 接线 | 命令 | 备注 |
|---|---|---|---|---|
| **SWD + ST-LINK（推荐）** | ST-LINK V2 或 V2.1 | SWDIO→PA13、SWCLK→PA14、GND、3.3V | `tools\lab\flash.bat` | 最稳，能调试能读回，不用动 BOOT0 |
| 串口 UART | USB-TTL（CH340/CP2102） | TTL_TX→PA10、TTL_RX→PA9、GND、3.3V 交叉对接 | `tools\lab\flash.bat COM4` | 需先 **BOOT0=1** 再按复位，烧完拨回 0 |
| USB DFU | 只要一根 USB 线 | STM32 的 USB 口（PA11/PA12） | `tools\lab\flash.bat DFU` | 需 **BOOT0=1** 再复位；驱动可能要装 |

> 板子没有板载调试器时，**串口或 DFU 是最省事的**：不用额外买 ST-LINK，但每次要拨 BOOT0。
> 有 ST-LINK 就用 SWD，一次接线，后面反复烧都不用动跳线。

烧录成功命令行会打印 `File download complete` / `Success`；失败见第 6 节排错。

---

## 4. 烧完第一次上电（重要安全步骤）

`main.c` 现在的上电流程是：**18 路依次摆到 90°（装配中位）→ 延时 500ms → `Gait_Init()` 摆到 home 姿态**。

1. **先不给舵机供电**，只插 USB：确认板子能跑、串口/I2C 正常。
2. 接上 5V/10A 舵机电源（**和单片机共地**），把机器人**架空**（四条腿悬空或整机侧放）。
3. 观察 18 个舵机依次到 90°：按《舵机接线与安装手册》§7 检查（髋指正侧方、大腿水平、小腿垂直）。
4. 再观察摆到 home 姿态：六条腿应基本对称、没有顶死、没有"哒哒"堵转声。
5. 有异常立刻断电：
   - 某条腿**转反** → 改 `Core/Src/gait.c` 里 `DIR_HIP / DIR_FEMUR / DIR_TIBIA` 对应格为 `-1`
   - 某关节**固定偏几度** → 改 `SERVO_TRIM[腿号*3+0/1/2]`
   - 六条腿髋**整体**偏同一角度 → 改 `HIP_MID_DEG`
6. 改完 `tools\lab\build.bat` 重新编译 → `flash.bat` 再烧。

> 想只验证中位、不要 home 姿态：把 `main.c` 里 `Gait_Init();` 注释掉。

---

## 5. 驱动（唯一可能需要管理员权限的东西）

| 设备 | 驱动 | 说明 |
|---|---|---|
| ST-LINK V2 / V2.1 | STSW-LINK009（ST-Link USB driver） | 装过一次，换 U 盘不用重装 |
| CH340 / CP2102 | CH341SER.EXE / CP210x VCP | 串口烧录必需；Win10/11 有时能自动装 |
| USB DFU | STM32CubeProgrammer 自带 DFU 驱动 | 装 CubeProgrammer 时勾选 |

**如果实验室电脑不能装驱动**：
- 优先找实验室里**已经装过** STM32 环境的机器；
- 或选 **DFU**（Windows 10/11 对 STM32 BOOTLOADER 有时会自动识别）；
- 最稳的办法：**带自己的笔记本**过去，U 盘环境直接可用。

---

## 6. 排错

| 现象 | 原因 / 处理 |
|---|---|
| `No ST-LINK detected` | 驱动没装 / USB 线是充电线 / ST-LINK 没识别到；换口换线 |
| `Error: No debug probe` 且板子发烫 | 3.3V 接反或短路，立即断电 |
| 串口烧录 `Unable to connect` | BOOT0 没置 1 或没按复位；TX/RX 没交叉；波特率不是 115200 |
| 烧录成功但程序不跑 | 串口/DFU 方式忘了把 **BOOT0 拨回 0** 再复位 |
| 烧录卡在 `Waiting for the target` | 上次开了读保护 → `-ob RDP=0xAA` 全片擦除再烧 |
| 只有一块 PCA9685 有反应 | 板 1 是 0x40、板 2 是 **0x60**（A0 误焊到 A5），确认 `PCA9685_ADDR_2 = 0xC0` |
| 舵机抽动 / 全乱 | 5V 电源功率不够或没共地；MG90S 18 个堵转电流很猛，先架空测 |
| `I2C` 扫不到设备 | SDA/SCL 接反、上拉缺失、两块板要各自独立总线（I2C1 / I2C2） |

---

## 7. 现场改参数速查（不用 IDE）

用记事本打开 `Core\Src\gait.c`：

- `LEG_HOME`：六条腿的站姿足端坐标 `{x 前, y 左, z 上}`
- `HIP_MID_DEG`：舵机 90° 时腿指向正侧方（左 +90°、右 −90°）
- `DIR_HIP/FEMUR/TIBIA`：每条腿每个关节是否反向（±1）
- `SERVO_TRIM[18]`：18 路微调角度
- `SERVO_MIN_DEG / MAX_DEG`：软限位（默认 15 / 165）

腿长在 `Core\Inc\leg_ik.h` 的 `LEG_L1/LEG_L2/LEG_L3`（当前 22.3 / 57.9 / 110.0）。

改完可以先用 PC 侧工具核对，**不用上机**：

```bat
python tools\ik_check.py
```
