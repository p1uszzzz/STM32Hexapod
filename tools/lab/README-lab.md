# 实验室便携环境（U 盘）使用说明

目标：**在家里编译好，把 U 盘插到实验室电脑上就能烧录**；如果现场要改参数重编译，U 盘里也带了完整工具链。

> 结论：**可以完全用 U 盘做移动环境**，因为这些工具都是"绿色"的（复制即用，不用安装）：
> `C:\Users\P1uszzzz\AppData\Local\stm32cube\bundles` 里已经有 **gcc 14.3.1 / ninja / cmake / STM32_Programmer_CLI 2.23.0**。
> 唯一要装的是 **USB 驱动**（管理员权限装一次，见第 5 节），工具本身不用装。

---

## 0. 全新电脑（出厂状态，什么软件都没有）——三步走

**在家做完 ①②，到实验室只做 ③。**

**① 编译固件**

```bat
tools\lab\build.bat
```

**② 做 U 盘（一键）**

```bat
tools\lab\prepare_usb.bat
```

它会问你 U 盘盘符，然后自动拷贝：仓库 + 编译好的固件 + 免安装工具链（gcc/ninja/cmake/programmer）+ `drivers\` 里的驱动安装包。
**做之前先把驱动下载好放进 `tools\lab\drivers\`**，见该目录下的《下载清单.md》——新电脑唯一装不上的就是驱动。

**③ 到实验室，新电脑上按顺序做**

| 步骤 | 操作 | 判断成功 |
|---|---|---|
| 1 | 把 U 盘里 `STM32Hexapod` 和 `bundles` 两个文件夹拷到本地（比如 `D:\`），U 盘直跑慢且容易中途拔掉 | 两个文件夹都在本地 |
| 2 | 装驱动：U 盘 `drivers\` 里对应你调试器的那一个，右键以管理员身份运行 | 设备管理器里没有黄色感叹号 |
| 3 | 插上 ST-LINK / USB-TTL，双击 `tools\lab\check_devices.bat` | 第 1 节出现 `ST-Link` 或 `CH340` 且状态 OK；第 3 节列出 ST-LINK 序列号 |
| 4 | 双击 `tools\lab\flash.bat`（串口用 `flash.bat COM4`，DFU 用 `flash.bat DFU`） | 打印 `File download complete` / `Success` |
| 5 | 先不接舵机电源，只插 USB 看板子能否运行；再上舵机电源、机器人架空 | 18 路依次到 90°，再摆到 home 姿态 |

> 如果实验室电脑**没有管理员权限**：找管理员装一次驱动，或借用已装过 STM32 工具的电脑，或带自己的笔记本（最稳）。

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
├─ VSCode\                           ← 可选的便携版编辑器（见第 8 节）
│   ├─ Code.exe
│   └─ data\                         ← 有这个文件夹 = 便携模式
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

---

## 8. 在实验室改代码：U 盘里的便携版 VSCode

**结论：可以。** VSCode 官方支持 **便携模式（Portable Mode）**：只要 `Code.exe` 同级目录下
存在一个 `data\` 文件夹，所有配置、扩展、缓存就都写进这个 `data\`，
不碰注册表、不碰 `%APPDATA%`，换电脑插上就能用，**不需要安装、不需要管理员权限**。

先分清一件事：**编译和烧录根本不需要 VSCode**。
`tools\lab\build.bat` 只用 `bundles\` 里的 cmake + ninja + arm-none-eabi-gcc；
VSCode 只是"改代码的编辑器 + 快捷键外壳"。所以三条路线可以只选一条：

| 路线 | 要装什么 | U 盘占用 | 适合场景 |
|---|---|---|---|
| **A. 便携版 VSCode**（推荐） | 不用装 | ~500 MB | 要改代码、要跳转/补全/看报错 |
| B. 只改几个参数 | 不用 | 0 | 改 `LEG_HOME` / `SERVO_TRIM` / 限幅，用记事本即可（见第 7 节） |
| C. 只烧录 | 不用 | 0 | 家里已经编译好，现场只 `flash.bat` |

### 8.1 一键把家里的 VSCode 做成便携版

```bat
tools\lab\prepare_vscode_usb.bat
```

问你要 U 盘盘符，然后自动做四件事：

1. 找到本机已装的 VSCode（`%LOCALAPPDATA%\Programs\Microsoft VS Code`，
   就是普通的用户级安装，**本来就不需要管理员权限**）
2. 整个目录 robocopy 到 `U盘\VSCode\`
3. 建 `U盘\VSCode\data\`（这一步就是"打开便携模式"的开关）
4. 把本机扩展 `%USERPROFILE%\.vscode\extensions` 和用户设置复制进 `data\`

完成后结构：

```text
VSCode\
├─ Code.exe              ← 双击这个启动
└─ data\
    ├─ extensions\       ← 扩展（C/C++、中文语言包等）
    └─ user-data\        ← 你的设置、快捷键、最近打开
```

> 也可以走官网路线：下载 `Windows x64 (zip)` 版，解压到 U 盘后**手动建一个空 `data` 文件夹**，
> 效果完全一样。官网 zip 是最干净的便携版来源。

### 8.2 实验室里怎么用

| 步骤 | 操作 |
|---|---|
| 1 | 双击 `U盘\VSCode\Code.exe`（不用安装，第一次启动比平时慢几秒） |
| 2 | 文件 → 打开文件夹 → 选 `STM32Hexapod`（**建议先把工程拷到本地磁盘**，见 8.3） |
| 3 | **Ctrl+Shift+B** 编译（等价于跑 `tools\lab\build.bat`，报错会带文件/行号，点击可跳转） |
| 4 | Ctrl+Shift+P → `Tasks: Run Task` → 选「② 烧录：SWD / 串口 / DFU」 |
| 5 | 编译一次后 `build\Debug\compile_commands.json` 才生成，之后 C/C++ 的跳转/补全才准 |

任务列表在 `.vscode\tasks.json` 里，全部直接调用 `tools\lab\*.bat`，
**不依赖 CMake Tools / STM32 扩展**，所以便携环境下一定能用。

### 8.3 四个坑，提前避开

| 坑 | 说明 / 处理 |
|---|---|
| `data` 放错位置 | 必须和 `Code.exe` **同级**；放到别处就等于普通安装版，配置会写到用户目录 |
| ST 官方扩展报错 | `stm32-vscode-extension` / `stm32cube-ide-*` 依赖本机的 `cube-cmake`、`cube`、`starm-clangd` shim 和 `CUBE_BUNDLE_PATH`。便携环境里找不到 → 在扩展面板里对它们选「禁用（工作区）」，编译用 Ctrl+Shift+B |
| U 盘直跑很卡 | 编辑器索引、编译中间文件在 U 盘上都很慢。**工具放 U 盘、工程拷到本地磁盘**是标准做法 |
| 便携版不自动更新 | 要升级就在家里重跑一次 `prepare_vscode_usb.bat` |

### 8.4 如果实验室禁止插 U 盘

- 最稳：带自己的笔记本（U 盘环境直接可用）；
- 其次：只带编译好的 `STM32Hexapod.hex`，用实验室电脑上**已有的** CubeProgrammer / STM32CubeIDE 烧；
- 提前问清楚机房的 USB 策略，比到现场发现插不上强。



## 9. 关于这些 .bat 脚本的编码（改动前必看）

这批脚本是 **GBK（ANSI）+ CRLF 换行** 保存的，原因是 `cmd.exe` 只按当前代码页
（简体中文系统 = 936）解析批处理文件：如果存成 UTF-8，中文会被拆成半个字节，
导致命令被截断，出现 `'ist' 不是内部或外部命令`、`此时不应有 ST-LINK。` 这类报错。

因此：

- **不要用 VSCode 直接保存这些 .bat**（VSCode 默认按 UTF-8 存盘，一存就又坏了）。
  非改不可时：右下角编码 -> `Save with Encoding` -> 选 `GBK`，并保持 CRLF。
- 更推荐改 `README-lab.md` / `docs/函数使用教程.md` / C 源码，别动 .bat。
- git 里看到这些文件 "binary files differ" 是正常的（GBK 字节不是合法 UTF-8）。
- `build.bat` 会自动找 cmake / ninja / arm-none-eabi-gcc（顺序：系统 PATH ->
  U 盘 `bundles\` -> 本机 `%LOCALAPPDATA%\stm32cube\bundles`），找不到会明确报错。
