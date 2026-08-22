#include "gps.h"
#include <string.h>

void GPS_Init(void) {
    /* TODO: USART 初始化, 波特率 9600, NMEA 解析 */
}

int GPS_Read(GPS_Data_t *data) {
    if (!data) return 0;
    memset(data, 0, sizeof(*data));
    return 0; /* 虚拟: 无数据 */
}

int GPS_HasFix(void) { return 0; }
