# OS

UEFI 환경에서 부팅되는 x86_64 교육용 운영체제 커널 프로젝트입니다. 프레임버퍼 콘솔을 기반으로 물리/가상 메모리 관리, 인터럽트, 간단한 IPC, 태스크, 시스템 콜의 기본 구조를 구현합니다.

## 목표

- UEFI 부트로더에서 메모리 맵과 프레임버퍼 정보를 커널에 전달합니다.
- 베어메탈 환경에서 최소한의 커널 서브시스템을 초기화합니다.
- 운영체제의 핵심 구성 요소가 연결되는 흐름을 학습합니다.

## 현재 구현된 기능

- UEFI GOP(Graphics Output Protocol)를 이용한 프레임버퍼 획득
- UEFI `ExitBootServices` 후 베어메탈 커널 진입
- 프레임버퍼 기반 텍스트 콘솔 및 간단한 출력 함수
- UEFI 메모리 맵을 활용한 페이지 단위 물리 메모리 관리자(PMM)
- 4단계 페이지 테이블 기반 가상 메모리 관리자(VMM)
- GDT 및 256개 엔트리의 IDT 초기화, 기본 예외 처리
- 로컬 APIC 초기화 코드
- 원형 큐 기반 Mach 스타일 포트/메시지 IPC
- 로컬 APIC 타이머 인터럽트로 전환되는 라운드 로빈 커널 스레드 스케줄러
- 태스크별 페이지 테이블/포트 권한 구조와 포트 생성·삭제 API
- `IA32_LSTAR` MSR 기반 x86_64 시스템 콜 진입점 및 `SYS_MACH_MSG` 예제 호출

## 프로젝트 구조

```text
.
├── bootloader/
│   └── main.c              # UEFI 진입점, GOP·메모리 맵 처리, 커널 호출
├── kernel/
│   ├── kernel_main.c       # 커널 초기화 순서와 시스템 콜 예제
│   ├── console.c, font.c   # 프레임버퍼 콘솔
│   ├── pmm.c, vmm.c        # 물리/가상 메모리 관리
│   ├── idt.c, apic.c       # GDT/IDT와 로컬 APIC
│   ├── mach_ipc.c          # 포트 기반 메시지 IPC
│   ├── scheduler.c         # 기본 스레드 스케줄러
│   ├── task.c              # 태스크 및 포트 권한 관리
│   └── syscall.c           # 시스템 콜 초기화와 디스패치
├── Makefile                # 빌드, QEMU 실행, 정리 명령
└── build/                  # 생성물(EFI 실행 파일, 디스크 이미지 등, Git 추적 제외)
```

## 요구 사항

이 프로젝트의 `Makefile`은 macOS와 Homebrew 설치를 기준으로 작성되었습니다.

- LLVM (`clang`)
- LLD (`lld-link`)
- QEMU (`qemu-system-x86_64` 및 OVMF 펌웨어)
- FAT 이미지 도구 (`mkfs.fat`, `mmd`, `mcopy`)

예시 설치 명령:

```bash
brew install llvm lld qemu mtools dosfstools
```

## 빌드와 실행

저장소 루트에서 다음 명령을 실행합니다.

```bash
# EFI 실행 파일과 FAT32 디스크 이미지 생성
make

# QEMU에서 부팅
make run

# 생성물 제거
make clean
```

빌드가 완료되면 다음 파일이 생성됩니다.

- `build/BOOTX64.EFI`: UEFI 애플리케이션
- `build/os_image.img`: QEMU에 연결하는 FAT32 부팅 이미지

정상적으로 부팅되면 화면에 `XNU OS KERNEL INIT...`, IPC 송수신 자가 검증, 시스템 콜 호출/복귀 메시지가 출력됩니다. 이후 APIC 타이머가 두 개의 예제 커널 스레드를 번갈아 실행합니다.

## 초기화 흐름

```text
UEFI efi_main
  → GOP·메모리 맵 수집
  → ExitBootServices
  → kernel_main
  → 콘솔 → PMM → VMM → GDT/IDT → IPC → 스케줄러 → 태스크 → 시스템 콜
  → IPC 자가 검증 → 예제 스레드 생성 → APIC 타이머 → 인터럽트 허용
```

## 수동 확인 방법

1. `make clean && make` 실행 후 오류 없이 `build/os_image.img`가 생성되는지 확인합니다.
2. `make run`으로 QEMU를 실행합니다.
3. `[TEST] IPC SEND/RECEIVE SUCCESSFUL!` 및 `[TEST] SYSCALL RETURN SUCCESSFUL!` 메시지가 표시되는지 확인합니다.
4. 이후 `[THREAD A]`와 `[THREAD B]` 로그가 번갈아 표시되는지 확인합니다.
5. 실패하면 `make`의 컴파일/링커 출력과 QEMU 콘솔 메시지를 함께 확인합니다. 특히 Homebrew 패키지 경로와 OVMF 펌웨어 탐색 여부를 점검합니다.

## 현재 한계와 다음 단계

이 프로젝트는 학습용 최소 구현입니다. 사용자 모드 전환, 안전한 메모리 해제/보호, 실행 파일 로더, 파일 시스템 및 정식 시스템 콜 ABI는 아직 완성되지 않았습니다.

다음 단계로는 QEMU에서 타이머 기반 스레드 전환을 검증하고, 사용자 모드 프로세스와 페이지 권한을 도입하는 것을 권장합니다.
