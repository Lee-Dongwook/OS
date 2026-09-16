#include "macho.h"
#include "fat32.h"
#include "vmm.h"
#include "pmm.h"

extern void *pmm_alloc_page(void);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
extern page_table_t *kernel_pml4;

static unsigned char file_buffer[64 * 1024];

static void memcpy(void *dest, const void *src, unsigned long long n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (unsigned long long i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static void memset(void *dest, int val, unsigned long long n) {
    unsigned char *d = (unsigned char *)dest;
    for (unsigned long long i = 0; i < n; i++) {
        d[i] = (unsigned char)val;
    }
}

int macho_load_binary(const unsigned char *binary_data, unsigned long long *entry_point) {
    mach_header_64_t *header = (mach_header_64_t *)binary_data;

    // Magic 넘버 검증 (0xFEEDFACF)
    if (header->magic != MH_MAGIC_64) {
        kputs("[MACHO] ERROR: INVALID MACH-O 64-BIT MAGIC\n", 0x00FF0000);
        return -1;
    }

    kputs("[MACHO] VALID MACH-O HEADER DETECTED\n", 0x00FFFF00);

    const unsigned char *ptr = binary_data + sizeof(mach_header_64_t);
    *entry_point = 0;

    for (unsigned int i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)ptr;

        if (cmd->cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)ptr;

            if (seg->vmsize > 0) {
                kputs("[MACHO] LOADING SEGMENT: ", 0x00FFFF00);
                kputs(seg->segname, 0x00FFFF00);
                kputs(" AT VMADDR: ", 0x00FFFF00);
                kput_hex(seg->vmaddr, 0x00FFFF00);
                kputs("\n", 0x00FFFF00);

                // 필요한 페이지 단위 크기 계산
                unsigned long long pages = (seg->vmsize + 0xFFF) / 0x1000;
                for (unsigned long long p = 0; p < pages; p++) {
                    unsigned long long virt = seg->vmaddr + (p * 0x1000);
                    void *phys = pmm_alloc_page();

                    // 유저 접근 권한 페이지 매핑
                    vmm_map_page(vmm_kernel_pml4(), virt, (unsigned long long)phys,
                                 PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

                    // 데이터 복사 및 BSS 영역 초기화
                    if (p * 0x1000 < seg->filesize) {
                        unsigned long long copy_len = seg->filesize - (p * 0x1000);
                        if (copy_len > 0x1000) copy_len = 0x1000;
                        memcpy(phys, binary_data + seg->fileoff + (p * 0x1000), copy_len);
                        if (copy_len < 0x1000) {
                            memset((unsigned char *)phys + copy_len, 0, 0x1000 - copy_len);
                        }
                    } else {
                        memset(phys, 0, 0x1000);
                    }
                }
            }
        } else if (cmd->cmd == LC_MAIN) {
            entry_point_command_t *ep = (entry_point_command_t *)ptr;
            *entry_point = ep->entryoff;
        }

        ptr += cmd->cmdsize;
    }

    if (*entry_point != 0) {
        kputs("[MACHO] ENTRY POINT LOADED: ", 0x0000FF00);
        kput_hex(*entry_point, 0x0000FF00);
        kputs("\n", 0x0000FF00);
        return 0;
    }

    return -1;
}

unsigned long long macho_load_from_fat32(const char *filename) {
    kputs("[MACHO-LOADER] LOADING FILE FROM FAT32: ", 0x00FFFF00);
    kputs(filename, 0x00FFFF00);
    kputs("\n", 0x00FFFF00);

    int read_bytes = fat32_read_file(filename, file_buffer, sizeof(file_buffer));
    if (read_bytes <= 0) {
        kputs("[MACHO-LOADER] ERROR: FILE READ FAILED OR EMPTY\n", 0x00FF0000);
        return 0;
    }

    mach_header_64_t *header = (mach_header_64_t *)file_buffer;
    if (header->magic != MH_MAGIC_64) {
        kputs("[MACHO-LOADER] ERROR: INVALID MACH-O MAGIC NUMBER\n", 0x00FF0000);
        return 0;
    }

    kputs("[MACHO-LOADER] VALID MACH-O 64-BIT HEADER FOUND!\n", 0x0000FF00);

    unsigned long long entry_point = 0;
    unsigned char *cmd_ptr = file_buffer + sizeof(mach_header_64_t);

    for (unsigned int i = 0; i < header->ncmds; i++) {
        unsigned int cmd = *(unsigned int *)cmd_ptr;
        unsigned int cmdsize = *(unsigned int *)(cmd_ptr + 4);

        if (cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)cmd_ptr;

            if (seg->vmsize > 0) {
                kputs("  -> SEGMENT: ", 0x0000FFFF);
                kputs(seg->segname, 0x0000FFFF);
                kputs(" VADDR: ", 0x0000FFFF);
                kput_hex(seg->vmaddr, 0x0000FFFF);
                kputs("\n", 0x0000FFFF);

                // 메모리 할당 및 VMM 매핑 (Ring 3 접근 허용: PAGE_USER | PAGE_WRITABLE)
                for (unsigned long long offset = 0; offset < seg->vmsize; offset += PAGE_SIZE) {
                    void *phys_page = pmm_alloc_page();
                    vmm_map_page(kernel_pml4, seg->vmaddr + offset, (unsigned long long)phys_page, 0x07); // Present | Writable | User
                }

                // 파일 세그먼트 데이터 복사
                if (seg->filesize > 0) {
                    unsigned char *dst = (unsigned char *)seg->vmaddr;
                    unsigned char *src = file_buffer + seg->fileoff;
                    for (unsigned long long b = 0; b < seg->filesize; b++) {
                        dst[b] = src[b];
                    }
                }

                // PAGEZERO 세그먼트가 아닌 첫 가상 주소를 entry_point 기본값으로 지정
                if (entry_point == 0 && seg->vmaddr != 0) {
                    entry_point = seg->vmaddr;
                }
            }
        }
        cmd_ptr += cmdsize;
    }

    return entry_point;
}

