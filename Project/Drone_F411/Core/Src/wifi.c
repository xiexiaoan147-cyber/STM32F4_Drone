#include "wifi.h"
#include <string.h>

void WiFi_Init(void) {
    /* TODO: SPI1 或 USART 初始化, ESP8266 波特率 115200 */
}

int WiFi_SendAT(const char *cmd, char *resp, uint16_t len) {
    (void)cmd; if (resp) resp[0] = '\0';
    return 0; /* 虚拟: 未连接 */
}

int WiFi_Connect(const char *ssid, const char *pwd) {
    (void)ssid; (void)pwd;
    return 0;
}

int WiFi_TCP_Send(const uint8_t *data, uint16_t len) {
    (void)data; (void)len;
    return 0;
}

WiFiState_t WiFi_GetState(void) { return WIFI_DISCONNECTED; }
