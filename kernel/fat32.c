#include "fat32.h"
#include "virtio_blk.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

static fat32_bpb_t bpb;
static unsigned long long fat_start_lba = 0;
static unsigned long long data_start_lba = 0;

static unsigned char sector_buf[512] __attribute__((aligned(16)));

static unsigned long long cluster_to_lba(unsigned int cluster) {
    return data_start_lba + (unsigned long long)(cluster - 2) * bpb.sectors_per_cluster;
}

static int memcmp(const void *s1, const void *s2, unsigned long long n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    for (unsigned long long i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return p1[i] - p2[i];
    }
    return 0;
}

int fat32_init(void) {
    unsigned char sector[512];

    if (virtio_blk_read(0, sector) != 0) {
        kputs("[FAT32] ERROR: FAILED TO READ BOOT SECTOR\n", 0x00FF0000);
        return -1;
    }

    fat32_bpb_t *bpb_ptr = (fat32_bpb_t *)sector;
    bpb = *bpb_ptr;

    fat_start_lba = bpb.reserved_sector_count;
    data_start_lba = fat_start_lba + ((unsigned long long)bpb.num_fats * bpb.fat_size_32);

    kputs("[FAT32] FS INITIALIZED! SECTORS PER CLUSTER: ", 0x00FFFF00);
    kput_hex(bpb.sectors_per_cluster, 0x00FFFF00);
    kputs("\n", 0x00FFFF00);

    return 0;
}

void fat32_list_root(void) {
    unsigned long long root_lba = cluster_to_lba(bpb.root_cluster);

    if (virtio_blk_read(root_lba, sector_buf) != 0) {
        kputs("[FAT32] ERROR: FAILED TO READ ROOT DIRECTORY\n", 0x00FF0000);
        return;
    }

    fat32_dir_entry_t *entries = (fat32_dir_entry_t *)sector_buf;
    kputs("\n[FAT32] ROOT DIRECTORY LISTINGS:\n", 0x0000FF00);

    for (int i = 0; i < 16; i++) {
        // 0x00: 더 이상 엔트리 없음, 0xE5: 삭제된 엔트리
        if (entries[i].name[0] == 0x00)
            break;
        if (entries[i].name[0] == 0xE5)
            continue;
        if (entries[i].attr == 0x0F)
            continue; // Long File Name(LFN) 엔트리 스킵

        char namebuf[12];
        for (int k = 0; k < 11; k++)
            namebuf[k] = entries[i].name[k];
        namebuf[11] = '\0';

        kputs("  - ", 0x00FFFF00);
        kputs(namebuf, 0x00FFFF00);
        kputs(" (SIZE: ", 0x00FFFF00);
        kput_hex(entries[i].file_size, 0x00FFFF00);
        kputs(" BYTES)\n", 0x00FFFF00);
    }
}

int fat32_read_file(const char *filename, void *buffer, unsigned int max_len) {
    unsigned long long root_lba = cluster_to_lba(bpb.root_cluster);

    if (virtio_blk_read(root_lba, sector_buf) != 0) {
        return -1;
    }

    fat32_dir_entry_t *entries = (fat32_dir_entry_t *)sector_buf;

    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] == 0x00)
            break;
        if (entries[i].name[0] == 0xE5 || entries[i].attr == 0x0F)
            continue;

        // 파일명 비교 (단순 8.3 포맷 기준)
        if (memcmp(entries[i].name, filename, 7) == 0) {
            unsigned int start_cluster =
                ((unsigned int)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            unsigned long long file_lba = cluster_to_lba(start_cluster);

            // 파일 데이터 섹터 읽기
            if (virtio_blk_read(file_lba, buffer) == 0) {
                return entries[i].file_size;
            }
        }
    }
    return -1; // 파일을 찾지 못함
}
