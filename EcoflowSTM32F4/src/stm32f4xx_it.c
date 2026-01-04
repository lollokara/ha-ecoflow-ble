#include "stm32f4xx_it.h"
#include "stm32f4xx_hal.h"
#include "stm32469i_discovery.h"
#include "stm32469i_discovery_lcd.h"
#include <stdio.h>

/* External variables */
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/

void NMI_Handler(void)
{
    printf("NMI_Handler\n");
    while (1) { }
}

void HardFault_Handler_C(uint32_t *args)
{
    uint32_t r0 = args[0];
    uint32_t r1 = args[1];
    uint32_t r2 = args[2];
    uint32_t r3 = args[3];
    uint32_t r12 = args[4];
    uint32_t lr = args[5];
    uint32_t pc = args[6];
    uint32_t psr = args[7];

    printf("\n[HARD FAULT]\n");
    printf("R0 : %08lX\n", r0);
    printf("R1 : %08lX\n", r1);
    printf("R2 : %08lX\n", r2);
    printf("R3 : %08lX\n", r3);
    printf("R12: %08lX\n", r12);
    printf("LR : %08lX\n", lr);
    printf("PC : %08lX\n", pc);
    printf("PSR: %08lX\n", psr);
    printf("HFSR: %08lX\n", SCB->HFSR);
    printf("CFSR: %08lX\n", SCB->CFSR);
    printf("MMFAR: %08lX\n", SCB->MMFAR);
    printf("BFAR: %08lX\n", SCB->BFAR);

    // Blink Red LED (PD5)
    RCC->AHB1ENR |= (1 << 3);
    GPIOD->MODER &= ~(3 << 10);
    GPIOD->MODER |= (1 << 10);

    while (1) {
        GPIOD->ODR ^= (1 << 5);
        for(volatile int i=0; i<2000000; i++); // Fast blink
    }
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile (
        " tst lr, #4 \n"
        " ite eq \n"
        " mrseq r0, msp \n"
        " mrsne r0, psp \n"
        " b HardFault_Handler_C \n"
    );
}

void MemManage_Handler(void)
{
    printf("MemManage_Handler\n");
    while (1) { }
}

void BusFault_Handler(void)
{
    printf("BusFault_Handler\n");
    while (1) { }
}

void UsageFault_Handler(void)
{
    printf("UsageFault_Handler\n");
    while (1) { }
}

void DebugMon_Handler(void)
{
}

/* SysTick, SVC, and PendSV are handled by FreeRTOS */

/******************************************************************************/
/*                       Peripheral Interrupt Handlers                        */
/******************************************************************************/

void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart3);
}

void USART6_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart6);
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                             */
/******************************************************************************/

/**
  * @brief  This function handles I2C1 event interrupt request.
  * @param  None
  * @retval None
  */
void I2C1_EV_IRQHandler(void)
{
  HAL_I2C_EV_IRQHandler(&heval_I2c1);
}

/**
  * @brief  This function handles I2C1 error interrupt request.
  * @param  None
  * @retval None
  */
void I2C1_ER_IRQHandler(void)
{
  HAL_I2C_ER_IRQHandler(&heval_I2c1);
}

/**
  * @brief  This function handles LTDC global interrupt request.
  * @param  None
  * @retval None
  */
void LTDC_IRQHandler(void)
{
  HAL_LTDC_IRQHandler(&hltdc_eval);
}

/**
  * @brief  This function handles LTDC error interrupt request.
  * @param  None
  * @retval None
  */
void LTDC_ER_IRQHandler(void)
{
  HAL_LTDC_IRQHandler(&hltdc_eval);
}

/**
  * @brief  This function handles DMA2D global interrupt request.
  * @param  None
  * @retval None
  */
void DMA2D_IRQHandler(void)
{
  HAL_DMA2D_IRQHandler(&hdma2d_eval);
}

/**
  * @brief  This function handles DSI global interrupt request.
  * @param  None
  * @retval None
  */
void DSI_IRQHandler(void)
{
  HAL_DSI_IRQHandler(&hdsi_eval);
}

/**
  * @brief  This function handles External line 9 to 5 interrupt request.
  * @param  None
  * @retval None
  */
void EXTI9_5_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
}
