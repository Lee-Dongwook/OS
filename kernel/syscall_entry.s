# kernel/syscall_entry.s
# Clang/LLVM 어셈블러 호환 x86_64 Syscall Entry

.global syscall_entry
.global _syscall_entry

syscall_entry:
_syscall_entry:
    # 1. User Stack(RSP) 백업 및 Task의 Kernel Stack으로 전환
    swapgs
    movq %rsp, %gs:0x08          # User RSP 백업 (%gs:0x08)
    movq %gs:0x00, %rsp          # Kernel RSP 로드 (%gs:0x00)

    # 2. Trap Frame 구축 (유저 문맥 백업)
    pushq %gs:0x08               # User RSP
    pushq %r11                   # RFLAGS (Syscall 호출 시 R11에 자동 저장됨)
    pushq %rcx                   # User RIP (Syscall 호출 시 RCX에 자동 저장됨)
    pushq %rax                   # Syscall Number
    pushq %rbx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # 3. System Call Dispatcher 호출 (x86_64 Syscall ABI: RDI, RSI, RDX, R10, R8, R9)
    # C 함수: unsigned long long do_syscall_dispatcher(sys_num, arg1, arg2, arg3, arg4, arg5)
    movq %rax, %rdi              # 1st arg: Syscall Number
    movq %rsi, %rsi              # 2nd arg: arg1 (유지)
    movq %rdx, %rdx              # 3rd arg: arg2 (유지)
    movq %r10, %rcx              # 4th arg: arg3 (Syscall 시 R10으로 넘어온 인자를 RCX로 이동)
    movq %r8,  %r8               # 5th arg: arg4 (유지)
    movq %r9,  %r9               # 6th arg: arg5 (유지)
    
    call do_syscall_dispatcher

    # 4. 유저 문맥 복구
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %r10                    # RSI 자리에 있던 백업값 복원
    popq %rdx
    popq %rbx
    popq %rax                    # 반환값은 RAX에 유지되도록 조정 시 주의
    popq %rcx                    # User RIP 복원
    popq %r11                    # RFLAGS 복원

    # 5. User Stack 복원 및 Ring 3 복귀
    movq %gs:0x08, %rsp
    swapgs

    # sysretq (Opcode: 48 0F 07)
    sysretq
