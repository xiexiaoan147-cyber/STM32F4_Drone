# ==============================================================================
# Drone_F411 — STM32F411CEU6 四轴飞控 Makefile
#
# 工具链: arm-none-eabi-gcc (如 xPack GNU Arm Embedded)
# 构建:   make            (输出 build/drone_f411.elf/.bin/.hex/.map)
# 烧录:   make flash      (st-flash, 也可用 make flash-openocd / make size)
# 清理:   make clean
# ==============================================================================

TARGET       := drone_f411
BUILD_DIR    := build

CC           := arm-none-eabi-gcc
AS           := arm-none-eabi-gcc
CP           := arm-none-eabi-objcopy
SZ           := arm-none-eabi-size

# ---------------- Cortex-M4F + 硬件FPU ----------------
CPU          := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

DEFS         := -DUSE_HAL_DRIVER -DSTM32F411xE

INCS         := -ICore/Inc \
                -IDrivers/STM32F4xx_HAL_Driver/Inc \
                -IDrivers/CMSIS/Include \
                -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
                -IMiddlewares/Third_Party/FreeRTOS/include \
                -IMiddlewares/Third_Party/FreeRTOS/portable/GCC/ARM_CM4F

# ---------------- 源文件 ----------------
C_SOURCES    := \
Core/Src/main.c \
Core/Src/gpio.c \
Core/Src/usart.c \
Core/Src/i2c.c \
Core/Src/stm32f4xx_it.c \
Core/Src/stm32f4xx_hal_msp.c \
Core/Src/system_stm32f4xx.c \
Core/Src/freertos.c \
Core/Src/mpu6050_i2c.c \
Core/Src/data_process.c \
Core/Src/mahony.c \
Core/Src/control.c \
Core/Src/mixer.c \
Core/Src/motor_pwm.c \
Core/Src/safety.c \
Core/Src/pid.c \
Core/Src/ble_protocol.c \
Core/Src/receiver.c \
Core/Src/wifi.c \
Core/Src/gps.c \
Core/Src/camera.c \
Core/Src/imu_virtual.c \
Core/Src/motor_virtual.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
Middlewares/Third_Party/FreeRTOS/tasks.c \
Middlewares/Third_Party/FreeRTOS/queue.c \
Middlewares/Third_Party/FreeRTOS/list.c \
Middlewares/Third_Party/FreeRTOS/timers.c \
Middlewares/Third_Party/FreeRTOS/portable/GCC/ARM_CM4F/port.c \
Middlewares/Third_Party/FreeRTOS/portable/MemMang/heap_4.c

ASM_SOURCES  := \
Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f411xe.s

LDSCRIPT     := STM32F411CEUX_FLASH.ld

# ---------------- 编译/链接选项 ----------------
COMMON_FLAGS := $(CPU) $(DEFS) $(INCS) -fdata-sections -ffunction-sections
# -u _printf_float (链接期): newlib-nano 默认裁掉浮点格式化, 日志用了 %f
CFLAGS       := $(COMMON_FLAGS) -std=gnu11 -Wall -Wextra -O2 -g3
ASFLAGS      := $(CPU) -x assembler-with-cpp $(INCS) $(DEFS)
LDFLAGS      := $(CPU) -specs=nano.specs -specs=nosys.specs \
                -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
                -Wl,--gc-sections -Wl,--print-memory-usage \
                -u _printf_float -lm

OBJECTS      := $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS      += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

DEPS         := $(OBJECTS:.o=.d)

.PHONY: all clean flash flash-openocd

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
	@$(SZ) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@ -MD -MP -MF $(BUILD_DIR)/$*.d

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(CP) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(CP) -O binary -S $< $@

$(BUILD_DIR):
	mkdir -p $@

flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08000000

flash-openocd: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	        -c "program $< verify reset exit"

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
