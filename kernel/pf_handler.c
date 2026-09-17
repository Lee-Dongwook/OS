#include "gdt.h"
#include <stdint.h>

typedef struct {
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rbp, rdi, rsi, rdx, rrc, rbx, rax;
  uint64_t error_code;
  uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) trap_frame_t;

extern void terminate_current_user_task(const char *reason,
                                        uint64_t fault_addr);

void do_page_fault_handler(trap_frame_t *frame) {
  uint64_t fault_address;
  __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_address));

  int is_user_fault = (frame->error_code & 0x04) != 0;

  if (is_user_fault || (frame->cs & 0x03) == 3) {
    // [Phase 1 요구사항] Ring 3에서 발생한 Fault는 해당 유저 Task만 격리/종료
    terminate_current_user_task("USERLAND_PAGE_FAULT_VIOLATION", fault_address);

    // 스케줄러에 의해 다음 프로세스로 컨텍스트 스위칭되며 이 라인으로는
    // 돌아오지 않음.
    return;
  }

  __asm__ __volatile__("cli");
  while (1) {
    __asm__ __volatile__("hlt");
  }
}
