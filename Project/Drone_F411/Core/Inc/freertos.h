/**
 * freertos.h — FreeRTOS 业务模块入口 + 跨文件队列句柄
 */
#ifndef FREERTOS_MODULE_H
#define FREERTOS_MODULE_H

#include "FreeRTOS.h"
#include "queue.h"

/* USART1 ISR → Comm 任务 的字节队列 (1 字节/格) */
extern QueueHandle_t xByteQueue;

void freertos_start(void);

#endif
