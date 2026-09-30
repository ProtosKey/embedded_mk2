APP = calc
TARGET = ${APP}

BUILD_DIR = build
SOURCE_DIR = src
DRIVER_DIR = drivers
INCLUDE_DIR = include
LIB_DIR = lib

LDSCRIPT = STM32F407VGTx_FLASH.ld
OPENOCD_CFG = SDK11M.cfg

SOURCE_FILE = $(shell find ${SOURCE_DIR} ${DRIVER_DIR} -name "*.c" -type f)
LIB_FILE = $(wildcard ${LIB_DIR}/STM32F4xx_HAL_Driver/Src/*.c)
ASM_FILE = startup/startup_stm32f407xx.s

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS = -DUSE_HAL_DRIVER -DSTM32F407xx
INCLUDE = -I${INCLUDE_DIR} \
          -I${LIB_DIR}/STM32F4xx_HAL_Driver/Inc \
          -I${LIB_DIR}/STM32F4xx_HAL_Driver/Inc/Legacy \
          -I${LIB_DIR}/CMSIS/Device/ST/STM32F4xx/Include \
          -I${LIB_DIR}/CMSIS/Include

BASE_CFLAGS = ${MCU} ${DEFS} ${INCLUDE} -std=gnu11 -Og -g3 -ffunction-sections -fdata-sections -MMD -MP
CFLAGS      = ${BASE_CFLAGS} -Wall -Wextra
LIB_CFLAGS  = ${BASE_CFLAGS}
LDFLAGS = ${MCU} -T ${LDSCRIPT} --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--print-memory-usage -Wl,--no-warn-rwx-segments

OBJ = $(addprefix $(BUILD_DIR)/,$(SOURCE_FILE:.c=.o) $(LIB_FILE:.c=.o) $(ASM_FILE:.s=.o))

all: $(BUILD_DIR)/$(TARGET).bin compile_commands.json

$(BUILD_DIR)/$(TARGET).elf: $(OBJ) $(LDSCRIPT)
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(LIB_DIR)/%.o: $(LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(MCU) -x assembler-with-cpp -c $< -o $@

compile_commands.json: Makefile $(SOURCE_FILE)
	@echo "[" > $@
	@first=1; for f in $(SOURCE_FILE); do \
		[ $$first -eq 1 ] || echo "," >> $@; first=0; \
		printf '  {"directory": "%s", "file": "%s", "command": "%s %s -c %s"}' \
			"$(CURDIR)" "$$f" "$(CC)" "$(CFLAGS)" "$$f" >> $@; \
	done; echo "" >> $@; echo "]" >> $@

flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f $(OPENOCD_CFG) -c "program $< verify reset exit"

erase:
	openocd -f $(OPENOCD_CFG) -c "init; reset halt; stm32f2x mass_erase 0; exit"

check:
	openocd -f $(OPENOCD_CFG) -c "init; targets; exit"

debug: $(BUILD_DIR)/$(TARGET).elf
	openocd -f $(OPENOCD_CFG)

PORT ?= $(lastword $(sort $(wildcard /dev/cu.usbserial-*)))
monitor:
	screen $(PORT) 115200

clean:
	rm -rf $(BUILD_DIR) compile_commands.json

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

-include $(OBJ:.o=.d)

.PHONY: all flash erase check debug monitor clean size
