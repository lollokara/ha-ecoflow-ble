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

void HardFault_Handler(void)
{
    printf("HardFault_Handler\n");
    while (1) { }
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

extern UART_HandleTypeDef huart4;
extern void Fan_RxByteISR(uint8_t byte);

void UART4_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart4);
}

// Override HAL callback to capture byte
// Note: This callback is weak in HAL_UART.c, but if it is defined elsewhere (uart_task.c?), we might conflict.
// Let's check uart_task.c. It defines HAL_UART_RxCpltCallback too?
// If so, we need to merge logic.
// Checking uart_task.c ...
// If not, we define it here.
