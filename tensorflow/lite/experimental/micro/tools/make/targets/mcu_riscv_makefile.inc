# Settings for RISCV MCU toolchain.
ifeq ($(TARGET), riscv32_mcu)
  TARGET_ARCH := riscv32_mcu
  TARGET_TOOLCHAIN_PREFIX := riscv64-unknown-elf-

  PLATFORM_FLAGS = \
    -march=rv32imac \
    -mabi=ilp32 \
    -mcmodel=medany \
    -fno-builtin-printf \
    -fno-exceptions \
    -DTF_LITE_MCU_DEBUG_LOG \
    -DTF_LITE_USE_GLOBAL_ROUND \
    -fno-rtti \
    -fno-unwind-tables \
    -fno-builtin \
    -ffunction-sections \
    -fdata-sections \
    -funsigned-char \
    -Wvla \
    -Wall \
    -Wextra \
    -Wno-unused-parameter \
    -Wno-missing-field-initializers \
    -Wno-write-strings \
    -Wno-sign-compare \
    -fno-delete-null-pointer-checks \
    -fomit-frame-pointer \
    -static \
    -Os

  CXXFLAGS += $(PLATFORM_FLAGS) \
    -fpermissive \
    -fno-exceptions \
    --std=gnu++11

  CCFLAGS += $(PLATFORM_FLAGS)

  BUILD_TYPE := micro

  INCLUDES += \
    -I$(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/include \
    -I$(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/drivers/ \
    -I$(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env \
    -I$(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env/sirv-e203-lichee

  MICROLITE_CC_SRCS += \
    $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/libwrap/misc/write_hex.c \
    $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env/bk_common/crt.S \
    $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env/bk_common/syscalls.c \
    $(wildcard tensorflow/lite/experimental/micro/riscv32_mcu/*.cc)

  #<<JC>> MICRO_SPEECH_TEST_SRCS += \
  #<<JC>>   $(wildcard $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/libwrap/sys/*.c) \
  #<<JC>>   $(wildcard $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/libwrap/sys/*.cc) \
  #<<JC>>   $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/libwrap/misc/write_hex.c \
  #<<JC>>   $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/libwrap/stdlib/malloc.c \
  #<<JC>>   $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env/start.S \
  #<<JC>>   $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env/entry.S \
  #<<JC>>   $(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env/sirv-e203-lichee/init.c

  LDFLAGS += \
   -T$(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env/bk_common/test.ld \
   -nostartfiles \
   -L$(MAKEFILE_DIR)/downloads/licheetang_e203_lib/bsp/env \
   --specs=nano.specs

# These are microcontroller-specific rules for converting the ELF output
# of the linker into a binary image that can be loaded directly.
OBJCOPY := $(TARGET_TOOLCHAIN_PREFIX)objcopy

$(BINDIR)/%.bin: $(BINDIR)/%
	@mkdir -p $(dir $@)
	$(OBJCOPY) $< $@ -O binary
endif
