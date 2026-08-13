//*******************************************************
// Copyright (c) OlliW, OlliW42, www.olliw.eu
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// my stripped down MCU standard library
//*******************************************************
#ifndef STDSTM32_LL_MCU_H
#define STDSTM32_LL_MCU_H
#ifdef __cplusplus
extern "C" {
#endif


//-------------------------------------------------------
// STM32 Device Information
//-------------------------------------------------------

#define STM32_MCU_UID_LEN  12

void mcu_uid(uint8_t uid[STM32_MCU_UID_LEN])
{
#if defined ICACHE
    // The UID sits in the OTP/system area, which cannot be cached even though the AHB
    // range is cacheable by default, so reading it with the ICACHE enabled raises a bus
    // error -> HardFault. For this single read at startup it is simpler to read it with
    // the ICACHE off than to add an MPU entry. Done by direct register write so this
    // does not depend on the project pulling in the ll_icache / hal_icache headers.
    // clearing EN also starts a full cache invalidate, see the ICACHE chapter of the RM
    uint32_t icache_was_enabled = READ_BIT(ICACHE->CR, ICACHE_CR_EN);
    if (icache_was_enabled) {
        WRITE_REG(ICACHE->FCR, ICACHE_FCR_CBSYENDF);
        CLEAR_BIT(ICACHE->CR, ICACHE_CR_EN);
        while (READ_BIT(ICACHE->CR, ICACHE_CR_EN) != 0U) {}
    }
#endif

    // shorter than using LL_GetUID_Word0(), LL_GetUID_Word1(), LL_GetUID_Word2()
    uint8_t* uid_ptr = (uint8_t*)UID_BASE;
    memcpy(uid, uid_ptr, STM32_MCU_UID_LEN);

#if defined ICACHE
    // wait for the invalidate started by the disable to finish before re-enabling,
    // else early accesses are treated as noncacheable
    if (icache_was_enabled) {
        while (READ_BIT(ICACHE->SR, ICACHE_SR_BSYENDF) == 0U) {}
        WRITE_REG(ICACHE->FCR, ICACHE_FCR_CBSYENDF);
        SET_BIT(ICACHE->CR, ICACHE_CR_EN);
    }
#endif
}


uint32_t mcu_cpu_id(void)
{
    // easier and more complete than using LL_CPUID_Getxxxx() functions
    return SCB->CPUID;
}


//-------------------------------------------------------
// BootLoaderInit
//-------------------------------------------------------

// see AN2606 for system flash start location
#ifdef STM32F1
#define ST_BOOTLOADER_ADDRESS               0x1FFFF000 // = SystemMemory: F103T8 F103CB F103RC
#elif defined STM32F3
#define ST_BOOTLOADER_ADDRESS               0x1FFFD800
#elif defined STM32G4 || defined STM32L4 || defined STM32WL
#define ST_BOOTLOADER_ADDRESS               0x1FFF0000
#elif defined STM32F070xB || defined STM32F072xB // system memory location varies across the STM32F0 family
#define ST_BOOTLOADER_ADDRESS               0x1FFFC800
#elif defined STM32C5
#define ST_BOOTLOADER_ADDRESS               0x0BF80080
#else
  #warning ST_BOOTLOADER_ADDRESS not defined for chip !
#endif


void (*SysMemBootJump)(void);


#ifdef STDSTM32_USE_USB
#include "stdstm32-usb-vcp.h"
#endif


void BootLoaderInit(void)
{
    SysMemBootJump = (void (*)(void)) (*((uint32_t*)(ST_BOOTLOADER_ADDRESS+4))); // point PC to system memory reset vector

    HAL_DeInit(); // is important

    // shut down any running tasks, done already by HAL_DeInit()!
#if defined STM32C5
    // HAL2 dropped the LL_xxx_DeInit() functions, they lived in the LL .c files and C5's LL is
    // header only. HAL_DeInit() above has already reset the peripherals, and the bootloader
    // configures its own clock tree (AN2606: 48 MHz off HSI/3), so nothing is missing here.
#else
    LL_GPIO_DeInit(GPIOA);
    LL_GPIO_DeInit(GPIOB);
    LL_GPIO_DeInit(GPIOC);
#ifdef USART1
    LL_USART_DeInit(USART1);
#endif
#ifdef USART2
    LL_USART_DeInit(USART2);
#endif
#ifdef USART3
    LL_USART_DeInit(USART3);
#endif
#ifdef UART4
    LL_USART_DeInit(UART4);
#endif
#ifdef UART5
    LL_USART_DeInit(UART5);
#endif
#ifdef SPI1
    LL_SPI_DeInit(SPI1);
#endif
#ifdef SPI3
    LL_SPI_DeInit(SPI3);
#endif

    LL_RCC_DeInit(); // HAL_RCC_DeInit(); ?? ATTENTION: HAL_RCC_DeInit() uses SysTick!
#endif

    // reset systick timer
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // select HSI as system clock source
#if defined STM32C5
    // C5 splits the HSI sysclk entries. HSI/3 = 48 MHz is what the bootloader itself runs on,
    // see AN2606 Table 23.
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSIDIV3);
#else
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI); // is done already in LL_RCC_Deinit() !?
#endif

    // disable interrupts
    //__set_PRIMASK(1);
    // this is what works for F0 to enter DFU!
    __disable_irq();
    for (uint8_t i = 0; i < (sizeof(NVIC->ICER) / sizeof(*NVIC->ICER)); i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    __enable_irq();

    // remap system memory
    // stated in several sources, but doesn't seem to be relevant
    // SYSCFG->CFGR1 = 0x01;
    // SYSCFG->MEMRMP = 0x01;
    // __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
    //LL_SYSCFG_SetRemapMemory(LL_SYSCFG_REMAP_SYSTEMFLASH);
    // note: with mLRS this appears to be required for G4 to properly enter bootloader
 #if defined STM32G4
     LL_SYSCFG_SetRemapMemory(LL_SYSCFG_REMAP_SYSTEMFLASH);
 #endif
    // no remap is done on C5, and none is needed: the remap is not what makes the jump work.
#if defined STM32C5
    // what actually blocks bootloader entry here is MSPLIM. C5 is Cortex-M33 (ARMv8-M),
    // and CubeMX startup code sets MSPLIM to the top of RAM. the bootloader's own MSP sits
    // well below that, so setting MSP without first lowering MSPLIM faults immediately.
    __set_MSPLIM(0x00000000U);

    // an enabled ICACHE interferes with bootloader execution, turn it off.
    // done by direct register write so this does not depend on the project pulling in
    // the ll_icache / hal_icache headers
#if defined(ICACHE)
    ICACHE->CR &= ~ICACHE_CR_EN;
#endif

    // point the vector table at the bootloader before jumping
    SCB->VTOR = ST_BOOTLOADER_ADDRESS;
#endif

    // set main stack pointer to its default
    __set_MSP( *((volatile uint32_t*)ST_BOOTLOADER_ADDRESS) );
    // jump
    SysMemBootJump();
    while(1);
}


//-------------------------------------------------------
#ifdef __cplusplus
}
#endif
#endif  // STDSTM32_LL_MCU_H
