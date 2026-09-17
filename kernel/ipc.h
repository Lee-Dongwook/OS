// kernel/ipc.h
#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include "capability.h"

#define IPC_MAX_MSG_SIZE 256  // Bounded-copy IPC 단일 메시지 최대 크기 (Bytes)
#define IPC_QUEUE_CAPACITY 8  // Endpoint당 메시지 큐 슬롯 수

// IPC 메시지 헤더 규격 (규격서 5.2절)
typedef struct {
    uint32_t sender_handle;   // 전송자 Capability Handle
    uint32_t msg_size;        // Payload 크기
    uint32_t opcode;          // 요청 명령 코드
    uint32_t flags;
} ipc_msg_header_t;

// 단일 IPC 메시지 슬롯
typedef struct {
    ipc_msg_header_t header;
    uint8_t payload[IPC_MAX_MSG_SIZE];
} ipc_message_t;

// 커널 내부 Endpoint 객체
typedef struct {
    uint32_t endpoint_id;
    ipc_message_t queue[IPC_QUEUE_CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    int receiver_waiting;     // 수신 대기 중인 Thread 존재 여부
} ipc_endpoint_t;

// IPC 커널 API
ipc_endpoint_t *ipc_endpoint_create(void);
int32_t sys_ipc_send(uint32_t ep_handle, const void *msg_ptr, uint32_t size, uint32_t opcode);
int32_t sys_ipc_recv(uint32_t ep_handle, void *out_msg_ptr, uint32_t max_size, uint32_t *out_opcode);

#endif
