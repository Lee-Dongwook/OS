# Phase 0 기준선과 현재 계약

이 문서는 `plan/roadmap.md`의 Phase 0 완료 증거를 현재 C 기반 커널에 맞춰 고정한다. 이는 최종 마이크로커널 설계가 완성되었다는 선언이 아니다.

## 재현 가능한 기준선

필수 도구는 README의 LLVM, LLD, QEMU, mtools, dosfstools이며, macOS/Homebrew 경로를 Makefile이 탐색한다.

```bash
make clean && make
make test
```

`make test`는 5초 동안 헤드리스 QEMU를 실행한다. 내장 USERAPP은 의도적으로 무한 루프를 실행하므로 `timeout`의 종료 코드 124가 정상이다. 다음 로그를 모두 확인하고, Mach-O/FAT32 오류나 예외 로그가 있으면 실패한다.

- VirtIO 블록 장치 초기화
- FAT32 초기화
- Mach-O `LC_UNIXTHREAD` 진입점 로드
- 사용자 매핑이 커널 전역 PML4에 공유되지 않았다는 VSpace 검사
- Ring 3 전환 직전의 커널 로그

`make test-user-fault`는 별도 USERAPP을 탑재해 supervisor 전용 하위 identity mapping에 쓰기를 시도한다. #PF 뒤 `[KERNEL] USER FAULT RECOVERED TO KERNEL` 로그가 나와야 하며, QEMU 종료나 double fault는 실패다.

## 부팅 handoff 계약

부트로더는 `BootInfo`를 통해 프레임버퍼, UEFI 메모리 맵과 descriptor 크기, 부팅 전에 읽은 `README.TXT` 데이터를 전달한다. `ExitBootServices`가 실패하면 메모리 맵과 key를 다시 받아 한 번 재시도한다.

현재 계약의 제한은 다음과 같다.

- handoff 구조체에는 아직 버전·길이·무결성 필드가 없다.
- 메모리 맵 버퍼와 부팅 파일 버퍼의 소유권은 부트로더 정적 저장소에 남아 있으며, 커널이 이를 해제하지 않는다.
- UEFI 서비스는 `ExitBootServices` 뒤 호출하지 않아야 한다.

## 현재 객체와 오류 계약

| 구성 요소 | 현재 소유자 | 실패 결과 | 다음 단계 |
| --- | --- | --- | --- |
| 물리 페이지 | PMM bitmap | `pmm_alloc_page()`가 0 반환 | 예약 영역 정확화, quota |
| 페이지 매핑 | VMM | 현재 API는 매핑 실패를 반환하지 않음 | Task별 VSpace·U/S·NX 계약 |
| FAT32 읽기 | `fat32.c` | -1 반환, 읽기 전용 | FAT chain·디렉터리 범위 확장 및 사용자 공간 이관 |
| Mach-O 이미지 | `macho.c` | 검증 실패 시 0 반환, Ring 3 진입 금지 | Task별 주소 공간·페이지 회수 |
| VirtIO 요청 | `virtio_blk.c` | -1 반환 또는 현재 폴링 대기 | timeout·취소·driver recovery |

## 구현 범위와 미구현 범위

현재 기준선은 UEFI 부팅, PMM/VMM, GDT/TSS/IDT, legacy VirtIO 블록 읽기, 읽기 전용 FAT32 파일 조회, 제한된 Mach-O 정적 이미지 로드 및 Task 전용 PML4에서의 Ring 3 진입을 검증한다.

아직 syscall의 사용자 안전 복귀, 사용자 예외 격리, Task별 주소 공간, capability 세대/철회, IPC의 사용자 ABI, DMA 격리, 드라이버 복구는 완료되지 않았다. 따라서 이 기준선은 Phase 1·2의 보안 완료 증거로 사용하지 않는다.
