#include "task.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

static task_t task_table[MAX_TASKS];
static int next_task_id = 1;

void task_init(void) {
  for (int i = 0; i < MAX_TASKS; i++) {
    task_table[i].task_id = 0;
    task_table[i].is_active = 0;
    task_table[i].pml4 = 0;
  }
  kputs("[TASK] USER TASK SUBSYSTEM INITIALIZED\n", 0x00FFFF00);
}

task_t *task_create(void) {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (!task_table[i].is_active) {
      task_t *t = &task_table[i];
      t->task_id = next_task_id++;
      t->is_active = 1;
      t->pml4 = vmm_create_user_pml4();
      if (!t->pml4) {
        t->task_id = 0;
        t->is_active = 0;
        return 0;
      }

      kputs("[TASK] CREATED TASK ID: ", 0x00FFFF00);
      kput_dec(t->task_id, 0x00FFFF00);
      kputs("\n", 0x00FFFF00);

      return t;
    }
  }
  return 0;
}
