###############################################################################
# Makefile for project 
###############################################################################

## LCD driver SPI or I2C
DRIVER	:=	SPI
RST_PIN	:=	8
CS_PIN	:=	10
DC_PIN	:=	9

PROJECT = 
MCU = ATmega328P
COMPILER = "Arduino AVR"
TARGET = Debug
SHELL = C:\Windows\system32\cmd.exe
ARDUINO_PATH="D:\Program Files\Proteus 8 Professional\Tools\ARDUINO"
AVRDUE_CONF = "D:\Program Files\Arduino\hardware\tools\avr\etc\avrdude.conf"
BUILD_DIR=bin
OPTIMIZE = Os

UPLOAD_PORT = COM3
UPLOAD_BAUD = 115200
UPLOAD_PROGRAMER = arduino

SRCS = $(wildcard *.c) 
OBJS = $(addprefix $(BUILD_DIR)/,$(SRCS:.c=.o))
THIS_DIR = $(abspath .)
CURR_FOLDER_NAME =$(lastword $(notdir $(THIS_DIR)))

MYLIB_PATH = ../mylib
MYLIB_OUT_PATH = $(BUILD_DIR)/mylib
MYLIB_OUT_PATH_RELATIVE_FROM_MYLIB = ../$(CURR_FOLDER_NAME)/$(MYLIB_OUT_PATH)

## Tools general options
APPFLAGS=-C pro328 -T -std=gnu++11 -T -fno-threadsafe-statics -N atmega328p -F 16000000
LDFLAGS=-Wl,--gc-sections -lm -mmcu=atmega328p \
		-L $(MYLIB_OUT_PATH) -L $(BUILD_DIR) -L $(BUILD_DIR)/arduino

CCFLAGS=-Wall -gdwarf-2 -fno-exceptions -DF_CPU=16000000 \
		-I . \
		-DARDUINO_ARCH_AVR @$(BUILD_DIR)/arduino/libs.inc \
		-$(OPTIMIZE) -mmcu=atmega328p
CFLAGS:=$(CCFLAGS)
##编译到boot区
# LDFLAGS += -Wl,--section-start=.text=0x3800

CCFLAGS+= -std=gnu++11 -fno-threadsafe-statics 
CFLAGS+= -std=gnu11  


##没用到的函数将不会编译进去
CCFLAGS+= -ffunction-sections -fdata-sections 
CFLAGS+= -ffunction-sections -fdata-sections 

## Processing Tools
APP=$(ARDUINO_PATH)\mksketch
CC=avr-gcc
ASM=avr-gcc
LD=avr-gcc

# Build tree:
all:	Debug

.PHONY: upload
upload: 
	avrdude -C$(AVRDUE_CONF) -v \
	-p$(MCU) -c$(UPLOAD_PROGRAMER) -P$(UPLOAD_PORT) -b$(UPLOAD_BAUD) -D -Uflash:w:$(BUILD_DIR)/Debug.hex:i

Debug: $(BUILD_DIR)/Debug.elf 
	avr-size $<

$(BUILD_DIR)/Debug.elf: $(BUILD_DIR)/main.o mylib.a SSD1306.o $(BUILD_DIR)/DS1307.o AT24C32.o
	$(LD) $(LDFLAGS) -o $@ $(addprefix -l:,$(notdir $^)) -l:arduino.a -lm 
	avr-objcopy -O ihex -R .eeprom "$(BUILD_DIR)/Debug.elf" "$(BUILD_DIR)/Debug.hex"
	avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 --no-change-warnings -O ihex "$(BUILD_DIR)/Debug.elf" "$(BUILD_DIR)/Debug.eep" || exit 0 

$(BUILD_DIR)/main.o: $(BUILD_DIR)/main.cpp 
	$(CC) $(CCFLAGS) -o $@ -c $<

$(BUILD_DIR)/main.cpp:	main.ino 
	-mkdir "$(BUILD_DIR)"
	$(APP) $(APPFLAGS) -A $(ARDUINO_PATH) -M $(BUILD_DIR)/arduino -O $@ $<
	make -f $(BUILD_DIR)/arduino/Makefile all

mylib.a: $(wildcard $(MYLIB_PATH)/*.c) $(wildcard $(MYLIB_PATH)/*.h) 
	make -C $(MYLIB_PATH) all OUT_DIR=$(MYLIB_OUT_PATH_RELATIVE_FROM_MYLIB)

AT24C32.o: $(wildcard $(MYLIB_PATH)/AT24C32/*.c) $(wildcard $(MYLIB_PATH)/AT24C32/*.h) 
	make -C $(MYLIB_PATH)/AT24C32 all OUT_DIR=../$(MYLIB_OUT_PATH_RELATIVE_FROM_MYLIB)


$(BUILD_DIR)/DS1307.o: $(wildcard $(MYLIB_PATH)/DS1307/*.c) $(wildcard $(MYLIB_PATH)/DS1307/*.h) 
	$(CC) $(CFLAGS) -o $@ -c $<	

# DS1307.o: $(wildcard $(MYLIB_PATH)/DS1307/*.c) $(wildcard $(MYLIB_PATH)/DS1307/*.h) 
# 	make -C $(MYLIB_PATH)/DS1307 all OUT_DIR=../$(MYLIB_OUT_PATH_RELATIVE_FROM_MYLIB)

SSD1306.o: $(wildcard $(MYLIB_PATH)/SSD1306/*.c) $(wildcard $(MYLIB_PATH)/SSD1306/*.h) 
	make -C $(MYLIB_PATH)/SSD1306 all OUT_DIR=../$(MYLIB_OUT_PATH_RELATIVE_FROM_MYLIB) \
	DRIVER=$(DRIVER) RST_PIN=$(RST_PIN) CS_PIN=$(CS_PIN) DC_PIN=$(DC_PIN) SSD1306_I2C_ADDRESS=$(SSD1306_I2C_ADDRESS)

# bootloader - attach bootloader to the firmware
bootloader:
	avr-objcopy -O ihex -R .eeprom "$(BUILD_DIR)/Debug.elf" "$(BUILD_DIR)/Debug.hex"
	copy /b bootloader.hex+"$(BUILD_DIR)/Debug.hex" "$(BUILD_DIR)/with.bootloader.hex"

# tidy - delete all temporary files which are not involved in the target generation
tidy:
	rm -rf $(BUILD_DIR)/main.d
# -make -f $(BUILD_DIR)/arduino/Makefile tidy

# cleanup - delete all generated files
clean:	tidy
	rm -rf $(BUILD_DIR)/Debug.elf
	rm -rf $(BUILD_DIR)/main.cpp
	rm -rf $(BUILD_DIR)/*.o
	rm -rf $(MYLIB_OUT_PATH)/*.o
	rm -rf $(MYLIB_OUT_PATH)/*.a
#-make -f $(BUILD_DIR)/arduino/Makefile clean