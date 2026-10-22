# PN 1.8 FSU Implementation and API Usage Guide

## 1. Scope and Conclusions

This document describes the existing implementation that adapts the PN 1.8 project to the MX25UW51245G on the RZ/N2L RSK. It covers code locations, boot principles, internal function parameters, usage constraints, download configuration, and verification results.

The implementation objective is:

> The Boot ROM continues to load the Loader using single-lane SPI. The Loader copies the App using 8D-8D-8D, then restores single-lane SPI for the original PN Flash/NVdata driver.

This solution does not replace the entire PN runtime Flash driver with an OSPI driver, nor does it change the byte layout of the image in Flash. The APIs described here are usage documentation for existing functions, not newly introduced public application interfaces.

The verification baseline is `Debug_App1_STANDARD`. Addresses, section sizes, and code footprints in this document are records of the tested images. After rebuilding, use the actual map/ELF as the reference.

**Latest measured performance conclusion (2026-10-21):** With hardware prefetch enabled, changing the Flash source to the cacheable mapping and adding source-cache maintenance reduced the four-section payload-copy time from 23.478912 ms to **11.111600 ms**. The entire copy function, including both source-cache maintenance passes and SPI handoff checks, decreased from 23.508072 ms to **11.732880 ms**. The interval from completion of clock initialization to immediately before the call to `main()` is approximately **91.83396 ms**. The current source and board retain the "prefetch + cacheable source" version; see Section 12.7. Sections 12.4-12.6 retain historical results from earlier iterations. These times exclude the Boot ROM and Loader clock initialization and must not be reported as complete reset-to-main or PN FSU times.

## 2. Hardware Prerequisite: CN8 Selects Between Two Different Devices

Page 12 of RSK schematic D017892_04 shows that the common terminal of CN8 is OQFLASH_CS0:

| CN8 | Selected device | Purpose |
|---|---|---|
| 1-2 | IC19, MX25UW51245G, 64MB | OSPI Flash used by this solution; supports switching between SPI and DTR OPI |
| 2-3 | IC21, MX25UR51245GMI00, 64MB | Original QSPI Flash; cannot execute the octal protocol used by this solution |

**Keep CN8 at 1-2 during downloading, booting, and PN operation. Do not test by changing the jumper while power is applied.**


## 3. Files and Changes

Paths are relative to the current IAR project directory.

| File | Change / role |
|---|---|
| [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c) | Includes the configuration header; adds Loader-private xSPI helpers and diagnostic variables; extends the original App copy function |
| [loader_xspi_cfg.h](rzn_cfg/driver/loader_xspi_cfg.h) | Adds switches for octal operation, copy prefetch, cacheable sources, and boot timing, plus the polling limit |
| [LOADER_OSPI.txt](LOADER_OSPI.txt) | Implementation notes and board-test records |
| [startup.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c) | Adds optional GSC boot timing; when enabled, moves counter initialization to immediately after clock initialization and records timestamps before/after copying and before calling main |
| [fsp_xspi0_boot.icf](script/fsp_xspi0_boot.icf) | Unchanged; existing rules already include the startup system objects in the BTCM Loader |
| [Runtime QSPI driver](src/fsp_mod/r_xspi_qspi.c) | Not modified for Loader acceleration |
| [Runtime Flash configuration](rzn_gen/hal_data.c) | Retains the original single-lane SPI configuration |

Key-symbol navigation: all symbols and line numbers below refer to [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c). Line numbers are only navigation aids for the current version.

| Symbol | Current line | Purpose |
|---|---:|---|
| `g_bsp_loader_ospi_stage` / `g_bsp_loader_ospi_error` | 691 / 692 | Stage and error diagnostics |
| `g_bsp_loader_ospi_copy_bmcfg` | 693 | BMCFG snapshot during copying; bit 16 enables prefetch |
| `g_bsp_loader_ospi_copy_source` | 694 | Address of the final word read in the last nonempty section |
| `g_bsp_loader_ospi_copy_sctlr` | 695 | SCTLR snapshot before copying; bit 2 enables D-cache |
| `bsp_loader_ospi_fail()` | 697 | Records an error and stops |
| `bsp_loader_ospi_wait()` | 707 | Bounded polling |
| `bsp_loader_ospi_command()` | 720 | Sends a direct command |
| `bsp_loader_ospi_write_cr2()` | 741 | Writes CR2 in SPI/OPI mode |
| `bsp_loader_ospi_pin()` | 754 | Configures one pin |
| `bsp_loader_ospi_pins_init()` | 766 | Configures the octal pins required by the Loader |
| `bsp_loader_ospi_invalidate_source()` | 787 | Invalidates the read-only Flash source range by cache line |
| `bsp_loader_ospi_invalidate_sources()` | 812 | Maintains source caches for the four linked sections |
| `bsp_loader_copy_from_flash()` | 829 | Exact-length DTR copy |
| `bsp_copy_to_ram()` | 886 | Original entry point; orchestrates switching, four-section copying, and restoration |

## 4. Boot Principles and Code Execution Location

```text
Boot ROM
  +-- Loads Loader into BTCM using 1S-1S-1S
       +-- SystemInit()
            +-- Clock initialization
            +-- Clear and start GSC (when boot timing is enabled)
            +-- Initialize Loader data and clear Loader BSS
            +-- Configure memory protection/cache attributes
            +-- bsp_copy_to_ram()
            |    +-- Save SPI state and reference vector word
            |    +-- Initialize octal pins
            |    +-- Flash -> DTR OPI
            |    +-- xSPI0 -> 8D-8D-8D
            |    +-- Verify configuration and mapped reads
            |    +-- Enable hardware prefetch (when configured)
            |    +-- VECTOR / USER_PRG / USER_DATA / USER_DATA_NONCACHE
            |    +-- Flash -> SPI
            |    +-- Restore and verify original xSPI0 configuration
            +-- Clear App BSS
            +-- Static constructors, POST_C, and other initialization
            +-- main -> PN -> spi_flash_init() -> NVdata
```

"Save SPI state and reference vector word" means saving the SPI configuration and reading the first 32-bit word of the vector table as a reference for read checks before and after protocol switching. After switching to octal DTR, the reading from the same address is corrected using `__REV16()` (see Section 7.3) and compared with the reference. After restoring SPI, the address is read and compared again. This is a quick read check of protocol switching, not an integrity check of the entire firmware.

The Boot ROM parameters, validation, and `0x6000`-byte Loader transfer length remain unchanged. In the tested layout, the Loader is loaded from Flash `0x6000004C` to BTCM `0x00102000`.

All new hardware-control and copy functions reside in startup system objects already included in the Loader. During the brief interval when the Flash and controller protocols do not match, the CPU fetches instructions from BTCM. It does not need to call the App before it has been copied, allocate dynamic memory, or use C library copy functions.

The measured Loader code footprint was `0x15E4` bytes for the original octal version, `0x1638` bytes after adding boot timing, and `0x166C` bytes after adding four-section payload-copy timing. The historical hardware-prefetch-only version used `0x16A8` bytes. The current "prefetch + cacheable source" version uses `0x17A8` bytes, still below the original `0x6000`-byte transfer budget. During maintenance, check the actual Loader image layout, not just the size of an individual function.

## 5. Configuration and Enabling

Existing configuration:

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

| Configuration | Meaning |
|---|---|
| `BSP_CFG_LOADER_OSPI_ENABLE=1` | Enables this solution |
| `BSP_CFG_LOADER_OSPI_ENABLE=0` | Routes copy calls back to the original `bsp_copy_multibyte()` for fallback or A/B measurements |
| `BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE=1` | Current default; enables controller prefetch only during four-section copying after the DTR mapping check passes; disables prefetch and clears its buffer before restoring SPI |
| `BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE=0` | Disables the prefetch optimization for octal prefetch A/B testing; does not disable the octal protocol itself |
| `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=1` | Current default; copies from the `0x600...` cacheable source, maintains the four source ranges before and after copying, and checks cacheable reads after SPI restoration |
| `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=0` | Copies from the `0x400...` noncacheable alias without the added source-cache maintenance; does not disable the CPU's global D-cache |
| `BSP_CFG_LOADER_BOOT_TIMING_ENABLE=1` | Current default; starts GSC early in the IAR xSPI0 Flash-boot configuration and records boot timestamps |
| `BSP_CFG_LOADER_BOOT_TIMING_ENABLE=0` | Disables the added timing and restores the original behavior of initializing GSC shortly before main |
| `BSP_CFG_LOADER_OSPI_POLL_LIMIT` | Maximum polling iterations for each wait; not a duration in milliseconds |

The compilation conditions require non-RAM execution and all of the following:

```c
BSP_CFG_LOADER_OSPI_ENABLE && defined(__ICCARM__) && BSP_CFG_XSPI0_X1_BOOT
```

The octal, prefetch, cacheable-source, and timing enable macros can be overridden through IAR compiler predefinitions. The polling limit is currently defined directly in the configuration header. The prefetch and cacheable-source switches affect only the octal branch and do not change the CPU's global cache configuration. Cacheable sources require the current cacheable MPU attributes for `0x600...` and D-cache to remain enabled. The timing switch is independent of the octal switch: enable timing in both A/B builds, not just one.

Recommended procedure:

1. Confirm that CN8 is fixed at 1-2 and the target device is MX25UW51245G.
2. Confirm that IAR uses the adapted Flash programming algorithm.
3. Rebuild and download after configuration changes. Editing the header and resetting old firmware is not sufficient.
4. Disable octal acceleration when testing with the original QSPI device.



## 6. SPI -> DTR -> SPI Implementation Sequence

### 6.1 Save State and Establish the SPI Reference

At entry, set `stage=1` and `error=0`, execute barriers, and wait for memory accesses and the write buffer to become idle.

Saved controller state:

| Register | Reason for saving |
|---|---|
| `LIOCFGCS[0]` | Protocol and chip-select timing |
| `WRAPCFG` | Wrapper settings, including DS shift |
| `BMCFG` | Mapped-access bridge / prefetch configuration |
| `BMCTL0` | AHB bridge access permissions |
| `CDCTL0` | Direct-command control |
| `CSa[0].CMCFG0/1/2` | Command length, address length, and mapped read/write commands |

Read the first vector word through the noncacheable alias and save it as `expected`. Then verify that the controller is currently in SPI mode.

This reference only confirms that mapped reads remain consistent across protocol switching. It is **not a CRC, signature, or full-image integrity check**.

### 6.2 Prepare Pins Early and Isolate Mapped Accesses

Configure P14.4-7 and P15.0-7 for the required peripheral functions, using secure-region, high-drive, and fast-slew settings.

Then disable prefetch, wait for accesses to complete, disable CS0 bridge access, and clear the prefetch buffer to prevent mapped accesses during protocol switching.

The GPIO write-protection state is restored. However, the pin multiplexing and drive settings configured by this function are not individually saved and rolled back. Subsequent normal pin initialization still follows the original boot flow.

### 6.3 Configure the Flash, Then the Controller

First read and save the following in SPI mode:

- CR2 address `0x00000000`: protocol mode.
- CR2 address `0x00000300`: dummy-cycle configuration.

After confirming that the Flash is in SPI mode:

1. Set the CR2 dummy field to `(saved_dc & ~7U) | 5U`, corresponding to 10 dummy clocks for memory reads.
2. Write the CR2 mode as `saved_mode | 2U` to enable DTR OPI.
3. Configure the controller for matching 8D-8D-8D operation.

| Parameter | Current value |
|---|---|
| Protocol PRTMD | `0x3FF` |
| DS shift | `8` |
| LIOCFGCS0 | `(6U << 16) | 0x3FFU` |
| Mapped command length | 2 bytes |
| Mapped address length | 4 bytes |
| Mapped read command | `0xEE11` |
| Mapped write command | `0x12ED`; configured only, with no data programming performed by the Loader |
| Memory-read dummy cycles | 10 clocks |
| DTR CR2-read dummy cycles | **4 clocks** |
| xSPI0 clock | Original configuration retained; measured at approximately 100MHz; no frequency increase in this change |

Next, read back the mode and dummy configuration using DTR commands. After verifying the register values, restore bridge access and compare the byte-order-corrected DTR vector word with the SPI reference. When cacheable sources are enabled, first invalidate the four read-only Flash source ranges to avoid hitting old SPI data. When prefetch is enabled, then wait for `MEMACC=0`, clear the prefetch buffer, set `BMCFG.PREEN=1`, and execute `DSB`. Finally, record BMCFG and SCTLR for the copy phase, set `stage=2`, and start copying.

### 6.4 Copy the App Using the Original Linked Sections

Section addresses and sizes still come from linker symbols; they are not hardcoded as one large image:

| Section | Contents | Measured size |
|---|---|---:|
| VECTOR | Exception vectors | 68 bytes |
| USER_PRG | App code | 1,169,712 bytes |
| USER_DATA | Initialized data | 77,068 bytes |
| USER_DATA_NONCACHE | Noncacheable initialized data | 0 bytes |
| Total | | 1,246,848 bytes |

### 6.5 Restore After Copying

Set `stage=3`. If prefetch is enabled, first disable `BMCFG.PREEN` and execute `DSB`. Then wait for mapped accesses to complete, disable CS0 bridge access, clear the prefetch buffer, and execute barriers so that DTR-prefetched data is not carried into SPI mode. The manual states that `PBUFCLR` must not be set while `COMSTT.MEMACC=1`, so waiting for idle is mandatory.

When cacheable sources are enabled, invalidate the four source ranges again before restoring Flash SPI, removing the byte-pair-swapped data cached during DTR operation. This does not discard the destination RAM cache.

Restoration order:

1. Restore the Flash to its original SPI mode **while still using DTR commands**.
2. Restore the controller protocol, wrapper, and mapped-command settings.
3. Restore the original Flash dummy configuration using SPI commands.
4. Read back and verify CR2 using SPI.
5. Restore direct-command control, clear the prefetch buffer, and restore bridge and prefetch settings.
6. Execute `DSB/ISB`.
7. Read the vector word again using SPI and verify that it matches the initial value.
8. If cacheable sources are enabled and data was actually copied, have the CPU compare cacheable and noncacheable SPI reads of the final source word. A mismatch also reports error 8.
9. Set `stage=4` and return to the original startup flow.

Restoring only the controller or only the Flash is insufficient: both ends must use the same protocol. Normal restoration does not erase or program user Flash data, nor does it migrate NVdata from the other device.

## 7. Exact-Length Copying and Byte Order

### 7.1 Address Aliases

The interface accepts original Flash mapped addresses in `0x60000000-0x63FFFFFF`. With the current `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=1`, these addresses are retained and use CPU D-cache as permitted by the existing MPU configuration. The outer function invalidates the four source ranges before and after copying.

When set to 0, the implementation subtracts `0x20000000` and uses the noncacheable alias `0x40000000-0x43FFFFFF` to avoid hitting old SPI cache contents. The controller prefetch buffer and CPU D-cache are separate mechanisms and require separate maintenance. Using a noncacheable alias is not a general-purpose operation that cleans all caches.

### 7.2 Alignment and Tail Handling

DTR OPI requires an even starting bus address. This implementation further ensures that every explicit CPU Flash load is a naturally aligned 32-bit read. On a cache miss in the cacheable path, actual external accesses are generated by cache-line fills; each CPU load can no longer be treated as an independent 4-byte Flash transaction.

- If both source and destination are 4-byte aligned and at least 4 bytes remain, use the word fast path.
- Otherwise, read the aligned word containing the requested bytes, then extract those bytes and write them to RAM.
- At the tail, write only the actual remaining length without rounding up.
- Edge-word reads, cache-line fills, and hardware prefetch may read neighboring Flash bytes outside the requested section, but RAM writes remain limited to the requested length. Explicit word reads are range-checked against the 64MB boundary; real cache/prefetch behavior at the very end of the device has not yet passed board-level acceptance testing.

Section lengths therefore do not need to be multiples of 2 or 4. The existing linker script already aligns ordinary sections to 4 bytes and the vector section to 32 bytes, favoring the fast path.

### 7.3 Adjacent-Byte Swapping

Measurements with the current hardware/protocol combination:

```text
SPI word:         0xE59FF020
Raw DTR word:     0x9FE520F0
After __REV16():  0xE59FF020
```

`__REV16()` is used because the same image programmed using SPI produces memory-mapped readings with the two bytes in each 16-bit halfword exchanged when read through the current MX25UW51245G/xSPI0 octal DTR configuration. This corrects the byte arrangement of the read result; it does not change Flash contents or the CPU's endianness. The behavior has been confirmed on the board. This observation alone does not prove that a particular sampling edge or internal controller stage is its cause.

Expressed as bytes from low to high address, the example is:

```text
Required image bytes:       20 F0 9F E5
Bytes in the raw DTR word:  F0 20 E5 9F
After __REV16():            20 F0 9F E5
```

The DTR probe, word copying, and tail handling therefore all apply CMSIS `__REV16()` first. It exchanges adjacent bytes within each halfword, rather than reversing the entire 32-bit word. Without the correction, the DTR probe does not match the SPI reference. If that check is skipped and copying proceeds, code and data written to RAM also have the wrong byte arrangement. Tail bytes must be extracted from the corrected word so that sections whose lengths are not multiples of 4 are copied correctly.

This conversion applies to the current DTR read path for an SPI-programmed image and must not be generalized to arbitrary Flash/controller combinations. It is not applied after SPI restoration, and the programming algorithm does not pre-swap image bytes.

## 8. Internal API Usage

Except for the original startup entry point, the following helpers are file-local `static` functions. They are not intended for application use or concurrent RTOS calls. Do not remove `static` and directly use them as PN runtime Flash APIs.

### 8.1 `bsp_copy_to_ram()`

```c
void bsp_copy_to_ram(void);
```

- Caller: the original `SystemInit()`, once, after Loader data/BSS and memory-attribute initialization.
- Inputs: four sets of source, destination, and length values supplied by the linker; no explicit parameters.
- Normal return: all four sections have been copied, both ends are restored to SPI, and `stage=4/error=0`.
- Failure: enters unrecoverable-error handling rather than returning success.
- Limitation: this is not a runtime "reload App" interface. Calling it again after App startup overwrites code and data.

### 8.2 `bsp_loader_ospi_command()`

```c
static uint32_t bsp_loader_ospi_command(
    uint32_t command, uint32_t command_bytes,
    uint32_t address, uint32_t address_bytes,
    uint32_t data, uint32_t data_bytes,
    uint32_t dummy, bool write);
```

| Parameter | Usage |
|---|---|
| `command` | Unshifted command, such as `0x71` or `0x718E` |
| `command_bytes` | Current calls use 1 or 2; the function selects the command write position accordingly |
| `address` | Address inside the Flash device / register address; **not a CPU mapped address** |
| `address_bytes` | Current calls use 0 or 4 |
| `data` | Transmit data; pass 0 for reads |
| `data_bytes` | This solution uses 0, 1, or 2, depending on the command |
| `dummy` | Dummy clocks required by the command |
| `write` | `true` for write direction; `false` for read direction |

The return value is the `CDD0` data register, not an error code. Timeouts are handled by the internal wait function.

Internally, the function waits for the command interface to become idle, clears the completion flag, configures `CDT/CDA/CDD0`, triggers `TRREQ`, waits for completion, reads the result, and clears the flag.

Actual call examples:

```c
/* Read the mode register in SPI; both controller and Flash must be in SPI. */
uint32_t mode = bsp_loader_ospi_command(
    0x71U, 1U, 0U, 4U, 0U, 1U, 0U, false) & 0xFFU;

/* Read CR2 in DTR; use only after both ends have switched to DTR. */
uint32_t mode_dtr = bsp_loader_ospi_command(
    0x718EU, 2U, 0U, 4U, 0U, 2U, 4U, false);
```

Constraints:

- The function implements only the compact command operations needed by this boot flow; it is not an arbitrary-length transfer API.
- There is no general parameter validation. The caller must ensure correct field values, direction, timing, and protocol.
- It does not automatically issue write enable or convert data byte order.
- The higher-level flow must coordinate direct-command access and mapped bridge access.

### 8.3 `bsp_loader_ospi_write_cr2()`

```c
static void bsp_loader_ospi_write_cr2(
    bool octal, uint32_t address, uint32_t value);
```

- `octal=false`: both ends are currently in SPI; sends `0x06` and `0x72`.
- `octal=true`: both ends are currently in DTR OPI; sends `0x06F9` and `0x728D`.
- `address`: CR2 address; this solution uses `0` and `0x300`.
- `value`: single-byte configuration value; the function transmits 1 byte.
- Return value: none.

The function issues write enable, writes CR2, and then uses a volatile delay loop to satisfy `tW2V >= 40ns`. Recheck timing if the CPU clock or delay implementation changes.

The function does not verify readback or update the controller protocol. The caller is responsible for subsequent synchronization of both ends and CR2 checks.

### 8.4 `bsp_loader_ospi_wait()`

```c
static void bsp_loader_ospi_wait(
    volatile const uint32_t * reg, uint32_t mask, uint32_t value);
```

The wait condition is:

```c
(*reg & mask) == value
```

- `reg`: valid hardware-register address.
- `mask`: bits to check.
- `value`: expected masked value; the caller must ensure that no bits outside the mask are set.
- Returns `void` on success; calls `bsp_loader_ospi_fail(1U)` if the configured iteration limit is exceeded.

This function provides no RTOS delay or scheduling and is not a timeout interface measured in real elapsed time.

### 8.5 `bsp_loader_ospi_pin()` / `bsp_loader_ospi_pins_init()`

```c
static void bsp_loader_ospi_pin(uint32_t port, uint32_t pin);
static void bsp_loader_ospi_pins_init(void);
```

The single-pin function configures the secure region, DRCTL, PFC0, and PMC. It is used only for predetermined xSPI pins and does not perform general port/pin validation.

The batch function saves the GPIO write-protection state, temporarily unlocks it, configures the two fixed pin groups, restores write protection, and executes barriers. It does not require the application's generated pin table and must not be called repeatedly during PN operation.

### 8.6 `bsp_loader_copy_from_flash()`

```c
static void bsp_loader_copy_from_flash(
    uintptr_t * src, uintptr_t * dst, uintptr_t bytesize);
```

| Parameter | Contract |
|---|---|
| `src` | Original Flash mapped address; pass a `0x600...` address, not the noncacheable alias |
| `dst` | Writable RAM with capacity for at least `bytesize` bytes |
| `bytesize` | **Byte count**, not a count of `uintptr_t` elements |

Octal configuration, mapping checks, and bridge-access enablement must already be complete before the call. This function does not switch protocols.

Behavior:

- Checks the source range and length; reports error 2 on an out-of-range request.
- Supports arbitrary source/destination alignment and arbitrary lengths; writes only the specified destination bytes.
- Does not access Flash for a zero-length request. The address immediately after the device is allowed only when the length is zero.
- Uses aligned word reads and `__REV16()`, followed by a final `DSB`.
- Selects the original mapping or noncacheable alias according to the cacheable-source switch. It does not maintain source caches itself; the outer copy flow must perform paired maintenance.
- For nonempty sections, updates `g_bsp_loader_ospi_copy_source` with the final aligned source-word address. A zero-length section does not overwrite the previous nonempty section's record.
- Does not validate destination address, capacity, or source/destination overlap. These are guaranteed by the linker layout and caller.
- When octal operation is disabled, this name is mapped by a macro back to the original `bsp_copy_multibyte()`. Do not assume that the original function has the octal branch's exact tail-handling contract.

Call pattern (already present in the original flow; do not add another call in the App):

```c
src  = (uintptr_t *) BSP_PRV_SECTION_USER_PRG_ROM_ADDRESS;
dst  = (uintptr_t *) BSP_PRV_SECTION_USER_PRG_RAM_START;
size = (uintptr_t) BSP_PRV_SECTION_USER_PRG_RAM_END -
       (uintptr_t) BSP_PRV_SECTION_USER_PRG_RAM_START;
bsp_loader_copy_from_flash(src, dst, size);
```

#### 8.6.1 Implementation Steps and Rationale

This is a synchronous Loader copy function; it does not use DMA. Its execution flow is:

```text
Check Flash source address and length
  -> Retain the cacheable address or convert to the noncacheable mapping
  -> For a nonempty section, record the final aligned source-word address
  -> Copy loop:
       Source and destination both 4-byte aligned, at least 4 bytes left
         -> Word fast path
       Otherwise
         -> Read an aligned word, extract required bytes, write RAM byte by byte
  -> DSB barrier
```

**1. Convert parameters to byte addresses**

```c
uintptr_t source = (uintptr_t) src;
uint8_t * destination = (uint8_t *) dst;
```

`source` is represented as an integer to simplify address masking, alignment, and range calculations. `destination` is a byte pointer for exact handling of unaligned addresses and tails. Although the entry-point type is `uintptr_t *`, `bytesize` always counts bytes; do not pass a word count.

**2. Check the source range**

Source data must be within `[0x60000000, 0x64000000)`, the 64MB Flash mapping. The code first checks the starting address, then checks:

```c
bytesize > 0x64000000U - source
```

Comparing against remaining capacity instead of directly calculating `source + bytesize` avoids address-addition overflow. C `||` short-circuit evaluation ensures that the subtraction check is not evaluated if the starting address is already out of range. On error, execution enters `bsp_loader_ospi_fail(2U)` rather than silently truncating the request or continuing startup.

A zero-length request must still pass source-address validation but does not read Flash. `source == 0x64000000` is permitted only for zero length.

**3. Select the source mapping according to the switch**

```c
#if !BSP_CFG_LOADER_OSPI_CACHE_ENABLE
source -= 0x20000000U;
#endif
```

By default, `0x60010000` is retained, allowing full cache-line fills and subsequent hits. The outer flow removes stale SPI/DTR source-cache data. When the switch is off, for example, `0x60010000` becomes the same Flash's noncacheable alias `0x40010000`. `volatile const uint32_t *` requests volatile reads of that address; `volatile` itself does not bypass CPU caches.

**4. Use the 4-byte fast path when its conditions are met**

```c
if (((source | (uintptr_t) destination) & 3U) == 0U && bytesize >= 4U)
{
    *(uint32_t *) destination = __REV16(*(volatile const uint32_t *) source);
    source += 4U;
    destination += 4U;
    bytesize -= 4U;
}
```

ORing the two addresses and checking the low two bits requires both addresses to be 4-byte aligned. Each iteration reads one aligned 32-bit Flash word, applies `__REV16()` to correct the within-halfword byte swap on the current DTR path, then writes one word to RAM.

"32-bit read" here describes the CPU access granularity of the memory-mapped window. It does not mean that the Flash uses 32 data lines. The external bus is still accessed by xSPI0 using the configured 8D-8D-8D protocol.

**5. For unaligned addresses or fewer than 4 remaining bytes, read an aligned word and extract bytes**

```c
uint32_t value = __REV16(*(volatile const uint32_t *) (source & ~(uintptr_t) 3U));
uint32_t count = 4U - (source & 3U);
if (count > bytesize)
{
    count = bytesize;
}
value >>= (source & 3U) * 8U;
```

- `source & ~3`: finds the aligned word containing the current source byte; the explicit CPU load remains an aligned word read.
- `source & 3`: gives the current source byte's offset within that word.
- `count`: gives the bytes available from that offset to the end of the word, capped at the remaining length.
- Applying `__REV16()` before shifting first restores the correct arrangement, then moves the first required byte into the low 8 bits.

The low 8 bits of `value` are then written to RAM one byte at a time, shifting right by 8 bits until `count` bytes have been written. The source address and remaining length are updated accordingly. This avoids word writes to unaligned destinations and prevents excess RAM writes when fewer than 4 bytes remain.

If source and destination have different alignment offsets, the byte-by-byte path may not subsequently reach the word fast path. Correctness takes priority over assuming that every unaligned request can be converted to word copying.

**6. Execute `__DSB()` after copying**

A data synchronization barrier at the end ensures that preceding explicit memory accesses complete as required by the barrier before startup continues. This is not equivalent to a general D-cache clean or I-cache invalidation and does not perform data-integrity verification.

#### 8.6.2 Example of Unaligned and Tail Handling

Assume source `0x60010001`, destination `0x00120001`, and a length of 6 bytes. The table below uses the cacheable-source switch set to off:

| Iteration | Actual Flash read address (noncacheable alias) | Selected byte offsets within the corrected word | RAM write addresses | Bytes written |
|---|---|---|---|---:|
| First | `0x40010000` | 1, 2, 3 | `0x00120001` to `0x00120003` | 3 |
| Second | `0x40010004` | 0, 1, 2 | `0x00120004` to `0x00120006` | 3 |

The CPU explicitly reads two aligned words, but exactly the requested 6 bytes are written to RAM. Neither `0x00120000` nor `0x00120007` is written. With the switch on, the corresponding read addresses are `0x60010000/0x60010004`. The explicit-read and RAM-write logic is the same, but actual bus traffic also depends on cache lines and controller prefetch.

This illustrates that read-alignment requirements and section-length alignment are different issues. The function handles arbitrary lengths by reading complete words containing edge bytes and extracting only the required bytes. Explicit word reads remain within the device; cache/prefetch may generate additional reads, subject to the limits in Section 7.2. This is suitable only for the ordinary Flash storage region described here and must not be directly applied to peripheral registers with read side effects.

### 8.7 `bsp_loader_ospi_fail()`

```c
static void bsp_loader_ospi_fail(uint32_t error);
```

Writes the global error variable, calls `BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0)`, and remains on the stopped execution path. It does not automatically restore the Flash mode or fall back to SPI and continue startup.

If a breakpoint is placed at function entry, the error variable has not yet been assigned; inspect ARM argument register `R0`. An error variable still equal to 0 at that point does not mean that no fault occurred.

### 8.8 Source-Cache Maintenance Functions

```c
static void bsp_loader_ospi_invalidate_source(uintptr_t source, uintptr_t bytesize);
static void bsp_loader_ospi_invalidate_sources(void);
```

Compiled only when `BSP_CFG_LOADER_OSPI_CACHE_ENABLE=1`. They reside in the same BTCM Loader object and do not call App cache functions before the App has been copied.

- `source` is the `0x600...` Flash source address, and `bytesize` is a byte count. Zero length performs no maintenance; an out-of-range request reports error 2.
- Following the BSP's address-based maintenance approach, the cache-line size is obtained dynamically from `CTR.DminLine`: `4U << ((__get_CTR() >> 16) & 0xFU)`.
- The start is aligned down to a cache-line boundary, and CMSIS `__set_DCIVAC()` is executed line by line with `DSB` before and after, including the line containing the final byte.
- The no-argument function maintains the Flash source ranges of VECTOR, USER_PRG, USER_DATA, and USER_DATA_NONCACHE in order. It does not maintain the entire 64MB Flash or invalidate the entire D-cache.
- The outer flow calls it once after the DTR check and before payload-copy sampling, and once after copying, after disabling prefetch and isolating bridge access but before restoring SPI.
- These sources are read-only and contain no dirty Flash source data that must be preserved. Do not directly use this function on RAM that may contain dirty data: invalidation could discard data that has not been written back.
- Maintenance may include neighboring Flash bytes in the same cache line. It does not invalidate dirty destination-RAM cache lines, replace xSPI `PBUFCLR`, or globally disable CPU caches.

## 9. Diagnostic Variables and Error Codes

```c
volatile uint32_t g_bsp_loader_ospi_stage;
volatile uint32_t g_bsp_loader_ospi_error;
volatile uint32_t g_bsp_loader_ospi_copy_bmcfg;
volatile uint32_t g_bsp_loader_ospi_copy_source;
volatile uint32_t g_bsp_loader_ospi_copy_sctlr;
```

These variables belong to Loader BSS. They may be 0 before execution begins and are not fault logs retained across resets. The last three record BMCFG during copying, the last word address of the final nonempty section, and SCTLR before copying, respectively. A set D-cache bit in SCTLR does not mean every address is cacheable; the source address and MPU attributes must also be considered.

| stage | Meaning |
|---:|---|
| 1 | Preparing, switching to DTR, and checking DTR |
| 2 | DTR checks passed; copying in progress |
| 3 | Copying complete; restoring SPI |
| 4 | SPI restoration checks passed |

| error | Meaning |
|---:|---|
| 0 | No recorded error |
| 1 | Hardware polling limit exceeded |
| 2 | Invalid Flash source range |
| 3 | Controller not in SPI mode at entry |
| 4 | Flash not in SPI mode at entry |
| 5 | DTR CR2 readback mismatch |
| 6 | DTR mapped-read probe mismatch |
| 7 | CR2 mismatch after restoration |
| 8 | SPI vector probe mismatch after restoration, or mismatch between cacheable and noncacheable SPI reads of the final source word |

For diagnosis, record stage, error, PC, and controller protocol together. If a failure occurs during DTR operation, do not immediately run the SPI programming algorithm and assume the Flash has been restored. Restore the protocol first or fully power-cycle the board.

## 10. Relationship to PN Runtime Flash/NVdata

The Loader does not open or modify `g_qspi0_ctrl`. PN subsequently continues to use the original `spi_flash_init()` and SPI read/write paths, configured as follows:

- Protocol: `1S-1S-1S`.
- 3-byte addresses.
- Read: `0x03`.
- Program: `0x02`.
- Write enable: `0x06`.
- Read status: `0x05`.
- 4KB erase: `0x20`.

QSPI in a function name does not imply that four-lane operation is enabled at runtime. A normal startup handoff requires the Flash itself to have returned to SPI. Do not assume that opening the driver replaces Flash protocol restoration.

| NV image | Noncacheable CPU address | Offset within Flash |
|---|---|---|
| SEC1 | `0x403F0000` | `0x003F0000` |
| SEC2 | `0x403F4000` | `0x003F4000` |

Both offsets are within the 3-byte address range, so these two NV images do not require changing the runtime driver to 4-byte addresses. This does not imply that the current 3-byte configuration can directly access every region of the full 64MB device.

The original NVdata path is:

```text
Bsp_nv_data_store()
  -> OsFlashProgram() queues the request
  -> Task_OsFlashPost()
  -> SPI erase / program / verify
  -> Save-completion callback

Startup:
Bsp_nv_data_init()
  -> OsFlashRead()
  -> Check version and checksum
  -> Use first copy / second copy / defaults
```

These runtime functions are not part of the Loader APIs added by this work.

## 11. IAR Flash Loader Configuration

Distinguish between:

- **The boot Loader described here**: part of the product firmware; copies the App at power-on.
- **The IAR Flash Loader**: a programming algorithm used by the debugger; not executed during normal PN startup.

The current programming algorithm and all companion files are backed up unchanged in [tools](tools) at the local Git repository root:

| File | Role |
|---|---|
| [MX25UW51245G.board](tools/MX25UW51245G.board) | Download entry point; references `.flash` and limits the download range |
| [MX25UW51245G.flash](tools/MX25UW51245G.flash) | Flash device description; references the algorithm and initialization macro |
| [MX25UW51245G.out](tools/MX25UW51245G.out) | Adapted, board-tested programming algorithm binary |
| [MX25UW51245G.mac](tools/MX25UW51245G.mac) | C-SPY SPI bus and pin-initialization macro |

All four files passed SHA-256 comparison against their current IAR installation copies. This directory is an installation bundle that can travel with the project, not a source package for rebuilding the programming algorithm. An installed IAR Embedded Workbench for Arm / C-SPY and the target debugger environment are still required.

On another computer, copy all four files together into the corresponding IAR installation's `arm\config\flashloader\Renesas` directory (administrator privileges may be required), then select the installed board file as described below. The files retain their original `$TOOLKIT_DIR$` references: selecting the backup board file directly does not automatically load its dependencies from the backup directory. This archival operation did not modify IAR installation files or project debugger options.

The adapted programming files are installed at:

```text
C:\Program Files\IAR Systems\Embedded Workbench 9.2_2\arm\config\flashloader\Renesas\
    MX25UW51245G.board
    MX25UW51245G.flash
    MX25UW51245G.out
    MX25UW51245G.mac
```

Dependencies:

```text
MX25UW51245G.board
  +-- MX25UW51245G.flash
       +-- MX25UW51245G.out
       +-- MX25UW51245G.mac
```

The configuration references `$TOOLKIT_DIR$` and has no session-directory dependency. The original vendor programming algorithm was not overwritten. The adapted algorithm was copied from the original SPI version, changing the expected ID from `0x001A35C2` to the board-measured `0x003A81C2`; this representation packs bytes as read from the register.

In IAR:

1. Open `Project -> Options -> Debugger -> Download`.
2. Enable `Use flash loader(s)`.
3. Enable `Override default .board file`.
4. Select `MX25UW51245G.board` from the standard directory.
5. Keep CN8 at 1-2, rebuild, and download.

Installing the files in the standard directory does not automatically change project options. Check the board file actually selected.

The current board file restricts downloading to `0x60000000-0x60152FFF`, the tested firmware range, not the entire Flash device. Recheck the range when firmware grows or the layout changes. This range does not cover either NV slot.

The programming algorithm uses SPI. If execution stops abnormally during the DTR phase, first restore SPI or fully power-cycle the board. Moving CN8 to the other device does not "restore" the same image.

## 12. Verification Results and Evidence Boundaries

### 12.1 Software Verification

- IAR `Debug_App1_STANDARD` compiled and linked with no errors or warnings.
- Modified source files were independently compiled for the other three configurations; their full linking was not completed.
- The branch with octal operation disabled was independently compiled successfully.
- Simulation of the final ARM machine code, including the byte-swap correction, passed 4,800 alignment, length, and boundary-sentinel cases, plus zero-length, invalid-range, and end-of-device tests.
- The model simulated both endpoints' protocols, commands, and register restoration, and fully compared all 1,246,848 bytes across the four sections.

Simulation does not replace acceptance testing of physical pins, timing margin, real Flash behavior, or FSU performance.

### 12.2 Loader Board Tests

With CN8 fixed at 1-2:

- The SPI reference vector was `0xE59FF020`.
- The Loader reached `stage=4/error=0`.
- After restoration: `LIOCFGCS0=0x70000`, `CMCFG0=8`, `CMCFG1=0x300`, and `WRAPCFG=0`.
- RAM exports passed Intel HEX checksum and range validation.
- Every valid byte in the four sections matched the ELF.
- Execution subsequently reached main and the original SPI initialization.
- After driver initialization, the protocol was 0 (SPI), and single-byte vector reads were correct.

### 12.3 NVdata Board Tests

Runtime code was unchanged. Both 16KB slots, totaling 32KB, were fully backed up before testing.

- Saving the temporary station name `ospi-nv-cn8-12` was triggered through the original COM3 console.
- Both NV images completed erase, programming, and verification. The actual image length was 8,569 bytes, with version `0x0411`.
- Both copies were identical, checksum `0xFFFF936A` was correct, and every byte matched the RAM image.
- After a hardware reset, NVdata in RAM matched the saved image, and Flash was not rewritten.
- Slot data outside the image remained unchanged.
- After testing, the original full 32KB was restored and verified byte for byte. The firmware region was also confirmed unchanged.
- Finally, the original configuration was restored and booted without breakpoints; the serial output showed `SYSTEM STARTUP FINISHED, OK`.

Linker-symbol allocation includes alignment padding, so the allocated NVdata size may be 8,572 bytes. The checksum must use the actual structure length of 8,569 bytes, excluding trailing padding.

<span style="color: red;"><strong>Unresolved failure:</strong> Online station-name changes also triggered a PN fatal error: <code style="color: inherit;">PackId 55, ModId 4, Line 197, Device 1</code>. It reproduced without debugger breakpoints. The corresponding LSAS check detects a non-success OHA completion status; the rejection reason has not yet been identified. Successful Flash persistence must not be reported as successful end-to-end PN online renaming, nor attributed to CN8 without evidence.</span>

### 12.4 GSC Boot-Time A/B Board Tests (2026-10-21, Before Prefetch Optimization)

#### Measurement Scope and Timing Implementation

The original `SystemInit()` called `bsp_global_system_counter_init()` only after App copying, BSS clearing, constructors, and Master MPU initialization. Reading GSC in main without changes would therefore omit App copying.

Timing controlled by `BSP_CFG_LOADER_BOOT_TIMING_ENABLE` was added to [startup.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c):

1. Call the original `bsp_global_system_counter_init()` after `bsp_clock_init()` returns, clearing and starting GSC. Loader BSS has not yet been initialized, so no timestamp globals are written at this point.
2. After Loader data/BSS and memory-attribute initialization, save counts immediately before and after `bsp_copy_to_ram()`.
3. Skip the original GSC clear near main to preserve the elapsed time already accumulated.
4. Save a final count before calling `main()`. Strictly speaking, the endpoint is the sampling point before the main call, with a small amount of store and branch overhead still remaining.

The three `volatile uint64_t` variables used for the overall boot interval belong to the startup object's Loader BSS and can be inspected in the debugger:

| Variable | Meaning |
|---|---|
| `g_bsp_loader_timing_copy_begin` | Accumulated ticks before calling the copy function |
| `g_bsp_loader_timing_copy_end` | Accumulated ticks after the copy function returns |
| `g_bsp_loader_timing_main` | Accumulated ticks before calling main |
| `g_bsp_loader_timing_ospi` | Octal-switch value of the measured firmware: 0 for single-lane, 1 for octal; this variable is `uint32_t` |

The internal `bsp_loader_boot_ticks()` reads GSC in "high 32 bits -> low 32 bits -> high 32 bits" order and retries if the high values differ, avoiding an incorrect 64-bit combination across a low-word rollover. There is no printing or intermediate halt breakpoint inside the measurement window.

The project's current `BSP_GLOBAL_SYSTEM_COUNTER_CLOCK_HZ` is 25,000,000 Hz, or 40 ns per tick:

```text
Copy-function time (ms) = (copy_end - copy_begin) / 25000
Time to main (ms) = main / 25000
```

`CNTFID0` declares the frequency; writing it does not arbitrarily change the actual counter clock. Conversion must match the real platform clock.

**The start is not physical reset.** This window excludes reset release, the Boot ROM, the Boot ROM's single-lane Loader loading, and execution from Loader entry until `bsp_clock_init()` returns. For complete reset-to-main measurements, use the reset signal and a GPIO marker at main with an oscilloscope/logic analyzer. Do not label the times in this section as reset-to-main.

Enabling timing also means that GSC already includes this startup interval when PN takes over, rather than starting from zero shortly before main. Disable timing and rebuild if the original counter start point is required.

#### A/B Conditions and Measurements

- `Debug_App1_STANDARD`, CN8 fixed at 1-2, using the same IC19.
- Timing enabled in both versions; only `BSP_CFG_LOADER_OSPI_ENABLE` was changed, with the same application source and section layout. Both copied 1,246,848 App bytes.
- The application data and vector sections were byte-identical. The code section differed in 9 bytes, all verified as relocation-address changes targeting the same Loader symbols, not functional application-code changes. The two ELFs must not be described as identical.
- Each version was measured over 5 hardware resets. Execution stopped only at the `main()` call location in BTCM to read previously saved timestamps; it was not paused during Flash protocol switching or copying.
- Download time, debugger connection/reset waits, and host macro execution time were excluded from the GSC window. A software breakpoint at application main is overwritten by startup copying, so measurements used the BTCM call location, which App copying does not overwrite.

| Item | Single-lane 1S-1S-1S | Octal 8D-8D-8D |
|---|---:|---:|
| `copy_begin` (ticks) | 1,450-1,451 | 1,436 |
| `copy_end` (ticks) | 2,740,415-2,740,416 | 2,968,269 |
| `copy_end - copy_begin` (ticks) | 2,738,965 | 2,966,833 |
| `main` (ticks) | 4,741,499 | 4,969,352 |
| Copy-function time, mean of 5 runs (ms) | 109.55860 | 118.67332 |
| Time to main, mean of 5 runs (ms) | 189.65996 | 198.77408 |

Across the 5 runs, the single-lane before/after copy timestamps shifted together by 1 tick, leaving the difference unchanged. Each version's main timestamps were identical across its runs. Additional verification after restoring the octal firmware produced the same octal timestamps.

**Historical-version conclusion: with hardware prefetch disabled, octal operation was 9.11412 ms slower to main, approximately 4.81%. The copy function itself was 9.11472 ms slower, approximately 8.32%. The intended boot acceleration had not yet been achieved.** The octal copy-function time includes protocol switching, pin initialization, checks, data copying, and SPI restoration, not just useful bus-payload transfer. See Section 12.6 for subsequent prefetch optimization results.

The single-lane path used the original mapping and copy function. The octal path read the noncacheable alias word by word and corrected the byte arrangement. CPU access patterns and cache/burst utilization differed, so final acceleration cannot be inferred from bus width. These were directions for further performance analysis; individual overheads were not independently separated in this round, so the entire difference must not be attributed to `__REV16()` or any other single factor.

#### Verification and Final State

- Single-lane timing, octal timing, and octal-with-timing-disabled branches all compiled and linked in the main configuration with 0 errors and 0 warnings.
- The timing-enabled octal version's real ARM machine-code tests again passed 4,800 boundary cases and the four-section protocol simulation.
- The octal timing version was restored at the end. On the board, `stage=4/error=0`, xSPI0 returned to SPI, and all 1,246,848 copied RAM bytes matched the final ELF.
- Both NVdata slots, totaling 32KB, remained byte-identical before and after testing; user configuration was neither migrated nor rewritten.
- The final boot used no timing breakpoints, and COM3 confirmed `SYSTEM STARTUP FINISHED, OK`. The debugger and COM3 were released, leaving the board running.
- This round did not measure true power-off cold boot, complete reset-to-main, PN communication-ready time, or FSU compliance time.

### 12.5 Additional Four-Section Payload-Copy Measurements (2026-10-21, Before Prefetch Optimization)

Internal timestamps were added around the four-section copy originally covered by lines 891-916 of [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c). They measured only parameter preparation and the four copy calls for VECTOR, USER_PRG, USER_DATA, and USER_DATA_NONCACHE. They excluded SPI-to-DTR switching, CR2/mapping probes, DTR-to-SPI restoration, and subsequent App BSS clearing.

After inserting the timing code in that iteration, the four-section copy began at line 903. Source line numbers change during maintenance; use the actual calls for these four sections as the boundaries.

Two `volatile uint64_t` variables were added under the existing timing switch, still in the startup object's Loader BSS:

| Variable | Sampling point |
|---|---|
| `g_bsp_loader_timing_payload_begin` | Before the four-section copy; then line 900 of system.c, currently line 963 |
| `g_bsp_loader_timing_payload_end` | After the four-section copy and before SPI restoration; then line 930 of system.c, currently line 993 |

`bsp_loader_boot_ticks()` was reused and changed from a startup-file-local static function into an internal function shared between startup/system. It is not a public PN runtime API. `DSB` is executed before both start and end sampling; the barrier before end sampling includes waiting for copy writes to complete in the window.

```text
Payload-copy time (ms) = (payload_end - payload_begin) / 25000
```

"Payload-copy time" here means the actual execution time of this CPU copy code, including calls, loops, address handling, DTR byte-order correction, barriers, and a small sampling overhead. It is not the time of useful data transfer measured only at the Flash pins.

Using the same IC19, CN8 at 1-2, 5 hardware resets per version, and 1,246,848 copied bytes, there were no halt breakpoints inside the window. Previously saved results were still read at the main call location in BTCM:

| Item | Single-lane SPI | Octal DTR |
|---|---:|---:|
| `payload_begin` (ticks) | 1,485 | 1,926 |
| `payload_end` (ticks) | 2,740,449 | 2,968,108 |
| Difference (ticks) | 2,738,964 | 2,966,182 |
| Four-section payload copy, mean of 5 runs (ms) | **109.55856** | **118.64728** |
| Effective copy throughput (MB/s, 10^6 bytes) | 11.381 | 10.509 |
| Entire copy function in the same firmware (ms) | 109.56120 | 118.67600 |
| Entire-function window minus payload-copy window (us) | 2.64 | 28.72 |

The above results were identical across each version's 5 runs. Adding sampling points introduces a small overhead, so the entire function was remeasured for this table. Do not subtract this section's new payload-copy time from Section 12.4's old entire-function time to decompose overhead.

**Historical-version conclusion: even after excluding surrounding operations such as protocol switching, octal copying with hardware prefetch disabled was still 9.08872 ms slower than single-lane copying, approximately 8.30%.** The octal time outside the payload-copy window was only approximately 28.72 us, including sampling and function entry/exit overhead. It must not all be interpreted as protocol-switching time. The measurements show that the approximately 9 ms gap was mainly in the copy path itself, not protocol switching. Enabling prefetch alone subsequently produced a substantial improvement; see Section 12.6.

Further analysis should focus on access efficiency of the original cacheable SPI mapping versus word-by-word noncacheable DTR reads, CPU loops, and burst utilization. The copy algorithm was unchanged in this round, and the factors were not independently separated; a single root cause must not be asserted.

After adding payload-copy timing, both main configurations compiled and linked with 0 errors/warnings. Octal machine-code simulation again passed 4,800 boundary cases and the four-section protocol model. The octal timing version was restored at the end: `stage=4/error=0`, SPI restoration was normal, all 1,246,848 RAM bytes matched the final ELF, and the 32KB NVdata matched its pre-test contents. The final boot had no timing breakpoints, COM3 confirmed `SYSTEM STARTUP FINISHED, OK`, and both debugger and serial port were released.

### 12.6 Octal Hardware-Prefetch A/B Board Tests (2026-10-21, Historical Noncacheable-Source Version)

#### Change Scope and Safe Sequencing

This round tested only "enable xSPI hardware prefetch during octal copying while retaining noncacheable copying":

- The original 100 MHz clock configuration and 10 memory-read dummy clocks were retained.
- The `0x400...` noncacheable alias, `volatile` aligned word reads, `__REV16()`, and exact tail handling were retained.
- Compiler optimization was unchanged, no DMA was introduced, and the PN runtime SPI/NVdata driver was unchanged.
- `BSP_CFG_LOADER_OSPI_PREFETCH_ENABLE` was added, currently defaulting to 1. Setting it to 0 returns to the octal branch with prefetch disabled. Rebuild and download after changing it.
- `g_bsp_loader_ospi_copy_bmcfg` was added to record BMCFG during actual copying; bit 16 is `PREEN`.

At that time, lines 896-902 of [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c) enabled prefetch after the DTR vector check passed; lines 944-953 disabled it and cleared the buffer before restoring SPI. The original "disable prefetch" operation before switching was retained and must not simply be changed to enable prefetch. The current implementation also includes the source-cache maintenance from Section 12.7; use the key-symbol navigation and current source for current line numbers.

```text
Save SPI reference and original configuration
  -> Disable prefetch, wait for idle, disable mapped access, clear buffer
  -> Configure and verify DTR
  -> Wait for idle, clear buffer, enable PREEN
  -> Save copy-phase BMCFG; copy four sections from noncacheable sources
  -> Disable PREEN, wait for MEMACC=0, disable mapped access, clear buffer
  -> Restore Flash SPI, controller, and all original configuration
  -> Recheck SPI mapped reads
```

The basis is `BMCFG.PREEN` on page 2038, `BMCTL1.PBUFCLR` on page 2043, and the prefetch description on pages 2077-2078 of the RZ/N2L Hardware Manual Rev.1.10. The prefetch buffer is a FIFO; sequential reads can hit prefetched data. It is not CPU D-cache. Buffer clearing requires mapped accesses to be idle.

#### Measurement Method and Data

CN8 remained at 1-2, using the same IC19, application source, and section layout, with 1,246,848 copied bytes. Both versions used octal DTR; only the prefetch switch changed. Each was measured over 5 hardware resets without halt breakpoints inside the timing window. Timestamps were again read at the main call location in BTCM.

The copy-function machine code was compared between the two builds. The 148-byte function differed only in one PC-relative literal-pool displacement, both resolving to the same source-range-check constant `0x64000001`; copy hot-loop instructions were identical. The two ELFs must not be described as byte-identical.

| Item | Octal prefetch off | Octal prefetch on |
|---|---:|---:|
| BMCFG during copying | `0x00000000` | `0x00010000` |
| BMCFG before returning to main | `0x00000000` | `0x00000000` |
| `payload_begin` (ticks) | 1,927 | 1,924 |
| `payload_end` (ticks) | 2,968,109 | 588,895 |
| Payload-copy difference (ticks) | 2,966,182 | 586,971 |
| Four-section payload copy, mean of 5 runs (ms) | **118.64728** | **23.47884** |
| Entire copy function, mean of 5 runs (ms) | 118.67608 | 23.50796 |
| Clock initialization complete to main, mean of 5 runs (ms) | 198.77720 | 103.608896 |
| Effective copy throughput (MB/s, 10^6 bytes) | 10.509 | 53.105 |

Each version's payload-copy count difference was identical across its 5 runs. With prefetch enabled, the main timestamp was 2,590,222-2,590,223 ticks, or 103.60888-103.60892 ms; with prefetch disabled it was 4,969,430 ticks. Conversion remained `tick / 25000 = ms`.

**Conclusion: enabling hardware prefetch alone reduced payload-copy time by 95.16844 ms, approximately 80.21%, and increased effective copy speed to approximately 5.05 times the previous value. The bounded window to main decreased by approximately 95.16830 ms.** This single-factor experiment shows that the noncacheable read path with controller prefetch disabled was one of the main performance bottlenecks. No pin waveforms or AHB traces were used to measure actual frame lengths in this round, so it cannot be claimed that bus burst types were directly observed.

#### Data Correctness, Handoff, and Final State

- Both main configurations compiled and linked with 0 errors and 0 warnings.
- Both ARM machine-code tests passed 4,800 alignment/length/boundary cases and the four-section protocol simulation. Assertions were added for prefetch being off before protocol switching and manual commands, buffer clearing while idle, the copy-phase prefetch snapshot, and final register restoration.
- The simulator does not model the real prefetch FIFO or electrical behavior. Prefetch performance and real data correctness are based on this round's board tests.
- The final prefetch-enabled version reached `stage=4/error=0`. All eight saved controller registers matched their Loader-entry values: WRAPCFG, BMCFG, BMCTL0, CDCTL0, CMCFG0/1/2, and LIOCFGCS0.
- All 68 vector bytes, 1,169,712 program bytes, and 77,068 initialized-data bytes in RAM, totaling 1,246,848 bytes, matched the final ELF.
- The firmware region and both NVdata slots were fully backed up before testing. After testing, all 32KB of NVdata remained byte-identical, with SHA-256 `dab19e04382ece2b533613eb758f8e2892504c586e7fd714477f9fa951f6ea02`.
- The final reset/boot used no timing breakpoints. COM3 showed that the first NVdata copy was selected, followed by `SYSTEM STARTUP FINISHED, OK`. No new configuration save or online rename was triggered.
- At the end of this round, source, build outputs, and board retained the prefetch-enabled, noncacheable-source version, with a Loader code footprint of `0x16A8` bytes. It was subsequently upgraded to the version in Section 12.7.

This round compared prefetch off/on between two DTR builds and did not depend on the historical SPI baseline. It did not modify or validate the previously raised issue concerning ordinary SPI READ commands and frequency compliance. Measurements exclude the Boot ROM and Loader clock initialization and do not represent PN communication-ready time. True power-off cold boot, reset during DTR, prefetch at the end of the device, and voltage/temperature margins still require systematic acceptance testing.

### 12.7 Prefetch + Cacheable-Source A/B Board Tests (2026-10-21, Current Version)

#### Implementation and Measurement Boundaries

Both versions in this round retained octal operation, hardware prefetch, 100 MHz, 10 dummy clocks, `__REV16()`, and the original copy loop. MPU settings, global CPU cache switches, compiler optimization, the PN driver, and image byte layout were unchanged. The only functional switch was `BSP_CFG_LOADER_OSPI_CACHE_ENABLE`:

- 0: read from the `0x400...` noncacheable source, without the added source-cache maintenance.
- 1: read from the `0x600...` cacheable source, maintain all four Flash source ranges before and after copying, and compare cacheable/noncacheable reads of the final word after SPI restoration.

Both versions had `SCTLR=0x30C5183D`, with the D-cache bit set. "Noncacheable source" therefore does not mean that the CPU's global D-cache was disabled. Copy-phase `BMCFG=0x00010000`, restored to the entry value of 0 on return in both versions. The final word source addresses were `0x40152D08/0x60152D08`, respectively.

Source-cache maintenance follows Section 8.8: first remove any residual SPI data, then remove raw DTR-arranged data after copying. After restoring SPI, the CPU actually loads and compares the cacheable/noncacheable aliases of the final word; handoff is not judged solely by debugger reads of Flash. The current SPI value of that word is `0x00302E30`, which differs from its raw DTR arrangement.

`payload_begin/end` are currently at lines 963/993 of [system.c](rzn/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c), excluding both source-cache maintenance passes. `copy_begin/end` still surround the entire function, including both maintenance passes, prefetch management, protocol switching, SPI checks, and the new cacheable-read check. CN8 was fixed at 1-2. Each final version was measured over 5 hardware resets, with no halt inside the sampling window. Results follow.

#### Final Results Across 5 Runs

| Item | Prefetch + noncacheable source | Prefetch + cacheable source |
|---|---:|---:|
| Mean four-section payload-copy time (ms) | **23.478912** | **11.111600** |
| Mean entire copy-function time (ms, including added cache maintenance) | **23.508072** | **11.732880** |
| Mean clock-initialization-complete-to-main time (ms) | 103.609712 | 91.833960 |
| Effective copy throughput (decimal MB/s) | 53.105 | 112.211 |
| Entire-function time minus payload-copy time (ms) | 0.029160 | 0.621280 |

Raw ticks, in the order `payload_begin / payload_end / copy_begin / copy_end / main`:

```text
Noncacheable, runs 1-4: 1942 / 588915 / 1455 / 589157 / 2590243
Noncacheable, run 5:    1942 / 588914 / 1455 / 589156 / 2590242
Cacheable, runs 1-5:    9251 / 287041 / 1444 / 294766 / 2295849
```

GSC ran at 25 MHz, still using `tick / 25000 = ms`. Noncacheable payload-copy time ranged from 23.47888 to 23.47892 ms, and entire-function time from 23.50804 to 23.50808 ms. All 5 cacheable-version counts were identical.

**Conclusion: payload-copy time decreased by a further 12.367312 ms, approximately 52.67%, for approximately 2.11 times the previous speed. Including the added maintenance and checks, the entire function still decreased by 11.775192 ms, approximately 50.09%.**

The surrounding-window difference increased by approximately 0.59212 ms, mainly from the two source-cache maintenance passes and the added handoff check. This is an aggregate overhead obtained by subtraction, not an independently measured duration of a particular cache API. The net benefit is approximately 11.78 ms; reporting only the 12.37 ms payload-copy saving would be incomplete.

The noncacheable baseline in this round differs from Section 12.6 by tens of nanoseconds to sub-microsecond amounts. It was remeasured after adding source-address/SCTLR diagnostics and changing code layout; timings from old ELFs were not mixed into these results.

#### Correctness Acceptance and Retained State

- Both main configurations compiled and linked with 0 errors and 0 warnings. Both final versions' machine-code tests passed 4,800 alignment/length/boundary cases and the four-section protocol model.
- The model verified that actual CPU loads selected the correct source alias and that four-section source maintenance was called once at stage 1 and once at stage 3. It does not model real cache-line fills, invalidation, or hardware FIFOs and cannot replace this round's board tests.
- All eight saved controller registers in the final cacheable version returned to their entry values. `stage=4/error=0`; both the noncacheable SPI vector check and the new CPU cacheable final-word check passed.
- The final RAM VECTOR section of 68 bytes, USER_PRG of 1,169,712 bytes, and USER_DATA of 77,068 bytes, totaling 1,246,848 bytes, all matched the ELF actually downloaded. The NONCACHE section was empty.
- Firmware and NVdata were fully backed up before downloading. After testing, all 32KB of NVdata remained byte-identical, with SHA-256 still `dab19e04382ece2b533613eb758f8e2892504c586e7fd714477f9fa951f6ea02`. No new NVdata erase/program operation or online rename was performed.
- The final reset used no timing breakpoints. Serial output confirmed selection of the first NVdata copy and showed `SYSTEM STARTUP FINISHED, OK`.
- OSPI, PREFETCH, CACHE, and TIMING are all currently enabled, and the source, final ELF, and board are consistent. Loader code footprint is `0x17A8`, within the `0x6000` budget. The debugger and COM3 were released, leaving the board running.

This round covered only the current firmware layout and the two source-mapping options with prefetch enabled. The combination of prefetch disabled and cacheable source enabled has not passed acceptance testing. Power-off cold boot, reset during DTR, real cache/prefetch behavior at the end of the device, and complete PN network FSU still require dedicated verification. The previously noted ordinary SPI READ command/frequency issue was not modified.

## 13. Usage Limits, Troubleshooting, and Further Acceptance Testing

| Symptom / risk | What to check |
|---|---|
| Default IDE download fails with CN8 at 1-2 | Confirm that the adapted board file is selected and Flash is already in SPI mode |
| error 5 | CR2 command, protocol, address, and register-read dummy clocks |
| error 6 | Mapped command, Flash contents, byte order, pins, dummy clocks, and DS timing |
| error 7/8 | Whether both Flash and controller are restored in the correct order, and whether cacheable sources are invalidated before returning to SPI |
| Failure after changing frequency, board, or Flash | Revalidate sampling, dummy clocks, delays, and pin electrical parameters; do not assume previous results still apply |
| Reset during DTR | Flash may retain DTR while the Boot ROM expects SPI; validate the Flash hardware-reset / power-reset strategy |

Recommended acceptance sequence:

1. Fix CN8 at 1-2 and confirm that the image is actually downloaded to IC19.
2. Confirm stage 4 and error 0, and verify the full copied contents.
3. Verify PN startup, NVdata reads/writes, and recovery after restart.
4. <span style="color: red;">Identify the OHA error during online renaming and complete configuration tests driven by network tools.</span>
5. Test true power-off/power-on, reset during normal operation, and reset interrupting DTR.
6. Continue evaluating CPU copy efficiency using the prefetch + cacheable-source version verified in Section 12.7, then measure complete reset-to-main and the actual PN FSU endpoint.

As of this record, the bounded GSC A/B data in Sections 12.4-12.7 is available, and both prefetch and cacheable sources substantially improve copying. However, **there are still no complete reset-to-main measurements or conclusions on PN FSU compliance. User-observed power-on behavior has not been treated as a substitute for systematic cold-boot/abnormal-reset acceptance testing.** Do not claim that the entire boot is eight times faster based solely on bus width: ROM loading, protocol switching, CPU copying, and PN initialization all incur overhead.

## 14. Maintenance Checklist

- Do not move protocol-switching code or its dependencies into an App region that has not yet been copied.
- Keep Loader data/BSS, stack, and execution addresses valid in internal RAM.
- Preserve register save/restore, both probes, and failure paths. Do not silently skip checks just to make the system boot.
- When prefetch is enabled, preserve the sequence of disabling it before protocol switching and clearing its buffer only while idle. Do not carry DTR-prefetched data into later SPI accesses.
- With cacheable sources enabled, preserve paired source-cache invalidation before and after copying and the CPU-read check after SPI restoration. Do not arbitrarily perform global invalidation on destination RAM.
- After changing section layouts, check source ranges, destination capacity, the Loader budget, and the programming algorithm's download range.
- Continue ensuring aligned-word DTR reads and byte swapping before tail extraction.
- Do not confuse the 4 dummy clocks for CR2 register reads with the 10 dummy clocks for memory reads.
- Do not place Flash-dependent software breakpoints, executing code, or debugger auto-read windows in the interval when protocols do not match.
- Preserve the single-lane handoff to the original PN driver. If octal runtime access is required in the future, treat it as a separate, complete driver-adaptation task.
- After upgrading IAR, reinstall/recheck the custom programming files. Custom files in the standard installation directory are not guaranteed to survive an upgrade.

---

Maintenance references: current source, [implementation and board-test records](LOADER_OSPI.txt), page 12 of RSK schematic D017892_04, and the command tables, CR2, DTR-read, and volatile-configuration-write timing descriptions in the MX25UW51245G datasheet.
