# LLVM 및 LLD 설치 경로 설정
LLVM_PATH = $(shell brew --prefix llvm 2>/dev/null)/bin
LLD_PATH  = $(shell brew --prefix lld 2>/dev/null)/bin

# 컴파일러 및 링커
CC = $(LLVM_PATH)/clang
LD = $(LLD_PATH)/lld-link

TARGET = x86_64-unknown-windows-coff
CFLAGS = -target $(TARGET) -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall

# 디렉터리 경로
BUILD_DIR = build
BOOT_DIR = bootloader
EFI_IMAGE = $(BUILD_DIR)/BOOTX64.EFI
DISK_IMG = $(BUILD_DIR)/os_image.img

# QEMU OVMF 펌웨어 경로
OVMF = $(shell find /opt/homebrew/Cellar/qemu /usr/local/Cellar/qemu -name "edk2-x86_64-code.fd" 2>/dev/null | head -n 1)

all: $(DISK_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/EFI/BOOT

# 1. UEFI 바이너리 빌드 (PE/COFF 형식)
$(EFI_IMAGE): $(BOOT_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(BOOT_DIR)/main.c -o $(BUILD_DIR)/main.o
	$(LD) -subsystem:efi_application -entry:efi_main $(BUILD_DIR)/main.o -out:$(EFI_IMAGE)

# 2. QEMU용 FAT 디스크 이미지 생성
$(DISK_IMG): $(EFI_IMAGE)
	cp $(EFI_IMAGE) $(BUILD_DIR)/EFI/BOOT/BOOTX64.EFI
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	mkfs.fat -F 32 $(DISK_IMG)
	mmd -i $(DISK_IMG) ::/EFI
	mmd -i $(DISK_IMG) ::/EFI/BOOT
	mcopy -i $(DISK_IMG) $(EFI_IMAGE) ::/EFI/BOOT/BOOTX64.EFI

# 3. QEMU 에뮬레이터 실행
run: $(DISK_IMG)
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly=on,file="$(OVMF)" \
	                   -drive file=$(DISK_IMG),format=raw,if=none,id=bootdisk \
	                   -device virtio-blk-pci,drive=bootdisk \
	                   -net none

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
