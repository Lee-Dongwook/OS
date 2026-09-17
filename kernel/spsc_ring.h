#ifndef SPSC_RING_H
#define SPSC_RING_H

#include <stdint.h>

#define RING_CAPACITY 16

typedef struct {
    uint32_t request_id;   // 채널 세대 및 요청 구분 ID
    uint32_t opcode;       // 명령 코드
    uint32_t buffer_handle;// 등록된 Shared Memory Handle
    uint32_t length;       // 데이터 길이
    uint32_t flags;        // State & Permission flags
    uint32_t reserved;
} ring_descriptor_t;

typedef struct {
    volatile uint32_t head __attribute__((aligned(64))); // Consumer가 갱신
    volatile uint32_t tail __attribute__((aligned(64))); // Producer가 갱신
    ring_descriptor_t descriptors[RING_CAPACITY];
} spsc_ring_buffer_t;

void spsc_ring_init(spsc_ring_buffer_t *ring);
int spsc_ring_push(spsc_ring_buffer_t *ring, const ring_descriptor_t *desc);
int spsc_ring_pop(spsc_ring_buffer_t *ring, ring_descriptor_t *out_desc);

#endif
