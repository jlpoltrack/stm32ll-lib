# STM32C5 Support — Notes

Branch: `C5-Support`.
Target devices: **STM32C531KC** (Cortex-M33, 144 MHz, 256 KB dual-bank flash, 64 KB SRAM, LQFP-64)
and **STM32C551/C552/C562** in packages **up to 64 pins**. C5A3 and above are out of scope.
Family gate: `STM32C5` (matches existing `STM32G4`, `STM32WL` pattern). No sub-family gate — see 2.11.

## 1. What was added

### Files touched

| Area | Files | Notes |
|---|---|---|
| Foundation | `src/stdstm32-peripherals.h`, `src/stdstm32-mcu.h` | RCC bus map, GPIO AF, `_tim_devider`, bootloader address |
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

### 2.9 DMA architecture changed

C5 has `LPDMA1` / `LPDMA2` (low-power DMA), no classic `DMA1` / `DMA2`, no `DMAMUX`. The existing lib's `rcc_init_dma` self-gates via `#if defined(DMA1)` so it compiles to nothing on C5 — harmless.

### 2.10 C531KC peripheral subset

The C531KC package has a leaner peripheral set than the plan assumed. Notably **absent**: `USART3`, `USART6`, `UART7`, `UART8`, `SPI3`, `TIM3`, `TIM16`, `TIM17`. The UART and SPI templates detect the missing USART3/SPI3 through the CMSIS presence macros (`#if !defined USART3`, `#if !defined SPI3`) rather than a part-number list — see 2.11. `USART6`/`UART7`/`UART8` `#error` on every C5. The timers are not gated by the lib.

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

## 3. What needs to be done

### Verification still required (the load-bearing item)

End-to-end hardware build of a downstream project (e.g. mLRS) against this branch, targeting an
STM32C531KC or a C551/C552/C562, and confirm:

- SPI v2 continuous-master path actually streams bytes correctly (untested on silicon; library code is correct per the LL header API contract but has not been validated against real hardware).
- UART TX/RX through the FIFO path works at typical baud rates.
- EEPROM emulation erases and writes flash cleanly across the bank-2 boundary (page index 16+).
- `BootLoaderInit` actually reaches the bootloader on C5. The MSPLIM lowering of 2.8 is implemented but unverified; if entry still fails, an RM0522-derived SBS/remap write inside the existing `#if defined STM32C5` guard in `mcu.h` is the next thing to try.
- EEPROM writes survive with the ICACHE enabled (2.7).

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
4. Add the CubeC5 LL drivers and DFP headers to the include path:
   - `<your-cubec5>/stm32c5xx_drivers/ll/`
   - `<your-cubec5>/stm32c5xx_dfp/Include/`

Then `#include "stdstm32-peripherals.h"` and the per-peripheral headers as usual.
