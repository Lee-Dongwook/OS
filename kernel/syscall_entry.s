.global syscall_entry
.extern do_syscall

syscall_entry:
    # 1. RCX(리턴 RIP)와 R11(RFLAGS) 백업
    push %rcx
    push %r11

    # 2. C 함수(do_syscall)로 인자 전달
    # Windows x64 ABI: 1st arg = RCX, 2nd arg = RDX, shadow space = 32 bytes
    mov %rdi, %rdx       # 유저 파라미터 -> 2번째 인자
    mov %rax, %rcx       # Syscall 번호 -> 1번째 인자

    sub $0x28, %rsp
    call do_syscall
    add $0x28, %rsp

    # 3. RCX, R11 복원
    pop %r11
    pop %rcx

    # 4. 커널 모드 테스트 중이므로 sysretq 대신 ret 사용
    ret
