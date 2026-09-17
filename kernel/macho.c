#include "macho.h"
#include "fat32.h"
#include "pmm.h"
#include "vmm.h"

extern void *pmm_alloc_page(void);
extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);
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

static int range_is_valid(unsigned long long offset, unsigned long long length,
                          unsigned long long total_size) {
    return offset <= total_size && length <= total_size - offset;
}

int macho_load_binary(const unsigned char *binary_data, unsigned long long binary_size,
                      page_table_t *target_pml4, unsigned long long *entry_point) {
    if (!binary_data || !target_pml4 || !entry_point || binary_size < sizeof(mach_header_64_t)) {
        return -1;
    }

    const mach_header_64_t *header = (const mach_header_64_t *)binary_data;
    if (header->magic != MH_MAGIC_64 ||
        !range_is_valid(sizeof(*header), header->sizeofcmds, binary_size)) {
        kputs("[MACHO] ERROR: INVALID OR TRUNCATED HEADER\n", 0x00FF0000);
        return -1;
    }

    unsigned long long command_offset = sizeof(*header);
    unsigned long long command_end = command_offset + header->sizeofcmds;
    unsigned long long requested_entry = 0;
    unsigned long long executable_start = 0;
    unsigned long long executable_end = 0;

    for (unsigned int index = 0; index < header->ncmds; index++) {
        if (!range_is_valid(command_offset, sizeof(load_command_t), command_end)) {
            return -1;
        }
        const load_command_t *command = (const load_command_t *)(binary_data + command_offset);
        if (command->cmdsize < sizeof(*command) ||
            !range_is_valid(command_offset, command->cmdsize, command_end)) {
            return -1;
        }

        if (command->cmd == LC_SEGMENT_64) {
            if (command->cmdsize < sizeof(segment_command_64_t)) {
                return -1;
            }
            const segment_command_64_t *segment = (const segment_command_64_t *)command;
            if (!range_is_valid(segment->fileoff, segment->filesize, binary_size) ||
                segment->filesize > segment->vmsize ||
                segment->vmaddr + segment->vmsize < segment->vmaddr) {
                return -1;
            }

            if (segment->vmsize > 0 && segment->vmaddr != 0) {
                unsigned long long page_start = segment->vmaddr & ~(PAGE_SIZE - 1ULL);
                unsigned long long page_end = (segment->vmaddr + segment->vmsize + PAGE_SIZE - 1ULL) &
                                              ~(PAGE_SIZE - 1ULL);
                unsigned long long flags = PAGE_PRESENT | PAGE_USER;
                if (segment->initprot & VM_PROT_WRITE) {
                    flags |= PAGE_WRITABLE;
                }

                for (unsigned long long virtual_page = page_start; virtual_page < page_end;
                     virtual_page += PAGE_SIZE) {
                    void *physical_page = pmm_alloc_page();
                    if (!physical_page) {
                        return -1;
                    }
                    memset(physical_page, 0, PAGE_SIZE);
                    vmm_map_page(target_pml4, virtual_page,
                                 (unsigned long long)physical_page, flags | PAGE_WRITABLE);

                    unsigned long long segment_end = segment->vmaddr + segment->filesize;
                    unsigned long long copy_start = virtual_page > segment->vmaddr ?
                                                        virtual_page : segment->vmaddr;
                    unsigned long long page_end = virtual_page + PAGE_SIZE;
                    unsigned long long copy_end = page_end < segment_end ? page_end : segment_end;
                    if (copy_start < copy_end) {
                        memcpy((unsigned char *)physical_page + (copy_start - virtual_page),
                               binary_data + segment->fileoff + (copy_start - segment->vmaddr),
                               copy_end - copy_start);
                    }
                }

                for (unsigned long long virtual_page = page_start; virtual_page < page_end;
                     virtual_page += PAGE_SIZE) {
                    if (vmm_protect_page(target_pml4, virtual_page, flags) != 0) {
                        return -1;
                    }
                }
                if (segment->initprot & VM_PROT_EXECUTE) {
                    executable_start = segment->vmaddr;
                    executable_end = segment->vmaddr + segment->vmsize;
                }
            }
        } else if (command->cmd == LC_MAIN && command->cmdsize >= sizeof(entry_point_command_t)) {
            requested_entry = ((const entry_point_command_t *)command)->entryoff;
        } else if (command->cmd == LC_UNIXTHREAD && command->cmdsize >= sizeof(unixthread_command_64_t)) {
            requested_entry = ((const unixthread_command_64_t *)command)->rip;
        }

        command_offset += command->cmdsize;
    }

    if (requested_entry == 0 || requested_entry < executable_start || requested_entry >= executable_end) {
        kputs("[MACHO] ERROR: MISSING OR INVALID ENTRY POINT\n", 0x00FF0000);
        return -1;
    }

    *entry_point = requested_entry;
    kputs("[MACHO] ENTRY POINT LOADED: ", 0x0000FF00);
    kput_hex(*entry_point, 0x0000FF00);
    kputs("\n", 0x0000FF00);
    return 0;
}

unsigned long long macho_load_from_fat32(const char *filename, page_table_t *target_pml4) {
    kputs("[MACHO-LOADER] LOADING FILE FROM FAT32: ", 0x00FFFF00);
    kputs(filename, 0x00FFFF00);
    kputs("\n", 0x00FFFF00);

    int read_bytes = fat32_read_file(filename, file_buffer, sizeof(file_buffer));
    if (read_bytes <= 0) {
        kputs("[MACHO-LOADER] ERROR: FILE READ FAILED OR EMPTY\n", 0x00FF0000);
        return 0;
    }

    unsigned long long entry_point = 0;
    if (macho_load_binary(file_buffer, (unsigned long long)read_bytes, target_pml4, &entry_point) != 0) {
        kputs("[MACHO-LOADER] ERROR: IMAGE VALIDATION FAILED\n", 0x00FF0000);
        return 0;
    }
    return entry_point;
}
