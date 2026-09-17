# ==========================================
# 툴체인 및 플래그 설정
# ==========================================
LLVM_PATH = $(shell brew --prefix llvm 2>/dev/null)/bin
CC        = $(LLVM_PATH)/clang
LD        = $(shell brew --prefix lld 2>/dev/null)/bin/lld-link

TARGET    = x86_64-unknown-windows-coff
CFLAGS    = -target $(TARGET) -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -I.
CLANG_TIDY ?= $(LLVM_PATH)/clang-tidy
TIDY_FLAGS = --config-file=.clang-tidy --quiet

# ==========================================
# 디렉터리 및 자동 파일 스캔
# ==========================================
BUILD_DIR  = build
BOOT_DIR   = bootloader
KERNEL_DIR = kernel

EFI_IMAGE  = $(BUILD_DIR)/BOOTX64.EFI
DISK_IMG   = $(BUILD_DIR)/os_image.img
USERAPP    = $(BUILD_DIR)/USERAPP
USERFAULT  = $(BUILD_DIR)/USERFAULT

# userapp.c를 커널 자동 스캔 대상에서 명확히 제외
C_SRCS     = $(filter-out $(KERNEL_DIR)/userapp.c, $(wildcard $(KERNEL_DIR)/*.c))
ASM_SRCS   = $(wildcard $(KERNEL_DIR)/*.s)

KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS)) \
              $(patsubst $(KERNEL_DIR)/%.s, $(BUILD_DIR)/%.o, $(ASM_SRCS))

BOOT_OBJ   = $(BUILD_DIR)/boot_main.o

# 빌드와 같은 타깃/컴파일 플래그로 정적 분석한다.
LINT_SRCS  = $(BOOT_DIR)/main.c $(C_SRCS)

OVMF       = $(shell find /opt/homebrew/Cellar/qemu /usr/local/Cellar/qemu -name "edk2-x86_64-code.fd" 2>/dev/null | head -n 1)
QEMU       ?= qemu-system-x86_64

# ==========================================
# 빌드 타겟 규칙
# ==========================================
all: $(DISK_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/EFI/BOOT

# 1. Mach-O 유저 애플리케이션 빌드
$(USERAPP): $(KERNEL_DIR)/userapp.c | $(BUILD_DIR)
	$(CC) -target x86_64-apple-macos -nostdlib -Wl,-static -Wl,-e,__start -o $@ $<

$(USERFAULT): $(KERNEL_DIR)/userapp.c | $(BUILD_DIR)
	$(CC) -target x86_64-apple-macos -nostdlib -DUSERAPP_FAULT_TEST -Wl,-static -Wl,-e,__start -o $@ $<

# 2. 커널 및 부트로더 오브젝트 파일 빌드
$(BOOT_OBJ): $(BOOT_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.s | $(BUILD_DIR)
	$(CC) --target=x86_64-unknown-windows-gnu -c $< -o $@

# `make` 및 개별 C 오브젝트 빌드 전에 자동으로 실행된다.
$(BOOT_OBJ) $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS)) $(USERAPP): lint

lint:
	$(CLANG_TIDY) $(TIDY_FLAGS) $(LINT_SRCS) -- $(CFLAGS)
	$(CLANG_TIDY) $(TIDY_FLAGS) $(KERNEL_DIR)/userapp.c -- -target x86_64-apple-macos -nostdlib -I.

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
	$(QEMU) -drive if=pflash,format=raw,readonly=on,file="$(OVMF)" \
	                   -drive file=$(DISK_IMG),format=raw,if=none,id=bootdisk \
	                   -device virtio-blk-pci,drive=bootdisk \
	                   -net none

# 프레임버퍼 창 없이 QEMU 디버그 포트(0xE9) 로그를 표준 출력으로 표시한다.
run-debug: $(DISK_IMG)
	$(QEMU) -display none -debugcon stdio -global isa-debugcon.iobase=0xe9 \
	        -drive if=pflash,format=raw,readonly=on,file="$(OVMF)" \
	        -drive file=$(DISK_IMG),format=raw,if=none,id=bootdisk \
	        -device virtio-blk-pci,drive=bootdisk -net none -no-reboot

# Phase 0 기준선: 빌드 결과가 QEMU에서 사용자 코드 진입 직전까지 도달하고,
# 예상치 못한 재부팅/예외 없이 지정 시간 동안 유지되는지 검사한다.
test-boot: $(DISK_IMG)
	./scripts/verify_boot.sh "$(OVMF)" "$(DISK_IMG)" "$(QEMU)"

test: test-boot

# 정상 USERAPP을 만든 뒤 fault 전용 이미지를 FAT 디스크에 교체한다.
# 이 검사는 사용자 보호 페이지 위반이 커널 복구 경로로 끝나는지 확인한다.
test-user-fault: $(DISK_IMG) $(USERFAULT)
	mcopy -o -i $(DISK_IMG) $(USERFAULT) ::/USERAPP
	./scripts/verify_user_fault.sh "$(OVMF)" "$(DISK_IMG)" "$(QEMU)"

help:
	@echo "Targets: all lint run run-debug test-boot test test-user-fault clean"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run run-debug test-boot test test-user-fault clean lint help
