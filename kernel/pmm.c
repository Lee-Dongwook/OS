#include "pmm.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern void kput_dec(unsigned long long val, unsigned int color);

#define BITMAP_SIZE (1024 * 1024 / 8)
static unsigned char pmm_bitmap[BITMAP_SIZE];
static unsigned long long total_usable_memory = 0;

static void set_bit(unsigned long long page_idx) {
  pmm_bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

static void clear_bit(unsigned long long page_idx) {
  pmm_bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

static int test_bit(unsigned long long page_idx) {
    return (pmm_bitmap[page_idx / 8] & (1 << (page_idx % 8))) != 0;
}

void pmm_init(void *memory_map, unsigned long long map_size,
              unsigned long long descriptor_size) {
  for (int i = 0; i < BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0xFF;
  }

  unsigned long long entries = map_size / descriptor_size;

  for (unsigned long long i = 0; i < entries; i++) {
    EFI_MEMORY_DESCRIPTOR *desc =
        (EFI_MEMORY_DESCRIPTOR *)((unsigned char *)memory_map +
                                  (i * descriptor_size));

    if (desc->type == EFI_CONVENTIONAL_MEMORY) {
      unsigned long long start_page = desc->physical_start / PAGE_SIZE;

      for (unsigned long long p = 0; p < desc->number_of_pages; p++) {
        if ((start_page + p) < (BITMAP_SIZE * 8)) {
          clear_bit(start_page + p);
          total_usable_memory += PAGE_SIZE;
        }
      }
    }
  }

  for (unsigned long long p = 0; p < 256; p++) {
        set_bit(p);
  }

    kputs("[PMM] TOTAL USABLE RAM: ", 0x00FFFF00);
    kput_dec(total_usable_memory / 1024 / 1024, 0x00FFFF00);
    kputs(" MB\n", 0x00FFFF00);
}

void *pmm_alloc_page(void) {
  for (unsigned long long i = 0; i < BITMAP_SIZE * 8; i++) {
    if (!test_bit(i)) {
      set_bit(i);
      return (void *)(i * PAGE_SIZE);
    }
  }
  return (void*)0;
}

void pmm_free_page(void *ptr) {
    if (!ptr) {
        return;
    }
    unsigned long long page_idx = (unsigned long long)ptr / PAGE_SIZE;
    // 저메모리와 비트맵 범위 밖의 주소는 커널/펌웨어 영역일 수 있으므로 해제하지 않는다.
    if (page_idx < 256 || page_idx >= BITMAP_SIZE * 8) {
        return;
    }
    clear_bit(page_idx);
}

unsigned long long pmm_total_usable_memory(void) {
    return total_usable_memory;
}

unsigned long long pmm_free_memory(void) {
    unsigned long long free_pages = 0;
    for (unsigned long long i = 0; i < BITMAP_SIZE * 8; i++) {
        if (!test_bit(i)) {
            free_pages++;
        }
    }
    return free_pages * PAGE_SIZE;
}
