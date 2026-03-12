NASM ?= nasm
CC ?= i686-elf-gcc
LD ?= i686-elf-ld
GRUB_MKRESCUE ?= grub-mkrescue
QEMU ?= qemu-system-i386
QEMU_OPTS ?= -accel tcg,thread=single -icount auto,sleep=on
QEMU_SERIAL_OPTS ?=
QEMU_VIDEO_OPTS ?= -vga std
QEMU_RAM ?= 4M
QEMU_LOW_RAM ?= 2M
ARCH ?= i386

BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/isofiles
OBJ_DIR := $(BUILD_DIR)/obj
USER_BUILD_DIR := $(BUILD_DIR)/user
KERNEL_DIR := kernel
ARCH_DIR := $(KERNEL_DIR)/arch/$(ARCH)

KERNEL_ELF := $(BUILD_DIR)/slop-kernel.elf
KERNEL_FB_ELF := $(BUILD_DIR)/slop-kernel-fb.elf
ISO_IMAGE := $(BUILD_DIR)/slop.iso
ISO_FB_IMAGE := $(BUILD_DIR)/slop-fb.iso
BOOT_BIN := $(BUILD_DIR)/boot.bin
LEGACY_KERNEL_BIN := $(BUILD_DIR)/kernel.bin
FLOPPY_IMAGE := $(BUILD_DIR)/slop-floppy.img

CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32 -march=i486 -mtune=i486 -O2 -Wall -Wextra -I$(KERNEL_DIR)
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
USER_CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32 -march=i486 -mtune=i486 -O2 -Wall -Wextra
USER_LDFLAGS := -m elf_i386 -T user/linker.ld -nostdlib

BOOT_SRC := $(ARCH_DIR)/boot32.asm
BOOT_FB_SRC := $(ARCH_DIR)/boot32_fb.asm
BOOT_OBJ := $(patsubst $(KERNEL_DIR)/%.asm,$(OBJ_DIR)/%.o,$(BOOT_SRC))
BOOT_FB_OBJ := $(patsubst $(KERNEL_DIR)/%.asm,$(OBJ_DIR)/%.o,$(BOOT_FB_SRC))
KERNEL_ASM_SRCS := $(ARCH_DIR)/irq_stubs.asm $(ARCH_DIR)/syscall_stubs.asm $(ARCH_DIR)/protection_low.asm
KERNEL_ASM_OBJ := $(patsubst $(KERNEL_DIR)/%.asm,$(OBJ_DIR)/%.o,$(KERNEL_ASM_SRCS))
KERNEL_GENERIC_C_SRCS := $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_ARCH_C_SRCS := $(wildcard $(ARCH_DIR)/*.c)
KERNEL_C_SRCS := $(KERNEL_GENERIC_C_SRCS) $(KERNEL_ARCH_C_SRCS)
KERNEL_C_OBJ := $(patsubst $(KERNEL_DIR)/%.c,$(OBJ_DIR)/%.o,$(KERNEL_C_SRCS))

USER_PROGS := cat touch ls pwd mkdir ps systeminfo free
USER_CRT0_OBJ := $(USER_BUILD_DIR)/crt0.o
USER_PROG_OBJ := $(addprefix $(USER_BUILD_DIR)/,$(addsuffix .o,$(USER_PROGS)))
USER_PROG_ELF := $(addprefix $(USER_BUILD_DIR)/,$(addsuffix .elf,$(USER_PROGS)))
ISO_USER_BIN := $(addprefix $(ISO_ROOT)/bin/,$(USER_PROGS))

ifeq ($(OS),Windows_NT)
SHELL := cmd
.SHELLFLAGS := /C

winpath = $(subst /,\,$1)
MKDIR_P = if not exist "$(call winpath,$1)" mkdir "$(call winpath,$1)"
RM_RF = if exist "$(call winpath,$1)" rmdir /S /Q "$(call winpath,$1)"
COPY_FILE = copy /Y "$(call winpath,$1)" "$(call winpath,$2)" >NUL
else
MKDIR_P = mkdir -p "$1"
RM_RF = rm -rf "$1"
COPY_FILE = cp "$1" "$2"
endif

.PHONY: all iso iso-fb floppy run run-lowmem run-fb run-floppy run-floppy-cd clean

all: iso

$(BUILD_DIR) $(OBJ_DIR) $(USER_BUILD_DIR):
	$(call MKDIR_P,$@)

$(OBJ_DIR)/%.o: $(KERNEL_DIR)/%.asm | $(OBJ_DIR)
	$(call MKDIR_P,$(dir $@))
	$(NASM) -f elf32 -o $@ $<

$(OBJ_DIR)/%.o: $(KERNEL_DIR)/%.c | $(OBJ_DIR)
	$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) -c -o $@ $<

$(USER_CRT0_OBJ): user/crt0.asm | $(USER_BUILD_DIR)
	$(NASM) -f elf32 -o $@ $<

$(USER_BUILD_DIR)/%.o: user/%.c | $(USER_BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_BUILD_DIR)/%.elf: $(USER_CRT0_OBJ) $(USER_BUILD_DIR)/%.o user/linker.ld | $(USER_BUILD_DIR)
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_CRT0_OBJ) $(USER_BUILD_DIR)/$*.o

$(ISO_ROOT)/bin/%: $(USER_BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(call MKDIR_P,$(ISO_ROOT)/bin)
	$(call COPY_FILE,$<,$@)

$(KERNEL_ELF): $(BOOT_OBJ) $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(BOOT_OBJ) $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)

$(KERNEL_FB_ELF): $(BOOT_FB_OBJ) $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(BOOT_FB_OBJ) $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)

$(ISO_IMAGE): $(KERNEL_ELF) iso/boot/grub/grub.cfg $(ISO_USER_BIN) | $(BUILD_DIR)
	$(call MKDIR_P,$(ISO_ROOT)/boot/grub)
	$(call COPY_FILE,$(KERNEL_ELF),$(ISO_ROOT)/boot/slop-kernel.elf)
	$(call COPY_FILE,iso/boot/grub/grub.cfg,$(ISO_ROOT)/boot/grub/grub.cfg)
	$(GRUB_MKRESCUE) -o $@ $(ISO_ROOT)

$(ISO_FB_IMAGE): $(KERNEL_FB_ELF) iso/boot/grub/grub_fb.cfg $(ISO_USER_BIN) | $(BUILD_DIR)
	$(call MKDIR_P,$(ISO_ROOT)/boot/grub)
	$(call COPY_FILE,$(KERNEL_FB_ELF),$(ISO_ROOT)/boot/slop-kernel-fb.elf)
	$(call COPY_FILE,iso/boot/grub/grub_fb.cfg,$(ISO_ROOT)/boot/grub/grub.cfg)
	$(GRUB_MKRESCUE) -o $@ $(ISO_ROOT)

$(BOOT_BIN): boot/boot.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

$(LEGACY_KERNEL_BIN): kernel/kernel.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

$(FLOPPY_IMAGE): $(BOOT_BIN) $(LEGACY_KERNEL_BIN) | $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=512 count=2880
	dd if=$(BOOT_BIN) of=$@ conv=notrunc
	dd if=$(LEGACY_KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc

iso: $(ISO_IMAGE)
iso-fb: $(ISO_FB_IMAGE)
floppy: $(FLOPPY_IMAGE)

run: $(ISO_IMAGE)
	$(QEMU) $(QEMU_OPTS) $(QEMU_SERIAL_OPTS) -machine isapc -cpu 486 -m $(QEMU_RAM) -cdrom $(ISO_IMAGE) -boot d

run-lowmem: $(ISO_IMAGE)
	$(QEMU) $(QEMU_OPTS) $(QEMU_SERIAL_OPTS) -machine isapc -cpu 486 -m $(QEMU_LOW_RAM) -cdrom $(ISO_IMAGE) -boot d

run-fb: $(ISO_FB_IMAGE)
	$(QEMU) $(QEMU_OPTS) $(QEMU_SERIAL_OPTS) $(QEMU_VIDEO_OPTS) -machine isapc -cpu pentium2 -m 16M -cdrom $(ISO_FB_IMAGE) -boot d

run-floppy: $(FLOPPY_IMAGE)
	$(QEMU) $(QEMU_OPTS) -machine isapc -cpu 486 -m $(QEMU_RAM) -fda $(FLOPPY_IMAGE) -boot a

run-floppy-cd: $(FLOPPY_IMAGE) $(ISO_IMAGE)
	$(QEMU) $(QEMU_OPTS) -machine isapc -cpu 486 -m $(QEMU_RAM) -fda $(FLOPPY_IMAGE) -cdrom $(ISO_IMAGE) -boot a

clean:
	$(call RM_RF,$(BUILD_DIR))
