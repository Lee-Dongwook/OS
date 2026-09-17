// kernel/ipc.c
#include "ipc.h"
#include "task.h"

extern task_t *get_current_task(void);
extern void kputs(const char *str, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

static ipc_endpoint_t global_ep_pool[16];
static uint32_t ep_count = 0;

ipc_endpoint_t *ipc_endpoint_create(void) {
  if (ep_count >= 16)
    return 0;
  ipc_endpoint_t *ep = &global_ep_pool[ep_count++];
  ep->endpoint_id = ep_count;
  ep->head = 0;
  ep->tail = 0;
  ep->count = 0;
  ep->receiver_waiting = 0;
  return ep;
}

// System Call: sys_ipc_send
// Capability 권한(CAP_RIGHT_SEND)을 검증 후 Bounded-Copy 수행
int32_t sys_ipc_send(uint32_t ep_handle, const void *msg_ptr, uint32_t size,
                     uint32_t opcode) {
  task_t *cur_task = get_current_task();
  if (!cur_task || !msg_ptr)
    return -1;
  if (size > IPC_MAX_MSG_SIZE)
    return -2; // 메시지 크기 초과

  // 1. CSpace에서 Handle 검증 (Endpoint 타입 및 CAP_RIGHT_SEND 권한 필수)
  capability_entry_t *cap = cap_lookup(&cur_task->cspace, ep_handle,
                                       CAP_TYPE_ENDPOINT, CAP_RIGHT_SEND);
  if (!cap) {
    kputs("[IPC ERROR] INVALID SEND HANDLE OR INSUFFICIENT RIGHTS\n",
          0x00FF0000);
    return -3; // Permission Denied / Invalid Handle
  }

  ipc_endpoint_t *ep = (ipc_endpoint_t *)cap->object_ptr;
  if (!ep)
    return -4;

  // 2. Queue Full 검사 (Backpressure / Quota 적용)
  if (ep->count >= IPC_QUEUE_CAPACITY) {
    kputs("[IPC WARNING] ENDPOINT QUEUE FULL!\n", 0x00FFFF00);
    return -5; // Buffer Overflow / Busy
  }

  // 3. Ring Buffer에 Bounded Copy 수행
  ipc_message_t *slot = &ep->queue[ep->tail];
  slot->header.sender_handle = ep_handle;
  slot->header.msg_size = size;
  slot->header.opcode = opcode;

  // Memory Copy (User Space -> Kernel Queue Buffer)
  const uint8_t *src = (const uint8_t *)msg_ptr;
  for (uint32_t i = 0; i < size; i++) {
    slot->payload[i] = src[i];
  }

  ep->tail = (ep->tail + 1) % IPC_QUEUE_CAPACITY;
  ep->count++;

  kputs("[IPC] MESSAGE SENT TO ENDPOINT ID: ", 0x0000FF00);
  kput_dec(ep->endpoint_id, 0x0000FF00);
  kputs("\n", 0x0000FF00);

  return 0; // Success
}

// System Call: sys_ipc_recv
// Capability 권한(CAP_RIGHT_RECEIVE) 검증 후 Bounded-Copy 수신
int32_t sys_ipc_recv(uint32_t ep_handle, void *out_msg_ptr, uint32_t max_size,
                     uint32_t *out_opcode) {
  task_t *cur_task = get_current_task();
  if (!cur_task || !out_msg_ptr)
    return -1;

  // 1. CSpace Handle 검증 (Endpoint 타입 및 CAP_RIGHT_RECEIVE 권한)
  capability_entry_t *cap = cap_lookup(&cur_task->cspace, ep_handle,
                                       CAP_TYPE_ENDPOINT, CAP_RIGHT_RECEIVE);
  if (!cap) {
    kputs("[IPC ERROR] INVALID RECV HANDLE OR INSUFFICIENT RIGHTS\n",
          0x00FF0000);
    return -3;
  }

  ipc_endpoint_t *ep = (ipc_endpoint_t *)cap->object_ptr;
  if (!ep)
    return -4;

  // 2. Queue Empty 검사
  if (ep->count == 0) {
    return -6; // Queue Empty
  }

  // 3. Queue에서 Pop 및 User Space로 복사
  ipc_message_t *slot = &ep->queue[ep->head];
  uint32_t copy_bytes =
      (slot->header.msg_size < max_size) ? slot->header.msg_size : max_size;

  uint8_t *dst = (uint8_t *)out_msg_ptr;
  for (uint32_t i = 0; i < copy_bytes; i++) {
    dst[i] = slot->payload[i];
  }

  if (out_opcode)
    *out_opcode = slot->header.opcode;

  ep->head = (ep->head + 1) % IPC_QUEUE_CAPACITY;
  ep->count--;

  kputs("[IPC] MESSAGE RECEIVED FROM ENDPOINT ID: ", 0x0000FF00);
  kput_dec(ep->endpoint_id, 0x0000FF00);
  kputs("\n", 0x0000FF00);

  return (int32_t)copy_bytes;
}
