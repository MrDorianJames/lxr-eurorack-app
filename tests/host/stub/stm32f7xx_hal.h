#pragma once
#include <stdint.h>
typedef struct { volatile uint32_t CYCCNT, CTRL; } DWT_Type; static DWT_Type dwt_stub;
#define DWT (&dwt_stub)
typedef struct {int x;} I2S_HandleTypeDef; typedef struct {int x;} TIM_HandleTypeDef; typedef struct {int x;} UART_HandleTypeDef;
typedef struct {int x;} SPI_HandleTypeDef; typedef struct {int x;} ADC_HandleTypeDef; typedef struct {int x;} DMA_HandleTypeDef;
typedef struct {int x;} SD_HandleTypeDef; typedef struct {int x;} RNG_HandleTypeDef; typedef struct {int x;} I2C_HandleTypeDef;
typedef struct {int x;} DAC_HandleTypeDef; typedef struct {int x;} PCD_HandleTypeDef; typedef struct {int x;} HCD_HandleTypeDef;
typedef struct {volatile uint32_t IDR, ODR, BSRR;} GPIO_TypeDef;
#define CoreDebug_DEMCR_TRCENA_Msk 0
#define DWT_CTRL_CYCCNTENA_Msk 0
typedef struct { volatile uint32_t DEMCR; } CoreDebug_Type; static CoreDebug_Type cd_stub;
#define CoreDebug (&cd_stub)
#define __STATIC_INLINE static inline
/* clock query used by delay.h; value only matters on the hardware */
static inline uint32_t HAL_RCC_GetHCLKFreq(void) { return 216000000u; }
