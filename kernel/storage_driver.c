// kernel/storage_driver.c
#include "storage_driver.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

void storage_driver_init(storage_driver_ctx_t *ctx, spsc_ring_buffer_t *ring) {
  if (!ctx)
    return;
  ctx->generation = 1; // Initial Generation
  ctx->state = DRIVER_STATE_RUNNING;
  ctx->pending_requests = 0;
  ctx->io_ring = ring;

  kputs("[STORAGE DRIVER] RUNNING AT GENERATION: ", 0x0000FF00);
  kput_dec(ctx->generation, 0x0000FF00);
  kputs("\n", 0x0000FF00);
}

// VirtIO I/O Ring 3 처리 루틴
int storage_driver_process_io(storage_driver_ctx_t *ctx) {
  if (!ctx || ctx->state != DRIVER_STATE_RUNNING) {
    kputs("[STORAGE DRIVER ERROR] CANNOT PROCESS I/O IN NON-RUNNING STATE!\n",
          0x00FF0000);
    return -1;
  }

  ring_descriptor_t desc;
  if (spsc_ring_pop(ctx->io_ring, &desc) == 0) {
    ctx->pending_requests++;

    // I/O Command 처리 시뮬레이션
    kputs("[STORAGE DRIVER] PROCESSED I/O REQ ID: ", 0x0000FF00);
    kput_dec(desc.request_id, 0x0000FF00);
    kputs(", SECTOR LENGTH: ", 0x0000FF00);
    kput_dec(desc.length, 0x0000FF00);
    kputs("\n", 0x0000FF00);

    ctx->pending_requests--;
    return 0; // Handled
  }
  return 1; // Ring Empty
}

// [Phase 4 장애 주입] 드라이버 Crash / Device Timeout 시뮬레이션
void storage_driver_inject_fault(storage_driver_ctx_t *ctx) {
  if (!ctx)
    return;

  kputs("\n[FAULT INJECTION] CRASH DETECTED IN STORAGE DRIVER!\n", 0x00FF0000);
  ctx->state = DRIVER_STATE_FAILED;
}

// 드라이버 장애 복구 상태 머신 수행
int storage_driver_recover(storage_driver_ctx_t *ctx) {
  if (!ctx || ctx->state != DRIVER_STATE_FAILED)
    return -1;

  // Step 1: Quiescing (DMA 정지 및 기존 Request 차단)
  ctx->state = DRIVER_STATE_QUIESCING;
  kputs("[STORAGE RECOVERY] STEP 1: QUIESCING DMA & BLOCKING NEW REQS...\n",
        0x00FFFF00);

  // Step 2: Device Reset (VirtIO Status Register Reset)
  ctx->state = DRIVER_STATE_RESETTING;
  kputs("[STORAGE RECOVERY] STEP 2: HARDWARE VIRTIO DEVICE RESETTING...\n",
        0x00FFFF00);

  // Step 3: Recovering (Generation 증가 및 Endpoint 재공개)
  ctx->state = DRIVER_STATE_RECOVERING;
  ctx->generation++; // 세대 번호 상승으로 이전 Stale Handle 접근 차단
  kputs("[STORAGE RECOVERY] STEP 3: NEW GENERATION ESTABLISHED: ", 0x00FFFF00);
  kput_dec(ctx->generation, 0x00FFFF00);
  kputs("\n", 0x00FFFF00);

  // Step 4: Normal State Restored
  ctx->state = DRIVER_STATE_RUNNING;
  ctx->pending_requests = 0;
  kputs("[STORAGE RECOVERY] SUCCESS: DRIVER RESTORED TO RUNNING STATE!\n\n",
        0x0000FF00);

  return 0;
}
