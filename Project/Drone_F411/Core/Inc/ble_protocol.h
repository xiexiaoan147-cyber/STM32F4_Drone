/**
 * ble_protocol.h — 蓝牙通信协议
 *
 * 帧格式 (二进制, 定界符 + 校验):
 *  [STX 0xA5] [CMD 1B] [LEN 1B] [DATA 0~16B] [CHKSUM 1B] [ETX 0x5A]
 *
 * 校验: CHKSUM = XOR(CMD, LEN, DATA[0..LEN-1])
 */
#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <stdint.h>

#define BLE_FRAME_MAX_DATA 16
#define BLE_FRAME_OVERHEAD 5  /* STX+CMD+LEN+CHK+ETX */
#define BLE_FRAME_MAX      (BLE_FRAME_OVERHEAD + BLE_FRAME_MAX_DATA)

/* 指令码 */
enum {
    BLE_CMD_PING       = 0x01,  /* 是否在连接状态 */
    BLE_CMD_THROTTLE   = 0x10,  /* 油门 (0.0~1.0) */
    BLE_CMD_ROLL       = 0x11,  /* 横滚目标 (±30°) */
    BLE_CMD_PITCH      = 0x12,  /* 俯仰目标 (±30°) */
    BLE_CMD_YAW        = 0x13,  /* 偏航角速度 (±180°/s) */
    BLE_CMD_EMERGENCY  = 0x20,  /* 紧急停机 */
    BLE_CMD_ALT_HOLD   = 0x21,  /* 定高开关 (0/1) */
    BLE_CMD_ATTITUDE   = 0x30,  /* 姿态四元数 (4×float) */
    BLE_CMD_SETPOINT   = 0x40,  /* 四通道综合指令 */
};

/* 四通道综合指令数据结构 */
typedef struct {
    float throttle;  /* 0.0 ~ 1.0 */
    float roll;      /* rad, ±0.52 (±30°) */
    float pitch;     /* rad */
    float yaw_rate;  /* rad/s, ±3.14 (±180°/s) */
} BleSetpoint_t;

/* 协议解析状态机 */
typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_CMD,
    BLE_STATE_LEN,
    BLE_STATE_DATA,
    BLE_STATE_CHK,
    BLE_STATE_ETX
} BleState_t;

/**
 * @brief 蓝牙协议初始化
 */
void BLE_Protocol_Init(void);

/**
 * @brief 喂入一个字节，状态机自动解析
 * @param byte 接收到的字节
 * @return 0=未完成, 1=解析到完整帧
 */
int BLE_Protocol_Feed(uint8_t byte);

/**
 * @brief 获取当前解包后的指令码
 */
uint8_t BLE_GetCmd(void);

/**
 * @brief 获取当前解包后的数据长度
 */
uint8_t BLE_GetDataLen(void);

/**
 * @brief 获取当前解包后的数据指针
 */
const uint8_t* BLE_GetData(void);

/**
 * @brief 解析四通道综合指令 (CMD=0x40)
 * @param sp 输出结构体
 * @return 0=失败, 1=成功
 */
int BLE_ParseSetpoint(BleSetpoint_t *sp);

/**
 * @brief 组装 ACK 应答帧到 buf
 * @param cmd 应答的指令码
 * @param status 0=成功
 * @param buf 输出缓冲
 * @return 帧长度
 */
int BLE_BuildAck(uint8_t cmd, uint8_t status, uint8_t *buf);

/**
 * @brief 虚拟注入一条指令（用于仿真测试，无需真实蓝牙）
 * @param cmd 指令码
 * @param data 数据
 * @param len 数据长度
 */
// void BLE_InjectCommand(uint8_t cmd, const uint8_t *data, uint8_t len);

#endif /* BLE_PROTOCOL_H */
