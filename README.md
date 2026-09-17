# XNU OS 학습용 커널

UEFI 환경에서 부팅하는 x86_64 교육용 운영체제 커널입니다. 현재 목표는 여러 실험 모듈을 나열하는 것이 아니라, **부팅부터 Ring 3 사용자 프로그램 실행까지의 검증 가능한 최소 경로**를 한 번에 실행하는 것입니다.

## 현재 동작 범위

`kernel_main.c`가 다음 순서를 단일 부팅 흐름으로 관리합니다.

```text
UEFI efi_main
  → kernel_main
  → 콘솔 초기화
  → PMM → VMM(CR3 전환) → GDT/TSS → IDT
  → VirtIO 블록 장치 → FAT32
  → USERAPP Mach-O 검증·적재
  → 사용자 스택 생성·주소 공간 격리 확인
  → Ring 3 USERAPP 진입
```

- UEFI GOP 프레임버퍼 기반 콘솔과 QEMU 디버그 포트(`0xE9`) 로그
- UEFI 메모리 맵 기반 페이지 단위 물리 메모리 관리자(PMM)
- 4단계 페이지 테이블 기반 가상 메모리 관리자(VMM)
- GDT/TSS 및 256개 엔트리 IDT 초기화
- 폴링 기반 VirtIO 블록 읽기와 읽기 전용 FAT32 파일 조회
- 범위 및 진입점 검사를 수행하는 제한된 정적 Mach-O 64 로더
- 사용자 이미지의 W^X 페이지 권한, 전용 PML4, Ring 3 진입

## 정리된 범위

미완성 상태였던 셸·PS/2 키보드·APIC 타이머 스케줄러·Mach IPC/Capability IPC·syscall·SPSC 스토리지 드라이버·initramfs 초안은 제거했습니다. 이 기능들은 서로 완결된 실행 계약이나 테스트가 없었고, 현재의 사용자 프로그램 실행 경로에도 필요하지 않았습니다.

새 기능을 다시 추가할 때는 `kernel_main.c`에 초기화 순서와 실패 처리를 연결하고, `Makefile`의 `C_SRCS` 또는 `ASM_SRCS`에 의도적으로 등록한 뒤 자동 테스트를 함께 추가합니다.

## 프로젝트 구조

```text
.
├── bootloader/
│   └── main.c              # UEFI 진입점, GOP·메모리 맵 수집, kernel_main 호출
├── kernel/
│   ├── kernel_main.c       # 전체 부팅 오케스트레이션과 사용자 프로그램 시작
│   ├── console.c, font.c   # 프레임버퍼 콘솔
│   ├── pmm.c, vmm.c        # 물리/가상 메모리 관리
│   ├── idt.c               # GDT/TSS·IDT와 기본 예외 처리
│   ├── virtio_blk.c        # VirtIO 블록 장치 읽기
│   ├── fat32.c             # FAT32 파일 조회
│   ├── macho.c             # USERAPP Mach-O 로더
│   ├── task.c              # 사용자 주소 공간을 소유하는 최소 Task
│   ├── userland.s          # Ring 3 iretq 전환
│   └── userapp.c           # 디스크 이미지에 담는 최소 사용자 프로그램
├── scripts/
│   └── verify_boot.sh      # 정상 부팅 자동 검사
└── Makefile                # 빌드, 실행, 테스트, 정리
```

## 요구 사항

Makefile은 macOS와 Homebrew 환경을 기준으로 합니다.

- LLVM (`clang`, `clang-tidy`)
- LLD (`lld-link`)
- QEMU (`qemu-system-x86_64`, OVMF 펌웨어)
- FAT 이미지 도구 (`mkfs.fat`, `mmd`, `mcopy`)

```bash
brew install llvm lld qemu mtools dosfstools
```

## 빌드·실행·테스트

저장소 루트에서 실행합니다.

```bash
# EFI 실행 파일과 USERAPP이 포함된 FAT32 디스크 이미지 생성
make

# 정적 분석만 실행
make lint

# QEMU 창에서 부팅
make run

# 화면 없이 부팅 로그를 터미널에 출력
make run-debug

# 정상 부팅 자동 검사
make test

# 생성물 제거
make clean
```

생성물은 `build/BOOTX64.EFI`와 `build/os_image.img`입니다.

## 확인 기준

정상 실행에서는 다음 핵심 로그가 순서대로 나타납니다.

```text
XNU OS KERNEL INIT...
[VIRTIO-BLK] INITIALIZED SUCCESSFULLY!
[FAT32] FS INITIALIZED!
[MACHO] ENTRY POINT LOADED: 0x100000320
[VMM] USER VSPACE ISOLATION VERIFIED
[KERNEL] JUMPING TO USERLAND AT: 0x100000320
```

`USERAPP`은 의도적으로 무한 루프를 실행합니다. 따라서 `make test`에서 QEMU의 10초 타임아웃은 실패가 아니라 사용자 코드 진입 성공 기준입니다.

## 현재 한계

이 프로젝트는 단일 사용자 프로그램을 적재·실행하는 최소 커널입니다. 프로세스 종료 후 자원 회수, 사용자 fault 복구, 다중 태스크 스케줄링, 키보드 입력, 시스템 콜 ABI, 읽기·쓰기 파일 시스템 및 드라이버 복구는 아직 제공하지 않습니다. 이후 기능을 추가할 때는 독립 모듈만 추가하지 말고, 부팅 경로·오류 처리·자동 검증까지 함께 확장해야 합니다.
