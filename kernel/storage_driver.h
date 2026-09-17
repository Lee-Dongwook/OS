// kernel/storage_driver.h
#ifndef STORAGE_DRIVER_H
#define STORAGE_DRIVER_H

#include <stdint.h>
#include "spsc_ring.h"

// 드라이버 복구 상태 머신 (규격서 5.3절)
typedef enum {
    DRIVER_STATE_RUNNING = 0, // 정상 작동 중
    DRIVER_STATE_QUIESCING,   // I/O 중단 및 DMA 정지 중
    DRIVER_STATE_FAILED,      // 드라이버 Crash / Timeout 발생
    DRIVER_STATE_RESETTING,   // VirtIO 장치 HW Reset 진행 중
    DRIVER_STATE_RECOVERING   // 서비스 세대 번호 변경 및 Endpoint 재공개
} driver_state_t;

// VirtIO Block Request Command (VirtIO Spec 1.1)
#define VIRTIO_BLK_T_IN  0 // Read
#define VIRTIO_BLK_T_OUT 1 // Write

typedef struct {
    uint32_t type;     // Read / Write
    uint32_t reserved;
    uint64_t sector;   // LBA Sector Index
} virtio_blk_req_hdr_t;

// Ring 3 Storage Driver Context
typedef struct {
    uint32_t generation;        // Service Generation Number
    driver_state_t state;       // 현재 상태
    uint32_t pending_requests;  // 미처리 I/O 수
    spsc_ring_buffer_t *io_ring;// Async Ring Buffer
} storage_driver_ctx_t;

void storage_driver_init(storage_driver_ctx_t *ctx, spsc_ring_buffer_t *ring);
int storage_driver_process_io(storage_driver_ctx_t *ctx);
void storage_driver_inject_fault(storage_driver_ctx_t *ctx); // Fault Injection Test
int storage_driver_recover(storage_driver_ctx_t *ctx);

#endif
