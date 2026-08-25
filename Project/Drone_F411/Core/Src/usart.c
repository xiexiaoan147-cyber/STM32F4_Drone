/**
 * usart.c — USART1 (调试/蓝牙) + printf 重定向 + BLE 字节接收
 *
 * 修复记录:
 *   1. 补上 printf 重定向 (原全仓库没有 _write/__io_putchar,
 *      所有 printf 输出落空)
 *   2. 补上接收路径: USART1 中断逐字节接收 → xByteQueue → Comm 任务
 *      (原 BLE 协议解析器无人喂数据, 指令链路不通)
 */
#include "usart.h"
#include "freertos.h"      /* xByteQueue */
#include <stdio.h>

UART_HandleTypeDef huart1;

void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);   /* = configMAX_SYSCALL, 可用 FromISR API */
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  }
}

/* ============================================================
 * printf 重定向 (newlib / newlib-nano 的 _write 系统调用)
 * 注意: 多任务同时 printf 可能交错; 调试日志可接受
 * ============================================================ */
int _write(int fd, char *ptr, int len)
{
  (void)fd;
  HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, 20);
  return len;
}

/* ============================================================
 * BLE 字节接收: 中断逐字节 → 队列 → Comm 任务解析
 * ============================================================ */
static uint8_t rx_byte;

void USART1_StartRx(void)
{
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(xByteQueue, &rx_byte, &woken);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* 重新武装接收 */
    portYIELD_FROM_ISR(woken);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  /* 溢出/噪声等错误后 HAL 会中止接收, 这里重新武装 */
  if (huart->Instance == USART1) {
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}
