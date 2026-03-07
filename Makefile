NASM ?= nasm
CC ?= i686-elf-gcc
LD ?= i686-elf-ld
GRUB_MKRESCUE ?= grub-mkrescue
QEMU ?= qemu-system-i386

BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/isofiles
OBJ_DIR := $(BUILD_DIR)/obj

KERNEL_ELF := $(BUILD_DIR)/slop-kernel.elf
ISO_IMAGE := $(BUILD_DIR)/slop.iso

CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32 -O2 -Wall -Wextra
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

KERNEL_ASM_OBJ := $(OBJ_DIR)/boot32.o
KERNEL_C_OBJ := $(OBJ_DIR)/kmain.o

.PHONY: all iso run clean

all: iso

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

$(KERNEL_ASM_OBJ): kernel/boot32.asm | $(OBJ_DIR)
	$(NASM) -f elf32 -o $@ $<

$(KERNEL_C_OBJ): kernel/kmain.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KERNEL_ELF): $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)

$(ISO_IMAGE): $(KERNEL_ELF) iso/boot/grub/grub.cfg | $(BUILD_DIR)
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot/grub
	cp $(KERNEL_ELF) $(ISO_ROOT)/boot/slop-kernel.elf
	cp iso/boot/grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_ROOT)

iso: $(ISO_IMAGE)

run: $(ISO_IMAGE)
	$(QEMU) \
		-machine isapc \
		-cpu 486 \
		-m 2M \
		-cdrom $(ISO_IMAGE) \
		-boot d

clean:
	rm -rf $(BUILD_DIR)
