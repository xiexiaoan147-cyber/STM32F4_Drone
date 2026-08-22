/** gps.h — GPS NMEA 解析驱动框架 (USART2 或独立 UART) */
#ifndef GPS_H
#define GPS_H
#include <stdint.h>

typedef struct {
    float lat, lon, alt;    /* 纬度(°), 经度(°), 高度(m) */
    float speed, course;    /* 速度(m/s), 航向(°) */
    uint8_t satellites;     /* 卫星数 */
    uint8_t fix;            /* 0=无定位, 1=GPS, 2=DGPS */
    uint32_t timestamp_ms;
} GPS_Data_t;

void GPS_Init(void);
int  GPS_Read(GPS_Data_t *data);
int  GPS_HasFix(void);

#endif
