.global enter_user_mode
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

.section .bss
.align 16
kernel_syscall_stack:
    .skip 8192
kernel_syscall_stack_top:
