LLVM_PATH = $(shell brew --prefix llvm 2>/dev/null)/bin
LLD_PATH  = $(shell brew --prefix lld 2>/dev/null)/bin

CC = $(LLVM_PATH)/clang
LD = $(LLD_PATH)/lld-link

TARGET = x86_64-unknown-windows-coff
CFLAGS = -target $(TARGET) -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -I.

BUILD_DIR = build
BOOT_DIR = bootloader
KERNEL_DIR = kernel
EFI_IMAGE = $(BUILD_DIR)/BOOTX64.EFI
DISK_IMG = $(BUILD_DIR)/os_image.img

OVMF = $(shell find /opt/homebrew/Cellar/qemu /usr/local/Cellar/qemu -name "edk2-x86_64-code.fd" 2>/dev/null | head -n 1)

all: $(DISK_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/EFI/BOOT

$(EFI_IMAGE): $(BOOT_DIR)/main.c $(KERNEL_DIR)/kernel_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(BOOT_DIR)/main.c -o $(BUILD_DIR)/boot_main.o
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/kernel_main.c -o $(BUILD_DIR)/kernel_main.o
	$(LD) -subsystem:efi_application -entry:efi_main $(BUILD_DIR)/boot_main.o $(BUILD_DIR)/kernel_main.o -out:$(EFI_IMAGE)

$(DISK_IMG): $(EFI_IMAGE)
	cp $(EFI_IMAGE) $(BUILD_DIR)/EFI/BOOT/BOOTX64.EFI
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	mkfs.fat -F 32 $(DISK_IMG)
	mmd -i $(DISK_IMG) ::/EFI
	mmd -i $(DISK_IMG) ::/EFI/BOOT
	mcopy -i $(DISK_IMG) $(EFI_IMAGE) ::/EFI/BOOT/BOOTX64.EFI

run: $(DISK_IMG)
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly=on,file="$(OVMF)" \
	                   -drive file=$(DISK_IMG),format=raw,if=none,id=bootdisk \
	                   -device virtio-blk-pci,drive=bootdisk \
	                   -net none

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
