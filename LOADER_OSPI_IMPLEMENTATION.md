# PN 1.8 FSU 实施说明与 API使用说明

## 1. 文档范围与结论

本文描述 PN 1.8 工程在 RZ/N2L RSK 上适配 MX25UW51245G 的现有实现，包含代码位置、启动原理、内部函数参数、使用约束、下载配置及验证结果。

实现目标是：

> Boot ROM 保持单线 SPI 加载 Loader；Loader 使用 8D-8D-8D 搬运 App；搬运完成后恢复单线 SPI，供原 PN Flash/NVdata 驱动使用。

本方案不是将 PN 运行时 Flash 驱动整体替换为 OSPI 驱动，也没有改变 Flash 中的镜像字节布局。文中的 API 是现有函数的使用说明，不代表新增了面向应用的公共接口。

验证基线为 `Debug_App1_STANDARD`。本文中的地址、段大小和代码占用是已测试镜像的记录，重新构建后应以实际 map/ELF 为准。

**最新性能实测结论（2026-10-21）：**在硬件预取开启的基础上，将 Flash 源地址改为可缓存映射并增加源缓存维护后，四段纯搬运由 23.478912 ms 降至 **11.111600 ms**；整个搬运函数（包含两次源缓存维护及 SPI 交接校验）由 23.508072 ms 降至 **11.732880 ms**。时钟初始化完成后至调用 `main()` 前约为 **91.83396 ms**。当前源码和板卡均保留“预取＋可缓存源”版本，详见第 12.7 节。第 12.4–12.6 节保留各轮历史结果。上述时间不包含 Boot ROM 和 Loader 时钟初始化，不能作为完整 reset→main 或 PN FSU 时间。

## 2. 硬件前提：CN8 选择两颗不同芯片

RSK 原理图 D017892_04 第 12 页显示，CN8 的公共端为 OQFLASH_CS0：

| CN8 | 选中的器件 | 用途 |
|---|---|---|
| 1–2 | IC19，MX25UW51245G，64MB | 本方案使用的 OSPI Flash，可在 SPI 和 DTR OPI 间切换 |
| 2–3 | IC21，MX25UR51245GMI00，64MB | 原 QSPI Flash，不能执行本方案的八线协议 |

**下载、启动和 PN 运行均应保持 CN8 1–2，不采用带电切换跳线的测试方式。**


## 3. 文件与改动清单

路径以当前 IAR 工程目录为基准。

| 文件 | 改动/作用 |
|---|---|
| [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c) | 包含配置头；新增 Loader 私有 xSPI 辅助函数、诊断变量；扩展原 App 搬运函数 |
| [loader_xspi_cfg.h](rzn_cfg/driver/loader_xspi_cfg.h) | 新增八线使能、搬运预取、可缓存源、启动计时开关和轮询次数上限 |
| [LOADER_OSPI.txt](LOADER_OSPI.txt) | 实现说明及板测记录 |
| [startup.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c) | 增加可关闭的 GSC 启动计时；开启时将计数器初始化前移到时钟初始化之后，记录搬运前后及调用 main 前的时间戳 |
| [fsp_xspi0_boot.icf](script/fsp_xspi0_boot.icf) | 未修改；原有规则已经将启动系统对象纳入 BTCM Loader |
| [运行时 QSPI 驱动](src/fsp_mod/r_xspi_qspi.c) | 未因 Loader 加速而修改 |
| [运行时 Flash 配置](rzn_gen/hal_data.c) | 保持原单线 SPI 配置 |

核心符号导航：下表所有符号及行号均对应 [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c) 文件（行号仅用于当前版本定位）。

| 符号 | 当前行号 | 用途 |
|---|---:|---|
| `g_bsp_loader_ospi_stage` / `g_bsp_loader_ospi_error` | 691 / 692 | 阶段、错误诊断 |
| `g_bsp_loader_ospi_copy_bmcfg` | 693 | 搬运阶段 BMCFG 快照，bit 16 为预取使能 |
| `g_bsp_loader_ospi_copy_source` | 694 | 最后一个非空段的最后一次 word 读取地址 |
| `g_bsp_loader_ospi_copy_sctlr` | 695 | 搬运前 SCTLR 快照，bit 2 为 D-cache 使能 |
| `bsp_loader_ospi_fail()` | 697 | 记录错误并停止 |
| `bsp_loader_ospi_wait()` | 707 | 有限次数轮询 |
| `bsp_loader_ospi_command()` | 720 | 直接命令发送 |
| `bsp_loader_ospi_write_cr2()` | 741 | SPI/OPI 下写 CR2 |
| `bsp_loader_ospi_pin()` | 754 | 配置单个引脚 |
| `bsp_loader_ospi_pins_init()` | 766 | 配置 Loader 所需八线引脚 |
| `bsp_loader_ospi_invalidate_source()` | 787 | 按 cache line 失效只读 Flash 源范围 |
| `bsp_loader_ospi_invalidate_sources()` | 812 | 维护四个链接段的源缓存 |
| `bsp_loader_copy_from_flash()` | 829 | 精确长度 DTR 搬运 |
| `bsp_copy_to_ram()` | 886 | 原入口；组织切换、四段搬运和恢复 |

## 4. 启动原理与代码执行位置

```text
Boot ROM
  └─ 1S-1S-1S 加载 Loader 到 BTCM
       └─ SystemInit()
            ├─ 时钟初始化
            ├─ GSC 清零并启动（启动计时开启时）
            ├─ Loader data 初始化、BSS 清零
            ├─ 内存保护/cache 属性设置
            ├─ bsp_copy_to_ram()
            │    ├─ 保存 SPI 状态及向量字基准
            │    ├─ 初始化八线引脚
            │    ├─ Flash → DTR OPI
            │    ├─ xSPI0 → 8D-8D-8D
            │    ├─ 校验配置和映射读
            │    ├─ 启用硬件预取（预取开关开启时）
            │    ├─ VECTOR / USER_PRG / USER_DATA / USER_DATA_NONCACHE
            │    ├─ Flash → SPI
            │    └─ xSPI0 恢复原配置并验证
            ├─ App BSS 清零
            ├─ 静态构造、POST_C 等初始化
            └─ main → PN → spi_flash_init() → NVdata
```

其中，“保存 SPI 状态及向量字基准”是指：保存 SPI 配置，并读取向量表首个 32 位字，作为协议切换前后的读数校验基准。切换到八线 DTR 后，将同一地址的读数经 `__REV16()` 修正（详见7.3）后与基准比较；恢复 SPI 后，再读取并与基准比较。这是协议切换的快速读数检查，不是整个固件的完整性校验。

Boot ROM 的启动参数、校验及 `0x6000` 字节 Loader 传输长度保持不变。已测布局中，Loader 从 Flash `0x6000004C` 加载到 BTCM `0x00102000`。

所有新增硬件控制和搬运函数位于已经纳入 Loader 的启动系统对象中。在 Flash 和控制器协议不一致的短暂窗口，CPU 从 BTCM 取指，不需要调用尚未搬运的 App、动态内存分配或 C 库复制函数。

原八线版本已测 Loader 代码占用为 `0x15E4` 字节；加入启动计时后为 `0x1638` 字节，进一步加入四段纯搬运计时后为 `0x166C` 字节；仅开启硬件预取的历史版本为 `0x16A8` 字节。当前“预取＋可缓存源”版本为 `0x17A8` 字节，仍小于原 `0x6000` 字节传输预算。维护时仍应检查实际 Loader 镜像布局，不能只检查单个函数大小。

## 5. 配置与启用方式

现有配置：

```c
#ifndef BSP_CFG_LOADER_OSPI_ENABLE
#define BSP_CFG_LOADER_OSPI_ENABLE (1)
#endif

#ifndef BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE
#define BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE (1)
#endif

#ifndef BSP_CFG_LOADER_OSPI_CACHE_ENABLE
#define BSP_CFG_LOADER_OSPI_CACHE_ENABLE (1)
#endif

#ifndef BSP_CFG_LOADER_BOOT_TIMING_ENABLE
#define BSP_CFG_LOADER_BOOT_TIMING_ENABLE (1)
#endif

#define BSP_CFG_LOADER_OSPI_POLL_LIMIT (1000000U)
```

| 配置 | 意义 |
|---|---|
| `BSP_CFG_LOADER_OSPI_ENABLE=1` | 启用本方案 |
| `BSP_CFG_LOADER_OSPI_ENABLE=0` | 搬运调用回到原 `bsp_copy_multibyte()`，用于回退或 A/B 测量 |
| `BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE=1` | 当前默认；只在 DTR 映射校验通过后、四段搬运期间开启控制器预取，恢复 SPI 前关闭并清空缓冲 |
| `BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE=0` | 关闭本次预取优化，用于八线预取开关 A/B；不关闭八线协议本身 |
| `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=1` | 当前默认；从 `0x600...` 可缓存源复制，在搬运前后维护四段源缓存，并校验恢复 SPI 后的可缓存读取 |
| `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=0` | 从 `0x400...` 非缓存别名复制，不执行新增源缓存维护；不关闭 CPU 全局 D-cache |
| `BSP_CFG_LOADER_BOOT_TIMING_ENABLE=1` | 当前默认；在 IAR xSPI0 Flash 启动配置中提前启动 GSC，记录启动时间戳 |
| `BSP_CFG_LOADER_BOOT_TIMING_ENABLE=0` | 关闭新增计时，恢复原来接近 main 时才初始化 GSC 的行为 |
| `BSP_CFG_LOADER_OSPI_POLL_LIMIT` | 每次等待的最大轮询次数，不是毫秒数 |

编译条件为非 RAM 执行，并同时满足：

```c
BSP_CFG_LOADER_OSPI_ENABLE && defined(__ICCARM__) && BSP_CFG_XSPI0_X1_BOOT
```

八线、预取、可缓存源和计时使能宏支持通过 IAR 编译器预定义覆盖；轮询上限当前直接定义在配置头中。预取和可缓存源开关只影响八线分支，不改变 CPU 全局缓存配置。可缓存源要求沿用当前 `0x600...` 的可缓存 MPU 属性和 D-cache 开启设置。计时开关独立于八线开关，A/B 测量都应开启计时，不能只对一版开启。

操作建议：

1. 确认 CN8 固定 1–2，目标器件为 MX25UW51245G。
2. 确认 IAR 使用适配烧录器。
3. 修改配置后重新编译、下载，不能只修改头文件后直接复位旧固件。
4. 在原 QSPI 芯片上测试时必须关闭八线加速。



## 6. SPI → DTR → SPI 的实施步骤

### 6.1 保存状态并建立 SPI 基准

入口设置 `stage=1`、`error=0`，执行屏障并等待内存访问/写缓冲空闲。

保存的控制器状态：

| 寄存器 | 保存目的 |
|---|---|
| `LIOCFGCS[0]` | 协议与片选时序 |
| `WRAPCFG` | DS shift 等 wrapper 设置 |
| `BMCFG` | 映射桥/预取配置 |
| `BMCTL0` | AHB 桥访问权限 |
| `CDCTL0` | 直接命令控制 |
| `CSa[0].CMCFG0/1/2` | 命令长度、地址长度及映射读写命令 |

通过非缓存别名读取向量首字，保存为 `expected`。随后检查控制器当前协议为 SPI。

该基准仅用于确认协议切换前后映射读取一致，**不是 CRC、签名或全镜像完整性检查**。

### 6.2 提前准备引脚并隔离映射访问

配置 P14.4–7、P15.0–7 到所需外设功能，采用安全区域、高驱动和快速 slew 设置。

然后关闭预取、等待访问结束、禁止 CS0 桥访问，并清除预取缓冲，防止协议切换期间发生映射访问。

GPIO 写保护状态会恢复，但本函数配置的引脚复用/驱动状态不是逐项保存后回滚；后续常规引脚初始化仍按原启动流程执行。

### 6.3 配置 Flash，再配置控制器

先在 SPI 下读取并保存：

- CR2 地址 `0x00000000`：协议模式；
- CR2 地址 `0x00000300`：dummy-cycle 配置。

确认 Flash 为 SPI 后：

1. CR2 dummy 字段设为 `(saved_dc & ~7U) | 5U`，对应 10 个内存读 dummy clocks。
2. CR2 模式写为 `saved_mode | 2U`，使能 DTR OPI。
3. 控制器设置为与 Flash 匹配的 8D-8D-8D。

| 参数 | 当前值 |
|---|---|
| 协议 PRTMD | `0x3FF` |
| DS shift | `8` |
| LIOCFGCS0 | `(6U << 16) | 0x3FFU` |
| 映射命令长度 | 2 字节 |
| 映射地址长度 | 4 字节 |
| 映射读命令 | `0xEE11` |
| 映射写命令 | `0x12ED`，只配置，Loader 不执行数据编程 |
| 内存读 dummy | 10 clocks |
| DTR CR2 读 dummy | **4 clocks** |
| xSPI0 时钟 | 沿用原配置，已测约 100MHz；本次不提频 |

随后用 DTR 命令读回模式和 dummy 配置。寄存器值正确后恢复桥访问，再比较经过字节顺序转换的 DTR 向量字与 SPI 基准。可缓存源开启时，先失效四段只读 Flash 源缓存，避免命中旧 SPI 数据。预取开关开启时，再等待 `MEMACC=0`、清除预取缓冲、设置 `BMCFG.PREEN=1` 并执行 `DSB`。随后记录搬运阶段 BMCFG 和 SCTLR，设 `stage=2`，开始搬运。

### 6.4 按原链接段搬运 App

段地址和大小仍来自链接器符号，不硬编码为单个大镜像：

| 段 | 内容 | 已测大小 |
|---|---|---:|
| VECTOR | 异常向量 | 68 字节 |
| USER_PRG | App 程序 | 1,169,712 字节 |
| USER_DATA | 初始化数据 | 77,068 字节 |
| USER_DATA_NONCACHE | 非缓存初始化数据 | 0 字节 |
| 合计 | | 1,246,848 字节 |

### 6.5 搬运完成后恢复

设置 `stage=3`。预取开关开启时先关闭 `BMCFG.PREEN` 并执行 `DSB`；然后等待映射访问结束、禁止 CS0 桥访问，再清空预取缓冲并执行屏障，避免带着 DTR 预取数据切回 SPI。手册规定 `PBUFCLR` 不应在 `COMSTT.MEMACC=1` 时设置，因此必须先等待空闲。

可缓存源开启时，在恢复 Flash SPI 之前再次失效四段源缓存，移除 DTR 阶段缓存的字节对交换数据；这不是丢弃目标 RAM 的缓存。

恢复顺序：

1. **仍使用 DTR 命令**把 Flash 模式恢复为原 SPI 模式。
2. 恢复控制器协议、wrapper 和映射命令设置。
3. 使用 SPI 命令恢复原 Flash dummy 配置。
4. 使用 SPI 读回 CR2 并核对。
5. 恢复直接命令控制、清预取缓冲、恢复桥及预取设置。
6. 执行 `DSB/ISB`。
7. 用 SPI 再读向量字，确认与初始值一致。
8. 可缓存源开启且实际搬运过数据时，由 CPU 比较最后一个源 word 的可缓存/非缓存 SPI 读取结果；不一致仍报错误 8。
9. 设置 `stage=4`，返回原启动流程。

只恢复控制器或只恢复 Flash 都不够，两端协议必须一致。正常恢复不擦写 Flash 用户数据，也不会迁移另一颗芯片上的 NVdata。

## 7. 精确长度搬运与字节顺序

### 7.1 地址别名

接口接收原 Flash 映射地址 `0x60000000–0x63FFFFFF`。当前 `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=1` 保留该地址，使用已有 MPU 配置允许的 CPU D-cache；搬运前后由外层函数失效四段源缓存。

设为 0 时，内部减去 `0x20000000`，使用非缓存别名 `0x40000000–0x43FFFFFF`，避免命中旧 SPI cache 内容。控制器预取缓冲与 CPU D-cache 是两种不同机制，必须分别维护。非缓存别名不等同于通用的全部 cache 清理操作。

### 7.2 对齐和尾部

DTR OPI 起始总线地址要求为偶数，本实现进一步保证所有显式 CPU Flash load 都是自然对齐的 32 位读取。可缓存路径缺失时，实际外部访问由 cache line 填充产生，不能再将每次 CPU load 等同于一笔独立 4 字节 Flash 事务。

- 源、目标均 4 字节对齐且剩余至少 4 字节：word 快速路径。
- 其他情况：读取包含目标字节的对齐 word，再提取所需字节写入 RAM。
- 尾部仅写实际剩余长度，不向上补写。
- 边缘 word、cache line 填充和硬件预取可能读取请求段之外的相邻 Flash 字节，RAM 仍只写请求长度。显式 word 的 64MB 边界已做范围保护；整颗 Flash 末端的真实 cache/预取行为尚未上板验收。

因此段长度不必为 2 或 4 的倍数；现有链接脚本已对一般段做 4 字节对齐，向量段为 32 字节对齐，有利于快速路径。

### 7.3 相邻字节交换

在当前硬件/协议组合下实测：

```text
SPI word：       0xE59FF020
DTR 原始 word：  0x9FE520F0
__REV16 后：     0xE59FF020
```

使用 `__REV16()` 的原因是：同一份按 SPI 方式烧录的镜像，在当前 MX25UW51245G 与 xSPI0 的八线 DTR 配置下，存储器映射读数相对于 SPI 读数呈现每个 16 位半字内两个字节互换的排列。这里修正的是读取结果的字节排列，不是修改 Flash 中的内容，也不是把 CPU 的大小端模式改掉。上述现象已由板测确认；不应仅凭这个读数现象推断其一定由某个采样沿或控制器内部环节导致。

以低地址到高地址的字节顺序表示，上述例子为：

```text
镜像所需字节：       20 F0 9F E5
DTR 原始 word 对应： F0 20 E5 9F
__REV16() 修正后：   20 F0 9F E5
```

所以 DTR probe、word 搬运及尾部处理均先执行 CMSIS `__REV16()`，交换每个半字内的相邻字节，不是颠倒整个 32 位 word。若不修正，DTR probe 会与 SPI 基准不一致；若跳过检查直接搬运，写入 RAM 的代码和数据也会排列错误。尾部字节必须在修正后的 word 中提取，才能保证非 4 字节整数长度的段也复制正确。

该转换是针对 SPI 编程镜像在当前 DTR 路径下的读取结果，不应推广到任意 Flash/控制器。恢复 SPI 后不做此转换；烧录器也不预先交换镜像字节。

## 8. 内部 API 使用说明

除原启动入口外，以下辅助函数均为文件内 `static`，不面向 App 或 RTOS 并发调用。不要通过移除 `static` 将其直接用作 PN 运行时 Flash API。

### 8.1 `bsp_copy_to_ram()`

```c
void bsp_copy_to_ram(void);
```

- 调用方：原 `SystemInit()`，在 Loader data/BSS 和内存属性初始化之后调用一次。
- 输入：链接器给出的四组源、目标、长度；无显式参数。
- 正常返回：四段搬运完成，两端恢复 SPI，`stage=4/error=0`。
- 失败：进入不可恢复错误处理，不返回成功状态。
- 限制：不是运行时“重新加载 App”接口；App 启动后重复调用会覆盖代码和数据。

### 8.2 `bsp_loader_ospi_command()`

```c
static uint32_t bsp_loader_ospi_command(
    uint32_t command, uint32_t command_bytes,
    uint32_t address, uint32_t address_bytes,
    uint32_t data, uint32_t data_bytes,
    uint32_t dummy, bool write);
```

| 参数 | 使用含义 |
|---|---|
| `command` | 未移位的命令，例如 `0x71`、`0x718E` |
| `command_bytes` | 当前调用使用 1 或 2；函数据此选择命令写入位置 |
| `address` | Flash 芯片内部地址/寄存器地址，**不是 CPU 映射地址** |
| `address_bytes` | 当前调用使用 0 或 4 |
| `data` | 发送数据；读取时传 0 |
| `data_bytes` | 本方案使用 0、1、2，取决于具体命令 |
| `dummy` | 对应命令要求的 dummy clocks |
| `write` | `true` 表示写方向，`false` 表示读方向 |

返回值为 `CDD0` 数据寄存器，不是错误码。超时由内部等待函数处理。

内部依次等待命令接口空闲、清完成标志、设置 `CDT/CDA/CDD0`、触发 `TRREQ`、等待完成、读结果并清标志。

实际调用示例：

```c
/* SPI 下读取模式寄存器；此时控制器和 Flash 必须均为 SPI。 */
uint32_t mode = bsp_loader_ospi_command(
    0x71U, 1U, 0U, 4U, 0U, 1U, 0U, false) & 0xFFU;

/* DTR 下读 CR2；仅可在两端完成 DTR 切换后使用。 */
uint32_t mode_dtr = bsp_loader_ospi_command(
    0x718EU, 2U, 0U, 4U, 0U, 2U, 4U, false);
```

约束：

- 当前函数仅实现本启动流程需要的紧凑命令操作，不能当成任意长度传输 API。
- 参数没有通用合法性检查，调用方必须保证字段值、方向、时序和协议正确。
- 不自动加写使能，不自动做数据字节顺序转换。
- 直接访问与映射桥访问必须由上层流程协调。

### 8.3 `bsp_loader_ospi_write_cr2()`

```c
static void bsp_loader_ospi_write_cr2(
    bool octal, uint32_t address, uint32_t value);
```

- `octal=false`：当前两端处于 SPI，发送 `0x06` 和 `0x72`。
- `octal=true`：当前两端处于 DTR OPI，发送 `0x06F9` 和 `0x728D`。
- `address`：CR2 地址；本方案使用 `0` 和 `0x300`。
- `value`：单字节配置值；函数按 1 字节发送。
- 返回值：无。

先写使能，再写 CR2，随后通过 volatile 延时循环满足 `tW2V >= 40ns`。若修改 CPU 时钟或延时实现，需重新核对时序。

函数不读回校验、不更新控制器协议；调用方负责后续两端同步及 CR2 检查。

### 8.4 `bsp_loader_ospi_wait()`

```c
static void bsp_loader_ospi_wait(
    volatile const uint32_t * reg, uint32_t mask, uint32_t value);
```

等待条件为：

```c
(*reg & mask) == value
```

- `reg`：有效的硬件寄存器地址。
- `mask`：需检查的位。
- `value`：掩码后的期望值；调用方应保证不包含掩码之外的位。
- 成功返回 `void`；超过配置次数调用 `bsp_loader_ospi_fail(1U)`。

不提供 RTOS 延时或调度，也不是按真实时间计量的超时接口。

### 8.5 `bsp_loader_ospi_pin()` / `bsp_loader_ospi_pins_init()`

```c
static void bsp_loader_ospi_pin(uint32_t port, uint32_t pin);
static void bsp_loader_ospi_pins_init(void);
```

单引脚函数设置安全区域、DRCTL、PFC0 和 PMC；只用于已经确定的 xSPI 引脚，不做通用端口/引脚合法性检查。

批量函数保存 GPIO 写保护状态、临时解锁、配置固定的两个引脚组、恢复写保护并执行屏障。无需应用生成的引脚表，不应在 PN 运行中反复调用。

### 8.6 `bsp_loader_copy_from_flash()`

```c
static void bsp_loader_copy_from_flash(
    uintptr_t * src, uintptr_t * dst, uintptr_t bytesize);
```

| 参数 | 合约 |
|---|---|
| `src` | 原 Flash 映射地址，传 `0x600...` 地址，不传非缓存别名 |
| `dst` | 可写 RAM，至少容纳 `bytesize` 字节 |
| `bytesize` | **字节数**，不是 `uintptr_t` 元素个数 |

调用前必须已经完成八线配置、映射检查并允许桥访问。函数自身不切换协议。

行为：

- 检查源范围及长度，越界报错误 2。
- 支持任意起点/目标对齐及任意长度，目标只写指定字节。
- 零长度不访问 Flash；器件末尾后一地址仅在零长度时允许。
- 使用对齐 word 读取并 `__REV16()`，最后执行 `DSB`。
- 按可缓存源开关选择原映射或非缓存别名；函数不自行维护源缓存，必须由外层搬运流程配对处理。
- 非空段更新 `g_bsp_loader_ospi_copy_source`，记录最后一个对齐源 word 地址；零长度不覆盖上一非空段的记录。
- 不验证目标地址合法性、容量或源目标重叠；这些由链接布局和调用方保证。
- 禁用八线功能时，此名称通过宏映射回原 `bsp_copy_multibyte()`；不要将八线分支的精确尾部合约推定为原函数也具备。

调用形式（原流程已具备，不需要在 App 中再加一次）：

```c
src  = (uintptr_t *) BSP_PRV_SECTION_USER_PRG_ROM_ADDRESS;
dst  = (uintptr_t *) BSP_PRV_SECTION_USER_PRG_RAM_START;
size = (uintptr_t) BSP_PRV_SECTION_USER_PRG_RAM_END -
       (uintptr_t) BSP_PRV_SECTION_USER_PRG_RAM_START;
bsp_loader_copy_from_flash(src, dst, size);
```

#### 8.6.1 实现步骤与设计原因

该函数是 Loader 的同步搬运函数，不使用 DMA。执行流程为：

```text
检查 Flash 源地址和长度
  → 按开关保留可缓存地址，或转换为非缓存映射地址
  → 非空段记录最后一个对齐源 word 地址
  → 循环搬运：
       源和目标均 4 字节对齐，且剩余至少 4 字节 → word 快速路径
       否则 → 对齐读取一个 word，提取所需字节后逐字节写 RAM
  → DSB 屏障
```

**1. 将参数转换为字节地址**

```c
uintptr_t source = (uintptr_t) src;
uint8_t * destination = (uint8_t *) dst;
```

`source` 用整数表示，便于地址掩码、对齐和范围计算；`destination` 使用字节指针，便于精确处理非对齐地址和尾部。虽然入口类型是 `uintptr_t *`，但 `bytesize` 始终按字节计数，不能按 word 数量传入。

**2. 检查源区间**

源数据必须落在 `[0x60000000, 0x64000000)`，即 64MB Flash 映射范围内。代码先检查起点，再检查：

```c
bytesize > 0x64000000U - source
```

用“剩余容量”比较而非直接计算 `source + bytesize`，避免地址加法溢出。C 语言 `||` 短路求值保证起点越界时不会继续执行后面的减法检查。错误时进入 `bsp_loader_ospi_fail(2U)`，不静默截短或继续启动。

零长度仍须通过源地址检查，但不会读取 Flash；`source == 0x64000000` 只允许零长度。

**3. 按开关选择源映射**

```c
#if !BSP_CFG_LOADER_OSPI_CACHE_ENABLE
source -= 0x20000000U;
#endif
```

默认保留 `0x60010000`，允许整条 cache line 填充及后续命中；外层流程负责清掉旧 SPI/DTR 源缓存。关闭开关时，例如将 `0x60010000` 转为同一 Flash 的 `0x40010000` 非缓存别名。`volatile const uint32_t *` 表示按易失性语义读取该地址；`volatile` 本身不负责绕过 CPU 缓存。

**4. 满足条件时走 4 字节快速路径**

```c
if (((source | (uintptr_t) destination) & 3U) == 0U && bytesize >= 4U)
{
    *(uint32_t *) destination = __REV16(*(volatile const uint32_t *) source);
    source += 4U;
    destination += 4U;
    bytesize -= 4U;
}
```

把两个地址按位或后检查低两位，相当于同时要求源和目标都按 4 字节对齐。每次从 Flash 读取一个对齐的 32 位 word，经 `__REV16()` 修正当前 DTR 路径下每半字内的字节交换，再以一个 word 写入 RAM。

这里的“32 位读取”是 CPU 对存储器映射窗口的访问粒度，不代表 Flash 改为 32 根数据线；外部总线仍由 xSPI0 按已经配置好的 8D-8D-8D 协议访问。

**5. 非对齐或不足 4 字节时，读取对齐 word 后提取**

```c
uint32_t value = __REV16(*(volatile const uint32_t *) (source & ~(uintptr_t) 3U));
uint32_t count = 4U - (source & 3U);
if (count > bytesize)
{
    count = bytesize;
}
value >>= (source & 3U) * 8U;
```

- `source & ~3`：找到包含当前源字节的对齐 word 地址，显式 CPU load 仍是对齐 word。
- `source & 3`：当前源字节在这个 word 内的偏移。
- `count`：本 word 从该偏移到末尾可用的字节数，再限制为不超过剩余长度。
- 先 `__REV16()` 再右移：先恢复正确排列，然后把需要的首字节移动到最低 8 位。

随后每次将 `value` 的低 8 位写到 RAM，右移 8 位，直到写完 `count` 字节，并同步更新源地址和剩余长度。这样不会对非对齐目标执行 word 写，也不会因尾部不足 4 字节而多写 RAM。

如果源和目标的对齐偏移不同，逐字节路径处理后也不一定能进入 word 快速路径；函数优先保证正确性，而不是假设所有非对齐输入都能转为 word 搬运。

**6. 搬运结束执行 `__DSB()`**

结束时执行数据同步屏障，确保前面的显式内存访问按屏障要求完成后再继续后续启动流程。它不等同于通用的 D-cache 清理或 I-cache 失效操作，也不执行数据完整性校验。

#### 8.6.2 非对齐与尾部处理示例

假设源地址为 `0x60010001`，目标地址为 `0x00120001`，长度为 6 字节。下表以可缓存源开关关闭为例：

| 次数 | Flash 实际读取地址（非缓存别名） | 修正后选取的 word 内字节偏移 | RAM 写入地址 | 写入字节数 |
|---|---|---|---|---:|
| 第一次 | `0x40010000` | 1、2、3 | `0x00120001` 至 `0x00120003` | 3 |
| 第二次 | `0x40010004` | 0、1、2 | `0x00120004` 至 `0x00120006` | 3 |

结果：CPU 显式读取两个对齐 word，但 RAM 恰好写入请求的 6 字节，`0x00120000` 和 `0x00120007` 不会被写入。开关开启时对应读取地址是 `0x60010000/0x60010004`，显式读取及 RAM 写入的逻辑相同，但真实总线流量还取决于 cache line 和控制器预取。

这说明“读取需要对齐”和“段长度必须对齐”是两回事：该函数通过读取包含边缘字节的完整 word，再精确提取，实现任意长度搬运。显式 word 不超出整颗 Flash；cache/预取可能产生额外读取，参见第 7.2 节限制。它只适用于这里的普通 Flash 存储区，不应直接用于有读取副作用的外设寄存器。

### 8.7 `bsp_loader_ospi_fail()`

```c
static void bsp_loader_ospi_fail(uint32_t error);
```

写入全局错误变量，调用 `BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0)`，然后保持在停止路径。不会自动恢复 Flash 模式，不会回退为 SPI 后继续启动。

若断点设在函数入口，错误变量尚未赋值，可观察 ARM 调用参数寄存器 `R0`；不能因变量仍为 0 就判断无故障。

### 8.8 源缓存维护函数

```c
static void bsp_loader_ospi_invalidate_source(uintptr_t source, uintptr_t bytesize);
static void bsp_loader_ospi_invalidate_sources(void);
```

仅在 `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=1` 时编译，位于同一个 BTCM Loader 对象中，不调用尚未搬运的 App 缓存函数。

- `source` 是 `0x600...` 的 Flash 源地址，`bytesize` 是字节数；零长度不执行维护，越界报错误 2。
- 沿用 BSP 按地址维护的做法，通过 `CTR.DminLine` 动态取得 cache line 大小：`4U << ((__get_CTR() >> 16) & 0xFU)`。
- 起点按 cache line 向下对齐，逐行执行 CMSIS `__set_DCIVAC()`，前后加 `DSB`；覆盖末尾所在 cache line。
- 无参数版本依次维护 VECTOR、USER_PRG、USER_DATA、USER_DATA_NONCACHE 的 Flash 源范围，不维护整颗 64MB Flash，也不失效整个 D-cache。
- 外层在 DTR 校验后、纯搬运采样前调用一次；搬运结束、关闭预取并隔离桥访问后，在恢复 SPI 前再调用一次。
- 这些源只读，不存在应保留的脏 Flash 源数据；不能把此函数直接用于可能含脏数据的 RAM，否则 invalidate 可能丢失尚未回写的数据。
- 函数可能覆盖同属一条 cache line 的相邻 Flash 字节；不会失效目标 RAM 的脏缓存。没有替代 xSPI `PBUFCLR`，也没有全局关闭 CPU cache。

## 9. 诊断变量与错误表

```c
volatile uint32_t g_bsp_loader_ospi_stage;
volatile uint32_t g_bsp_loader_ospi_error;
volatile uint32_t g_bsp_loader_ospi_copy_bmcfg;
volatile uint32_t g_bsp_loader_ospi_copy_source;
volatile uint32_t g_bsp_loader_ospi_copy_sctlr;
```

这些变量为 Loader BSS 数据。未开始运行时可能是 0；它们不是跨复位保留的故障日志。后三者分别记录搬运阶段 BMCFG、最后一个非空段的末 word 地址和搬运前 SCTLR。SCTLR 的 D-cache 位为 1 并不表示所有地址都可缓存，仍须结合源地址和 MPU 属性判断。

| stage | 含义 |
|---:|---|
| 1 | 准备、切换并检查 DTR |
| 2 | DTR 检查通过，执行搬运 |
| 3 | 搬运完成，恢复 SPI |
| 4 | SPI 恢复检查通过 |

| error | 含义 |
|---:|---|
| 0 | 无已记录错误 |
| 1 | 硬件轮询超限 |
| 2 | Flash 源范围非法 |
| 3 | 入口控制器非 SPI |
| 4 | 入口 Flash 模式非 SPI |
| 5 | DTR CR2 读回不一致 |
| 6 | DTR 映射 probe 不一致 |
| 7 | 恢复后 CR2 不一致 |
| 8 | 恢复后 SPI 向量 probe 不一致，或可缓存源末 word 与非缓存 SPI 读数不一致 |

诊断时同时记录 stage、error、PC 和控制器协议。若在 DTR 阶段失败，不要直接运行 SPI 烧录器并假设 Flash 已恢复；需先恢复协议或断电重上电。

## 10. 与 PN 运行时 Flash/NVdata 的关系

Loader 不打开或修改 `g_qspi0_ctrl`。PN 后续仍运行原 `spi_flash_init()` 和 SPI 读写路径，配置为：

- 协议 `1S-1S-1S`；
- 3 字节地址；
- 读取 `0x03`；
- 编程 `0x02`；
- 写使能 `0x06`；
- 状态读取 `0x05`；
- 4KB 擦除 `0x20`。

函数名中的 QSPI 不代表当前运行时实际启用四线。正常启动交接要求 Flash 本身已回到 SPI，不能认为驱动 Open 会替代 Flash 协议恢复。

| NV 镜像 | 非缓存 CPU 地址 | Flash 内偏移 |
|---|---|---|
| SEC1 | `0x403F0000` | `0x003F0000` |
| SEC2 | `0x403F4000` | `0x003F4000` |

两个偏移位于 3 字节地址范围内，不需要为了这两个 NV 镜像将运行时驱动改成 4 字节地址。不要据此推断当前 3 字节配置可直接访问整颗 64MB 的所有区域。

NVdata 原路径为：

```text
Bsp_nv_data_store()
  → OsFlashProgram() 排队
  → Task_OsFlashPost()
  → SPI 擦除 / 编程 / 校验
  → 保存完成回调

启动：
Bsp_nv_data_init()
  → OsFlashRead()
  → 检查版本和 checksum
  → 采用第一副本 / 第二副本 / 默认值
```

这些运行时函数不属于本次新增 Loader API。

## 11. IAR Flash Loader 配置

注意区分：

- **本文的启动 Loader**：产品固件的一部分，上电搬运 App。
- **IAR Flash Loader**：调试器使用的烧录算法，不随 PN 正常启动执行。

已将当前使用的烧录器及全部配套文件原样备份到本地 Git 仓库根目录的 [tools](tools)：

| 文件 | 作用 |
|---|---|
| [MX25UW51245G.board](tools/MX25UW51245G.board) | 下载入口，引用 `.flash` 并限制下载范围 |
| [MX25UW51245G.flash](tools/MX25UW51245G.flash) | Flash 器件描述，引用算法和初始化宏 |
| [MX25UW51245G.out](tools/MX25UW51245G.out) | 已适配、板测使用的烧录算法二进制 |
| [MX25UW51245G.mac](tools/MX25UW51245G.mac) | C-SPY SPI 总线和引脚初始化宏 |

四个文件均与当前 IAR 安装目录中的版本通过 SHA-256 一致性校验。此目录是可随工程携带的安装文件包，不是重新构建烧录算法的源码包；仍需已安装的 IAR Embedded Workbench for Arm / C-SPY 和目标调试器环境。

在其他电脑使用时，将以上四个文件一起复制到对应 IAR 安装的 `arm\config\flashloader\Renesas` 目录（可能需要管理员权限），再按下文选择安装后的 board 文件。文件保留原 `$TOOLKIT_DIR$` 引用，直接选择备份目录中的 board 不会自动使依赖改为从备份目录加载。此次归档未修改 IAR 安装文件或工程调试器选项。

适配烧录器已安装到：

```text
C:\Program Files\IAR Systems\Embedded Workbench 9.2_2\arm\config\flashloader\Renesas\
    MX25UW51245G.board
    MX25UW51245G.flash
    MX25UW51245G.out
    MX25UW51245G.mac
```

依赖关系：

```text
MX25UW51245G.board
  └─ MX25UW51245G.flash
       ├─ MX25UW51245G.out
       └─ MX25UW51245G.mac
```

配置使用 `$TOOLKIT_DIR$` 引用，无会话目录依赖。原厂下载器未覆盖；算法由原 SPI 版本复制适配，期望 ID 从 `0x001A35C2` 改为板测的 `0x003A81C2`，此表示按读出寄存器中的字节打包。

在 IAR 中选择：

1. `Project → Options → Debugger → Download`。
2. 启用 `Use flash loader(s)`。
3. 启用 `Override default .board file`。
4. 选择标准目录中的 `MX25UW51245G.board`。
5. CN8 保持 1–2，重新编译并下载。

安装标准目录文件不等于工程选项已自动切换；需检查实际所选 board。

当前 board 限制下载区间为 `0x60000000–0x60152FFF`，是已测固件范围，不是整颗 Flash 下载配置。固件增长或布局变化时应同步检查范围；本范围不覆盖两个 NV 槽位。

烧录算法使用 SPI。若在 DTR 阶段异常停止，应先恢复 SPI 或完整断电重上电，不能通过把 CN8 改到另一颗芯片来“恢复”同一份镜像。

## 12. 验证结果与证据边界

### 12.1 软件验证

- IAR `Debug_App1_STANDARD` 编译、链接通过，无错误和警告。
- 其余三个配置曾完成修改源文件的独立编译；未完成其完整链接。
- 关闭八线开关的分支曾独立编译通过。
- 最终含字节交换修正的 ARM 机器码模拟，通过 4,800 组对齐、长度及边界哨兵用例，以及零长度、非法范围、器件末尾测试。
- 模型模拟两端协议、命令和寄存器恢复，完整比较四段 1,246,848 字节。

模拟验证不能替代物理引脚、时序裕量、真实 Flash 或 FSU 性能验收。

### 12.2 Loader 板测

CN8 固定 1–2：

- SPI 基准向量 `0xE59FF020`；
- Loader 达到 `stage=4/error=0`；
- 恢复后 `LIOCFGCS0=0x70000`、`CMCFG0=8`、`CMCFG1=0x300`、`WRAPCFG=0`；
- RAM 导出经过 Intel HEX 校验和及范围检查；
- 四段所有有效字节与 ELF 一致；
- 后续进入 main 和原 SPI 初始化；
- 驱动初始化后协议为 0（SPI），向量单字节读取正确。

### 12.3 NVdata 板测

未修改运行时代码；测试前完整备份两个 16KB 槽位，共 32KB。

- 通过原 COM3 控制台触发临时站名 `ospi-nv-cn8-12` 的保存。
- 两份 NV 镜像均完成擦除、编程、校验；实际镜像长度 8,569 字节，版本 `0x0411`。
- 两副本内容一致，checksum `0xFFFF936A` 正确，全部字节与 RAM 镜像一致。
- 硬件复位后 RAM中的NVdata 与保存镜像一致，Flash 未被重写。
- 镜像以外的槽位数据保持不变。
- 测试结束恢复了原始完整 32KB 数据，逐字节一致；固件区也确认未改变。
- 最后恢复原配置无断点启动，串口输出 `SYSTEM STARTUP FINISHED, OK`。

链接符号分配有对齐填充，NVdata 的分配大小可能为 8,572 字节。checksum 必须按实际结构长度 8,569 字节计算，不能把尾部填充算进去。

<span style="color: red;"><strong>未通过项：</strong>在线改站名同时出现 PN fatal：<code style="color: inherit;">PackId 55, ModId 4, Line 197, Device 1</code>；无调试断点时仍复现。对应 LSAS 中 OHA 完成状态非成功的检查，拒绝原因尚未定位。不能把 Flash 持久化通过报告为完整 PN 在线改名功能通过，也不能无证据归因于 CN8。</span>

### 12.4 GSC 启动时间 A/B 板测（2026-10-21，预取优化前）

#### 测量范围和计时实现

原 `SystemInit()` 在 App 搬运、BSS 清零、构造和 Master MPU 初始化之后才调用 `bsp_global_system_counter_init()`，所以原样在 main 中读取 GSC 会漏掉 App 搬运。

本次在 [startup.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c) 中加入受 `BSP_CFG_LOADER_BOOT_TIMING_ENABLE` 控制的计时：

1. 在 `bsp_clock_init()` 返回后调用原 `bsp_global_system_counter_init()`，清零并启动 GSC；此时尚未初始化 Loader BSS，不向时间戳全局变量写入数据。
2. Loader data/BSS 和内存属性初始化后，在 `bsp_copy_to_ram()` 前后分别保存计数。
3. 跳过原先接近 main 时的 GSC 清零，避免丢失前面累计的时间。
4. 在调用 `main()` 前保存最后一个计数；测量终点严格说是 main 调用前的采样点，尚有少量存储和分支指令开销。

用于整段启动计时的三个 `volatile uint64_t` 变量均属于 startup 对象的 Loader BSS，可在调试器中观察：

| 变量 | 含义 |
|---|---|
| `g_bsp_loader_timing_copy_begin` | 调用搬运函数之前的累计 tick |
| `g_bsp_loader_timing_copy_end` | 搬运函数返回之后的累计 tick |
| `g_bsp_loader_timing_main` | 调用 main 之前的累计 tick |
| `g_bsp_loader_timing_ospi` | 测量固件的八线开关值，0 为单线、1 为八线；此变量为 `uint32_t` |

内部 `bsp_loader_boot_ticks()` 按“高 32 位 → 低 32 位 → 高 32 位”读取 GSC，若高位不同则重读，避免低位溢出时拼接出错误的 64 位数。测量窗口内不打印、不设中途停机断点。

当前工程 `BSP_GLOBAL_SYSTEM_COUNTER_CLOCK_HZ` 为 25,000,000 Hz，即每 tick 为 40 ns：

```text
搬运函数耗时(ms) = (copy_end - copy_begin) / 25000
至 main 的耗时(ms) = main / 25000
```

`CNTFID0` 用于声明频率，并不是写入该寄存器就能任意改变实际计数时钟；换算需与实际平台时钟保持一致。

**起点不是物理 reset。**该窗口不包含复位释放、Boot ROM、Boot ROM 的单线 Loader 加载，以及 Loader 入口到 `bsp_clock_init()` 返回之前的执行时间。若要完整 reset→main，应使用复位信号与 main GPIO 标记配合示波器/逻辑分析仪测量；不能把本节的时间直接标为 reset→main。

计时开启也会使 PN 接手时的 GSC 已包含这段启动时间，而不是原先接近 main 才从零开始。需要保持原计数起点时，关闭计时开关并重新构建。

#### A/B 条件和实测结果

- `Debug_App1_STANDARD`，CN8 始终为 1–2，同一颗 IC19。
- 两版均开启计时，只切换 `BSP_CFG_LOADER_OSPI_ENABLE`，保留相同应用源代码和段布局；应用总搬运量均为 1,246,848 字节。
- 两版应用数据段、向量段逐字节相同；程序段存在 9 个不同字节，均已核实为指向相同 Loader 符号的重定位地址变化，不是应用功能代码改动，不能称两个 ELF 完全相同。
- 每版执行 5 次硬件复位测量。只在 BTCM 中的 `main()` 调用位置停止，读取此前已保存的时间戳；没有在 Flash 协议切换或搬运过程中暂停。
- 不把下载耗时、调试器连接/复位等待或主机宏执行时间算入 GSC 窗口。应用 main 处的软件断点会被启动搬运覆盖，因此测量使用不会被 App 搬运覆盖的 BTCM 调用位置。

| 项目 | 单线 1S-1S-1S | 八线 8D-8D-8D |
|---|---:|---:|
| `copy_begin`（tick） | 1,450–1,451 | 1,436 |
| `copy_end`（tick） | 2,740,415–2,740,416 | 2,968,269 |
| `copy_end - copy_begin`（tick） | 2,738,965 | 2,966,833 |
| `main`（tick） | 4,741,499 | 4,969,352 |
| 搬运函数耗时，5 次均值（ms） | 109.55860 | 118.67332 |
| 至 main 耗时，5 次均值（ms） | 189.65996 | 198.77408 |

5 次测量中，单线的搬运前后时间戳有 1 tick 的同步变化，搬运差值不变；两版各自的 main 时间戳均一致。最终恢复八线固件后的追加验证也得到相同的八线时间戳。

**该历史版本结论：关闭硬件预取时，八线至 main 慢 9.11412 ms，约 4.81%；搬运函数本身慢 9.11472 ms，约 8.32%。当时尚未达到预期的启动加速目标。**八线搬运函数耗时包含切换协议、引脚初始化、校验、数据复制及恢复 SPI，不是纯总线有效载荷传输时间。最新预取优化结果见第 12.6 节。

当前单线使用原映射地址和原复制函数；八线使用非缓存别名逐个 word 读取，并执行字节排列修正。两者的 CPU 访问和缓存/突发利用方式不同，不能根据总线位宽推断最终加速倍数。这些是后续性能分析方向，本次没有独立分解各项开销，不能把全部差值归因于 `__REV16()` 或某一个因素。

#### 验证与最终状态

- 单线计时、八线计时及关闭计时的八线分支，均完成主配置编译链接，0 错误、0 警告。
- 开启计时的八线版本，真实 ARM 机器码测试再次通过 4,800 组边界用例和四段协议模拟。
- 最终恢复八线计时版本，板上 `stage=4/error=0`、xSPI0 恢复 SPI；RAM 全部 1,246,848 个搬运字节与最终 ELF 相同。
- 测试前后两个 NVdata 槽位共 32KB 逐字节一致，未迁移或重写用户配置。
- 最后无计时断点启动，COM3 确认 `SYSTEM STARTUP FINISHED, OK`；调试器和 COM3 已释放，板卡保持运行。
- 本次未测真正断电冷启动、完整 reset→main 或 PN 通信就绪/FSU 达标时间。

### 12.5 四段纯搬运时间补充实测（2026-10-21，预取优化前）

针对原 [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c) 第 891–916 行所覆盖的四段搬运，增加内部时间戳，只测 VECTOR、USER_PRG、USER_DATA、USER_DATA_NONCACHE 的参数准备及四次复制调用，不包含 SPI→DTR 切换、CR2/映射探测和 DTR→SPI 恢复，也不包含后续 App BSS 清零。

该次插入计时代码后，四段搬运从当时第 903 行开始；源码行号会随维护变化，应以这四个段的实际调用为边界。

在原有计时开关下新增两个 `volatile uint64_t` 变量，仍位于 startup 对象的 Loader BSS：

| 变量 | 采样位置 |
|---|---|
| `g_bsp_loader_timing_payload_begin` | 四段搬运之前，当时 system.c 第 900 行；当前为第 963 行 |
| `g_bsp_loader_timing_payload_end` | 四段搬运完成之后、恢复 SPI 之前，当时 system.c 第 930 行；当前为第 993 行 |

复用 `bsp_loader_boot_ticks()`，将其从 startup 文件内的静态函数改为 startup/system 之间共享的内部函数，不作为 PN 运行时公共 API。开始采样前和结束采样前均执行 `DSB`；结束采样前的屏障保证将搬运写入的完成等待纳入窗口。

```text
纯搬运耗时(ms) = (payload_end - payload_begin) / 25000
```

这里“纯搬运”是这段 CPU 搬运代码的实际执行耗时，包含函数调用、循环、地址处理、DTR 字节排列修正、屏障及少量采样开销；不等同于仅在 Flash 引脚上统计有效数据传输的时间。

同一 IC19、CN8 1–2，两版各 5 次硬件复位，搬运总量 1,246,848 字节。窗口内没有停机断点，仍在 BTCM 的 main 调用点读取已保存的结果：

| 项目 | 单线 SPI | 八线 DTR |
|---|---:|---:|
| `payload_begin`（tick） | 1,485 | 1,926 |
| `payload_end`（tick） | 2,740,449 | 2,968,108 |
| 差值（tick） | 2,738,964 | 2,966,182 |
| 四段纯搬运，5 次均值（ms） | **109.55856** | **118.64728** |
| 有效搬运吞吐量（MB/s，按 10^6 字节） | 11.381 | 10.509 |
| 同一版固件的整个搬运函数耗时（ms） | 109.56120 | 118.67600 |
| 整个函数窗口减去纯搬运窗口（µs） | 2.64 | 28.72 |

两版各自 5 次的上述结果均一致。增加采样点会引入少量开销，因此本表重新测了整个函数，不能把第 12.4 节旧版整个函数时间减去本节新版纯搬运时间来分解开销。

**该历史版本结论：排除协议切换等外围操作后，关闭硬件预取的八线搬运仍比单线慢 9.08872 ms，约 8.30%。**该次八线函数窗口之外的差额只有约 28.72 µs，其中还包含采样和函数进出等开销，不能把这 28.72 µs 全部解释成协议切换时间。实测说明约 9 ms 的差距主要存在于搬运路径本身，而不是协议切换。后续仅启用预取即获得明显改善，见第 12.6 节。

后续应关注原 SPI 缓存映射读取与 DTR 非缓存逐 word 读取的访问效率、CPU 循环和突发利用；本次未改动复制算法，未独立分解各因素，不能直接认定单一根因。

加入纯搬运计时后，两版主配置均编译链接通过，0 错误/警告；八线机器码模拟再次通过 4,800 组边界用例及四段协议模型。最终已恢复八线计时版本，板上 `stage=4/error=0`、SPI 恢复正常，RAM 全部 1,246,848 字节与最终 ELF 一致，32KB NVdata 与测试前相同。最后无计时断点启动，COM3 确认 `SYSTEM STARTUP FINISHED, OK`，调试器和串口均已释放。

### 12.6 八线硬件预取开关 A/B 板测（2026-10-21，非缓存源历史版本）

#### 改动范围和安全顺序

本轮只验证“八线搬运期间开启 xSPI 硬件预取，保持非缓存复制”：

- 时钟仍为原 100 MHz 配置，内存读 dummy 仍为 10。
- 保留 `0x400...` 非缓存别名、`volatile` 对齐 word 读取、`__REV16()` 和精确尾部处理。
- 不调整编译优化级别，不引入 DMA，不改变 PN 运行时 SPI/NVdata 驱动。
- 新增 `BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE`，当前默认为 1；设为 0 可回到预取关闭的八线分支，修改后必须重新构建和下载。
- 新增 `g_bsp_loader_ospi_copy_bmcfg` 记录真实搬运阶段的 BMCFG；bit 16 为 `PREEN`。

[system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c) 当时第 896–902 行在 DTR 向量校验通过后启用预取；第 944–953 行在恢复 SPI 前关闭并清空缓冲。切换前原有的“关闭预取”操作仍保留，不能直接把它改成开启。当前已加入第 12.7 节的源缓存维护，行号以核心符号导航及当前源码为准。

```text
SPI 基准与原配置保存
  → 关闭预取、等待空闲、禁止映射访问、清缓冲
  → 配置并校验 DTR
  → 等待空闲、清缓冲、开启 PREEN
  → 保存搬运阶段 BMCFG，四段非缓存复制
  → 关闭 PREEN，等待 MEMACC=0，禁止映射访问，清缓冲
  → 恢复 Flash SPI、控制器和全部原配置
  → SPI 映射再校验
```

依据 RZ/N2L 硬件手册 Rev.1.10 第 2038 页的 `BMCFG.PREEN`、第 2043 页的 `BMCTL1.PBUFCLR` 和第 2077–2078 页的预取说明。预取缓冲是 FIFO，连续读取可命中已预取数据；它不是 CPU D-cache。清缓冲必须等待映射访问空闲。

#### 测量方法与数据

CN8 固定 1–2，同一 IC19，同一应用源代码/段布局，总搬运量仍为 1,246,848 字节。两版均为八线 DTR，仅切换预取开关，各执行 5 次硬件复位；计时窗口内无停机断点，时间戳仍在 BTCM 的 main 调用位置读出。

已对比两版复制函数机器码：148 字节函数仅有一处 PC 相对常量池位移不同，均解析为相同的源范围检查常量 `0x64000001`；搬运热循环指令相同。不能称两个 ELF 逐字节相同。

| 项目 | 八线预取关闭 | 八线预取开启 |
|---|---:|---:|
| 搬运阶段 BMCFG | `0x00000000` | `0x00010000` |
| 返回 main 前 BMCFG | `0x00000000` | `0x00000000` |
| `payload_begin`（tick） | 1,927 | 1,924 |
| `payload_end`（tick） | 2,968,109 | 588,895 |
| 纯搬运差值（tick） | 2,966,182 | 586,971 |
| 四段纯搬运，5 次均值（ms） | **118.64728** | **23.47884** |
| 整个搬运函数，5 次均值（ms） | 118.67608 | 23.50796 |
| 时钟初始化后至 main，5 次均值（ms） | 198.77720 | 103.608896 |
| 有效搬运吞吐量（MB/s，按 10^6 字节） | 10.509 | 53.105 |

两版各自 5 次的纯搬运计数差均一致。预取开启的 main 时间戳为 2,590,222–2,590,223 tick，即 103.60888–103.60892 ms；关闭时为 4,969,430 tick。公式仍为 `tick / 25000 = ms`。

**结论：只启用硬件预取，纯搬运缩短 95.16844 ms，耗时减少约 80.21%，有效搬运速度约为原来的 5.05 倍。至 main 的限定窗口缩短约 95.16830 ms。**该单因素实验表明，原先关闭控制器预取的非缓存读取路径是主要性能瓶颈之一；本轮没有使用引脚波形或 AHB trace 统计实际帧长，不能声称已直接观测了总线突发类型。

#### 数据正确性、交接及最终状态

- 两版主配置编译链接均为 0 错误、0 警告。
- 两版 ARM 机器码测试均通过 4,800 组对齐/长度/边界用例和四段协议模拟；新增了协议切换前关闭预取、手动命令前关闭预取、空闲时清缓冲、搬运阶段预取快照及最终寄存器恢复的断言。
- 模拟器不模拟真实预取 FIFO/电气行为，预取性能和真实数据正确性以本轮板测为依据。
- 最终预取开启版本 `stage=4/error=0`；八个保存的控制器寄存器逐项与进入 Loader 时一致：WRAPCFG、BMCFG、BMCTL0、CDCTL0、CMCFG0/1/2、LIOCFGCS0。
- RAM 向量 68 字节、程序 1,169,712 字节、初始化数据 77,068 字节，合计 1,246,848 字节，与最终 ELF 全部一致。
- 测试前完整备份固件区和两个 NVdata 槽位；测试后 32KB NVdata 逐字节不变，SHA-256 为 `dab19e04382ece2b533613eb758f8e2892504c586e7fd714477f9fa951f6ea02`。
- 最后无计时断点复位启动，COM3 显示采用第一份 NVdata，随后 `SYSTEM STARTUP FINISHED, OK`。未触发新的配置保存或在线改名。
- 该轮结束时源码、构建产物和板卡保留预取开启、非缓存源版本，Loader 代码占用 `0x16A8` 字节；随后升级到第 12.7 节版本。

本轮是两版 DTR 的预取开关比较，不依赖历史 SPI 基线；没有修改或验收此前提出的 SPI 普通 READ 指令/频率合规问题。测量不包含 Boot ROM、Loader 时钟初始化，也不是 PN 通信就绪时间。真正断电冷启动、DTR 中途复位、整颗 Flash 末端预取及温压裕量仍未系统验收。

### 12.7 预取＋可缓存源 A/B 板测（2026-10-21，当前版本）

#### 实施及测量边界

本轮两版均保留八线、硬件预取、100 MHz、10 dummy clocks、`__REV16()` 和原复制循环，不改变 MPU、CPU 全局 cache 开关、编译优化级别、PN 驱动或镜像字节布局。唯一功能开关为 `BSP_CFG_LOADER_OSPI_CACHE_ENABLE`：

- 0：从 `0x400...` 非缓存源读取，无新增源缓存维护。
- 1：从 `0x600...` 可缓存源读取，搬运前后各维护四段 Flash 源缓存，恢复 SPI 后增加可缓存/非缓存末 word 比较。

两版的 `SCTLR=0x30C5183D`，D-cache 位都为 1。因此“非缓存源”不等于 CPU 全局 D-cache 被关闭。搬运阶段 `BMCFG=0x00010000`，返回时均恢复为入口的 0。末次 word 源地址分别为 `0x40152D08/0x60152D08`。

源缓存维护按第 8.8 节实现；先清掉可能残留的 SPI 数据，搬运后再清掉 DTR 原始排列数据。恢复 SPI 后由 CPU 实际加载末 word 的可缓存/非缓存别名并比较，不是单靠调试器直读 Flash 判断交接。当前该 word 的 SPI 值为 `0x00302E30`，与其 DTR 原始排列不同。

`payload_begin/end` 当前位于 [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c) 第 963/993 行，排除两次源缓存维护。`copy_begin/end` 仍包围整个函数，包含两次维护、预取管理、协议切换、SPI 校验及新增的可缓存读取校验。CN8 固定 1–2，两个最终版本各测 5 次硬件复位，采样窗口内不停机；结果如下。

#### 最终 5 次结果

| 项目 | 预取＋非缓存源 | 预取＋可缓存源 |
|---|---:|---:|
| 四段纯搬运均值（ms） | **23.478912** | **11.111600** |
| 整个搬运函数均值（ms，包含新增缓存维护） | **23.508072** | **11.732880** |
| 时钟初始化完成后至 main 均值（ms） | 103.609712 | 91.833960 |
| 有效搬运吞吐量（十进制 MB/s） | 53.105 | 112.211 |
| 函数总耗时减纯搬运耗时（ms） | 0.029160 | 0.621280 |

原始 tick 记录，顺序为 `payload_begin / payload_end / copy_begin / copy_end / main`：

```text
非缓存，第 1–4 次：1942 / 588915 / 1455 / 589157 / 2590243
非缓存，第 5 次：  1942 / 588914 / 1455 / 589156 / 2590242
可缓存，第 1–5 次：9251 / 287041 / 1444 / 294766 / 2295849
```

25 MHz GSC，仍按 `tick / 25000 = ms` 换算。非缓存纯搬运范围为 23.47888–23.47892 ms，整个函数为 23.50804–23.50808 ms；可缓存版本 5 次计数均一致。

**结论：纯搬运再缩短 12.367312 ms，耗时下降约 52.67%，速度约为上一方案的 2.11 倍。计入新增维护和校验，整个函数仍缩短 11.775192 ms，耗时下降约 50.09%。**

外围窗口差额增加约 0.59212 ms，主要来自两次源缓存维护及新增交接校验；这是相减得到的合计开销，不是单独测得的某一个缓存 API 耗时。净收益约 11.78 ms，不应只报告纯复制节省的 12.37 ms。

本轮非缓存基线与第 12.6 节相差约几十纳秒至亚微秒，是新增源地址/SCTLR 诊断及代码布局变化后的重新实测，未混用旧 ELF 的时间。

#### 正确性验收及保留状态

- 两版主配置编译链接均 0 错误、0 警告；最终两版机器码测试均通过 4,800 组对齐/长度/边界用例及四段协议模型。
- 模型验证实际 CPU load 选中正确的源别名，并验证四段源维护在 stage 1/3 各调用一次。模型不模拟真实 cache line 填充、失效或硬件 FIFO，不能用模型替代本轮板测。
- 最终可缓存版本八个保存的控制器寄存器均恢复入口值；`stage=4/error=0`，SPI 非缓存向量和新增 CPU 可缓存末 word 校验均通过。
- 最终 RAM 的 VECTOR 68 字节、USER_PRG 1,169,712 字节、USER_DATA 77,068 字节，共 1,246,848 字节，全部与实际下载的 ELF 一致；NONCACHE 段为空。
- 下载前完整备份固件与 NVdata。测试后 32KB NVdata 逐字节不变，SHA-256 仍为 `dab19e04382ece2b533613eb758f8e2892504c586e7fd714477f9fa951f6ea02`；没有执行新的 NVdata 擦写或在线改名。
- 最后不设计时断点复位，串口确认采用第一份 NVdata，并输出 `SYSTEM STARTUP FINISHED, OK`。
- 当前配置为 OSPI、PREFETCH、CACHE、TIMING 均开启，源码、最终 ELF 和板卡保持一致。Loader 代码占用 `0x17A8`，预算仍为 `0x6000`；调试器与 COM3 已释放，板卡运行。

本轮只覆盖当前固件布局和两种“预取开启”的源映射方案，未验收关闭预取但开启可缓存源的组合。断电冷启动、DTR 中途复位、整颗 Flash 末端的真实 cache/预取行为和完整 PN 网络 FSU 仍需专项验证。未改动之前指出的普通 SPI READ 指令/频率问题。

## 13. 使用限制、排错及后续验收

| 现象/风险 | 检查方向 |
|---|---|
| CN8 1–2 默认 IDE 下载失败 | 确认选择适配 board，且 Flash 已处于 SPI |
| error 5 | CR2 命令、协议、地址和寄存器读 dummy |
| error 6 | 映射命令、Flash 内容、字节顺序、引脚、dummy、DS 时序 |
| error 7/8 | Flash 自身及控制器两端是否按正确顺序恢复，可缓存源是否在返回 SPI 前失效 |
| 更换频率/板卡/Flash 后异常 | 重新验证采样、dummy、延时和引脚电气参数，不套用已测结果 |
| DTR 过程中复位 | Flash 可能保留 DTR 而 Boot ROM 期望 SPI；需验证 Flash 硬复位/供电复位策略 |

推荐验收顺序：

1. CN8 固定 1–2，确认镜像实际下载到 IC19。
2. 确认阶段 4、错误 0，并验证完整搬运内容。
3. 验证 PN 启动、NVdata 读写及重启恢复。
4. <span style="color: red;">定位在线改名 OHA 错误，完成网络工具驱动的配置测试。</span>
5. 测试真正断电上电、正常运行复位及 DTR 中断复位。
6. 在第 12.7 节已验证的预取＋可缓存源版本上继续评估 CPU 搬运效率，再测完整 reset→main 及实际 PN FSU 终点。

截至本文记录，已有第 12.4–12.7 节限定边界的 GSC A/B 数据，预取和可缓存源均明显改善搬运，但**尚无完整 reset→main 数据及 PN FSU 达标结论，也未将用户观察到的断电启动代替系统性冷启动/异常复位验收**。不能按总线位宽直接宣称整个启动快八倍：ROM 加载、协议切换、CPU 搬运和 PN 初始化均有开销。

## 14. 维护检查清单

- 不把协议切换代码或其依赖移到尚未搬运的 App 区。
- 保持 Loader data/BSS、栈和执行地址在内部 RAM 中有效。
- 保留寄存器保存/恢复、两次 probe 和失败路径，不能为“能启动”而静默跳过校验。
- 开启预取时，保持协议切换前关闭预取、空闲时清缓冲的顺序；不能把 DTR 预取数据带入后续 SPI 访问。
- 可缓存源开启时，保持搬运前后成对的源缓存失效和恢复 SPI 后的 CPU 读数校验；不能对目标 RAM 随意做全局 invalidate。
- 改动段布局后核查源范围、目标容量、Loader 预算和烧录器下载范围。
- 继续保证 DTR 总线读为对齐 word，字节交换发生在尾部提取之前。
- 不把 CR2 寄存器读的 4 dummy 与内存读的 10 dummy 混用。
- 不在协议不一致窗口放置依赖 Flash 的软件断点、执行代码或调试器自动读窗口。
- 保留原 PN 驱动单线协议交接；如未来决定运行时也用八线，应作为另一个完整驱动改造任务处理。
- 升级 IAR 后重新安装/核对自定义烧录器，标准安装目录中的定制文件不保证被升级程序保留。

---

维护依据：当前源码、[实现与板测记录](LOADER_OSPI.txt)、RSK 原理图 D017892_04 第 12 页，以及 MX25UW51245G 数据手册的命令表、CR2、DTR 读和易失配置写入时序说明。
