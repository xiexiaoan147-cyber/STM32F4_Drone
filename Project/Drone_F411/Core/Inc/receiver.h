/**
 * receiver.h — 遥控接收机驱动框架 (SBUS/IBUS)
 * 预留 USART2 RX (PA3), 后续接硬件替换虚拟实现
 */
#ifndef RECEIVER_H
#define RECEIVER_H
#include <stdint.h>

/* 接收机通道数据 */
typedef struct {
    uint16_t ch[16];    /* 16 通道原始值 */
    uint8_t  frame_lost; /* 丢帧标志 */
    uint8_t  failsafe;   /* 失控保护激活 */
} ReceiverChannels_t;

/* 归一化后的摇杆值 (-1.0 ~ +1.0) */
typedef struct {
    float roll;
    float pitch;
    float throttle;
    float yaw;
    float aux1, aux2, aux3;
} ReceiverNorm_t;

void Receiver_Init(void);
int  Receiver_Read(ReceiverChannels_t *raw);
void Receiver_Normalize(ReceiverChannels_t *raw, ReceiverNorm_t *norm);
int  Receiver_IsConnected(void);

#endif
