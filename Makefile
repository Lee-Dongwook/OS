LLVM_PATH = $(shell brew --prefix llvm 2>/dev/null)/bin
LLD_PATH  = $(shell brew --prefix lld 2>/dev/null)/bin

CC = $(LLVM_PATH)/clang
LD = $(LLD_PATH)/lld-link

TARGET = x86_64-unknown-windows-coff
CFLAGS = -target x86_64-unknown-windows-gnu -ffreestanding -fno-stack-protector -mno-red-zone -mgeneral-regs-only -Wall

BUILD_DIR = build
BOOT_DIR = bootloader
KERNEL_DIR = kernel
EFI_IMAGE = $(BUILD_DIR)/BOOTX64.EFI
DISK_IMG = $(BUILD_DIR)/os_image.img

OVMF = $(shell find /opt/homebrew/Cellar/qemu /usr/local/Cellar/qemu -name "edk2-x86_64-code.fd" 2>/dev/null | head -n 1)

# userland.s에는 Ring 3 진입과 SYSRET 기반 시스템 콜 진입점이 함께 있다.
ASM_SRCS = $(KERNEL_DIR)/context_switch.s $(KERNEL_DIR)/keyboard_isr.s $(KERNEL_DIR)/userland.s
ASM_OBJS = $(ASM_SRCS:$(KERNEL_DIR)/%.s=$(BUILD_DIR)/%.o)

KERNEL_SRCS = $(KERNEL_DIR)/kernel_main.c $(KERNEL_DIR)/font.c $(KERNEL_DIR)/console.c $(KERNEL_DIR)/debug.c $(KERNEL_DIR)/keyboard.c $(KERNEL_DIR)/shell.c $(KERNEL_DIR)/initramfs.c $(KERNEL_DIR)/pmm.c $(KERNEL_DIR)/vmm.c $(KERNEL_DIR)/idt.c $(KERNEL_DIR)/mach_ipc.c $(KERNEL_DIR)/scheduler.c $(KERNEL_DIR)/apic.c $(KERNEL_DIR)/task.c $(KERNEL_DIR)/syscall.c $(KERNEL_DIR)/macho.c
KERNEL_OBJS = $(KERNEL_SRCS:$(KERNEL_DIR)/%.c=$(BUILD_DIR)/%.o) $(ASM_OBJS)

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.s
	clang --target=x86_64-unknown-windows-gnu -c $< -o $@

all: $(DISK_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/EFI/BOOT

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot_main.o: $(BOOT_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(EFI_IMAGE): $(BUILD_DIR)/boot_main.o $(KERNEL_OBJS)
	$(LD) -subsystem:efi_application -entry:efi_main $^ -out:$(EFI_IMAGE)

$(DISK_IMG): $(EFI_IMAGE)
	cp $(EFI_IMAGE) $(BUILD_DIR)/EFI/BOOT/BOOTX64.EFI
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	mkfs.fat -F 32 $(DISK_IMG)
	mmd -i $(DISK_IMG) ::/EFI
	mmd -i $(DISK_IMG) ::/EFI/BOOT
	mcopy -i $(DISK_IMG) $(EFI_IMAGE) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(DISK_IMG) assets/README.TXT ::/README.TXT

run: $(DISK_IMG)
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly=on,file="$(OVMF)" \
	                   -drive file=$(DISK_IMG),format=raw,if=none,id=bootdisk \
	                   -device virtio-blk-pci,drive=bootdisk \
	                   -net none

run-debug: $(DISK_IMG)
	qemu-system-x86_64 -display none -debugcon stdio -global isa-debugcon.iobase=0xe9 \
	                   -drive if=pflash,format=raw,readonly=on,file="$(OVMF)" \
	                   -drive file=$(DISK_IMG),format=raw,if=none,id=bootdisk \
	                   -device virtio-blk-pci,drive=bootdisk \
	                   -net none

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run run-debug clean
