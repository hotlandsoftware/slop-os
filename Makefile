NASM ?= nasm
CC ?= i686-elf-gcc
LD ?= i686-elf-ld
GRUB_MKRESCUE ?= grub-mkrescue
QEMU ?= qemu-system-i386
QEMU_OPTS ?= -accel tcg,thread=single -icount auto,sleep=on

BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/isofiles
OBJ_DIR := $(BUILD_DIR)/obj

KERNEL_ELF := $(BUILD_DIR)/slop-kernel.elf
ISO_IMAGE := $(BUILD_DIR)/slop.iso

CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32 -march=i486 -mtune=i486 -O2 -Wall -Wextra
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

KERNEL_ASM_SRCS := kernel/boot32.asm kernel/irq_stubs.asm
KERNEL_ASM_OBJ := $(patsubst kernel/%.asm,$(OBJ_DIR)/%.o,$(KERNEL_ASM_SRCS))
KERNEL_C_SRCS := $(wildcard kernel/*.c)
KERNEL_C_OBJ := $(patsubst kernel/%.c,$(OBJ_DIR)/%.o,$(KERNEL_C_SRCS))

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

.PHONY: all iso run clean

all: iso

$(BUILD_DIR) $(OBJ_DIR):
	$(call MKDIR_P,$@)

$(OBJ_DIR)/%.o: kernel/%.asm | $(OBJ_DIR)
	$(NASM) -f elf32 -o $@ $<

$(OBJ_DIR)/%.o: kernel/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KERNEL_ELF): $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)

$(ISO_IMAGE): $(KERNEL_ELF) iso/boot/grub/grub.cfg | $(BUILD_DIR)
	$(call RM_RF,$(ISO_ROOT))
	$(call MKDIR_P,$(ISO_ROOT)/boot/grub)
	$(call COPY_FILE,$(KERNEL_ELF),$(ISO_ROOT)/boot/slop-kernel.elf)
	$(call COPY_FILE,iso/boot/grub/grub.cfg,$(ISO_ROOT)/boot/grub/grub.cfg)
	$(GRUB_MKRESCUE) -o $@ $(ISO_ROOT)

iso: $(ISO_IMAGE)

run: $(ISO_IMAGE)
	$(QEMU) $(QEMU_OPTS) -machine isapc -cpu 486 -m 2M -cdrom $(ISO_IMAGE) -boot d

clean:
	$(call RM_RF,$(BUILD_DIR))
