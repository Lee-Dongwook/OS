.global syscall_entry
.extern do_syscall

syscall_entry:
    # 1. RCX(리턴 RIP)와 R11(RFLAGS) 백업
    push %rcx
    push %r11

    # 2. C 함수(do_syscall)로 인자 전달
    # System V ABI: 1st arg = RDI, 2nd arg = RSI
    # 요청한 Syscall 번호(RAX)를 RDI로 복사
    mov %rdi, %rsi       # 유저 파라미터 -> 2번째 인자
    mov %rax, %rdi       # Syscall 번호 -> 1번째 인자

    call do_syscall

    # 3. RCX, R11 복원
    pop %r11
    pop %rcx

    # 4. 커널 모드 테스트 중이므로 sysretq 대신 ret 사용
    ret
