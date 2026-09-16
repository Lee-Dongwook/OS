#ifndef MACHO_H
#define MACHO_H

#define MH_MAGIC_64 0xFEEDFACF
#define LC_SEGMENT_64 0x19
#define LC_MAIN 0x80000028

typedef struct {
    unsigned int magic;
    int cputype;
    int cpusubtype;
    unsigned int filetype;
    unsigned int ncmds;
    unsigned int sizeofcmds;
    unsigned int flags;
    unsigned int reserved;
} __attribute__((packed)) mach_header_64_t;

typedef struct {
    unsigned int cmd;
    unsigned int cmdsize;
} __attribute__((packed)) load_command_t;

typedef struct {
    unsigned int cmd;
    unsigned int cmdsize;
    char segname[16];
    unsigned long long vmaddr;
    unsigned long long vmsize;
    unsigned long long fileoff;
    unsigned long long filesize;
    int maxprot;
    int initprot;
    unsigned int nsects;
    unsigned int flags;
} __attribute__((packed)) segment_command_64_t;

typedef struct {
    unsigned int cmd;
    unsigned int cmdsize;
    unsigned long long entryoff; // 메인 엔트리 포인트 오프셋
    unsigned long long stacksize;
} __attribute__((packed)) entry_point_command_t;

int macho_load_binary(const unsigned char *binary_data, unsigned long long *entry_point);
unsigned long long macho_load_from_fat32(const char *filename);

#endif
