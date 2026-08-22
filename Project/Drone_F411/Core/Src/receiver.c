#include "receiver.h"
#include <string.h>

void Receiver_Init(void) {
    /* TODO: USART2 DMA RX 初始化, SBUS 波特率 100k, 8E2 */
}

int Receiver_Read(ReceiverChannels_t *raw) {
    if (!raw) return 0;
    memset(raw, 0, sizeof(*raw));
    raw->ch[0] = 1024; /* 摇杆中位 = 中立 */
    raw->ch[2] = 1024;
    raw->ch[3] = 576;  /* 油门最低 = 安全 */
    raw->frame_lost = 0;
    raw->failsafe   = 0;
    return 1;
}

void Receiver_Normalize(ReceiverChannels_t *raw, ReceiverNorm_t *norm) {
    if (!raw || !norm) return;
    norm->roll     = (float)((int)raw->ch[0] - 1024) / 672.0f;
    norm->pitch    = (float)((int)raw->ch[1] - 1024) / 672.0f;
    norm->throttle = (float)((int)raw->ch[2] - 576)  / 1216.0f;
    norm->yaw      = (float)((int)raw->ch[3] - 1024) / 672.0f;
}

int Receiver_IsConnected(void) {
    return 0; /* 虚拟模式: 未连接 */
}
