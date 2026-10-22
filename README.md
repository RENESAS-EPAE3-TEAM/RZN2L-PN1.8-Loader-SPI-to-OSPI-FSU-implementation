# RZN2L-PN1.8-Loader-SPI-to-OSPI-FSU-implementation
This project change the PN 1.8 Loader part spi flash driver to ospi flash driver which improve the efficiency of the code transfering from external flash to rzn2l internal ram space.

for the detail info, please refer to the LOADER_OSPI_IMPLEMENTATION.md and LOADER_OSPI_IMPLEMENTATION_EN.md implementation guide.

## Software Environment

| Item | Environment / configuration |
|---|---|
| Host operating system | Windows |
| Project baseline | Renesas PROFINET IRT DEVKIT V1.8.0 (PN 1.8) |
| Installed compiler | IAR ANSI C/C++ Compiler V9.50.1.380/W64 for ARM, verified using `iccarm.exe --version` |
| Renesas FSP | 2.0.0, as recorded in [configuration.xml](configuration.xml) |
| RTOS | FreeRTOS (AWS FreeRTOS configuration) |
| IAR workspace | [RZN2L_PROFINET_IRT_XSPI_BOOT.eww](RZN2L_PROFINET_IRT_XSPI_BOOT.eww) |
| Verified build configuration | `Debug_App1_STANDARD` |
| Flash programming files | Adapted MX25UW51245G `.board`, `.flash`, `.out`, and `.mac` files in [tools](tools) |



To use the bundled Flash programming files, copy all four files from [tools](tools) into the IAR installation's `arm\config\flashloader\Renesas` directory. In `Project > Options > Debugger > Download`, enable `Use flash loader(s)` and `Override default .board file`, then select the installed `MX25UW51245G.board`. The files retain `$TOOLKIT_DIR$` references; selecting the repository copy alone does not redirect its dependencies to that directory.

## Hardware Environment

| Item | Environment / configuration |
|---|---|
| Evaluation board | Renesas RZ/N2L RSK |
| Target MCU | RZ/N2L `R9A07G084M04GBG`, Arm Cortex-R52 |
| Debug / programming probe | IAR I-jet; the verified build configuration selects the I-jet driver |
| Flash used by this implementation | IC19, Macronix MX25UW51245G, 64MB |
| Flash-selection jumper | CN8 pins **1-2** select IC19 |
| Other Flash device | CN8 pins 2-3 select IC21, MX25UR51245GMI00; this device is not supported by the octal Loader path |
| Loader transfer interface | xSPI0, 8D-8D-8D DTR OPI, approximately 100 MHz, with 10 memory-read dummy clocks |
| Boot and runtime interface | Boot ROM loads the Loader using 1S-1S-1S SPI; the Loader restores SPI before handing control to the original PN Flash/NVdata driver |
| Serial console | 115200 baud; COM3 was used in the recorded tests, but the port number depends on the host |

**Keep CN8 at 1-2 during downloading, booting, and PN operation. Do not move the jumper while power is applied.** If execution stops during DTR operation, restore Flash to SPI or fully power-cycle the board before using the SPI programming algorithm.

The recorded build and board-test baseline is `Debug_App1_STANDARD`. It does not certify every build configuration, complete reset-to-main timing, or end-to-end PN FSU compliance.

For setup details, limitations, and verification results, see the [Chinese implementation guide](LOADER_OSPI_IMPLEMENTATION.md) or [English implementation guide](LOADER_OSPI_IMPLEMENTATION_EN.md).