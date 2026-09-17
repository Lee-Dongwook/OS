#ifndef CAPABILITY_H
#define CAPABILITY_H

#include <stdint.h>

typedef enum {
  CAP_TYPE_NONE = 0,
  CAP_TYPE_TASK,
  CAP_TYPE_THREAD,
  CAP_TYPE_ADDRESS_SPACE,
  CAP_TYPE_MEMORY_OBJECT,
  CAP_TYPE_ENDPOINT, // IPC 통신용 Endpoint
  CAP_TYPE_NOTIFICATION,
  CAP_TYPE_DEVICE
} cap_type_t;

#define CAP_RIGHT_READ     (1U << 0)
#define CAP_RIGHT_WRITE    (1U << 1)
#define CAP_RIGHT_EXECUTE  (1U << 2)
#define CAP_RIGHT_SEND     (1U << 3)  // IPC Send Right
#define CAP_RIGHT_RECEIVE  (1U << 4)  // IPC Receive Right
#define CAP_RIGHT_GRANT    (1U << 5)  // 타 Task로 Capability 전송 권한
#define CAP_RIGHT_MANAGE (1U << 6)    // 객체 파괴/철회 권한

#define MAX_CSPACE_ENTRIES 64

typedef struct {
    cap_type_t type;
    uint32_t rights;
    uint32_t generation;        // Generation ID (오래된 핸들 재사용 방지)
    void *object_ptr;           // 커널 내부 실제 객체 포인터
} capability_entry_t;

typedef struct {
    capability_entry_t entries[MAX_CSPACE_ENTRIES];
    uint32_t active_count;
} cspace_t;

void cspace_init(cspace_t *cspace);
int32_t cap_alloc(cspace_t *cspace, cap_type_t type, uint32_t rights, void *obj_ptr, uint32_t *out_handle);
capability_entry_t *cap_lookup(cspace_t *cspace, uint32_t handle, cap_type_t expected_type, uint32_t required_rights);
int cap_revoke(cspace_t *cspace, uint32_t handle);

#endif
