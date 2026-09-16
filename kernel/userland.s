.global enter_user_mode
.global syscall_entry
.extern do_syscall

enter_user_mode:
    # Windows x64 ABI (이 커널의 C 컴파일 타깃):
    # RCX: User RIP (Entry Point), RDX: User RSP (User Stack)

    # CS=0x23 (User Code), SS=0x1B (User Data)
    mov $0x1B, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # iretq 스택 프레임 수동 구축
    pushq $0x1B        # SS (User Data Selector)
    pushq %rdx         # RSP (User Stack Pointer)
    pushq $0x202       # RFLAGS (IF=1 enabled)
    pushq $0x23        # CS (User Code Selector)
    pushq %rcx         # RIP (User Entry Point)

    iretq              # Ring 3로 전환

syscall_entry:
    # syscall 실행 시 RCX=RIP, R11=RFLAGS가 자동 저장됨
    # RSP 스위칭: 커널 스택으로 전환
    mov %rsp, %r10     # 유저 RSP 임시 보관
    lea kernel_syscall_stack_top(%rip), %rsp

    push %r10          # 유저 RSP 저장
    push %r11          # RFLAGS 저장
    push %rcx          # 유저 RIP 저장

    # Windows x64 ABI: RCX=syscall ID, RDX=첫 번째 인자, 32바이트 shadow space.
    mov %rax, %rcx
    mov %rsi, %rdx
    sub $0x28, %rsp    # shadow space + call 전 16바이트 정렬
    call do_syscall
    add $0x28, %rsp

    pop %rcx           # 유저 RIP 복원
    pop %r11           # RFLAGS 복원
    pop %rsp           # 유저 RSP 복원

    sysretq

.section .bss
.align 16
kernel_syscall_stack:
    .skip 8192
kernel_syscall_stack_top:
