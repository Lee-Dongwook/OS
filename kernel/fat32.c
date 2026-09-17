#include "fat32.h"
#include "virtio_blk.h"

extern void kputs(const char *str, unsigned int color);
extern void kput_hex(unsigned long long val, unsigned int color);

static fat32_bpb_t bpb;
static unsigned long long fat_start_lba = 0;
static unsigned long long data_start_lba = 0;

static unsigned char sector_buf[512] __attribute__((aligned(16)));

static void copy_bpb(fat32_bpb_t *destination, const fat32_bpb_t *source) {
  unsigned char *dest = (unsigned char *)destination;
  const unsigned char *src = (const unsigned char *)source;
  for (unsigned long long index = 0; index < sizeof(*destination); index++) {
    dest[index] = src[index];
  }
}

static unsigned long long cluster_to_lba(unsigned int cluster) {
  return data_start_lba +
         (unsigned long long)(cluster - 2) * bpb.sectors_per_cluster;
}

static int file_name_matches(const unsigned char entry_name[11],
                             const char *filename) {
  unsigned int index = 0;

  while (filename[index] != '\0' && filename[index] != '.' && index < 8) {
    if (entry_name[index] != (unsigned char)filename[index]) {
      return 0;
    }
    index++;
  }
  if (filename[index] != '\0' && filename[index] != '.') {
    return 0;
  }
  while (index < 8) {
    if (entry_name[index++] != ' ') {
      return 0;
    }
  }

  if (filename[index] == '.') {
    index++;
    for (unsigned int extension = 0; extension < 3; extension++) {
      if (filename[index] != '\0') {
        if (entry_name[8 + extension] != (unsigned char)filename[index++]) {
          return 0;
        }
      } else if (entry_name[8 + extension] != ' ') {
        return 0;
      }
    }
    return filename[index] == '\0';
  }

  return entry_name[8] == ' ' && entry_name[9] == ' ' && entry_name[10] == ' ';
}

static int fat32_next_cluster(unsigned int cluster,
                              unsigned int *next_cluster) {
  unsigned long long fat_offset =
      (unsigned long long)cluster * sizeof(unsigned int);
  unsigned long long fat_sector = fat_start_lba + fat_offset / 512;
  unsigned int offset_in_sector = (unsigned int)(fat_offset % 512);

  if (offset_in_sector > 508 || virtio_blk_read(fat_sector, sector_buf) != 0) {
    return -1;
  }

  *next_cluster =
      (*(unsigned int *)(sector_buf + offset_in_sector)) & 0x0FFFFFFF;
  return 0;
}

int fat32_init(void) {
  unsigned char sector[512];

  if (virtio_blk_read(0, sector) != 0) {
    kputs("[FAT32] ERROR: FAILED TO READ BOOT SECTOR\n", 0x00FF0000);
    return -1;
  }

  copy_bpb(&bpb, (const fat32_bpb_t *)sector);

  if (bpb.bytes_per_sector != 512 || bpb.sectors_per_cluster == 0 ||
      bpb.num_fats == 0 || bpb.fat_size_32 == 0 || bpb.root_cluster < 2) {
    kputs("[FAT32] ERROR: UNSUPPORTED OR MALFORMED BPB\n", 0x00FF0000);
    return -1;
  }

  fat_start_lba = bpb.reserved_sector_count;
  data_start_lba =
      fat_start_lba + ((unsigned long long)bpb.num_fats * bpb.fat_size_32);

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

    if (file_name_matches(entries[i].name, filename)) {
      unsigned int start_cluster =
          ((unsigned int)entries[i].first_cluster_high << 16) |
          entries[i].first_cluster_low;
      unsigned int file_size = entries[i].file_size;
      unsigned int remaining = file_size;
      unsigned int bytes_written = 0;
      unsigned int cluster = start_cluster;
      unsigned int cluster_visits = 0;
      unsigned int max_clusters =
          bpb.total_sectors_32 / bpb.sectors_per_cluster + 1;

      if (file_size > max_len || (file_size > 0 && cluster < 2)) {
        return -1;
      }

      while (remaining > 0) {
        if (cluster < 2 || cluster >= 0x0FFFFFF8 ||
            cluster_visits++ >= max_clusters) {
          return -1;
        }

        unsigned long long cluster_lba = cluster_to_lba(cluster);
        for (unsigned int sector = 0;
             sector < bpb.sectors_per_cluster && remaining > 0; sector++) {
          unsigned int copy_size = remaining < 512 ? remaining : 512;
          if (virtio_blk_read(cluster_lba + sector, sector_buf) != 0) {
            return -1;
          }
          unsigned char *destination = (unsigned char *)buffer + bytes_written;
          for (unsigned int byte = 0; byte < copy_size; byte++) {
            destination[byte] = sector_buf[byte];
          }
          bytes_written += copy_size;
          remaining -= copy_size;
        }

        if (remaining > 0 && fat32_next_cluster(cluster, &cluster) != 0) {
          return -1;
        }
      }
      return (int)file_size;
    }
  }
  return -1; // 파일을 찾지 못함
}
