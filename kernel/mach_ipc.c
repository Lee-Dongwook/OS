#include "mach_ipc.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

#define MAX_PORTS 32
static mach_port_t port_table[MAX_PORTS];

void *memcpy(void *dest, const void *src, unsigned long long n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (unsigned long long i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

// IPC 서브시스템 초기화
void mach_ipc_init(void) {
    for (int i = 0; i < MAX_PORTS; i++) {
        port_table[i].port_id = i;
        port_table[i].is_active = 0;
        port_table[i].head = 0;
        port_table[i].tail = 0;
        port_table[i].count = 0;
    }
    kputs("[MACH IPC] IPC SUBSYSTEM INITIALIZED\n", 0x00FFFF00);
}

// 새로운 Mach Port 할당
unsigned int mach_port_allocate(void) {
    for (int i = 1; i < MAX_PORTS; i++) { // 0번 포트는 예약
        if (!port_table[i].is_active) {
            port_table[i].is_active = 1;
            port_table[i].head = 0;
            port_table[i].tail = 0;
            port_table[i].count = 0;
            return port_table[i].port_id;
        }
    }
    return 0; // 할당 실패
}

int mach_port_destroy(unsigned int port_id) {
    if (port_id == 0 || port_id >= MAX_PORTS || !port_table[port_id].is_active) {
        return -1;
    }

    port_table[port_id].is_active = 0;
    port_table[port_id].head = 0;
    port_table[port_id].tail = 0;
    port_table[port_id].count = 0;
    return 0;
}

int mach_port_active_count(void) {
    int count = 0;
    for (int i = 1; i < MAX_PORTS; i++) {
        if (port_table[i].is_active) {
            count++;
        }
    }
    return count;
}

// 메시지 전송 (Port Queue에 push)
int mach_msg_send(mach_message_t *msg) {
    if (!msg) {
        return -1;
    }
    unsigned int dest_port = msg->header.msgh_remote_port;

    if (dest_port >= MAX_PORTS || !port_table[dest_port].is_active) {
        return -1; // 유효하지 않은 포트
    }

    mach_port_t *port = &port_table[dest_port];

    if (port->count >= MAX_QUEUE_SIZE) {
        return -2; // 큐가 가득 참
    }

    // 큐 복사
    port->queue[port->tail] = *msg;
    port->tail = (port->tail + 1) % MAX_QUEUE_SIZE;
    port->count++;

    return 0; // 성공
}

// 메시지 수신 (Port Queue에서 pop)
int mach_msg_receive(unsigned int port_id, mach_message_t *out_msg) {
    if (!out_msg || port_id >= MAX_PORTS || !port_table[port_id].is_active) {
        return -1; // 유효하지 않은 포트
    }

    mach_port_t *port = &port_table[port_id];

    if (port->count == 0) {
        return -2; // 큐가 비어 있음
    }

    // 메시지 인출
    *out_msg = port->queue[port->head];
    port->head = (port->head + 1) % MAX_QUEUE_SIZE;
    port->count--;

    return 0; // 성공
}
