# ==========================================
# 툴체인 및 플래그 설정
# ==========================================
LLVM_PATH = $(shell brew --prefix llvm 2>/dev/null)/bin
CC        = $(LLVM_PATH)/clang
LD        = $(shell brew --prefix lld 2>/dev/null)/bin/lld-link

TARGET    = x86_64-unknown-windows-coff
CFLAGS    = -target $(TARGET) -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -I.

# ==========================================
# 디렉터리 및 자동 파일 스캔
# ==========================================
BUILD_DIR  = build
BOOT_DIR   = bootloader
KERNEL_DIR = kernel

EFI_IMAGE  = $(BUILD_DIR)/BOOTX64.EFI
DISK_IMG   = $(BUILD_DIR)/os_image.img
USERAPP    = $(BUILD_DIR)/USERAPP

# userapp.c를 커널 자동 스캔 대상에서 명확히 제외
C_SRCS     = $(filter-out $(KERNEL_DIR)/userapp.c, $(wildcard $(KERNEL_DIR)/*.c))
ASM_SRCS   = $(wildcard $(KERNEL_DIR)/*.s)

KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS)) \
              $(patsubst $(KERNEL_DIR)/%.s, $(BUILD_DIR)/%.o, $(ASM_SRCS))

BOOT_OBJ   = $(BUILD_DIR)/boot_main.o

OVMF       = $(shell find /opt/homebrew/Cellar/qemu /usr/local/Cellar/qemu -name "edk2-x86_64-code.fd" 2>/dev/null | head -n 1)

# ==========================================
# 빌드 타겟 규칙
# ==========================================
all: $(DISK_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/EFI/BOOT

# 1. Mach-O 유저 애플리케이션 빌드
$(USERAPP): $(KERNEL_DIR)/userapp.c | $(BUILD_DIR)
	$(CC) -target x86_64-apple-macos -nostdlib -Wl,-static -Wl,-e,__start -o $@ $<

# 2. 커널 및 부트로더 오브젝트 파일 빌드
$(BOOT_OBJ): $(BOOT_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.s | $(BUILD_DIR)
	$(CC) --target=x86_64-unknown-windows-gnu -c $< -o $@

# 3. UEFI 바이너리 링킹
$(EFI_IMAGE): $(BOOT_OBJ) $(KERNEL_OBJS)
	$(LD) -subsystem:efi_application -entry:efi_main $^ -out:$(EFI_IMAGE)

# 4. FAT32 디스크 이미지 생성 (USERAPP 탑재)
$(DISK_IMG): $(EFI_IMAGE) $(USERAPP)
	cp $(EFI_IMAGE) $(BUILD_DIR)/EFI/BOOT/BOOTX64.EFI
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	mkfs.fat -F 32 $(DISK_IMG)
	mmd -i $(DISK_IMG) ::/EFI
	mmd -i $(DISK_IMG) ::/EFI/BOOT
	mcopy -i $(DISK_IMG) $(EFI_IMAGE) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(DISK_IMG) $(USERAPP) ::/USERAPP

run: $(DISK_IMG)
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly=on,file="$(OVMF)" \
	                   -drive file=$(DISK_IMG),format=raw,if=none,id=bootdisk \
	                   -device virtio-blk-pci,drive=bootdisk \
	                   -net none

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
