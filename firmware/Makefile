# Bare-metal firmware for the exercises in ../README.md (Nucleo-F429ZI).
#
#   make EX=07           build exercise 7 -> build/ex07.elf and build/ex07.bin
#   make flash EX=07     build it and flash it through the board's ST-LINK
#   make all             build every exercise (this is the compile check)
#   make clean
#
# Options:
#   OPT=-O0                    optimisation level (exercise 5 compares -O0 and -O2)
#   EXTRA_CFLAGS=-DSTEP=1      extra compiler flags, e.g. to pick a step in an exercise
#   FLASH_TOOL=openocd         cubeprog (default), openocd or stflash

EX         ?= 02
OPT        ?= -O2
FLASH_TOOL ?= cubeprog

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Exercise sources are named exNN_description.c
EX_FILES  := $(wildcard ex[0-9][0-9]_*.c)
EXERCISES := $(sort $(foreach f,$(EX_FILES),$(patsubst ex%,%,$(word 1,$(subst _, ,$(f))))))

EX_SRC     := $(wildcard ex$(EX)_*.c)
COMMON_SRC := startup.c $(wildcard common/*.c)
BUILD      := build/ex$(EX)
OBJS       := $(addprefix $(BUILD)/,$(EX_SRC:.c=.o) $(COMMON_SRC:.c=.o))

ifneq ($(filter-out all clean,$(or $(MAKECMDGOALS),build)),)
  ifeq ($(EX_SRC),)
    $(error No source for EX=$(EX). Use two digits; available: $(EXERCISES))
  endif
endif

CPU     = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS  = $(CPU) $(OPT) -g3 -std=c11 -Wall -Wextra \
          -ffunction-sections -fdata-sections \
          -DSTM32F429xx -Icmsis/device -Icmsis/core -Icommon \
          -MMD -MP $(EXTRA_CFLAGS)
LDFLAGS = $(CPU) -T linker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD).map \
          --specs=nano.specs -u _printf_float -Wl,--no-warn-rwx-segments
LDLIBS  = -lm

# Rebuild when the flags change (e.g. a different STEP or OPT), not only when sources do.
FLAGS_FILE := $(BUILD)/flags
ifneq ($(filter-out all clean,$(or $(MAKECMDGOALS),build)),)
  $(shell mkdir -p $(BUILD); echo '$(CFLAGS) $(LDFLAGS)' | cmp -s - $(FLAGS_FILE) || echo '$(CFLAGS) $(LDFLAGS)' > $(FLAGS_FILE))
endif

.PHONY: build flash all clean

build: build/ex$(EX).bin

build/ex$(EX).elf: $(OBJS) linker.ld $(FLAGS_FILE)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	$(SIZE) $@

build/ex$(EX).bin: build/ex$(EX).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/%.o: %.c Makefile $(FLAGS_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(FLASH_TOOL),cubeprog)
flash: build
	STM32_Programmer_CLI -c port=SWD mode=UR -w build/ex$(EX).elf -v -rst
else ifeq ($(FLASH_TOOL),openocd)
flash: build
	openocd -f board/st_nucleo_f4.cfg -c "program build/ex$(EX).elf verify reset exit"
else ifeq ($(FLASH_TOOL),stflash)
flash: build
	st-flash --reset write build/ex$(EX).bin 0x08000000
else
  $(error Unknown FLASH_TOOL=$(FLASH_TOOL): use cubeprog, openocd or stflash)
endif

all:
	@for ex in $(EXERCISES); do \
	  $(MAKE) --no-print-directory EX=$$ex build || exit 1; \
	done

clean:
	rm -rf build

-include $(OBJS:.o=.d)
