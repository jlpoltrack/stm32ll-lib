# STM32C5 Support — Notes

Branch: `C5-Support`, head `974e7f4` (2026-08-12).
Target devices: **STM32C531KC** (Cortex-M33, 144 MHz, 256 KB dual-bank flash, 64 KB SRAM, LQFP-64)
and **STM32C551/C552/C562** in packages **up to 64 pins**. C5A3 and above are out of scope.
Family gate: `STM32C5` (matches existing `STM32G4`, `STM32WL` pattern). No sub-family gate — see 2.11.

Status: compiles clean against the real CubeC5 (HAL2) headers for `STM32C552xx`, and a downstream
project builds and links against it — mLRS `rx-diy-WeAct-E22-c552ce` (WeAct STM32C552CET6 CoreBoard
+ E22/SX1262), ~51 KB flash / ~14 KB RAM. **Nothing has been run on silicon.**

## 1. What was added

### Files touched

| Area | Files | Notes |
|---|---|---|
| Foundation | `src/stdstm32-peripherals.h`, `src/stdstm32-mcu.h` | RCC bus map, GPIO AF, `_tim_devider`, bootloader address, HAL2 shims (2.13) |
| SPI template | `stdstm32-spi-template.h` + regen of `src/stdstm32-{spi,spib,spic}.h` | v2 IP path inside template |
| UART template | `stdstm32-uart-template.h` + regen of `src/stdstm32-{uart,uartb,uartc,uartd,uarte,uartf}.h` | FIFO, NECF, AN2606 systemboot |
| Long-tail | `src/stdstm32-{uart-sw,eeprom}.h` | Per-peripheral add + correctness fixes |
| **Out of scope** | `src/stdstm32-{adc,dac,i2c}.h` | Deliberately untouched, see 2.5 |
| Build hygiene | `.gitignore` | vendor driver/doc dirs and `docs/*.pdf` excluded |
| **No edits needed** | `src/stdstm32-{delay,stack,subghz}.h` | Family-agnostic or WL-only |

### Incidental fix (not C5-specific)

`UART$_USE_UART6_PC6PC7` was listed in the template's interface block but was missing from the
`#elif` chain that selects USART6, so defining it alone selected no UART at all. Fixed in
`stdstm32-uart-template.h` and the six generated files. This affects F4/F7, where `PC6/PC7` is
the canonical USART6 mapping — not a C5 change, just something found during this work.

## 2. What was different from the original "mirror G4" expectation

The initial expectation was "C5 mirrors G4 with minor renames." Implementation discovered several substantive divergences, summarised here.

### 2.1 SPI is v2 IP (biggest surprise)

C5 SPI is **not** the classic SPI v1 the rest of the lib uses. Differences:

- Status flags renamed: `RXNE / TXE / BSY` → `RXP / TXP / EOT` (+ new `SUSP`).
- Master transfers require `LL_SPI_SetTransferSize(N) + LL_SPI_StartMasterTransfer()` instead of just enabling and writing TDR.
- No `LL_SPI_Init` / `LL_SPI_InitTypeDef`: use `LL_SPI_SetConfig(p_spix, cfg1_bits, cfg2_bits)` with manually-OR'd bitmasks.
- Constants renamed: `LL_SPI_POLARITY_*` → `LL_SPI_CLOCK_POLARITY_*`, `LL_SPI_PHASE_*` → `LL_SPI_CLOCK_PHASE_*_EDGE`, `LL_SPI_BAUDRATEPRESCALER_DIV*` → `LL_SPI_BAUD_RATE_PRESCALER_*`.
- `LL_SPI_TransmitData8` / `LL_SPI_ReceiveData8` still exist (v1-compat data calls).

Resolution: a second compile path inside `stdstm32-spi-template.h` gated on `#if defined STM32C5`. The v1 path is preserved unchanged for all other families. C5 init uses **continuous master mode** (`SetTransferSize(0)`) so per-byte `transmitchar` only polls `TXP`/`RXP` — no per-byte teardown.

### 2.2 USART is V2+ with FIFO

- FIFO: yes. C5 grouped with G4/WL in the register-style block (`LL_USART_ISR_RXNE_RXFNE`, `_TXE_TXFNF`).
- ICR flag naming: `NECF` (not `NCF`). Grouped with G4/L4/WL.
- `LL_USART_PRESCALER_DIV1` present. Grouped with G4.
- **FIFO threshold macro name diverges**: G4 = `LL_USART_FIFOTHRESHOLD_1_8`, C5 = `LL_USART_FIFO_THRESHOLD_1_8` (underscore). Split into separate `#if defined G4` / `#elif defined STM32C5` arms.

### 2.3 LPUART1 on a new bus (APB3)

Existing lib's `rcc_init_uart` used `LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_LPUART1)`. C5 puts LPUART1 on `LL_APB3_GRP1_EnableClock(LL_APB3_GRP1_PERIPH_LPUART1)`. Implementation rewrote the LPUART1 line as a positive-guard fork with `#error` catch-all so unhandled families fail loudly.

### 2.4 LPUART1 PA2/PA3 silicon swap

C5 AF table: `PA2 = LPUART1_RX` (AF3), `PA3 = LPUART1_TX` (AF3). Opposite of G4/WL convention. Workaround inside the existing `UART$_USE_LPUART1_PA2PA3` arm: `#undef UART$_TX_IO` and `UART$_RX_IO` after the default G4-style assignment, then re-`#define` swapped (TX=PA3, RX=PA2) and set AF to `IO_AF_3`.

### 2.5 ADC, DAC and I2C are out of scope

These three were added in an earlier cut of this work and have been **reverted to their
pre-C5 state**; `src/stdstm32-{adc,dac,i2c}.h` are byte-identical to `main`. Anyone adding
them back should know the C5 IP differs substantially from G4: ADC and DAC are v2 IP with no
`LL_ADC_Init` / `LL_DAC_Init` (individual setters instead), ADC needs a new
`LL_ADC_SetChannelPreselection()` step, ADC+DAC share one RCC clock selector
(`LL_RCC_SetADCDACClockSource`), and I2C timing values must come from CubeMX2 for the actual
PCLK1 rather than being hardcoded.

### 2.6 EEPROM uses LL primitives (HAL diverged too far)

- C5 HAL is handle-based: no `HAL_FLASH_Unlock`, `HAL_FLASH_Lock`, `HAL_FLASHEx_Erase`, `HAL_FLASH_Program` in the form the existing eeprom.h expects.
- Replaced with `LL_FLASH_SetUnlockKey(KEY1/KEY2)` + `LL_FLASH_Lock`, `LL_FLASH_StartErasePage`, `LL_FLASH_EnableProgramming` + raw 32-bit writes + flag waits.
- Flash page size = 8 KB. Dual-bank — `FLASH_PAGE_NB` is used dynamically so the code is correct across all C5 variants (256/512/1024 KB total flash).
- Required canonical sequence (per C5 HAL reference): clear EOP + ERRORS_ALL before each op, wait on `LL_FLASH_FLAG_STATUS_ALL` (= BSY|WBNE|DBNE) not just BSY, call `LL_FLASH_DisablePageErase` after each erase. The current implementation enforces all of these; an earlier draft had brick-risk bugs (hardcoded 16 pages-per-bank, partial-flag waits, missing PER clear) — kept in mind in case any of these patterns reappear during future edits.
- `EE_USE_QUADWORD` / `FLASH_ProgramQuadWord()` are dead scaffolding: they only ever served H5,
  and nothing enables them now — `FLASH_ProgramQuadWord()` returns `FLASH_STATUS_ERROR_PG`. Left
  in place because the machinery is family-neutral and woven through the page-copy loop.

### 2.7 EEPROM must not be written with the ICACHE enabled

C5 flash is cacheable, so erasing or programming it with the ICACHE on leaves stale cache lines
behind. `ee_hal_unlock()` records the ICACHE state, clears the busy-end flag and disables the
cache; `ee_hal_lock()` waits for the invalidate that the *disable* kicks off — per the RM's
ICACHE chapter software must test BSYENDF before re-enabling, else early accesses are silently
treated as noncacheable — and then restores it. Guarded by `#if defined ICACHE` so it compiles
out on families without the peripheral. Every erase/program path in the file sits inside an
`ee_hal_unlock`/`ee_hal_lock` bracket, so nothing escapes the guard.

### 2.8 Bootloader entry needs MSPLIM lowered, not a memory remap

`LL_SYSCFG_REMAP_SYSTEMFLASH` does **not** exist on C5 — SYSCFG is replaced by SBS (the driver
package ships `ll/stm32c5xx_ll_sbs.h` and no `ll_syscfg`). No remap is done, and the remap is not
what makes the jump work.

What actually blocks bootloader entry is **MSPLIM**. C5 is Cortex-M33 (ARMv8-M) and CubeMX startup
code sets MSPLIM to the top of RAM; the bootloader's own MSP sits well below that, so `__set_MSP()`
faults immediately unless MSPLIM is lowered first. `mcu.h::BootLoaderInit` therefore has a
`#if defined STM32C5` arm that lowers MSPLIM, turns the ICACHE off (an enabled ICACHE interferes
with bootloader execution) and points `SCB->VTOR` at the bootloader before jumping.

Diagnosed on the H5 side originally; C5 is Cortex-M33 too and would have faulted identically.
Implemented, **untested on silicon**.

Two further C5 arms were added to `BootLoaderInit` once it was compiled for real (2.13): the
`LL_xxx_DeInit()` block is skipped entirely — those functions do not exist in HAL2, and `HAL_DeInit()`
above has already reset the peripherals — and the sysclk source is `LL_RCC_SYS_CLKSOURCE_HSIDIV3`
rather than `_HSI`, C5 having split the HSI entries. HSI/3 = 48 MHz is exactly what the bootloader
runs on per AN2606 Table 23. The bootloader itself is at `0x0BF80080` (33 KB of system memory),
Pattern19 / boot model V1 / version V16.1, which is what the lib's C5 arm already had. Boot
peripherals: USART1 PA9/PA10, USART2 PA2/PA3, FDCAN1 PB5/PB6, SPI1 PA4-PA7, and **USB DFU on
PA11/PA12**.

### 2.9 DMA architecture changed

C5 has `LPDMA1` / `LPDMA2` (low-power DMA), no classic `DMA1` / `DMA2`, no `DMAMUX`. The existing lib's `rcc_init_dma` self-gates via `#if defined(DMA1)` so it compiles to nothing on C5 — harmless.

### 2.10 C531KC peripheral subset

The C531KC package has a leaner peripheral set than the plan assumed. Notably **absent**: `USART3`, `USART6`, `UART7`, `UART8`, `SPI3`, `TIM3`, `TIM16`, `TIM17`. The UART and SPI templates detect the missing USART3/SPI3 through the CMSIS presence macros (`#if !defined USART3`, `#if !defined SPI3`) rather than a part-number list — see 2.11. `USART6`/`UART7`/`UART8` `#error` on every C5. The timers are not gated by the lib.

The C55x parts have **no TIM3 and no TIM4** either — present are TIM1/8, TIM2/5/12/15/16/17, TIM6/7
and LPTIM1. Downstream projects that default a general-purpose timer to TIM3 must move it (mLRS's
C552 target uses TIM5, which is 32-bit and free). Also **PB15 has no FDCAN function on C5**; use
PB12 = FDCAN1_RX **AF10** and PB13 = FDCAN1_TX **AF9** — note the two pins take different AFs.

### 2.11 C551/C552/C562 up to 64 pins

**No sub-family define is used.** An earlier cut introduced `STM32C5_C55x`, gating on
`STM32C551xx || STM32C552xx || STM32C562xx`, and it was removed: every difference it expressed turned
out to be *peripheral presence*, which CMSIS already answers. `USART3` and `SPI3` are defined by the
device header only on parts that have them, so the C5 arms test `#if !defined USART3` / `#if !defined SPI3`
— the same idiom the lib already uses in `stdstm32-peripherals.h` and `stdstm32-mcu.h`.

This matters because a hand-maintained part-number list fails *wrong*, not safe: any C5 not on the list
would have been told it has no USART3 and no SPI3. The presence macros are right for C531/532/542, for
C5A3, and for parts that do not exist yet, with no edit. A sub-family define would only be justified by
real per-die **AF** differences for a peripheral both dies have, which no presence macro can encode.
C5 has no established difference of that kind.

The C552 and C562 datasheets (DS14928 / DS14927 Rev 2, Table 13/14) were parsed and diffed: their
**UART and SPI alternate-function maps are identical**, so one gate covers both, and C551 shares the
C55x line. Neither has `USART6`, `UART7` or `UART8` at all — those keep their `#error` on every C5.

Pin choices assume **at most 64 pins**. Differences from C531 that matter:

| | C531 | C551/C552/C562 up to 64 pins |
|---|---|---|
| USART1 | `PA9/PA10`, `PB6/PB7` AF7 | same, plus `PB14/PB15` AF4 |
| USART3 | absent | `PC10/PC11` AF7 (`PB3/PB4` AF11 exists, not offered) |
| UART4 | `PA0/PA1` AF8 | same, plus `PC10/PC11` AF8 |
| UART5 | `PC12/PD2` AF8 | same |
| LPUART1 | `PA2/PA3` AF3 **swapped** | same, plus `PA9/PA10` AF3 and `PB6/PB7` AF8, neither swapped |
| SPI3 | absent | `PC10`/`PC11`/`PC12` AF6 |

Two traps worth calling out:

- **SPI3 `PB3/PB4/PB5` is mixed-AF on C55x** — SCK and MISO are AF6 but `PB5` is SPI3_MOSI on **AF7**.
  The lib applies one AF to all three SPI pins, so the C55x arm moves the whole set to `PC10/PC11/PC12`
  rather than introducing per-pin AFs.
- **`PD5/PD6` is a real USART2 mapping but is not bonded** at or below 64 pins, so it `#error`s.

`UART$_USE_UART1_PB14PB15` resolves to AF4 on every C5. The AF4 is datasheet-verified on C551/C552/C562
only; no datasheet was available for the smaller parts, so this is an assumption, flagged in the code.

### 2.12 HAL module-enable define naming differs on C5

CubeC5 HAL uses `USE_HAL_X_MODULE`, where the lib gates on `HAL_X_MODULE_ENABLED`. `src/stdstm32-eeprom.h` has an `#error` guard at the top that fires if the legacy name isn't defined, so C5 users must define the FLASH equivalent manually. This is a structural limitation of the existing file layout, not specific to C5.

### 2.13 C5 ships HAL2, which breaks far more than naming (the second biggest surprise)

Everything above 2.12 was written against *assumed* API names — the branch had never been compiled
against the real driver package. It was, on 2026-08-12, and only SPI (2.1) came through with zero
errors. The through-line for every other failure is that **C5 is ST's new HAL2 package**, not the
Cube HAL every other family uses:

- Tree shape: `hal/` and `ll/` side by side (not `Inc/`+`Src/`), the **LL is header-only**, device
  headers live in a separate `stm32c5xx_dfp` repo, and startup files are `.c`, not `.s`.
- **The aggregate LL initializers are gone.** No `LL_GPIO_Init`, `LL_TIM_Init`, `LL_TIM_OC_Init`,
  `LL_USART_Init`, `LL_LPUART_Init` and none of their `*_InitTypeDef` structs — HAL2 ships only
  per-attribute setters. The lib shims all five (structs + functions) under `#if defined STM32C5`,
  so every shared function body stays family-neutral: GPIO/TIM in `stdstm32-peripherals.h`,
  USART/LPUART in `stdstm32-uart-template.h`.
- **`LL_xxx_DeInit()` is gone too** (it lived in the LL `.c` files). See 2.14.
- **`FunctionalState` is gone** — on every other family it comes from the CMSIS device header, and
  callers pass `ENABLE`/`DISABLE` around. Defined locally in the C5 block of
  `stdstm32-peripherals.h`.
- **Every register-access macro is renamed** with an `STM32_` prefix: `SET_BIT` →
  `STM32_SET_BIT`, and likewise `CLEAR_BIT` / `READ_BIT` / `CLEAR_REG` / `WRITE_REG` / `READ_REG` /
  `MODIFY_REG` / `POSITION_VAL`. All aliased back in the same C5 block.
- **USART/LPUART constants renamed**: `STOPBITS_1` → `STOP_BIT_1`, `DATAWIDTH_8B` →
  `DATAWIDTH_8_BIT`, `FIFOTHRESHOLD` → `FIFO_THRESHOLD` (the same underscore split noted in 2.2),
  `LL_USART_ReadReg`/`WriteReg` → `LL_USART_READ_REG`/`WRITE_REG`, and the RXNE/TXE interrupt calls
  gained FIFO suffixes (`LL_USART_EnableIT_TXE` → `_TXE_TXFNF`). Aliased.
- **No `LL_RCC_GetUSARTClockFreq()`**, so the shimmed `LL_USART_Init` derives the baud clock itself
  via `_ll_uart_periphclk()`: `SystemCoreClock` + `LL_RCC_CALC_PCLKn_FREQ`. It assumes every UART is
  left on its APB clock, which is the reset default of the `RCC_CCIPR` `xxxSEL` fields — USART1 on
  PCLK2, LPUART1 on PCLK3 (APB3, per 2.3), everything else PCLK1.
- `LL_TIM_CLOCKSOURCE_INTERNAL` → `LL_TIM_CLK_INTERNAL`; `LPTIM1` moved APB1 → APB3, like LPUART1.
- `IO_PE2`..`IO_PE6` were missing from the `GPIOE` block and are now defined — the WeAct C552 board
  puts its LED on PE2.

Two HAL2 gaps are **not** the lib's to fix and must be handled downstream:

- `HAL_StatusTypeDef` is `hal_status_t` in HAL2. Alias it in the *project's* `Core/Inc/main.h` —
  that is the file the lib's consumers include to pick up the family HAL types.
- `HAL_RCC_OscConfig`, `HAL_RCC_ClockConfig`, `HAL_NVIC_SetPriority` and `HAL_ICACHE_Enable` do not
  exist in HAL2, so a CubeMX-style `SystemClock_Config()` cannot be ported from another family and
  has to be rebuilt on LL_RCC. See 2.15.

### 2.14 `mcu_uid()` must read with the ICACHE off

Same hazard as 2.7, different peripheral: `UID_BASE` is `0x08FFF800` on C5, in the read-only/OTP
area that **cannot be cached** even though the AHB range is cacheable by default. Reading it with
the ICACHE enabled raises a bus error → HardFault. `mcu_uid()` therefore disables the ICACHE around
the read by direct register write (so it doesn't drag in `ll_icache`/`hal_icache`), waits on
`BSYENDF` for the invalidate the disable kicks off, then restores the previous state. Guarded by
`#if defined ICACHE`.

Note `FLASH_PAGE_NB` expands to a read of `FLASHSIZE_BASE`, right beside `UID_BASE` in the same
non-cacheable area. In the eeprom it is only evaluated inside the `ee_hal_unlock`/`ee_hal_lock`
bracket where the ICACHE is already off, so that path is safe — but **any new use of `FLASH_SIZE` /
`FLASH_PAGE_NB` outside such a bracket will fault.**

### 2.15 C5 has no PLL at all

Not a lib concern, but it shapes every downstream target and there is no other obvious place to
record it. RM0522 has zero `LL_RCC_PLL*` functions: SYSCLK can only be HSIS (144 MHz RC), HSIDIV3
(48 MHz), PSIS, or HSE *directly* — so an 8 MHz crystal on its own gives an 8 MHz SYSCLK. The C5
equivalent of "HSE + PLL" is the **PSI in PLL mode**, locked to HSE as reference:
`SetPSIClkSource(HSE)` / `SetPSIRef(8MHZ)` / `SetPSIFreqOutput(144MHZ)`, then `SetSysClkSource(PSIS)`.
Flash latency must be set **first** — `LL_FLASH_LATENCY_4WS` + `LL_FLASH_PROGRAM_DELAY_2` for
136–144 MHz, RM0522 Table 20. Gotchas: the LL_FLASH and LL_ICACHE calls take the peripheral pointer
(`LL_FLASH_SetLatency(FLASH, ...)`), latency is `_4WS` not `_4`, and GPIO clocks are on **AHB2**.

The H5 SPI kernel-clock trap does **not** transfer: `RCC_CCIPR.SPI1SEL` defaults to `rcc_pclk2`
(SPI2/3 to `rcc_pclk1`), which is a legal source on C5, so no `PeriphCommonClock_Config()` fixup is
needed.

## 3. What needs to be done

### Done since the first cut

- Compiles clean against the real CubeC5 headers, as a standalone TU for `STM32C552xx`
  (`-DSTM32C5 -DSTM32C552xx -DUSE_FULL_LL_DRIVER -mcpu=cortex-m33`). This is what turned up
  everything in 2.13; SPI was the only part that needed no changes.
- A downstream project builds and **links** — mLRS `rx-diy-WeAct-E22-c552ce`, ~51 KB flash /
  ~14 KB RAM. Link time is what surfaced the missing `LL_TIM_OC_Init` shim.
- G4 and F1 mLRS targets still build, confirming no other family is affected. Every C5 change is
  behind `#if defined STM32C5`.

### Verification still required (the load-bearing item)

**Nothing in this branch has been run on silicon.** Flash a C531KC or a C551/C552/C562 and confirm:

- SPI v2 continuous-master path actually streams bytes correctly (untested on silicon; library code is correct per the LL header API contract but has not been validated against real hardware).
- UART TX/RX through the FIFO path works at typical baud rates.
- EEPROM emulation erases and writes flash cleanly across the bank-2 boundary (page index 16+).
- `BootLoaderInit` actually reaches the bootloader on C5. The MSPLIM lowering of 2.8 is implemented but unverified; if entry still fails, an RM0522-derived SBS/remap write inside the existing `#if defined STM32C5` guard in `mcu.h` is the next thing to try.
- EEPROM writes survive with the ICACHE enabled (2.7).
- `mcu_uid()` returns a sane UID with the ICACHE enabled rather than HardFaulting (2.14).
- UART baud rates are actually right — `_ll_uart_periphclk()` (2.13) assumes the reset-default
  `RCC_CCIPR` clock selection, and a wrong assumption there shows up as a mis-scaled baud rate.

### Open feature gaps (deliberate, deferred)

| Item | Status | Notes |
|---|---|---|
| `LL_SYSCFG_REMAP_SYSTEMFLASH` equivalent on C5 | Not added, likely not needed | See 2.8 — MSPLIM was the actual blocker. Add an SBS/remap write only if hardware testing still shows a problem. |
| Bonus pin combos | Partly exposed | `LPUART1 PB6/PB7` (AF8) and `PA9/PA10` (AF3) are now in the template for C5. `USART3 PB3/PB4` (AF11) still is not. |
| FDCAN / SPI bootloader entry | Not in template | AN2606 lists FDCAN2 PB5/PB6 and SPI1/SPI2 slave modes for C531 boot; existing `UART$_HAS_SYSTEMBOOT` only handles UART-based boot. |
| C551/C552/C562 up to 64 pins | Pin map done, **not run on silicon** | See 2.11. Pin/AF maps checked against DS14928/DS14927 and resolved through the preprocessor; nothing tested on hardware. |
| C551/C552/C562 above 64 pins | Not mapped | Ports D (beyond PD2) and E are not in the defaults. Reachable via `UART$_USE_TX_IO` / `_RX_IO` / `_IO_AF`. |
| C5A3 / C591 / C593 | Out of scope | Presence macros give them the right USART3/SPI3 answer, but their pin/AF map was never checked. Verify against their datasheet before using. |
| C531/C532/C542 pin map | Inherited, mostly unverified | No datasheet was available. Their arms are unchanged except `USART1 PB14/PB15`, which now resolves to AF4 on the C55x evidence. |

### TIM clock tree — worth flagging

`_tim_devider()` returns 1 for C5 with a comment "all timer run on 144 MHz" — this mirrors G4's simplification and assumes default PLL config (HCLK = SYSCLK = 144 MHz, APBn prescalers = 1). If a downstream project reconfigures APB prescalers, the lib's TIM rate calculations will be off. RM0522 § RCC clock tree confirmation could harden this with a per-TIM table mirroring F7's pattern.

## 4. Quick how-to-use

For a downstream project targeting an STM32C5:

1. `#define STM32C5` (typically via the CMSIS device header which sets it).
2. `#define` the chip variant — `STM32C531xx`, or `STM32C551xx` / `STM32C552xx` / `STM32C562xx`
   for the parts that have USART3 and SPI3.
3. Add the lib's `src/` to the include path.
4. Add the CubeC5 drivers and DFP headers to the include path:
   - `<your-cubec5>/stm32c5xx_drivers/ll/`
   - `<your-cubec5>/stm32c5xx_drivers/hal/`
   - `<your-cubec5>/stm32c5xx_drivers/templates/common/` (for `stm32c5xx_hal_conf.h`, `stm32_assert.h`)
   - `<your-cubec5>/stm32c5xx_dfp/Include/`
   - `<cmsis_core>/Include/`
5. Define the FLASH module enable under the legacy name the lib expects (2.12), and alias
   `HAL_StatusTypeDef` → `hal_status_t` in your `Core/Inc/main.h` (2.13).
6. Build `SystemClock_Config()` on LL_RCC — there is no PLL and no `HAL_RCC_ClockConfig` (2.15).

Then `#include "stdstm32-peripherals.h"` and the per-peripheral headers as usual.

Startup is a `.c` file (`startup_stm32c5xxxx.c`), not `.s`, and linker templates are in
`stm32c5xx_dfp/Source/Templates/gcc/linker/`. They use ST's newer symbol names (`__end__`,
`STACK_SIZE`) rather than the `_end` / `_Min_Stack_Size` that CubeMX's `sysmem.c` expects, and their
`/DISCARD/` of libc/libm/libgcc has to go if you link the standard libraries.
