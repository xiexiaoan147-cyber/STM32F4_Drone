#include "camera.h"
#include <string.h>

void Camera_Init(void) {
    /* TODO: SPI2 或 DCMI 初始化, OV2640 寄存器配置 */
}

int Camera_Capture(CameraFrame_t *frame) {
    if (frame) memset(frame, 0, sizeof(*frame));
    return 0;
}

int Camera_StartStream(void)  { return 0; }
void Camera_StopStream(void)  {}
