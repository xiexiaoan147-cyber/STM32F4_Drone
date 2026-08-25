#ifndef STUB_HAL_H
#define STUB_HAL_H
/* 主机端桩: 只提供 control.c 用到的 HAL 接口 */
#include <stdint.h>
uint32_t HAL_GetTick(void);
typedef struct { int inst; } I2C_HandleTypeDef;
#endif
