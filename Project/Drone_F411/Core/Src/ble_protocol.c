/**
 * ble_protocol.c — 蓝牙通信协议实现
 *
 * 帧格式:
 *  [STX] [CMD] [LEN] [DATA...] [CHKSUM] [ETX]
 *   0xA5   1B    1B    0~16B     XOR      0x5A
 *
 * 数据编码约定 (SETPOINT):
 *  - 4 × float32, IEEE-754, 小端序 (STM32 原生, 与发送端约定一致)
 *  - throttle   [0.0, 1.0]
 *  - roll/pitch [rad, ±0.52] (±30°)
 *  - yaw_rate   [rad/s, ±3.14] (±180°/s)
 *
 * 帧校验 (严格):
 *  - CHK 匹配后必须收到独立 ETX(0x5A) 才算完整帧
 *  - 校验失败/帧尾错误 → 整帧丢弃
 */
#include "ble_protocol.h"
#include <string.h>
#include <math.h>

#define STX 0xA5
#define ETX 0x5A

/* 控制边界 (与 ble_protocol.h 及 control_logic.md 统一) */
#define CTRL_ROLL_MAX_RAD   0.52f   /* ±30° */
#define CTRL_PITCH_MAX_RAD  0.52f   /* ±30° */
#define CTRL_YAW_RATE_MAX   3.14f   /* ±180°/s */

static uint8_t rx_buf[BLE_FRAME_MAX];
static uint8_t cmd, data_len, chk_calc;
static uint8_t data_idx;
static BleState_t state = BLE_STATE_IDLE;

void BLE_Protocol_Init(void)
{
    state = BLE_STATE_IDLE;
    cmd = 0; data_len = 0; data_idx = 0; chk_calc = 0;
}

int BLE_Protocol_Feed(uint8_t byte)
{
    switch (state) {
    case BLE_STATE_IDLE:
        if (byte == STX) {
            state = BLE_STATE_CMD;
            chk_calc = 0;
        }
        break;

    case BLE_STATE_CMD:
        cmd = byte;
        chk_calc ^= byte;
        state = BLE_STATE_LEN;
        break;

    case BLE_STATE_LEN:
        data_len = byte;
        chk_calc ^= byte;
        if (data_len > BLE_FRAME_MAX_DATA) {
            state = BLE_STATE_IDLE; /* 长度非法, 丢弃 */
            return 0;
        }
        data_idx = 0;
        state = (data_len > 0) ? BLE_STATE_DATA : BLE_STATE_CHK;
        break;

    case BLE_STATE_DATA:
        rx_buf[data_idx++] = byte;
        chk_calc ^= byte;
        if (data_idx >= data_len) state = BLE_STATE_CHK;
        break;

    case BLE_STATE_CHK:
        /* 校验通过 → 等待 ETX; 失败 → 丢弃 */
        if (byte == chk_calc) state = BLE_STATE_ETX;
        else                  state = BLE_STATE_IDLE;
        break;

    case BLE_STATE_ETX:
        /* 独立 ETX 状态: 必须收到 0x5A 才确认完整帧 */
        if (byte == ETX) 
        {
            state = BLE_STATE_IDLE;
            return 1;   /* 完整帧校验成功 */
        }
        state = BLE_STATE_IDLE; /* 帧尾非法, 整帧丢弃 */
        break;

    default:
        state = BLE_STATE_IDLE;
        break;
    }
    return 0;
}

uint8_t BLE_GetCmd(void)        { return cmd; }
uint8_t BLE_GetDataLen(void)    { return data_len; }
const uint8_t *BLE_GetData(void){ return rx_buf; }

int BLE_ParseSetpoint(BleSetpoint_t *sp)
{
    if (cmd != BLE_CMD_SETPOINT || data_len != 16) return 0;

    /* IEEE-754 float32, 小端序 (STM32 原生) */
    memcpy(&sp->throttle, rx_buf,      4);
    memcpy(&sp->roll,     rx_buf + 4,  4);
    memcpy(&sp->pitch,    rx_buf + 8,  4);
    memcpy(&sp->yaw_rate, rx_buf + 12, 4);

    /* 合法性与范围检查 (与头文件/控制文档一致) */
    float t = sp->throttle, r = sp->roll, p = sp->pitch, yr = sp->yaw_rate;
    if (!isfinite(t) || t < 0.0f || t > 1.0f)                        return 0;
    if (!isfinite(r) || r < -CTRL_ROLL_MAX_RAD  || r > CTRL_ROLL_MAX_RAD)  return 0;
    if (!isfinite(p) || p < -CTRL_PITCH_MAX_RAD || p > CTRL_PITCH_MAX_RAD) return 0;
    if (!isfinite(yr)|| yr < -CTRL_YAW_RATE_MAX || yr > CTRL_YAW_RATE_MAX) return 0;
    return 1;
}

int BLE_BuildAck(uint8_t ack_cmd, uint8_t status, uint8_t *buf)
{
    buf[0] = STX;
    buf[1] = ack_cmd | 0x80; /* bit7=1 表示应答 */
    buf[2] = 1;               /* 1 字节数据 */
    buf[3] = status;
    buf[4] = buf[1] ^ buf[2] ^ buf[3]; /* XOR 校验 */
    buf[5] = ETX;
    return 6;
}

/* 虚拟指令注入 (仿真用) */
// void BLE_InjectCommand(uint8_t inject_cmd, const uint8_t *data, uint8_t len)
// {
//     uint8_t frame[BLE_FRAME_MAX];
//     int i;

//     frame[0] = STX;
//     frame[1] = inject_cmd;
//     frame[2] = len;
//     uint8_t chk = inject_cmd ^ len;
//     for (i = 0; i < len; i++) {
//         frame[3 + i] = data[i];
//         chk ^= data[i];
//     }
//     frame[3 + len] = chk;
//     frame[4 + len] = ETX;

//     for (i = 0; i < (int)(5 + len); i++) {
//         if (BLE_Protocol_Feed(frame[i])) break;
//     }
// }
