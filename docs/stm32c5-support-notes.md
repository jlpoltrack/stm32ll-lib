# STM32C5 Support — Notes

Branch: `C5-Support`, last updated 2026-10-12.
Target devices: **STM32C531KC** (Cortex-M33, 144 MHz, 256 KB dual-bank flash, 64 KB SRAM, LQFP-64)
and **STM32C551/C552/C562** in packages **up to 64 pins**. C5A3 and above are out of scope.
Family gate: `STM32C5` (matches existing `STM32G4`, `STM32WL` pattern). No sub-family gate — see 2.11.

Status: compiles clean against the real CubeC5 (HAL2) headers for `STM32C552xx`, `STM32C562xx` and
`STM32C531xx`, every UART/SPI/I2C mapping included (3), and mLRS builds against it for the WeAct
STM32C552CET6 CoreBoard (`rx-`/`tx-diy-WeAct-E22-c552ce`). The ADC, I2C and EEPROM fixes in the
commit log came out of running that board. The 2026-10-12 changes — the I2C interrupt rework, the
`mcu_uid()` ICACHE sequence, the SPI suspend guard and the added pin mappings — are compile-tested
only.

## 1. What was added

### Files touched

| Area | Files | Notes |
|---|---|---|
| Foundation | `src/stdstm32-peripherals.h`, `src/stdstm32-mcu.h` | RCC bus map, GPIO AF, `_tim_devider`, bootloader address, HAL2 shims (2.13) |
| SPI template | `stdstm32-spi-template.h` + regen of `src/stdstm32-{spi,spib,spic}.h` | v2 IP path inside template |
| UART template | `stdstm32-uart-template.h` + regen of `src/stdstm32-{uart,uartb,uartc,uartd,uarte,uartf}.h` | FIFO, NECF, AN2606 systemboot |
| Long-tail | `src/stdstm32-{uart-sw,eeprom,adc,i2c}.h` | Per-peripheral add + correctness fixes, see 2.5 - 2.7 |
| **Out of scope** | `src/stdstm32-dac.h` | F3 only, untouched |
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

### 2.5 ADC and I2C are in, DAC is still out

ADC and I2C were out of scope in the first cut and have since been added, for the mLRS OLED
and 5-way switch on the WeAct C552 board. `stdstm32-dac.h` is F3 only and was left alone.

ADC:

- **No `LL_ADC_Init` / `LL_ADC_REG_Init` / `LL_ADC_CommonInit`.** Those live in the `_ll_*.c`
  files that HAL2 does not ship, so `adc_init_one_channel()` has a separate C5 arm built from
  the individual setters. There is no data alignment setter either, C5 data is always right
  aligned.
- **The ADC comes out of reset in deep power down, where config writes are lost.**
  `adc_init_begin()` therefore leaves deep power down and brings the regulator up before any
  config, and first stops and disables an ADC that is still running from before a soft restart.
- **`LL_ADC_SetChannelPreselection()` is mandatory.** PCSEL drives the analog switch inside the
  I/O, and RM0522 21.4.11 requires it for every channel placed in the sequencer. Miss it and
  the input simply reads nothing.
- **`LL_ADC_SetGainCompensation(ADCx, 0)` zeroes every result.** It enables GCOMP with a
  coefficient of 0. Unity gain is `LL_ADC_GAIN_COMPENSATION_DIV`.
- **ADC+DAC share `LL_RCC_SetADCDACClockSource`,** and there is no prescaler inside the ADC.
  fADC is specified as 8..36 MHz, so the kernel clock has to be in range already.
  `adc_init_begin()` uses **HSIK at /4.5 = 32 MHz**. Not PSIK: its divider is programmed by the
  FDCAN kernel clock setup, and the two would fight over `PSIKDIV`.
- **Constants were renamed:** `LL_ADC_IN_SINGLE_ENDED` (not `LL_ADC_SINGLE_ENDED`), and the
  sampling time ladder is 3/5/8/13/25/48/139/289 cycles.
- ADC1 and ADC2 share one clock enable (`ADC12`). `ADC_USE_DMA` is not implemented and `#error`s.
- Every wait is bounded, the init runs with the interrupts off and must not hang the firmware.

I2C — the C5 arm is LL only. HAL2 has no `I2C_HandleTypeDef` and no DMAMUX, so none of the
classic HAL/DMA path survives, see 2.9:

- `i2c_put()` sends on LPDMA, everything else is polled. Completion is taken from the I2C STOP
  flag rather than the DMA transfer complete, which fires one byte early. Reads are stubbed out.
- **NBYTES is 8 bits**, so transfers over 255 bytes go out in RELOAD chunks, reloaded on TCR —
  from the event ISR on the DMA path, inline on the polled one.
- **The I2C interrupts are enabled only while a DMA transfer is in flight.** The polled paths
  wait on the same STOP and TCR flags, and a live ISR clears them first: every polled transfer
  then ran into its timeout and reported an error.
- **Errors end a DMA transfer too.** BERR, ARLO and an SCL low timeout raise the error interrupt
  (`I2Cx_ERR_IRQn` — C5 spells it `ERR`, not `ER`), whose handler stops the DMA and resets the
  peripheral. The SCL timeout is the peripheral's own TIMEOUTA counter, `I2C_SCL_LOW_TMO`,
  58 ms by default, so no timebase is needed. A NACK brings its own STOP; the ISR then also
  flushes the byte the DMA left in TXDR.
- `I2C_TIMINGR` defaults to `0xF042060D` for a 144 MHz PCLK1: PRESC is 4 bits, so the finest
  tick is /16 = 111.1 ns, and 14 + 7 ticks give ~390 kHz with the rise times. Override it per
  target for another clock or speed.
- I2C1 (default PB8/PB9) and I2C2 (default PB10/PB12) are supported. Other pins through
  `I2C_USE_SCL_IO` / `I2C_USE_SDA_IO` and `I2C_USE_IO_AF` or the per pin `I2C_USE_SCL_IO_AF` /
  `I2C_USE_SDA_IO_AF`, see 2.11. `I2C_LPDMA_CHANNEL` selects the LPDMA channel.

### 2.6 EEPROM uses LL primitives (HAL diverged too far)

- C5 HAL is handle-based: no `HAL_FLASH_Unlock`, `HAL_FLASH_Lock`, `HAL_FLASHEx_Erase`, `HAL_FLASH_Program` in the form the existing eeprom.h expects.
- Replaced with `LL_FLASH_SetUnlockKey(KEY1/KEY2)` + `LL_FLASH_Lock`, `LL_FLASH_StartErasePage`, `LL_FLASH_EnableProgramming` + raw 32-bit writes + flag waits.
- Flash page size = 8 KB. Dual-bank — `FLASH_PAGE_NB` is used dynamically so the code is correct across all C5 variants (256/512/1024 KB total flash).
- Required canonical sequence (per C5 HAL reference): clear EOP + ERRORS_ALL before each op, wait on `LL_FLASH_FLAG_STATUS_ALL` (= BSY|WBNE|DBNE) not just BSY, call `LL_FLASH_DisablePageErase` after each erase. The current implementation enforces all of these; an earlier draft had brick-risk bugs (hardcoded 16 pages-per-bank, partial-flag waits, missing PER clear) — kept in mind in case any of these patterns reappear during future edits.
- **The main flash programs in 128 bit quad words only**, like the H5. A double word leaves half
  a line unprogrammed, the next one reprograms it, and that breaks the line's ECC — which raises
  an **NMI** when read. So C5 forces `EE_USE_QUADWORD` and `FLASH_ProgramDoubleWord()` refuses.
  The four word writes are done with the interrupts masked, a full line starts the write itself.
- Every flash status poll is bounded, a half filled write buffer keeps WBNE set forever.
- A reset in the middle of a program can still leave a line with broken ECC, and `ee_init()` then
  takes the NMI when it reads the page. The project's NMI handler should clear `ECCD` and return,
  the lib does not install one.

### 2.7 EEPROM must not be written with the ICACHE enabled

C5 flash is cacheable, so erasing or programming it with the ICACHE on leaves stale cache lines
behind. `ee_hal_unlock()` records the ICACHE state and disables the cache; `ee_hal_lock()`
re-enables it and runs an explicit `CACHEINV`, then waits (bounded) for `BSYENDF`. **`BSYENDF` is
raised by `CACHEINV` only** — an earlier version waited on it right after clearing EN, and spun
forever. Guarded by `#if defined ICACHE` so it compiles out on families without the peripheral.
Every erase/program path in the file sits inside an `ee_hal_unlock`/`ee_hal_lock` bracket, so
nothing escapes the guard.

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
runs on per AN2606 Table 23. HSIDIV3 is enabled and waited for first (bounded), a target running
from PSI/HSE does not have it on. The bootloader itself is at `0x0BF80080` (33 KB of system memory),
Pattern19 / boot model V1 / version V16.1, which is what the lib's C5 arm already had. Boot
peripherals: USART1 PA9/PA10, USART2 PA2/PA3, FDCAN1 PB5/PB6, SPI1 PA4-PA7, and **USB DFU on
PA11/PA12**.

### 2.9 DMA architecture changed

C5 has `LPDMA1` / `LPDMA2` (low-power DMA), no classic `DMA1` / `DMA2`, no `DMAMUX`. The existing lib's `rcc_init_dma` self-gates via `#if defined(DMA1)` so it compiles to nothing on C5 — harmless.

Note that this is not the same as "C5 has no DMA". Both LPDMAs are full masters on the AHB
matrix (RM0522 Table 1), both list I2C/SPI/USART hardware requests, and RM0522 2.2.3 states
SRAM1 and SRAM2 "can be addressed both by CPU and DMAs" — so there is no restricted-access
limitation like U5's LPDMA, and DMA buffers need no special linker placement. What changed is
the programming model: it is the U5/H5-style channel API (`LL_DMA_ConfigAddresses`,
`LL_DMA_SetBlkDataLength`, `LL_DMA_SetPeriphRequest`, `LL_DMA_EnableChannel`) with the request
selected per channel instead of through a DMAMUX. The I2C arm uses it single-shot; none of the
linked-list machinery is needed.

### 2.10 C531KC peripheral subset

The C531KC package has a leaner peripheral set than the plan assumed. Notably **absent**: `USART3`, `USART6`, `UART7`, `UART8`, `SPI3`, `TIM3`, `TIM16`, `TIM17`. The UART and SPI templates detect the missing USART3/SPI3 through the CMSIS presence macros (`#if !defined USART3`, `#if !defined SPI3`) rather than a part-number list — see 2.11. `USART6`/`UART7`/`UART8` `#error` on every C5. The timers are not gated by the lib.

The C55x parts have **no TIM3 and no TIM4** either — present are TIM1/8, TIM2/5/12/15/16/17, TIM6/7
and LPTIM1. Downstream projects that default a general-purpose timer to TIM3 must move it (mLRS's
C552 target uses TIM5, which is 32-bit and free). `rcc_init_tim()` knows TIM12 on C5. Also **PB15
has no FDCAN function on C5**; use PB12 = FDCAN1_RX and PB13 = FDCAN1_TX. DS14928 Rev 3 Table 13
lists every FDCAN1 pin on **AF9** — an earlier version of this note had PB12 on AF10, check it
against the datasheet revision in use.

### 2.11 C551/C552/C562 up to 64 pins

**No sub-family define is used.** An earlier cut introduced `STM32C5_C55x`, gating on
`STM32C551xx || STM32C552xx || STM32C562xx`, and it was removed: every difference it expressed turned
out to be *peripheral presence*, which CMSIS already answers. `USART3` and `SPI3` are defined by the
device header only on parts that have them, so the C5 arms test `#if !defined USART3` / `#if !defined SPI3`.
A hand-maintained part-number list fails *wrong*, not safe: any C5 not on the list would have been
told it has no USART3 and no SPI3.

The pin map below is DS14928 Rev 3 (STM32C55xxx), Table 12 and 13, restricted to the pins bonded
up to LQFP64: PA0-15, PB0-10, PB12-15, PC0-15, PD2, PE2, PH0-2. There is **no PB11**. PC0-12 and
PD2 need 64 pins, PE3/PE4/PE5 exist only on the alternative LQFP64 pinout (`C55xRxTxJ`), which
gives up PC13-15 for them. The C552 and C562 UART/SPI maps are identical; no datasheet was
available for C531/532/542, their arms take the same AFs on that assumption.

Named mappings (TX,RX order) and the AF they select; bold is the default of the plain
`_USE_UARTx` style define where there is one:

| Peripheral | Mappings |
|---|---|
| USART1 | `PA9PA10` AF7, `PB6PB7` AF7, `PA6PA7` AF7, `PB14PB15` AF4, `PE4PE3` AF7 |
| USART2 | `PA2PA3` AF7, `PA11PA12` AF4 (USB pins) |
| USART3 | `PC10PC11` AF7, `PB10PB1` AF7, `PB3PB4` AF11 (JTDO/NJTRST) |
| UART4 | `PA0PA1` AF8, `PC10PC11` AF8, `PB9PB8` AF8, `PA12PA11` AF6 (USB pins) |
| UART5 | `PC12PD2` AF8, `PB13PB12` AF14, `PB6PB5` AF14 |
| LPUART1 | `PA3PA2` AF3 (= `PA2PA3`, **swapped**, see 2.4), `PA9PA10` AF3, `PB6PB7` AF8 |
| SPI1 | **`PA5/PA6/PA7`** AF5, `PB3PB4PB5` AF5 |
| SPI2 | **`PB13/PB14/PB15`** AF5 |
| SPI3 | **`PC10/PC11/PC12`** AF6, `PB3PB4PB5` AF6/AF6/**AF7** |
| I2C1 | **`PB8/PB9`** AF4 |
| I2C2 | **`PB10/PB12`** AF4 |

Every other pin goes through the generic overrides — `UART$_USE_TX_IO` / `_RX_IO` / `_IO_AF`,
`SPI$_USE_SCK_IO` / `_MISO_IO` / `_MOSI_IO` / `_IO_AF`, `I2C_USE_SCL_IO` / `_SDA_IO` / `_IO_AF` —
and, new, a **per pin AF** where the pins of one peripheral do not share one:
`UART$_USE_TX_IO_AF` / `_RX_IO_AF`, `SPI$_USE_SCK_IO_AF` / `_MISO_IO_AF` / `_MOSI_IO_AF`,
`I2C_USE_SCL_IO_AF` / `_SDA_IO_AF`. The complete pin lists:

| Signal | Pins |
|---|---|
| USART1 TX / RX | PA6, PA9, PB6, PE4 AF7, PB14 AF4, PA15 AF11 / PA7, PA10, PB7, PE3 AF7, PB15 AF4 |
| USART2 TX / RX | PA2, PB0 AF7, PA11 AF4 / PA3 AF7, PA12 AF4 |
| USART3 TX / RX | PB10, PC10 AF7, PB3 AF11 / PB1, PC4, PC11 AF7, PB4 AF11 |
| UART4 TX / RX | PA0, PB9, PC10 AF8, PA12 AF6 / PA1, PB8, PC11 AF8, PA11 AF6 |
| UART5 TX / RX | PC12 AF8, PB6, PB13 AF14 / PD2 AF8, PB5, PB12, PB15 AF14 |
| LPUART1 TX / RX | PA3, PA9, PC3 AF3, PB3, PB6 AF8 / PA2, PA10 AF3, PB7, PB13 AF8 |
| SPI1 SCK / MISO / MOSI | PA5, PB3 AF5 / PA6, PB4 AF5 / PA7, PB5 AF5, PB15 AF6 |
| SPI2 SCK / MISO / MOSI | PA9, PA12, PB10, PB13 AF5, PA5, PB2 AF6 / PB14, PC2 AF5, PA7 AF6 / PB15, PC1, PC3 AF5, PA8 AF6 |
| SPI3 SCK / MISO / MOSI | PB3, PB7, PB9, PC10 AF6, PB1 AF4, PE2 AF5 / PB4, PB6, PC11 AF6, PB0, PE5 AF5 / PA3, PC12 AF6, PB2, PB5, PB15 AF7, PA4 AF4 |
| I2C1 SCL / SDA | PB6, PB8, PH1 AF4, PA8 AF9 / PB7, PB9, PH0 AF4, PC9 AF9 |
| I2C2 SCL / SDA | PB10 AF4, PA11 AF8, PB3 AF9 / PB12, PB3 AF4, PA12 AF8, PB4 AF9 |

Mappings of other families that do not exist on C5 `#error`: `UART1_PC4PC5`, `UART2_PD5PD6` (real,
but not bonded up to 64 pins), `UART2_PB3PB4`, `UART2_PA14PA15`, `UART2_PB3PA15`, `UART3_PB10PB11`,
`UART3_PB8PB9`, `LPUART1_PC1PC0`, and USART6/UART7/UART8 altogether.

Not pins, but part of "everything up to 64 pins": ADC1 and ADC2 (2.5), TIM1/2/5/6/7/8/12/15/16/17
and LPTIM1 in `rcc_init_tim()` / `rcc_init_lptim()`, `IO_PH0`..`IO_PH2`. Still not covered: DAC1
(PA4), FDCAN1 (lives downstream), I3C1, COMP1, USB, and the I2S side of the SPIs.

### 2.12 HAL module-enable define naming differs on C5

CubeC5 HAL uses `USE_HAL_X_MODULE`, where the lib gates on `HAL_X_MODULE_ENABLED`. The only such
gate is the one in `stdstm32-i2c.h`, and C5 is exempt from it, its I2C arm being LL only. Nothing
has to be defined for C5.

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

- `HAL_StatusTypeDef` is `hal_status_t` in HAL2. `stdstm32-peripherals.h` aliases it; a project
  whose own headers need the name before the lib is included can repeat the typedef, C11 and C++
  both allow that.

One HAL2 gap is **not** the lib's to fix and must be handled downstream:

- `HAL_RCC_OscConfig`, `HAL_RCC_ClockConfig`, `HAL_NVIC_SetPriority` and `HAL_ICACHE_Enable` do not
  exist in HAL2, so a CubeMX-style `SystemClock_Config()` cannot be ported from another family and
  has to be rebuilt on LL_RCC. See 2.15.

### 2.14 `mcu_uid()` must read with the ICACHE off

Same hazard as 2.7, different peripheral: `UID_BASE` is `0x08FFF800` on C5, in the read-only/OTP
area that **cannot be cached** even though the AHB range is cacheable by default. Reading it with
the ICACHE enabled raises a bus error → HardFault. `mcu_uid()` therefore disables the ICACHE around
the read by direct register write (so it doesn't drag in `ll_icache`/`hal_icache`), and restores it
with the same re-enable + `CACHEINV` + bounded `BSYENDF` wait as `ee_hal_lock()`, see 2.7. Guarded by
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

### Done

- Compiles clean, without warnings, against the real CubeC5 headers (`stm32c5xx-drivers`,
  `stm32c5xx-dfp`) with `-DSTM32C5 -DUSE_FULL_LL_DRIVER -mcpu=cortex-m33`, for `STM32C552xx`,
  `STM32C562xx` and `STM32C531xx`. Each named UART, SPI and I2C mapping of 2.11 was compiled on its
  own, the mappings that must `#error` were checked to do so, and the AF actually reaching
  `gpio_init_af()` was checked in the preprocessor output for the mixed-AF ones.
- mLRS builds and links for `rx-`/`tx-diy-WeAct-E22-c552ce`. It has not been rebuilt against the
  2026-10-12 changes; the defines its C552 targets use are unchanged.
- G4, WL and F1 still compile with the shared template changes (per pin AF, LPUART prescaler).

### Verification still required

On silicon, for the 2026-10-12 changes:

- I2C: OLED detection and the polled init with the interrupts on, a 1025 byte DMA frame, and the
  error paths — pull the module mid transfer, hold SCL low — must leave `i2c_device_ready()`
  returning something other than `HAL_BUSY`.
- `mcu_uid()` with the ICACHE enabled returns the UID and returns at all (2.14).
- `spi_setmode()` and `spi_disable()` after the suspend guard.
- LPUART1 below 50 kBaud, which now runs on the /16 prescaler.
- Any of the new pin mappings before relying on it.

And from before, where not already covered by running the WeAct board:

- EEPROM across the bank-2 boundary (page index 32+ on a 512 KB part), and writes with the ICACHE on.
- `BootLoaderInit` actually reaches the bootloader. The MSPLIM lowering of 2.8 is unverified; if
  entry still fails, an RM0522-derived SBS/remap write is the next thing to try.
- UART baud rates — `_ll_uart_periphclk()` (2.13) assumes the reset-default `RCC_CCIPR` clock
  selection, and a wrong assumption there shows up as a mis-scaled baud rate.

### Open feature gaps (deliberate, deferred)

| Item | Status | Notes |
|---|---|---|
| `LL_SYSCFG_REMAP_SYSTEMFLASH` equivalent on C5 | Not added, likely not needed | See 2.8 — MSPLIM was the actual blocker. Add an SBS/remap write only if hardware testing still shows a problem. |
| I2C reads | Stubbed | `i2c_get()` / `i2c_get_blocked()` return `HAL_ERROR` on C5, nothing needs them yet. |
| DAC1, I3C1, COMP1, I2S | Not implemented | See the end of 2.11. |
| FDCAN / SPI bootloader entry | Not in template | AN2606 lists FDCAN2 PB5/PB6 and SPI1/SPI2 slave modes for C531 boot; existing `UART$_HAS_SYSTEMBOOT` only handles UART-based boot. |
| C551/C552/C562 up to 64 pins | Pin map complete | See 2.11. Every UART/SPI/I2C pin of DS14928 Rev 3 is reachable, by name or by override. |
| C551/C552/C562 above 64 pins | Not mapped | Ports D (beyond PD2) and E are not in the defaults. Reachable via `UART$_USE_TX_IO` / `_RX_IO` / `_IO_AF`. |
| C5A3 / C591 / C593 | Out of scope | Presence macros give them the right USART3/SPI3 answer, but their pin/AF map was never checked. Verify against their datasheet before using. |
| C531/C532/C542 pin map | Unverified | No datasheet was available, they get the C55x AFs. Check against their datasheet before use. |

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
5. Provide an NMI handler that clears a flash ECC error if the EEPROM emulation is used (2.6).
6. Build `SystemClock_Config()` on LL_RCC — there is no PLL and no `HAL_RCC_ClockConfig` (2.15).

Then `#include "stdstm32-peripherals.h"` and the per-peripheral headers as usual.

Startup is a `.c` file (`startup_stm32c5xxxx.c`), not `.s`, and linker templates are in
`stm32c5xx_dfp/Source/Templates/gcc/linker/`. They use ST's newer symbol names (`__end__`,
`STACK_SIZE`) rather than the `_end` / `_Min_Stack_Size` that CubeMX's `sysmem.c` expects, and their
`/DISCARD/` of libc/libm/libgcc has to go if you link the standard libraries.
