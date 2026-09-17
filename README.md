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
- QEMU legacy VirtIO 블록 장치의 폴링 기반 읽기
- 읽기 전용 FAT32 파일 조회와 FAT cluster chain 읽기
- 범위 검사와 `LC_UNIXTHREAD` 진입점 검사를 수행하는 제한된 정적 Mach-O 64 로더
- Mach-O 텍스트 페이지를 로드 후 쓰기 금지로 전환하는 기본 W^X 경로
- GDT/TSS와 Task 전용 PML4를 이용한 최소 Ring 3 진입
- QEMU 디버그 포트(`0xE9`) 기반 헤드리스 부팅 로그와 자동 기준선 검사

## 소스에 있으나 현재 부팅 경로에서 미연결인 실험 모듈

`mach_ipc.c`, `task.c`, `scheduler.c`, `syscall.c`, `keyboard.c`, `shell.c`, `initramfs.c`에는 학습용 초안이 남아 있습니다. 현재 `kernel_main`은 이들을 초기화하거나 완료 기능으로 검증하지 않으므로, IPC·스케줄링·시스템 콜·대화형 셸을 사용할 수 있다고 간주하면 안 됩니다. 이 구분은 `plan/roadmap.md`의 Phase 1/2 작업에서 다시 연결할 때 검증 가능한 계약으로 바꾸기 위한 것입니다.

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

# clang-tidy 정적 분석만 실행
make lint

# QEMU에서 부팅
make run

# 화면 없이 부팅 로그를 터미널에서 확인
make run-debug

# QEMU 헤드리스 부팅 기준선 자동 검사
make test

# Ring 3 보호 페이지 위반과 커널 복구 경로 검사
make test-user-fault

# 생성물 제거
make clean
```

빌드가 완료되면 다음 파일이 생성됩니다.

- `build/BOOTX64.EFI`: UEFI 애플리케이션
- `build/os_image.img`: QEMU에 연결하는 FAT32 부팅 이미지

정상적으로 부팅되면 화면에 `XNU OS KERNEL INIT...`, VirtIO 블록 장치/FAT32 초기화, Mach-O 진입점 로드 메시지가 출력됩니다. 현재 USERAPP은 Ring 3에서 무한 루프를 실행하는 최소 전환 검증 프로그램입니다. 대화형 셸은 아직 현재 부팅 경로에 연결되어 있지 않습니다.

## VS Code 저장 시 린팅·포맷

VS Code에서 **clangd** 확장(`llvm-vs-code-extensions.vscode-clangd`)을 설치한 뒤 이 저장소를 다시 열면 설정이 자동 적용됩니다.

- C 파일을 저장하면 `clang-format`이 `.clang-format` 규칙으로 코드를 포맷합니다.
- 편집 중 `clangd`가 `.clang-tidy` 규칙으로 진단을 표시합니다. 저장할 필요 없이 즉시 갱신되며, 경고 위치에서 Quick Fix를 선택해 안전한 수정안을 적용할 수 있습니다.
- `compile_flags.txt`는 clangd가 커널·부트로더를 실제 빌드와 같은 freestanding 타깃으로 해석하게 합니다.

## 자동 린팅

`make`로 C 소스 오브젝트를 빌드하기 전에 `clang-tidy`가 자동 실행됩니다. 규칙은 저장소 루트의 `.clang-tidy`에서 관리하며, 별도로 확인하려면 다음 명령을 사용합니다.

```bash
make lint
```

현재 린트는 경고를 출력하지만 경고만으로 빌드를 중단하지 않습니다. CI에서 경고를 실패로 처리하려면 `TIDY_FLAGS`에 `--warnings-as-errors='*'`를 추가하면 됩니다.

| 명령 | 설명 |
| --- | --- |
| `help` | 사용 가능한 명령 출력 |
| `clear` | 콘솔 화면 지우기 |
| `mem` | 전체/가용 물리 메모리 출력 |
| `tasks` | 활성 태스크와 커널 스레드 수 출력 |
| `ports` | 활성 IPC 포트 수 출력 |
| `spawn` | 태스크와 IPC 포트 한 개 생성 |
| `ls` | initramfs의 파일 목록 출력 |
| `cat README.TXT` | 내장 initramfs 파일 내용 출력 (대소문자 무관) |
| `cat BOOT.TXT` | FAT32 부팅 이미지에서 UEFI가 읽어 전달한 파일 내용 출력 |

위 명령은 미연결 실험 셸이 다시 부팅 경로에 연결된 뒤의 인터페이스 초안입니다. 현재 빌드에서는 사용할 수 없습니다.

## 초기화 흐름

```text
UEFI efi_main
  → GOP·메모리 맵 수집
  → ExitBootServices
  → kernel_main
  → 콘솔 → PMM → VMM → GDT/TSS → IDT → VirtIO 블록 → FAT32
  → Mach-O 검증·적재 → Ring 3 USERAPP 진입
```

## 수동 확인 방법

1. `make clean && make` 실행 후 오류 없이 `build/os_image.img`가 생성되는지 확인합니다.
2. `make run`으로 QEMU를 실행합니다.
3. `[VIRTIO-BLK] INITIALIZED SUCCESSFULLY!`, `[FAT32] FS INITIALIZED!`, `[MACHO] ENTRY POINT LOADED:` 메시지가 표시되는지 확인합니다.
4. GUI 없이 이 과정을 자동 판정하려면 `make test`를 실행합니다. USERAPP이 계속 실행되므로 5초 타임아웃은 성공 조건입니다.
5. 실패하면 `make`의 컴파일/링커 출력과 `make run-debug`의 QEMU 로그를 함께 확인합니다. 특히 Homebrew 패키지 경로와 OVMF 펌웨어 탐색 여부를 점검합니다.

## 현재 한계와 다음 단계

이 프로젝트는 학습용 최소 구현입니다. 현재 Ring 3 진입은 가능하지만 Task별 주소 공간·사용자 예외 격리·안전한 syscall 복귀는 아직 완성되지 않았습니다. 커널에는 legacy VirtIO 블록 읽기와 제한된 읽기 전용 FAT32·Mach-O 로더가 있으나, 드라이버 timeout/복구·FAT의 전체 디렉터리 처리·실행 이미지의 자원 회수는 후속 작업입니다. 페이지 권한 보호와 정식 사용자 공간 시스템 콜 ABI도 후속 작업입니다.

다음 단계로는 VirtIO 블록 드라이버와 FAT 읽기 계층을 추가하고, TSS/GDT 사용자 세그먼트와 페이지 권한을 바탕으로 Ring 3 프로세스를 도입하는 것을 권장합니다.
