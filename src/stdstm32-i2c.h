//*******************************************************
// Copyright (c) OlliW, OlliW42, www.olliw.eu
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// STM32Cube HAL based I2C standard library
//*******************************************************
// a bit limited so far !!!
//
// Interface:
//
// #define I2C_USE_I2C1, I2C_USE_I2C2, I2C_USE_I2C3
//
// #define I2C_USE_ITMODE
// #define I2C_IT_IRQ_PRIORITY // may also be needed for DMA mode !
//
// #define I2C_USE_DMAMODE
// #define I2C_DMA_PRIORITY
// #define I2C_DMA_IRQ_PRIORITY
//
// #defined I2C_CLOCKSPEED_100KHZ
// #defined I2C_CLOCKSPEED_400KHZ
// #defined I2C_CLOCKSPEED_1000KHZ
//
//*******************************************************
#ifndef STDSTM32_I2C_H
#define STDSTM32_I2C_H
#ifdef __cplusplus
extern "C" {
#endif
#ifndef HAL_I2C_MODULE_ENABLED
  #error HAL_I2C_MODULE_ENABLED not defined, enable it in Core\Inc\stm32yyxx_hal_conf.h!
#else


//-------------------------------------------------------
// Defines
//-------------------------------------------------------

#include "stdstm32-peripherals.h"

#if defined I2C_USE_I2C1
  #define I2C_I2Cx               I2C1
#ifdef STM32F1  
  #define I2C_SCL_IO             IO_PB6
  #define I2C_SDA_IO             IO_PB7
  #define I2C_SCL_IO_AF          IO_AF_DEFAULT
  #define I2C_SDA_IO_AF          IO_AF_DEFAULT
  
  #define I2C_EV_IRQn            I2C1_EV_IRQn
  #define I2C_ER_IRQn            I2C1_ER_IRQn
  #define I2C_EV_IRQHandler      I2C1_EV_IRQHandler
  #define I2C_ER_IRQHandler      I2C1_ER_IRQHandler

  #define I2C_TX_DMAx_Channely_IRQn        DMA1_Channel6_IRQn
  #define I2C_RX_DMAx_Channely_IRQn        DMA1_Channel7_IRQn
  #define I2C_TX_DMAx_Channely_IRQHandler  DMA1_Channel6_IRQHandler
  #define I2C_RX_DMAx_Channely_IRQHandler  DMA1_Channel7_IRQHandler
#elif defined STM32WL
  #define I2C_SCL_IO             IO_PA9
  #define I2C_SDA_IO             IO_PA10
  #define I2C_SCL_IO_AF          IO_AF_4
  #define I2C_SDA_IO_AF          IO_AF_4

  #define I2C_EV_IRQn            I2C1_EV_IRQn
  #define I2C_ER_IRQn            I2C1_ER_IRQn
  #define I2C_EV_IRQHandler      I2C1_EV_IRQHandler
  #define I2C_ER_IRQHandler      I2C1_ER_IRQHandler

  #define I2C_TX_DMAx_Channely_IRQn        DMA1_Channel2_IRQn
  #define I2C_RX_DMAx_Channely_IRQn        DMA1_Channel1_IRQn
  #define I2C_TX_DMAx_Channely_IRQHandler  DMA1_Channel2_IRQHandler
  #define I2C_RX_DMAx_Channely_IRQHandler  DMA1_Channel1_IRQHandler
#else
  #error Error in stdstm32-i2c.h, selected I2C1 not supported !
#endif

#elif defined I2C_USE_I2C2 || defined I2C_USE_I2C2_PB13PB14
  #define I2C_I2Cx               I2C2
#ifdef STM32WL
#if defined I2C_USE_I2C2
  #define I2C_SCL_IO             IO_PA12
  #define I2C_SDA_IO             IO_PA11
  #define I2C_SCL_IO_AF          IO_AF_4
  #define I2C_SDA_IO_AF          IO_AF_4
#else
  #error Error in stdstm32-i2c.h, selected USE_I2C2 not supported !
#endif
  #define I2C_EV_IRQn            I2C2_EV_IRQn
  #define I2C_ER_IRQn            I2C2_ER_IRQn
  #define I2C_EV_IRQHandler      I2C2_EV_IRQHandler
  #define I2C_ER_IRQHandler      I2C2_ER_IRQHandler

  #define I2C_TX_DMAx_Channely_IRQn        DMA1_Channel2_IRQn
  #define I2C_RX_DMAx_Channely_IRQn        DMA1_Channel1_IRQn
  #define I2C_TX_DMAx_Channely_IRQHandler  DMA1_Channel2_IRQHandler
  #define I2C_RX_DMAx_Channely_IRQHandler  DMA1_Channel1_IRQHandler
#elif defined STM32F0
#if defined I2C_USE_I2C2_PB13PB14
  #define I2C_SCL_IO             IO_PB13
  #define I2C_SDA_IO             IO_PB14
  #define I2C_SCL_IO_AF          IO_AF_5
  #define I2C_SDA_IO_AF          IO_AF_5
#else
  #error Error in stdstm32-i2c.h, selected USE_I2C2 not supported !
#endif
  #define I2C_IRQn               I2C2_IRQn
  #define I2C_IRQHandler         I2C2_IRQHandler

  #define I2C_TX_DMAx_Channely_IRQn        DMA1_Channel4_5_IRQn
  #define I2C_RX_DMAx_Channely_IRQn        DMA1_Channel4_5_IRQn
  #define I2C_TX_DMAx_Channely_IRQHandler  DMA1_Channel4_5_IRQHandler
  #define I2C_RX_DMAx_Channely_IRQHandler  DMA1_Channel4_5_IRQHandler
#else
  #error Error in stdstm32-i2c.h, selected I2C2 not supported !
#endif

#elif defined I2C_USE_I2C3
  #define I2C_I2Cx               I2C3
#ifdef STM32G4
  #define I2C_SCL_IO             IO_PC9
  #define I2C_SDA_IO             IO_PA8
  #define I2C_SCL_IO_AF          IO_AF_8 // GPIO_AF8_I2C3
  #define I2C_SDA_IO_AF          IO_AF_2 // GPIO_AF2_I2C3

  #define I2C_EV_IRQn            I2C3_EV_IRQn
  #define I2C_ER_IRQn            I2C3_ER_IRQn
  #define I2C_EV_IRQHandler      I2C3_EV_IRQHandler
  #define I2C_ER_IRQHandler      I2C3_ER_IRQHandler

  #define I2C_TX_DMAx_Channely_IRQn        DMA1_Channel2_IRQn
  #define I2C_RX_DMAx_Channely_IRQn        DMA1_Channel1_IRQn
  #define I2C_TX_DMAx_Channely_IRQHandler  DMA1_Channel2_IRQHandler
  #define I2C_RX_DMAx_Channely_IRQHandler  DMA1_Channel1_IRQHandler
#else
  #error Error in stdstm32-i2c.h, selected I2C3 not supported !
#endif

#else
  #error Error in stdstm32-i2c.h !
#endif
#ifndef I2C_I2Cx
  #error No I2C defined !
#endif


// allows us to overwrite the default pin assignment
#ifdef I2C_USE_SCL_IO
  #undef I2C_SCL_IO
  #define I2C_SCL_IO             I2C_USE_SCL_IO
#endif
#ifdef I2C_USE_SDA_IO
  #undef I2C_SDA_IO
  #define I2C_SDA_IO             I2C_USE_SDA_IO
#endif
#ifdef I2C_USE_IO_AF
  #undef I2C_IO_AF
  #define I2C_IO_AF              I2C_USE_IO_AF
#endif


#ifndef I2C_IT_IRQ_PRIORITY
  #define I2C_IT_IRQ_PRIORITY  15
#endif

#ifndef I2C_DMA_PRIORITY
  #define I2C_DMA_PRIORITY  DMA_PRIORITY_LOW
#endif
#ifndef I2C_DMA_IRQ_PRIORITY
  #define I2C_DMA_IRQ_PRIORITY  15
#endif

#ifndef I2C_DEVICE_READY_TMO_MS
  #define I2C_DEVICE_READY_TMO_MS  HAL_MAX_DELAY
#endif
#ifndef I2C_BLOCKING_TMO_MS
  #define I2C_BLOCKING_TMO_MS  HAL_MAX_DELAY
#endif


//-------------------------------------------------------
//  HAL interface
//-------------------------------------------------------

I2C_HandleTypeDef hi2c;
#ifdef I2C_USE_DMAMODE
DMA_HandleTypeDef hdma_i2c_rx;
DMA_HandleTypeDef hdma_i2c_tx;
#endif


// MspInit(), called in HAL_I2C_Init()
// ST recommends to not mix HAL and LL for a peripheral, so let's use HAL only for i2c
#ifndef STDSTM32_I2C_MSPINIT
#define STDSTM32_I2C_MSPINIT

void HAL_I2C_MspInit(I2C_HandleTypeDef* _hi2c)
{
    rcc_init_afio();

    gpio_init_af(I2C_SCL_IO, IO_MODE_OUTPUT_ALTERNATE_OD, I2C_SCL_IO_AF, IO_SPEED_FAST);
    gpio_init_af(I2C_SDA_IO, IO_MODE_OUTPUT_ALTERNATE_OD, I2C_SDA_IO_AF, IO_SPEED_FAST);

#if defined I2C_USE_I2C1
    if (_hi2c->Instance == I2C1) { __HAL_RCC_I2C1_CLK_ENABLE(); }
#endif
#if defined I2C_USE_I2C2 || defined I2C_USE_I2C2_PB13PB14
    if (_hi2c->Instance == I2C2) { __HAL_RCC_I2C2_CLK_ENABLE(); }
#endif
#if defined I2C_USE_I2C3
    if (_hi2c->Instance == I2C3) { __HAL_RCC_I2C3_CLK_ENABLE(); }
#endif

#ifdef I2C_USE_DMAMODE
    hdma_i2c_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_i2c_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c_rx.Init.Mode = DMA_NORMAL;
    hdma_i2c_rx.Init.Priority = I2C_DMA_PRIORITY; //DMA_PRIORITY_LOW;

    hdma_i2c_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_i2c_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c_tx.Init.Mode = DMA_NORMAL;
    hdma_i2c_tx.Init.Priority = I2C_DMA_PRIORITY; //DMA_PRIORITY_LOW;

#ifdef STM32F1
#if defined I2C1 && defined I2C_USE_I2C1
    if (_hi2c->Instance == I2C1) {
        __HAL_RCC_DMA1_CLK_ENABLE();

        hdma_i2c_rx.Instance = DMA1_Channel7;
        if (HAL_DMA_Init(&hdma_i2c_rx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmarx, hdma_i2c_rx);

        hdma_i2c_tx.Instance = DMA1_Channel6;
        if (HAL_DMA_Init(&hdma_i2c_tx) != HAL_OK) { }

        __HAL_LINKDMA(_hi2c, hdmatx, hdma_i2c_tx);
    }
#endif
#endif // #ifdef STM32F1

#if defined STM32G4 || defined STM32WL
#if defined I2C_USE_I2C1
    if (_hi2c->Instance == I2C1) {
        __HAL_RCC_DMAMUX1_CLK_ENABLE(); // STM32CubeMX placed these incorrectly !
        __HAL_RCC_DMA1_CLK_ENABLE();

        hdma_i2c_rx.Instance = DMA1_Channel1;
        hdma_i2c_rx.Init.Request = DMA_REQUEST_I2C1_RX;
        if (HAL_DMA_Init(&hdma_i2c_rx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmarx, hdma_i2c_rx);

        hdma_i2c_tx.Instance = DMA1_Channel2;
        hdma_i2c_tx.Init.Request = DMA_REQUEST_I2C1_TX;
        if (HAL_DMA_Init(&hdma_i2c_tx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmatx, hdma_i2c_tx);
    }
#endif

#if defined I2C_USE_I2C2
    if (_hi2c->Instance == I2C2) {
        __HAL_RCC_DMAMUX1_CLK_ENABLE(); // STM32CubeMX placed these incorrectly !
        __HAL_RCC_DMA1_CLK_ENABLE();

        hdma_i2c_rx.Instance = DMA1_Channel1;
        hdma_i2c_rx.Init.Request = DMA_REQUEST_I2C2_RX;
        if (HAL_DMA_Init(&hdma_i2c_rx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmarx, hdma_i2c_rx);

        hdma_i2c_tx.Instance = DMA1_Channel2;
        hdma_i2c_tx.Init.Request = DMA_REQUEST_I2C2_TX;
        if (HAL_DMA_Init(&hdma_i2c_tx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmatx, hdma_i2c_tx);
    }
#endif

#if defined I2C_USE_I2C3
    if (_hi2c->Instance == I2C3) {
        __HAL_RCC_DMAMUX1_CLK_ENABLE(); // STM32CubeMX placed these incorrectly !
        __HAL_RCC_DMA1_CLK_ENABLE();

        hdma_i2c_rx.Instance = DMA1_Channel1;
        hdma_i2c_rx.Init.Request = DMA_REQUEST_I2C3_RX;
        if (HAL_DMA_Init(&hdma_i2c_rx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmarx, hdma_i2c_rx);

        hdma_i2c_tx.Instance = DMA1_Channel2;
        hdma_i2c_tx.Init.Request = DMA_REQUEST_I2C3_TX;
        if (HAL_DMA_Init(&hdma_i2c_tx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmatx, hdma_i2c_tx);
    }
#endif
#endif // #if defined STM32G4 || defined STM32WL

#if defined STM32F0
#if defined I2C_USE_I2C2 || defined I2C_USE_I2C2_PB13PB14
    if (_hi2c->Instance == I2C2) {
        __HAL_RCC_DMA1_CLK_ENABLE();

        hdma_i2c_rx.Instance = DMA1_Channel5;
        if (HAL_DMA_Init(&hdma_i2c_rx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmarx, hdma_i2c_rx);

        hdma_i2c_tx.Instance = DMA1_Channel4;
        if (HAL_DMA_Init(&hdma_i2c_tx) != HAL_OK) {}

        __HAL_LINKDMA(_hi2c, hdmatx, hdma_i2c_tx);
    }
#endif
#endif // #if defined STM32F0

#endif // #ifdef I2C_USE_DMAMODE
}

#endif // #ifndef STDSTM32_I2C_MSPINIT


void MX_I2C_Init(void)
{
    hi2c.Instance = I2C_I2Cx;

    hi2c.Init.OwnAddress1 = 0;
    hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c.Init.OwnAddress2 = 0;
    hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

#ifdef STM32F1
#if defined I2C_CLOCKSPEED_100KHZ
    hi2c.Init.ClockSpeed = 100000;
#elif defined I2C_CLOCKSPEED_400KHZ
    hi2c.Init.ClockSpeed = 400000;
#elif defined I2C_CLOCKSPEED_1000KHZ
    #error I2C: 1000 kHz not supported!
#else
    hi2c.Init.ClockSpeed = 100000;
    #warning I2C: no clock speed defined, set to 100 kHz!
#endif
    hi2c.Init.DutyCycle = I2C_DUTYCYCLE_2;

#elif defined STM32G4
#if defined I2C_CLOCKSPEED_100KHZ
    hi2c.Init.Timing = 0x30A0A7FB; // StandardMode 100 kHz
#elif defined I2C_CLOCKSPEED_400KHZ
    hi2c.Init.Timing = 0x10802D9B; // FastdMode 400 kHz
#elif defined I2C_CLOCKSPEED_1000KHZ
    hi2c.Init.Timing = 0x00802172; // FastdMode 1000 kHz
#else
    hi2c.Init.Timing = 0x30A0A7FB;
    #warning I2C: no clock speed defined, set to 100 kHz!
#endif
    hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;

#elif defined STM32WL || defined STM32F0
#if defined I2C_CLOCKSPEED_100KHZ
    hi2c.Init.Timing = 0x20303E5D;
#elif defined I2C_CLOCKSPEED_400KHZ
    hi2c.Init.Timing = 0x2010091A;
#elif defined I2C_CLOCKSPEED_1000KHZ
    hi2c.Init.Timing = 0x20000209;
#else
    hi2c.Init.Timing = 0x20303E5D;
    #warning I2C: no clock speed defined, set to 100 kHz!
#endif
    hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;

#else    
#error STM not supported !
#endif

    if (HAL_I2C_Init(&hi2c) != HAL_OK) return; // abort

#if defined STM32G4 || defined STM32WL || defined STM32F0
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK) return;
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c, 0) != HAL_OK) return;
#endif

#if defined I2C_USE_ITMODE || (defined I2C_USE_DMAMODE && (defined STM32F1 || defined STM32G4 || defined STM32WL || defined STM32F0))
    // somehow G4,WL,F0 seem to need isr also for DMA mode
#ifndef STM32F0
    nvic_irq_enable_w_priority(I2C_EV_IRQn, I2C_IT_IRQ_PRIORITY);
    nvic_irq_enable_w_priority(I2C_ER_IRQn, I2C_IT_IRQ_PRIORITY);
#else
    nvic_irq_enable_w_priority(I2C_IRQn, I2C_IT_IRQ_PRIORITY);
#endif
#endif

#ifdef I2C_USE_DMAMODE
#ifndef STM32F0
    nvic_irq_enable_w_priority(I2C_TX_DMAx_Channely_IRQn, I2C_DMA_IRQ_PRIORITY);
    nvic_irq_enable_w_priority(I2C_RX_DMAx_Channely_IRQn, I2C_DMA_IRQ_PRIORITY);
#else
    nvic_irq_enable_w_priority(I2C_TX_DMAx_Channely_IRQn, I2C_DMA_IRQ_PRIORITY);
#endif
#endif
}


//-------------------------------------------------------
// ISR routines
//-------------------------------------------------------

#if defined I2C_USE_ITMODE || (defined I2C_USE_DMAMODE && (defined STM32F1 || defined STM32G4 || defined STM32WL))
void I2C_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c);
}

void I2C_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c);
}
#elif defined I2C_USE_ITMODE || (defined I2C_USE_DMAMODE && (defined STM32F0))
void I2C_IRQHandler(void)
{
    if (hi2c.Instance->ISR & (I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR)) {
        HAL_I2C_ER_IRQHandler(&hi2c);
    } else {
        HAL_I2C_EV_IRQHandler(&hi2c);
    }
}
#endif

#ifdef I2C_USE_DMAMODE
#ifndef STM32F0
void I2C_TX_DMAx_Channely_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_i2c_tx);
}

void I2C_RX_DMAx_Channely_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_i2c_rx);
}
#else
void I2C_TX_DMAx_Channely_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_i2c_tx);
    HAL_DMA_IRQHandler(&hdma_i2c_rx);
}
#endif
#endif


//-------------------------------------------------------
//  I2C user routines
//-------------------------------------------------------

uint8_t i2c_dev_adr;


// call this before transaction
void i2c_setdeviceadr(uint8_t dev_adr)
{
    i2c_dev_adr = dev_adr;
}


HAL_StatusTypeDef i2c_put_blocked(uint8_t reg_adr, uint8_t* buf, uint16_t len)
{
    return HAL_I2C_Mem_Write(&hi2c, i2c_dev_adr << 1, reg_adr, 1, buf, len, I2C_BLOCKING_TMO_MS);
}


HAL_StatusTypeDef i2c_put_buf_blocked(uint8_t* buf, uint16_t len)
{
    return HAL_I2C_Master_Transmit(&hi2c, i2c_dev_adr << 1, buf, len, I2C_BLOCKING_TMO_MS);
}


HAL_StatusTypeDef i2c_put(uint8_t reg_adr, uint8_t* buf, uint16_t len)
{
#ifdef I2C_USE_DMAMODE
    return HAL_I2C_Mem_Write_DMA(&hi2c, i2c_dev_adr << 1, reg_adr, 1, buf, len);
#elif defined I2C_USE_ITMODE
    return HAL_I2C_Mem_Write_IT(&hi2c, i2c_dev_adr << 1, reg_adr, 1, buf, len);
#else
    return HAL_I2C_Mem_Write(&hi2c, i2c_dev_adr << 1, reg_adr, 1, buf, len, I2C_BLOCKING_TMO_MS);
#endif
}


HAL_StatusTypeDef i2c_put_buf(uint8_t* buf, uint16_t len)
{
#ifdef I2C_USE_DMAMODE
    return HAL_I2C_Master_Transmit_DMA(&hi2c, i2c_dev_adr << 1, buf, len);
#elif defined I2C_USE_ITMODE
    return HAL_I2C_Master_Transmit_IT(&hi2c, i2c_dev_adr << 1, buf, len);
#else
    return i2c_put_buf_blocked(buf, len);
#endif
}


HAL_StatusTypeDef i2c_device_ready(void)
{
    return HAL_I2C_IsDeviceReady(&hi2c, i2c_dev_adr << 1, 10, I2C_DEVICE_READY_TMO_MS);
}


//-------------------------------------------------------
// INIT routines
//-------------------------------------------------------

void i2c_init(void)
{
    MX_I2C_Init();

    i2c_dev_adr = 0;
}


//-------------------------------------------------------
#endif
#ifdef __cplusplus
}
#endif
#endif // STDSTM32_I2C_H
