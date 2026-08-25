# STM32F4_Drone — STM32F411 小四轴飞控

基于 **STM32F411CEU6**(WeAct 黑色蚯蚓板) + **MPU6050** + FreeRTOS 的四轴无人机飞控。

## 目录结构

```
Project/Drone_F411
├── Core/                    应用代码
│   ├── Inc/                   头文件 (含 FreeRTOSConfig.h / stm32f4xx_hal_conf.h)
│   └── Src/                   main / freertos / 控制链 / 驱动
├── Drivers/                 第三方 (vendored)
│   ├── STM32F4xx_HAL_Driver/  ST HAL (按 hal_conf 启用的模块子集)
│   └── CMSIS/                 CMSIS 5 核心 + F4 设备头 + GCC 启动文件
├── Middlewares/Third_Party/FreeRTOS/   FreeRTOS 内核 (tasks/queue/list/timers + CM4F 端口 + heap_4)
├── Tools/host_test/         主机端回归测试 (无需 arm 工具链)
├── Makefile
└── STM32F411CEUX_FLASH.ld
```

## 构建

需要 `arm-none-eabi-gcc`(推荐 xPack GNU Arm Embedded ≥ 10)和 `make`:

```bash
cd Project/Drone_F411
make              # 产物: build/drone_f411.{elf,bin,hex,map}
make flash        # st-flash 烧录 (或 make flash-openocd)
```

无 arm 工具链时也可跑主机端控制逻辑回归:

```bash
cd Project/Drone_F411/Tools/host_test && ./run.sh
```

## 硬件配置

| 外设 | 引脚 | 用途 |
|---|---|---|
| I2C1 | PB8(SCL)/PB9(SDA), 400kHz | MPU6050 (0x68) |
| TIM3 CH1-4 | PA6/PA7/PB0/PB1, 4kHz PWM | 电机 M2/M1/M3/M4 (映射见 motor_pwm.h) |
| USART1 | PA9(TX)/PA10(RX), 115200 | 调试 printf + 蓝牙指令 |
| HSE | 25MHz → PLL 100MHz | 时钟 |
| IWDG | LSI, 1s | 看门狗 (控制任务喂狗) |

## 软件架构

```
USART1 ISR ─字节→ xByteQueue ─→ Comm 任务 ─BLE帧→ Control_SetTarget / Armed / Emergency
MPU6050 ─→ ImuRead (500Hz) ─xImuQueue→ Attitude (500Hz, 滤波+Mahony) ─g_att→
Control (200Hz: 安全链→状态机→串级PID→X混控) ─→ TIM3 PWM
```

- **姿态**: 二阶 IIR 低通 + Mahony 互补滤波 (四元数), 垂直加速度去重力 (az 悬停闭环)
- **控制**: 角度环 P → 角速度环 PID(测量微分 D) → X 型混控
- **状态机**: IDLE → DISARMED → FOLLOW(油门跟随) → HOVER(az 闭环, 3s 自动降落) → LANDING(落地自锁)
- **安全链**: 急停锁存 → 未解锁 → NaN 防护 → 45°姿态(200ms 去抖) → 失控 1s 缓降 → 姿态丢失 0.5s 急停 → IWDG

## 蓝牙协议 (BLE_Protocol)

帧: `[0xA5][CMD][LEN][DATA 0~16B][XOR 校验][0x5A]`, 小端 float32。

| CMD | 数据 | 行为 |
|---|---|---|
| 0x01 PING | - | 回 ACK |
| 0x20 EMERGENCY | - | 急停 |
| 0x24 ARM | 1 字节 1/0 | 解锁/上锁 |
| 0x40 SETPOINT | 4×float | throttle/roll/pitch/yaw_rate |

SETPOINT 取值: throttle ∈ [0,1], roll/pitch ∈ ±0.52 rad, yaw_rate ∈ ±3.14 rad/s。

## 首飞检查清单 (必做)

- [ ] `motor_pwm.h` 的 LOGICAL_Mx_CHANNEL 与 PCB 实际接线一致
- [ ] 台架验证: +roll 指令 → 左侧电机加速; +pitch → 前侧加速; +yaw 方向与桨转向匹配 (方向错=首炸)
- [ ] MPU6050 芯片方向与 mahony 假设一致 (静止时 roll/pitch ≈ 0)
- [ ] 电池电压/重量确定 HOVER 油门在 [0.30, 0.85] 钳位带内

## 状态与已知限制

- receiver(SBUS)/wifi/gps/camera 为占位桩, 未实现
- 低电压保护未接 ADC 采样 (Safety_Update 的电池参数暂传 0 跳过检测)
- HOVER 为 az 闭环 (无气压计), 大机动后高度基准会缓慢漂移
