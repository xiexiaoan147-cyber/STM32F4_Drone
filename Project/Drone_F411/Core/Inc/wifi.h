/** wifi.h — ESP8266 WiFi AT 命令驱动框架 (SPI1 或 UART) */
#ifndef WIFI_H
#define WIFI_H
#include <stdint.h>

typedef enum { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED } WiFiState_t;

void      WiFi_Init(void);
int       WiFi_SendAT(const char *cmd, char *resp, uint16_t resp_len);
int       WiFi_Connect(const char *ssid, const char *pwd);
int       WiFi_TCP_Send(const uint8_t *data, uint16_t len);
WiFiState_t WiFi_GetState(void);

#endif
