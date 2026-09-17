// kernel/spsc_ring.c
#include "spsc_ring.h"

// x86_64 Memory Barrier Primitives
#define memory_barrier() __asm__ __volatile__("" ::: "memory")
#define smp_store_release(p, v)                                                \
  do {                                                                         \
    memory_barrier();                                                          \
    *(p) = (v);                                                                \
  } while (0)
#define smp_load_acquire(p)                                                    \
  ({                                                                           \
    __typeof__(*p) ___p1 = *(p);                                               \
    memory_barrier();                                                          \
    ___p1;                                                                     \
  })

void spsc_ring_init(spsc_ring_buffer_t *ring) {
  if (!ring)
    return;
  ring->head = 0;
  ring->tail = 0;
  for (int i = 0; i < RING_CAPACITY; i++) {
    ring->descriptors[i].request_id = 0;
    ring->descriptors[i].opcode = 0;
    ring->descriptors[i].buffer_handle = 0;
    ring->descriptors[i].length = 0;
    ring->descriptors[i].flags = 0;
  }
}

// Producer Push (Release Publish)
int spsc_ring_push(spsc_ring_buffer_t *ring, const ring_descriptor_t *desc) {
  if (!ring || !desc)
    return -1;

  uint32_t current_tail = ring->tail;
  uint32_t current_head = smp_load_acquire(&ring->head);

  // Queue Full 검사 (Wraparound 안전)
  if ((current_tail - current_head) >= RING_CAPACITY) {
    return -2; // Queue Full (Backpressure)
  }

  uint32_t slot = current_tail & (RING_CAPACITY - 1);

  // Descriptor 복사 (비신뢰 입력 스냅샷)
  ring->descriptors[slot] = *desc;

  // Release Publish: Descriptor 기록이 완전히 끝난 후 tail 갱신
  smp_store_release(&ring->tail, current_tail + 1);

  return 0; // Success
}

// Consumer Pop (Acquire Fetch)
int spsc_ring_pop(spsc_ring_buffer_t *ring, ring_descriptor_t *out_desc) {
  if (!ring || !out_desc)
    return -1;

  uint32_t current_head = ring->head;
  uint32_t current_tail = smp_load_acquire(&ring->tail);

  // Queue Empty 검사
  if (current_head == current_tail) {
    return -3; // Queue Empty
  }

  uint32_t slot = current_head & (RING_CAPACITY - 1);

  // Descriptor 안전 복사 (Sanitize & Validation)
  *out_desc = ring->descriptors[slot];

  // Acquire Fetch: 읽기가 끝난 후 head 갱신
  smp_store_release(&ring->head, current_head + 1);

  return 0; // Success
}
