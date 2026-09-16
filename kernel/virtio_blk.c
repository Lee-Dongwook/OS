#include "virtio_blk.h"
#include "vmm.h"

extern void *pmm_alloc_page(void);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

static unsigned short virtio_io_base = 0;
static vring_desc_t *desc_table = 0;
static vring_avail_t *avail_ring = 0;
static vring_used_t *used_ring = 0;
static unsigned short queue_size = 0;

static inline unsigned int pci_read32(unsigned char bus, unsigned char slot,
                                      unsigned char func,
                                      unsigned char offset) {
  unsigned int address =
      (unsigned int)((bus << 16) | (slot << 11) | (func << 8) |
                     (offset & 0xFC) | ((unsigned int)0x80000000));
  __asm__ __volatile__("outl %0, %1"
                       :
                       : "a"(address),
                         "Nd"((unsigned short)PCI_CONFIG_ADDRESS));
  unsigned int val;
  __asm__ __volatile__("inl %1, %0"
                       : "=a"(val)
                       : "Nd"((unsigned short)PCI_CONFIG_DATA));
  return val;
}

static inline unsigned char inb(unsigned short port) {
  unsigned char ret;
  __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned short inw(unsigned short port) {
  unsigned short ret;
  __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outw(unsigned short port, unsigned short val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(unsigned short port, unsigned int val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

void virtio_blk_init(void) {
  for (unsigned short bus = 0; bus < 256; bus++) {
    for(unsigned char slot = 0; slot < 32; slot++) {
      unsigned int vendor_dev = pci_read32(bus, slot, 0, 0);
      unsigned short vendor = vendor_dev & 0xFFFF;
      unsigned short device = (vendor_dev >> 16) & 0xFFFF;

      if (vendor == VIRTIO_VENDOR_ID && device == VIRTIO_BLK_DEV_ID) {
        kputs("[VIRTIO-BLK] DEVICE FOUND AT PCI BUS", 0x00FFFF00);
        kput_hex(bus, 0x00FFFF00);
        kputs("\n", 0x00FFFF00);

        unsigned int bar0 = pci_read32(bus, slot, 0, 0x10);
        virtio_io_base = bar0 & ~0x3;

        outb(virtio_io_base + 18, 0);
        outb(virtio_io_base + 18, 1 | 2);

        outw(virtio_io_base + 14, 0);
        queue_size = inw(virtio_io_base + 12);

        void *vq_page = pmm_alloc_page();
        desc_table = (vring_desc_t *)vq_page;
        avail_ring = (vring_avail_t *)((unsigned char *)vq_page +
                                       queue_size * sizeof(vring_desc_t));

        unsigned long long used_off =
            (sizeof(vring_desc_t) * queue_size +
             sizeof(unsigned short) * (3 + queue_size) + 4095) &
            ~4095ULL;
        used_ring = (vring_used_t *)((unsigned char *)vq_page + used_off);

        outl(virtio_io_base + 8, ((unsigned long long)vq_page) >> 12);

        outb(virtio_io_base + 18, 1 | 2 | 4);
        kputs("[VIRTIO-BLK] INITIALIZED SUCCESSFULLY!\n", 0x0000FF00);
        return;
      }
    }
  }
  kputs("[VIRTIO-BLK] NO VIRTIO BLOCK DEVICE FOUND\n", 0x00FF0000);
}

int virtio_blk_read(unsigned long long sector, void *buffer) {
  if (!virtio_io_base)
    return -1;

  static virtio_blk_req_t req;
  static unsigned char status;

  req.type = VIRTIO_BLK_T_IN;
  req.reserved = 0;
  req.sector = sector;

  desc_table[0].addr = (unsigned long long)&req;
  desc_table[0].len = sizeof(virtio_blk_req_t);
  desc_table[0].flags = VRING_DESC_F_NEXT;
  desc_table[0].next = 1;

  desc_table[1].addr = (unsigned long long)buffer;
  desc_table[1].len = 512;
  desc_table[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
  desc_table[1].next = 2;

  desc_table[2].addr = (unsigned long long)&status;
  desc_table[2].len = 1;
  desc_table[2].flags = VRING_DESC_F_WRITE;
  desc_table[2].next = 0;

  avail_ring->ring[avail_ring->idx % queue_size] = 0;
  avail_ring->idx++;

  outw(virtio_io_base + 16, 0);

  while (used_ring->idx != avail_ring->idx) {
    __asm__ __volatile__("pause");
  }

  return (status == 0) ? 0 : -1;
}
