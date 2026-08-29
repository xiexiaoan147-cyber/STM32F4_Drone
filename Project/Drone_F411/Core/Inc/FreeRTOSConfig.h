/*
 * FreeRTOSConfig.h — STM32F411CEU6 小四轴无人机定制配置
 * 依据：100MHz 时钟 / 128KB SRAM / 8 任务 / PID 200Hz
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f4xx_hal.h"

/*---------------------------------------------------------------------------*/
/* 基础调度配置                                                               */
/*---------------------------------------------------------------------------*/
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1   /* Cortex-M 硬件前导零指令 */
#define configUSE_TICKLESS_IDLE                 0   /* 飞控不进入低功耗 */
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      1000 /* 1ms tick, 配合 PID 200Hz */
#define configMAX_PRIORITIES                    16  /* 0~15, 实际用 0~6 */
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 ) /* 128 words = 512 bytes */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 15 * 1024 ) ) /* 15KB, 8 任务够用 */
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0   /* 32-bit tick, 49 天不溢出 */
#define configIDLE_SHOULD_YIELD                 1

/*---------------------------------------------------------------------------*/
/* 同步与通信                                                                 */
/*---------------------------------------------------------------------------*/
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8

/*---------------------------------------------------------------------------*/
/* 软件定时器未使用: 优先级设为 1 (低), 仅作预留 */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               1
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

/*---------------------------------------------------------------------------*/
/* 堆栈溢出检测                                                              */
/*---------------------------------------------------------------------------*/
#define configCHECK_FOR_STACK_OVERFLOW          2   /* 方法 2: canary 检测 */

/*---------------------------------------------------------------------------*/
/* 运行时统计（调试阶段启用，发布可关）                                        */
/*---------------------------------------------------------------------------*/
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1   /* 开启 vTaskList 等调试 API */
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/*---------------------------------------------------------------------------*/
/* 可选 API 函数                                                              */
/*---------------------------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     0   /* 飞控任务不动态删除 */
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_eTaskGetState                   1

/*---------------------------------------------------------------------------*/
/* 中断服务相关                                                               */
/*---------------------------------------------------------------------------*/
#define configKERNEL_INTERRUPT_PRIORITY         0xF0  /* 最低 15, 给 FreeRTOS API */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    0x50  /* 5~15 可调用 FromISR */
/* 等价映射: configLIBRARY_LOWEST_INTERRUPT_PRIORITY=15, configLIBRARY_MAX_SYSCALL=5 */

/*---------------------------------------------------------------------------*/
/* Hook 函数                                                                  */
/*---------------------------------------------------------------------------*/
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0   /* HAL 时基由 stm32f4xx_it.c 的
                                                       SysTick_Handler 直接双跳实现,
                                                       无需 Tick Hook */
#define configUSE_MALLOC_FAILED_HOOK            1   /* 内存不足时触发 (freertos.c 已实现) */
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/*---------------------------------------------------------------------------*/
/* ARM Cortex-M 特定                                                         */
/*---------------------------------------------------------------------------*/
#define configPRIO_BITS                         4   /* STM32F4 使用 4 位优先级 */
#define configENABLE_FPU                        1   /* FPU 由任务上下文保存/恢复 */
#define configENABLE_MPU                        0   /* 暂不启用 MPU */
#define configENABLE_TRUSTZONE                  0

/* FreeRTOS 使用 PendSV/SVC 做上下文切换, 通过符号映射挂到向量表;
 * SysTick 不在这里映射 —— 由 stm32f4xx_it.c 的 SysTick_Handler
 * 同时调用 HAL_IncTick() 与 xPortSysTickHandler() (见该文件) */
#define xPortPendSVHandler      PendSV_Handler
#define vPortSVCHandler         SVC_Handler

/*---------------------------------------------------------------------------*/
/* assert 映射到 HAL                                                          */
/*---------------------------------------------------------------------------*/
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

#endif /* FREERTOS_CONFIG_H */
