#ifndef TASK_H
#define TASK_H

#include "vmm.h"
#include "scheduler.h"
#include "capability.h"

#define MAX_TASKS 8
#define MAX_TASK_PORTS 16

typedef struct task {
    int task_id;
    int is_active;
    page_table_t *pml4;                    // Task 전용 독립 가상 메모리 공간 (CR3)
    unsigned int ports[MAX_TASK_PORTS];    // Task가 소유한 Mach Port 권한 목록
    int port_count;
    cspace_t cspace; // Task 전용 Capability Space
} task_t;

void task_init(void);
task_t *task_create(void);
int task_add_port(task_t *task, unsigned int port_id);
int task_destroy(task_t *task);
int task_active_count(void);

#endif
