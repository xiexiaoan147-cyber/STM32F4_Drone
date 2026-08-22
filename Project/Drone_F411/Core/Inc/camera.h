/** camera.h — 摄像头图像采集框架 (SPI2 OV2640 / DCMI) */
#ifndef CAMERA_H
#define CAMERA_H
#include <stdint.h>

typedef struct {
    uint8_t *buf;
    uint32_t len;
    uint32_t width, height;
} CameraFrame_t;

void Camera_Init(void);
int  Camera_Capture(CameraFrame_t *frame);
int  Camera_StartStream(void);
void Camera_StopStream(void);

#endif
