#ifndef MACH_IPC_H
#define MACH_IPC_H

#define MAX_QUEUE_SIZE 16
#define MAX_MSG_BODY 128

typedef struct {
    unsigned int msgh_bits;
    unsigned int msgh_size;
    unsigned int msgh_remote_port; // 수신 포트 ID
    unsigned int msgh_local_port;  // 송신 포트 ID
    unsigned int msgh_id;          // 메시지 ID / 종류
} mach_msg_header_t;

typedef struct {
    mach_msg_header_t header;
    char body[MAX_MSG_BODY];
} mach_message_t;

// Mach Port 구조체
typedef struct {
    unsigned int port_id;
    int is_active;
    mach_message_t queue[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} mach_port_t;

void mach_ipc_init(void);
unsigned int mach_port_allocate(void);
int mach_msg_send(mach_message_t *msg);
int mach_msg_receive(unsigned int port_id, mach_message_t *out_msg);

#endif
