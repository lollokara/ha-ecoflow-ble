#include "stm32f4xx_it.h"
#include "stm32f4xx_hal.h"
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
