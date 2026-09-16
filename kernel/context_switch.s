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

    # 2. APIC EOI 전송. 이 프로젝트의 C 코드는 Windows x64 ABI를 사용하므로
    # 호출자 shadow space(32바이트)와 정렬 여유를 확보한다.
    sub $0x28, %rsp
    call apic_send_eoi
    add $0x28, %rsp

    # 3. 현재 스택 포인터(RSP)를 schedule() 첫 번째 인자(RCX)로 전달
    mov %rsp, %rcx
    sub $0x28, %rsp
    call schedule
    add $0x28, %rsp

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
