#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define VIRTIO_VENDOR_ID 0x1AF4
#define VIRTIO_BLK_DEV_ID 0x1001

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1

#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2

typedef struct {
  unsigned long long addr;
  unsigned int len;
  unsigned short flags;
  unsigned short next;
} __attribute__((packed)) vring_desc_t;

typedef struct {
    unsigned short flags;
    unsigned short idx;
    unsigned short ring[];
} __attribute__((packed)) vring_avail_t;

typedef struct {
    unsigned int id;
    unsigned int len;
} __attribute__((packed)) vring_used_elem_t;

typedef struct {
    unsigned short flags;
    unsigned short idx;
    vring_used_elem_t ring[];
} __attribute__((packed)) vring_used_t;

typedef struct {
    unsigned int type;
    unsigned int reserved;
    unsigned long long sector;
} __attribute__((packed)) virtio_blk_req_t;

typedef struct {
    unsigned int   type;     // VIRTIO_BLK_T_IN (0)
    unsigned int   ioprio;   // 0
    unsigned long long sector; // 읽을 LBA 섹터
} __attribute__((packed)) virtio_blk_req_h;

void virtio_blk_init(void);
int virtio_blk_read(unsigned long long sector, void *buffer);
int virtio_blk_write(unsigned long long sector, const void *buffer);

#endif
