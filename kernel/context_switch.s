.global timer_isr
.extern schedule
.extern apic_send_eoi

timer_isr:
    # 1. 모든 범용 레지스터 백업
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    # 2. APIC EOI 전송
    call apic_send_eoi

    # 3. 현재 스택 포인터(RSP)를 schedule() 함수 인자로 전달 (RDI)
    mov %rsp, %rdi
    call schedule

    # 4. schedule()이 반환한 새로운 스레드의 RSP 적용 (RAX)
    mov %rax, %rsp

    # 5. 레지스터 복원
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    # 6. 스레드로 복귀
    iretq
