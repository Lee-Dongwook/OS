#include "task.h"
#include "pmm.h"
#include <stdint.h>

extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

static task_t task_table[MAX_TASKS];
static int next_task_id = 1;
static task_t *current_running_task = 0;

void task_init(void) {
  for (int i = 0; i < MAX_TASKS; i++) {
    task_table[i].task_id = 0;
    task_table[i].is_active = 0;
    task_table[i].pml4 = 0;
    task_table[i].port_count = 0;
  }
  kputs("[TASK] MACH TASK SUBSYSTEM INITIALIZED\n", 0x00FFFF00);
}

// 새로운 Mach Task 생성
task_t *task_create(void) {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (!task_table[i].is_active) {
      task_t *t = &task_table[i];
      t->task_id = next_task_id++;
      t->is_active = 1;
      cspace_init(&t->cspace);

      // Task 전용 PML4 할당
      t->pml4 = (page_table_t *)pmm_alloc_page();
      if (t->pml4) {
        for (int j = 0; j < 512; j++) {
          t->pml4->entries[j] = 0;
        }
      }

      // 최초 생성된 Task를 current_running_task 기본값으로 설정
      if (!current_running_task) {
        current_running_task = t;
      }

      kputs("[TASK] CREATED TASK ID: ", 0x00FFFF00);
      kput_dec(t->task_id, 0x00FFFF00);
      kputs("\n", 0x00FFFF00);

      return t;
    }
  }
  return 0;
}

// Task에 Mach Port 권한 추가
int task_add_port(task_t *task, unsigned int port_id) {
  if (!task || !task->is_active || port_id == 0 ||
      task->port_count >= MAX_TASK_PORTS)
    return -1;
  task->ports[task->port_count++] = port_id;
  return 0;
}

int task_destroy(task_t *task) {
  if (!task || !task->is_active) {
    return -1;
  }
  if (task->pml4) {
    pmm_free_page(task->pml4);
  }
  task->task_id = 0;
  task->is_active = 0;
  task->pml4 = 0;
  task->port_count = 0;
  return 0;
}

int task_active_count(void) {
  int count = 0;
  for (int i = 0; i < MAX_TASKS; i++) {
    if (task_table[i].is_active) {
      count++;
    }
  }
  return count;
}

void terminate_current_user_task(const char *reason, uint64_t fault_addr) {
  kputs("\n[USERLAND FAULT ISOLATION] ", 0x00FF0000);
  kputs(reason, 0x00FF0000);
  kputs(" AT ADDR: 0x", 0x00FF0000);
  kput_hex(fault_addr, 0x00FF0000);
  kputs("\n[TASK] TERMINATING CURRENT USER TASK & RESCHEDULING...\n",
        0x00FFFF00);

  // TODO: Phase 1/2 스케줄러 연동 시 current_task->state = TERMINATED 처리
  // 현재는 커널 안전 복귀를 검증하기 위해 무한 루프 또는 다음 타스크 스위칭
  // 수행
  while (1) {
    __asm__ __volatile__("hlt");
  }
}

task_t *get_current_task(void) {
  // 스케줄러 전환 전 단일 Task 동작 시 첫 번째 활성 Task를 반환하도록 세이프티
  // 처리
  if (!current_running_task) {
    for (int i = 0; i < MAX_TASKS; i++) {
      if (task_table[i].is_active) {
        current_running_task = &task_table[i];
        break;
      }
    }
  }
  return current_running_task;
}
