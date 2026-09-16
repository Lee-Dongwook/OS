# ==========================================
# 툴체인 및 플래그 설정
# ==========================================
LLVM_PATH = $(shell brew --prefix llvm 2>/dev/null)/bin
LLD_PATH  = $(shell brew --prefix lld 2>/dev/null)/bin
CC = $(LLVM_PATH)/clang
LD = $(LLD_PATH)/lld-link
TARGET    = x86_64-unknown-windows-coff
CFLAGS    = -target $(TARGET) -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -I.

# ==========================================
# 디렉터리 및 자동 파일 스캔 (wildcard)
# ==========================================
BUILD_DIR = build
BOOT_DIR = bootloader
KERNEL_DIR = kernel
EFI_IMAGE = $(BUILD_DIR)/BOOTX64.EFI
DISK_IMG = $(BUILD_DIR)/os_image.img

# kernel 디렉터리 내의 모든 .c 및 .s 파일 자동 검색
C_SRCS     = $(wildcard $(KERNEL_DIR)/*.c)
ASM_SRCS   = $(wildcard $(KERNEL_DIR)/*.s)

# .c 및 .s 파일 목록을 기반으로 build/*.o 목록 자동 생성
KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS)) \
              $(patsubst $(KERNEL_DIR)/%.s, $(BUILD_DIR)/%.o, $(ASM_SRCS))

BOOT_OBJ   = $(BUILD_DIR)/boot_main.o

OVMF = $(shell find /opt/homebrew/Cellar/qemu /usr/local/Cellar/qemu -name "edk2-x86_64-code.fd" 2>/dev/null | head -n 1)

# ==========================================
# 빌드 타겟 규칙
# ==========================================
all: $(DISK_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/EFI/BOOT

# 부트로더 컴파일
$(BOOT_OBJ): $(BOOT_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# C 파일 자동 패턴 컴파일
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly(.s) 파일 자동 패턴 컴파일
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.s | $(BUILD_DIR)
	$(CC) --target=x86_64-unknown-windows-gnu -c $< -o $@

# UEFI 애플리케이션 링킹 (스캔된 모든 .o 자동 포함)
$(EFI_IMAGE): $(BOOT_OBJ) $(KERNEL_OBJS)
	$(LD) -subsystem:efi_application -entry:efi_main $^ -out:$(EFI_IMAGE)

# QEMU FAT 디스크 이미지 생성
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
