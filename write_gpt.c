#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <uchar.h> 
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

// -------------------------------------
// Global Typedefs
// -------------------------------------
// Globally Unique IDentifier (aka UUID) 
typedef struct {
    uint32_t time_lo;
    uint16_t time_mid;
    uint16_t time_hi_and_ver;       // Highest 4 bits are version #
    uint8_t clock_seq_hi_and_res;   // Highest bits are variant #
    uint8_t clock_seq_lo;
    uint8_t node[6];
} __attribute__ ((packed)) Guid;

// MBR Partition
typedef struct {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_lba;
} __attribute__ ((packed)) Mbr_Partition;

// Master Boot Record
typedef struct {
    uint8_t boot_code[440];
    uint32_t mbr_signature;
    uint16_t unknown;
    Mbr_Partition partition[4];
    uint16_t boot_signature;
} __attribute__ ((packed)) Mbr;

// GPT Header
typedef struct {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved_1;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    Guid disk_guid;
    uint64_t partition_table_lba;
    uint32_t number_of_entries;
    uint32_t size_of_entry;
    uint32_t partition_table_crc32;

    uint8_t reserved_2[512-92];
} __attribute__ ((packed)) Gpt_Header;

// GPT Partition Entry
typedef struct {
    Guid partition_type_guid;
    Guid unique_guid;
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    char16_t name[36];  // UCS-2 (UTF-16 limited to code points 0x0000 - 0xFFFF)
} __attribute__ ((packed)) Gpt_Partition_Entry;

// FAT32 Volume Boot Record (VBR)
typedef struct {
    uint8_t  BS_jmpBoot[3];
    uint8_t  BS_OEMName[8];
    uint16_t BPB_BytesPerSec;
    uint8_t  BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t  BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t  BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t  BPB_Reserved[12];
    uint8_t  BS_DrvNum;
    uint8_t  BS_Reserved1;
    uint8_t  BS_BootSig;
    uint8_t  BS_VolID[4];
    uint8_t  BS_VolLab[11];
    uint8_t  BS_FilSysType[8];

    // Not in fatgen103.doc tables
    uint8_t  boot_code[510-90];
    uint16_t bootsect_sig;      // 0xAA55
} __attribute__ ((packed)) Vbr;

// FAT32 File System Info Sector
typedef struct {
    uint32_t FSI_LeadSig;
    uint8_t  FSI_Reserved1[480];
    uint32_t FSI_StrucSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Nxt_Free;
    uint8_t  FSI_Reserved2[12];
    uint32_t FSI_TrailSig;
} __attribute__ ((packed)) FSInfo;

// FAT32 Directory Entry (Short Name)
typedef struct {
    uint8_t  DIR_Name[11];
    uint8_t  DIR_Attr;
    uint8_t  DIR_NTRes;
    uint8_t  DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} __attribute__ ((packed)) FAT32_Dir_Entry_Short;

// FAT32 Directory Entry Attributes
typedef enum {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN    = 0x02,
    ATTR_SYSTEM    = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE   = 0x20,
    ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN |
                     ATTR_SYSTEM    | ATTR_VOLUME_ID,
} FAT32_Dir_Attr;


// FAT32 File "types"
typedef enum {
    TYPE_DIR,   // Directory
    TYPE_FILE,  // Regular file
} File_Type;

// Common Virtual Hard Disk Footer, for a "fixed" vhd
// All fields are in network byte order (Big Endian),
//   since I'm lazy or otherwise a bad programmer,
//   we'll use byte arrays here
typedef struct {
    uint8_t cookie[8];
    uint8_t features[4];
    uint8_t version[4];
    uint64_t data_offset;
    uint8_t timestamp[4];
    uint8_t creator_app[4];
    uint8_t creator_ver[4];
    uint8_t creator_OS[4];
    uint8_t original_size[8];
    uint8_t current_size[8];
    uint8_t disk_geometry[4];
    uint8_t disk_type[4];
    uint8_t checksum[4];
    Guid unique_id;
    uint8_t saved_state;
    uint8_t reserved[427];
} __attribute__ ((packed)) Vhd;

// Internal Options object for commandline args
typedef struct {
    char *image_name;
    uint32_t lba_size;
    uint32_t esp_size;
    uint32_t data_size;
    char **esp_file_paths;
    uint32_t num_esp_file_paths;
    FILE **esp_files;
    char **data_files;
    uint32_t num_data_files;
    bool vhd;
    bool help;
    bool error;
} Options;

// -------------------------------------
// Global constants, enums
// -------------------------------------
// EFI System Partition GUID
const Guid ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, 
                        { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } };

// (Microsoft) Basic Data GUID
const Guid BASIC_DATA_GUID = { 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0,
                                { 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };

enum {
    GPT_TABLE_ENTRY_SIZE = 128,
    NUMBER_OF_GPT_TABLE_ENTRIES = 128,
    GPT_TABLE_SIZE = 16384,             // Minimum size per UEFI spec 2.10
    ALIGNMENT = 1048576,                // 1 MiB alignment value
};

// -------------------------------------
// Global Variables
// -------------------------------------
uint64_t lba_size = 512;
uint64_t esp_size = 1024*1024*33;   // 33 MiB
uint64_t data_size = 1024*1024*1;   // 1 MiB
uint64_t image_size = 0;
uint64_t esp_size_lbas = 0, data_size_lbas = 0, image_size_lbas = 0,  
         gpt_table_lbas = 0;                              // Sizes in lbas
uint64_t align_lba = 0, esp_lba = 0, data_lba = 0,
         fat32_fats_lba = 0, fat32_data_lba = 0;          // Starting LBA values

bool opened_info_file = false;

// =====================================
// Convert bytes to LBAs
// =====================================
uint64_t bytes_to_lbas(const uint64_t bytes) {
    return (bytes + (lba_size - 1)) / lba_size;
}

// =====================================
// Pad out 0s to full lba size
// =====================================
void write_full_lba_size(FILE *image) {
    uint8_t zero_sector[512];
    for (uint8_t i = 0; i < (lba_size - sizeof zero_sector) / sizeof zero_sector; i++)
        fwrite(zero_sector, sizeof zero_sector, 1, image);
}

// =====================================
// Get next highest aligned lba value after input lba
// =====================================
uint64_t next_aligned_lba(const uint64_t lba) {
    return lba - (lba % align_lba) + align_lba;
}

// =====================================
// Create a new Version 4 Variant 2 GUID
// =====================================
Guid new_guid(void) {
    uint8_t rand_arr[16] = { 0 };

    for (uint8_t i = 0; i < sizeof rand_arr; i++)
        rand_arr[i] = rand() & 0xFF;    // Equivalent to modulo 256

    // Fill out GUID
    Guid result = {
        .time_lo         = *(uint32_t *)&rand_arr[0],
        .time_mid        = *(uint16_t *)&rand_arr[4],
        .time_hi_and_ver = *(uint16_t *)&rand_arr[6],
        .clock_seq_hi_and_res = rand_arr[8],
        .clock_seq_lo = rand_arr[9],
        .node = { rand_arr[10], rand_arr[11], rand_arr[12], rand_arr[13],
                  rand_arr[14], rand_arr[15] },
    };

    // Fill out version bits - version 4
    result.time_hi_and_ver &= ~(1 << 15); // 0b_0_111 1111
    result.time_hi_and_ver |= (1 << 14);  // 0b0_1_00 0000
    result.time_hi_and_ver &= ~(1 << 13); // 0b11_0_1 1111
    result.time_hi_and_ver &= ~(1 << 12); // 0b111_0_ 1111

    // Fill out variant bits
    result.clock_seq_hi_and_res |= (1 << 7);    // 0b_1_000 0000
    result.clock_seq_hi_and_res |= (1 << 6);    // 0b0_1_00 0000
    result.clock_seq_hi_and_res &= ~(1 << 5);   // 0b11_0_1 1111

    return result;
}

// =====================================
// Create CRC32 table values
// =====================================
uint32_t crc_table[256];

void create_crc32_table(void) {
    uint32_t c = 0;

    for (int32_t n = 0; n < 256; n++) {
        c = (uint32_t)n;
        for (uint8_t k = 0; k < 8; k++) {
            if (c & 1) 
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
}

// =====================================
// Calculate CRC32 value for range of data
// =====================================
uint32_t calculate_crc32(void *buf, int32_t len) {
    static bool made_crc_table = false;

    uint8_t *bufp = buf;
    uint32_t c = 0xFFFFFFFFL;

    if (!made_crc_table) {
        create_crc32_table();
        made_crc_table = true;
    }

    for (int32_t n = 0; n < len; n++) 
        c = crc_table[(c ^ bufp[n]) & 0xFF] ^ (c >> 8);

    // Invert bits for return value
    return c ^ 0xFFFFFFFFL;
}

// =====================================
// Get new date/time values for FAT32 directory entries
// ===================================== 
void get_fat_dir_entry_time_date(uint16_t *in_time, uint16_t *in_date) {
    time_t curr_time;
    curr_time = time(NULL);
    struct tm tm = *localtime(&curr_time);

    // FAT32 needs # of years since 1980, localtime returns tm_year as # years since 1900,
    //   subtract 80 years for correct year value. Also convert month of year from 0-11 to 1-12
    //   by adding 1
    *in_date = ((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday;

    // Seconds is # 2-second count, 0-29
    if (tm.tm_sec == 60) tm.tm_sec = 59;
    *in_time = tm.tm_hour << 11 | tm.tm_min << 5 | (tm.tm_sec / 2);
}

// =====================================
// Write protective MBR
// ===================================== 
bool write_mbr(FILE *image) {
    uint64_t mbr_image_lbas = image_size_lbas;
    if (mbr_image_lbas > 0xFFFFFFFF) mbr_image_lbas = 0x100000000;

    Mbr mbr = {
        .boot_code = { 0 },
        .mbr_signature = 0,
        .unknown = 0,
        .partition[0] = {
            .boot_indicator = 0,
            .starting_chs = { 0x00, 0x02, 0x00 },
            .os_type = 0xEE,        // Protective GPT
            .ending_chs = { 0xFF, 0xFF, 0xFF },
            .starting_lba = 0x00000001,
            .size_lba = mbr_image_lbas - 1,
        },
        .boot_signature = 0xAA55,
    };

    if (fwrite(&mbr, 1, sizeof mbr, image) != sizeof mbr)
        return false;
    write_full_lba_size(image);

    return true;
}
// =====================================
// Write GPT headers & tables, primary & secondary
// =====================================
bool write_gpts(FILE *image) {
    // Fill out primary GPT header
    Gpt_Header primary_gpt = {
        .signature = { "EFI PART" },
        .revision = 0x00010000,   // Version 1.0
        .header_size = 92,
        .header_crc32 = 0,      // Will calculate later
        .reserved_1 = 0,
        .my_lba = 1,            // LBA 1 is right after MBR
        .alternate_lba = image_size_lbas - 1,
        .first_usable_lba = 1 + 1 + gpt_table_lbas, // MBR + GPT header + primary gpt table
        .last_usable_lba = image_size_lbas - 1 - gpt_table_lbas - 1, // 2nd GPT header + table
        .disk_guid = new_guid(),
        .partition_table_lba = 2,   // After MBR + GPT header
        .number_of_entries = 128,   
        .size_of_entry = 128,
        .partition_table_crc32 = 0, // Will calculate later
        .reserved_2 = { 0 },
    };

    // Fill out primary table partition entries
    Gpt_Partition_Entry gpt_table[NUMBER_OF_GPT_TABLE_ENTRIES] = {
        // EFI System Paritition
        {
            .partition_type_guid = ESP_GUID,
            .unique_guid = new_guid(),
            .starting_lba = esp_lba,
            .ending_lba = esp_lba + esp_size_lbas - 1,      // 0-based index lba size
            .attributes = 0,
            .name = u"EFI SYSTEM",
        },

        // Basic Data Paritition
        {
            .partition_type_guid = BASIC_DATA_GUID,
            .unique_guid = new_guid(),
            .starting_lba = data_lba,
            .ending_lba = data_lba + data_size_lbas - 1,    // 0-based index lba size
            .attributes = 0,
            .name = u"BASIC DATA",
        },
    };

    // Fill out primary header CRC values
    primary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof gpt_table);
    primary_gpt.header_crc32 = calculate_crc32(&primary_gpt, primary_gpt.header_size);

    // Write primary gpt header to file
    if (fwrite(&primary_gpt, 1, sizeof primary_gpt, image) != sizeof primary_gpt)
        return false;
    write_full_lba_size(image);

    // Write primary gpt table to file
    if (fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table)
        return false;

    // Fill out secondary GPT header
    Gpt_Header secondary_gpt = primary_gpt;

    secondary_gpt.header_crc32 = 0;
    secondary_gpt.partition_table_crc32 = 0;
    secondary_gpt.my_lba = primary_gpt.alternate_lba;
    secondary_gpt.alternate_lba = primary_gpt.my_lba;
    secondary_gpt.partition_table_lba = image_size_lbas - 1 - gpt_table_lbas;

    // Fill out secondary header CRC values
    secondary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof gpt_table);
    secondary_gpt.header_crc32 = calculate_crc32(&secondary_gpt, secondary_gpt.header_size);

    // Go to position of secondary table
    fseek(image, secondary_gpt.partition_table_lba * lba_size, SEEK_SET);
    
    // Write secondary gpt table to file
    if (fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table)
        return false;

    // Write secondary gpt header to file
    if (fwrite(&secondary_gpt, 1, sizeof secondary_gpt, image) != sizeof secondary_gpt)
        return false;
    write_full_lba_size(image);

    return true;
}

// =====================================
// Write EFI System Partition (ESP) w/FAT32 filesystem
// =====================================
bool write_esp(FILE *image) {
    // Reserved sectors region --------------------------
    // Fill out Volume Boot Record (VBR)
    const uint8_t reserved_sectors = 32;    // FAT32
    Vbr vbr = {
        .BS_jmpBoot      = { 0xEB, 0x00, 0x90 },
        .BS_OEMName      = { "THISDISK" },
        .BPB_BytesPerSec = lba_size,         // This is limited to only 512/1024/2048/4096
        .BPB_SecPerClus  = 1,
        .BPB_RsvdSecCnt  = reserved_sectors,
        .BPB_NumFATs     = 2,                // 2 FAT tables
        .BPB_RootEntCnt  = 0,
        .BPB_TotSec16    = 0,
        .BPB_Media       = 0xF8,             // "Fixed" non-removable media; Could also be 0xF0 for e.g. flash drive
        .BPB_FATSz16     = 0,
        .BPB_SecPerTrk   = 0,  
        .BPB_NumHeads    = 0,    
        .BPB_HiddSec     = esp_lba - 1,      // # of sectors before this partition/volume
        .BPB_TotSec32    = esp_size_lbas,    // Size of this partition
        .BPB_FATSz32     = 0,                // Filled out below
        .BPB_ExtFlags    = 0,                // Mirrored FATs
        .BPB_FSVer       = 0,
        .BPB_RootClus    = 2,                // Clusters 0 & 1 are reserved; root dir cluster starts at 2
        .BPB_FSInfo      = 1,                // Sector 0 = this VBR; FS Info sector follows it
        .BPB_BkBootSec   = 6,                // 6 seems to be common for backup boot sector #
        .BPB_Reserved    = { 0 },
        .BS_DrvNum       = 0x80,             // 1st hard drive
        .BS_Reserved1    = 0,
        .BS_BootSig      = 0x29,
        .BS_VolID        = { 0 }, 
        .BS_VolLab       = { "NO NAME    " },// No volume label 
        .BS_FilSysType   = { "FAT32   " },

        // Not in fatgen103.doc tables
        .boot_code       = { 0 },
        .bootsect_sig    = 0xAA55,     
    };

    //.BPB_FATSz32 = From FAT docs: TMP1 = disk_size_sectors - reserved sector count;
    //                              TMP2 = (256 * sectors per cluster) + number of FATs;
    //                              For FAT32 => TMP2 = TMP2 / 2;
    //                              FATSz32 = (TMP1 + (TMP2 - 1)) / TMP2;
    // This should get the # of sectors (LBAs) required to hold all of the clusters for the ESP size,
    //   for 1 FAT table.
    uint32_t temp = ((256 * vbr.BPB_SecPerClus) + vbr.BPB_NumFATs) / 2;
    vbr.BPB_FATSz32 = ((esp_size_lbas - reserved_sectors) + (temp-1)) / temp;

    // Fill out file system info sector
    FSInfo fsinfo = {
        .FSI_LeadSig    = 0x41615252,
        .FSI_Reserved1  = { 0 },
        .FSI_StrucSig   = 0x61417272,
        .FSI_Free_Count = 0xFFFFFFFF,
        .FSI_Nxt_Free   = 5,            // First available cluster (value = 0) after /EFI/BOOT
        .FSI_Reserved2  = { 0 },
        .FSI_TrailSig   = 0xAA550000,
    };

    fat32_fats_lba = esp_lba + vbr.BPB_RsvdSecCnt;
    fat32_data_lba = fat32_fats_lba + (vbr.BPB_NumFATs * vbr.BPB_FATSz32);

    // Write VBR and FSInfo sector
    fseek(image, esp_lba * lba_size, SEEK_SET);
    if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
        fprintf(stderr, "Error: Could not write ESP VBR to image\n");
        return false;
    }
    write_full_lba_size(image);

    if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP File System Info Sector to image\n");
        return false;
    }
    write_full_lba_size(image);

    // Go to backup boot sector location
    fseek(image, (esp_lba + vbr.BPB_BkBootSec) * lba_size, SEEK_SET);

    // Write VBR and FSInfo at backup location
    fseek(image, esp_lba * lba_size, SEEK_SET);
    if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
        fprintf(stderr, "Error: Could not write VBR to image\n");
        return false;
    }
    write_full_lba_size(image);

    if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP File System Info Sector to image\n");
        return false;
    }
    write_full_lba_size(image);

    // FAT region --------------------------
    // Write FATs (NOTE: FATs will be mirrored)
    for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++) {
        fseek(image, 
              (fat32_fats_lba + (i*vbr.BPB_FATSz32)) * lba_size, 
              SEEK_SET);

        uint32_t cluster = 0;

        // Cluster 0; FAT identifier, lowest 8 bits are the media type/byte
        cluster = 0xFFFFFF00 | vbr.BPB_Media;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 1; End of Chain (EOC) marker
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 2; Root dir '/' cluster start, if end of file/dir data then write EOC marker
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 3; '/EFI' dir cluster
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 4; '/EFI/BOOT' dir cluster
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // Cluster 5+; Other files/directories...
        // e.g. if adding a file with a size = 5 sectors/clusters
        //cluster = 6;    // Point to next cluster containing file data
        //cluster = 7;    // Point to next cluster containing file data
        //cluster = 8;    // Point to next cluster containing file data
        //cluster = 9;    // Point to next cluster containing file data
        //cluster = 0xFFFFFFFF; // EOC marker, no more file data after this cluster
    }

    // Data region --------------------------
    // Write File/Dir data...
    fseek(image, fat32_data_lba * lba_size, SEEK_SET);

    // Root '/' Directory entries
    // "/EFI" dir entry 
    FAT32_Dir_Entry_Short dir_ent = {
        .DIR_Name = { "EFI        " },
        .DIR_Attr = ATTR_DIRECTORY,
        .DIR_NTRes = 0,
        .DIR_CrtTimeTenth = 0,
        .DIR_CrtTime = 0,
        .DIR_CrtDate = 0,
        .DIR_LstAccDate = 0,
        .DIR_FstClusHI = 0,
        .DIR_WrtTime = 0,
        .DIR_WrtDate = 0,
        .DIR_FstClusLO = 3,
        .DIR_FileSize = 0,  // Directories have 0 file size
    };

    uint16_t create_time = 0, create_date = 0;
    get_fat_dir_entry_time_date(&create_time, &create_date);

    dir_ent.DIR_CrtTime = create_time;
    dir_ent.DIR_CrtDate = create_date;
    dir_ent.DIR_WrtTime = create_time;
    dir_ent.DIR_WrtDate = create_date;

    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    // /EFI Directory entries
    fseek(image, (fat32_data_lba + 1) * lba_size, SEEK_SET);

    memcpy(dir_ent.DIR_Name, ".          ", 11);    // "." dir entry, this directory itself
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    memcpy(dir_ent.DIR_Name, "..         ", 11);    // ".." dir entry, parent dir (ROOT dir)
    dir_ent.DIR_FstClusLO = 0;                      // Root directory does not have a cluster value
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    memcpy(dir_ent.DIR_Name, "BOOT       ", 11);    // /EFI/BOOT directory
    dir_ent.DIR_FstClusLO = 4;                      // /EFI/BOOT cluster
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    // /EFI/BOOT Directory entries
    fseek(image, (fat32_data_lba + 2) * lba_size, SEEK_SET);

    memcpy(dir_ent.DIR_Name, ".          ", 11);    // "." dir entry, this directory itself
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    memcpy(dir_ent.DIR_Name, "..         ", 11);    // ".." dir entry, parent dir (/EFI dir)
    dir_ent.DIR_FstClusLO = 3;                      // /EFI directory cluster
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    return true;
}

// =============================
// Add a new directory or file to a given parent directory
// =============================
bool add_file_to_esp(char *file_name, FILE *file, FILE *image, File_Type type, uint32_t *parent_dir_cluster) {
    // First grab FAT32 filesystem info for VBR and File System info
    Vbr vbr = { 0 };
    fseek(image, esp_lba * lba_size, SEEK_SET);
    if (fread(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
        fprintf(stderr, "Error: Could not fead vbr.\n");
        return false;
    }

    FSInfo fsinfo = { 0 };
    fseek(image, (esp_lba + 1) * lba_size, SEEK_SET);
    if (fread(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not fead fsinfo.\n");
        return false;
    }

    // Get file size of file
    uint64_t file_size_bytes = 0, file_size_lbas = 0;
    if (type == TYPE_FILE) {
        fseek(file, 0, SEEK_END);
        file_size_bytes = ftell(file);
        file_size_lbas = bytes_to_lbas(file_size_bytes);
        rewind(file);
    }

    // Get next free cluster in FATs
    uint32_t next_free_cluster = fsinfo.FSI_Nxt_Free;
    const uint32_t starting_cluster = next_free_cluster;  // Starting cluster for new dir/file

    // Add new clusters to FATs
    for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++) {
        uint32_t cluster = fsinfo.FSI_Nxt_Free;
        next_free_cluster = cluster;

        // Go to cluster location in FATs region
        fseek(image, (fat32_fats_lba + (i * vbr.BPB_FATSz32)) * lba_size, SEEK_SET);
        fseek(image, next_free_cluster * sizeof next_free_cluster, SEEK_CUR);

        if (type == TYPE_FILE && file_size_bytes != 0) {
            // Final cluster holds the end of chain (EOC) marker for final lba, can write until
            //   size -1 lbas first.
            for (uint64_t lba = 0; lba < file_size_lbas - 1; lba++) {
                cluster++;  // Each cluster points to next cluster of file data
                next_free_cluster++;
                fwrite(&cluster, sizeof cluster, 1, image);
            }
        }

        // Write EOC marker cluster for final file lba; this would be the only cluster added 
        //   for a directory (type == TYPE_DIR)
        cluster = 0xFFFFFFFF;
        next_free_cluster++;
        fwrite(&cluster, sizeof cluster, 1, image);
    }

    // Update next free cluster in FS Info
    fsinfo.FSI_Nxt_Free = next_free_cluster;
    fseek(image, (esp_lba + 1) * lba_size, SEEK_SET);
    fwrite(&fsinfo, sizeof fsinfo, 1, image); 

    // Go to Parent Directory's data location in data region
    fseek(image, (fat32_data_lba + *parent_dir_cluster - 2) * lba_size, SEEK_SET);

    // Add new directory entry for this new dir/file at end of current dir_entrys 
    FAT32_Dir_Entry_Short dir_entry = { 0 };

    while (fread(&dir_entry, 1, sizeof dir_entry, image) == sizeof dir_entry &&
           dir_entry.DIR_Name[0] != '\0')
        ;

    // sizeof dir_entry = 32, back up to overwrite this empty spot
    fseek(image, -32, SEEK_CUR);    

    // Set 8.3 file name
    memcpy(dir_entry.DIR_Name, file_name, 11);

    if (type == TYPE_DIR) dir_entry.DIR_Attr = ATTR_DIRECTORY;

    uint16_t fat_time, fat_date;
    get_fat_dir_entry_time_date(&fat_time, &fat_date);
    dir_entry.DIR_CrtTime = fat_time;
    dir_entry.DIR_CrtDate = fat_date;
    dir_entry.DIR_WrtTime = fat_time;
    dir_entry.DIR_WrtDate = fat_date;

    dir_entry.DIR_FstClusHI = (starting_cluster >> 16) & 0xFFFF;
    dir_entry.DIR_FstClusLO = starting_cluster & 0xFFFF;

    if (type == TYPE_FILE)
        dir_entry.DIR_FileSize = file_size_bytes;

    fwrite(&dir_entry, 1, sizeof dir_entry, image);

    // Go to this new file's cluster's data location in data region
    fseek(image, (fat32_data_lba + starting_cluster - 2) * lba_size, SEEK_SET);

    // Add new file data
    // For directory add dir_entrys for "." and ".."
    if (type == TYPE_DIR) {
        memcpy(dir_entry.DIR_Name, ".          ", 11);  // "." dir_entry; this directory itself
        fwrite(&dir_entry, 1, sizeof dir_entry, image);

        memcpy(dir_entry.DIR_Name, "..         ", 11);  // ".." dir_entry; parent directory
        dir_entry.DIR_FstClusHI = (*parent_dir_cluster >> 16) & 0xFFFF;
        dir_entry.DIR_FstClusLO = *parent_dir_cluster & 0xFFFF;
        fwrite(&dir_entry, 1, sizeof dir_entry, image);
    } else {
        // For file, add file data
        uint8_t *file_buf = calloc(1, lba_size);
        for (uint64_t i = 0; i < file_size_lbas; i++) {
            // In case last lba is less than a full lba in size, use actual bytes read
            //   to write file to disk image
            size_t bytes_read = fread(file_buf, 1, lba_size, file);
            fwrite(file_buf, 1, bytes_read, image);
        }
        free(file_buf);
    }

    // Set dir_cluster for new parent dir, if a directory was just added
    if (type == TYPE_DIR)
        *parent_dir_cluster = starting_cluster;

    return true;
}

// =============================
// Add a file path to the EFI System Partition;
//   will add new directories if not found, and
//   new file at end of path
// =============================
bool add_path_to_esp(char *path, FILE *file, FILE *image) {
    // Parse input path for each name
    if (*path != '/') return false; // Path must begin with root '/'

    // Uppercase path for that smooth DOS feel, but probably doesn't matter for any modern UEFI
    //   or FAT implementations
    for (size_t i = 0; i < strlen(path); i++) 
        path[i] = toupper(path[i]);

    File_Type type = TYPE_DIR;
    char *start = path + 1; // Skip initial slash
    char *end = start;
    uint32_t dir_cluster = 2;   // Next directory's cluster location; start at root
    bool any_files_added = false;

    // Get next name from path, until reached end of path for file to add
    while (type == TYPE_DIR) {
        while (*end != '/' && *end != '\0') end++;

        if (*end == '/') type = TYPE_DIR;
        else             type = TYPE_FILE;  // Reached end of path

        *end = '\0';    // Null terminate next name in case of directory


        char *dot_pos = strchr(start, '.');
        if ((type == TYPE_DIR  && strlen(start) > 11) ||
            (type == TYPE_FILE && strlen(start) > 12) || 
            (dot_pos && ((dot_pos - start > 8) ||           // Name 8 too long
                         (end - dot_pos) > 4))) {           // Ext 3 too long
            // Name is too long or invalid 8.3 naming
            fprintf(stderr, "WARNING: Name '%s' is too long for 8.3 naming. Truncating to fit.\n",
                    start);

            if (dot_pos) dot_pos[4] = '\0';  // Truncate file name
            else {
                // Directory
                size_t diff = strlen(start) - 11;   // Get name overlength amount

                *end = '/';                 // End new name
                char *next_start = end+1;   // Start of next name in path

                // Truncate directory name by shifting rest of path back
                memmove(start+12, next_start, strlen(next_start));

                // Set new end of full path by shortened amount
                *(path + strlen(path) - diff) = '\0';   

                end = start+11;
                *end = '\0';    // End new name
            }
        }

        // Convert file name to 8.3 name before checking if exists
        // e.g. "FOO.BAR"  -> "FOO     BAR" 
        //      "BA.Z"     -> "BA      Z  " 
        //      "ELEPHANT" -> "ELEPHANT   "
        char short_name[12] = {0};
        memset(short_name, ' ', 11);
        if (type == TYPE_DIR || !dot_pos)  {
            memcpy(short_name, start, strlen(start));  // No '.', copy full name
        } else {
            memcpy(short_name, start, dot_pos - start); // Name 8 in 8.3
            strncpy(&short_name[8], dot_pos+1, 3);      // Extension 3 in 8.3
        }

        // Search for name in current directory's file data (dir_entrys)
        FAT32_Dir_Entry_Short dir_entry = { 0 };
        bool found = false;
        fseek(image, (fat32_data_lba + dir_cluster - 2) * lba_size, SEEK_SET);
        do {
            if (fread(&dir_entry, 1, sizeof dir_entry, image) == sizeof dir_entry &&
                !memcmp(dir_entry.DIR_Name, short_name, 11)) {
                // Found name in directory, save cluster for last directory found
                dir_cluster = (dir_entry.DIR_FstClusHI << 16) | dir_entry.DIR_FstClusLO;
                found = true;
                break;
            }
        } while (dir_entry.DIR_Name[0] != '\0');

        if (!found) {
            // Add new directory or file to last found directory;
            //   if new directory, update current directory cluster to check/use
            //   for next new files 
            if (!add_file_to_esp(short_name, file, image, type, &dir_cluster))
                return false;

            any_files_added = true;
        }

        *end++ = '/';
        start = end;
    }

    *--end = '\0';  // Don't add extra slash to end of path, final file name is not a directory

    // Show info to user
    if (any_files_added) printf("Added '%s' to EFI System Partition\n", path);

    return true;
}

// =========================================================================
// Add disk image info file to hold at minimum the size of this disk image
// =========================================================================
bool add_disk_image_info_file(FILE *image) {
    FILE *fp = NULL;
    if (!opened_info_file) {
        opened_info_file = true;
        fp = fopen("FILE.TXT", "wb");  // Truncate on first open
    } else {
        fp = fopen("FILE.TXT", "ab");  // Add to end of file
    }
    if (!fp) return false;

    fprintf(fp, "DISK_SIZE=%"PRIu64"\n", image_size);
    fclose(fp);

    fp = fopen("FILE.TXT", "rb");

    char path[25] = { 0 };
    strcpy(path, "/EFI/BOOT/FILE.TXT");
    if (!add_path_to_esp(path, fp, image)) return false;
    fclose(fp);

    return true;
}

// ======================================
// Add file to the Basic Data Partition
// ======================================
bool add_file_to_data_partition(char *filepath, FILE *image) {
    // Will save location of next spot to put a file in
    static uint64_t starting_lba = 0;

    // Go to data partition
    fseek(image, (data_lba + starting_lba) * lba_size, SEEK_SET);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filepath);
        return false;
    }

    // Get file size 
    uint64_t file_size_bytes = 0, file_size_lbas = 0;
    fseek(fp, 0, SEEK_END);
    file_size_bytes = ftell(fp);
    file_size_lbas = bytes_to_lbas(file_size_bytes);
    rewind(fp);

    // Check if adding next file will overrun data partition size
    if ((starting_lba + file_size_lbas) * lba_size >= data_size) {
        fprintf(stderr, 
                "Error: Can't add file %s to Data Partition; "
                "Data Partition size is %"PRIu64 "(%"PRIu64" LBAs) and all files added "
                "would overrun this size\n",
                filepath,
                data_size, data_size_lbas);
    }

    uint8_t *file_buf = calloc(1, lba_size);
    for (uint64_t i = 0; i < file_size_lbas; i++) {
        uint64_t bytes_read = fread(file_buf, 1, lba_size, fp);
        fwrite(file_buf, 1, bytes_read, image);
    }
    free(file_buf);
    fclose(fp);

    // Print info to user
    char *name = NULL;
    char *slash = strrchr(filepath, '/'); 
    if (!slash) name = filepath;
    else name = slash + 1;

    printf("Added '%s' from path '%s' to Data Partition\n", 
           name,
           filepath);

    // Add to info file for each file added 
    char info_file[12] = "FILE.TXT"; // "Data (partition) files info"

    if (!opened_info_file) {
        opened_info_file = true;
        fp = fopen(info_file, "wb");    // Truncate before writing again
    } else {
        fp = fopen(info_file, "ab");    // Add to end of previous info
    }

    if (!fp) {
        fprintf(stderr, "Error: Could not open file '%s'\n", info_file);
        return false;
    }

    fprintf(fp,
            "FILE_NAME=%s\n"
            "FILE_SIZE=%"PRIu64"\n"
            "DISK_LBA=%"PRIu64"\n\n",  // Add extra line between files
            name,
            file_size_bytes,
            data_lba + starting_lba);  // Offset from start of data partition

    fclose(fp);

    // Set next spot to write a file at
    starting_lba += file_size_lbas;

    return true;
}

// =============================
// Get/parse input arguments from command line
// =============================
Options get_opts(int argc, char *argv[]) {
    Options options = { 0 };

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") ||
            !strcmp(argv[i], "--help")) {
            // Print help text and exit
            options.help = true;
            return options;
        }

        if (!strcmp(argv[i], "-i") ||
            !strcmp(argv[i], "--image-name")) {
            // Set name of image, instead of using default name
            if (++i >= argc) {
                options.error = true;
                return options;
            }

            options.image_name = argv[i];
            continue;
        }

        if (!strcmp(argv[i], "-l") ||
            !strcmp(argv[i], "--lba-size")) {
            // Set size of lba/disk sector, instead of default 512 bytes
            if (++i >= argc) {
                options.error = true;
                return options;
            }

            options.lba_size = strtol(argv[i], NULL, 10);

            if (options.lba_size != 512  &&
                options.lba_size != 1024 &&
                options.lba_size != 2048 &&
                options.lba_size != 4096) {
                // Error: invalid LBA size
                fprintf(stderr, "Error: Invalid LBA size, must be one of 512/1024/2048/4096\n");
                options.error = true;
                return options;
            }

            // Enforce minimum size of ESP per LBA size
            if ((options.lba_size == 512  && options.esp_size < 33)  ||
                (options.lba_size == 1024 && options.esp_size < 65)  ||
                (options.lba_size == 2048 && options.esp_size < 129) ||
                (options.lba_size == 4096 && options.esp_size < 257)) {

                fprintf(stderr, "Error: ESP Must be a minimum of 33/65/129/257 MiB for "
                                "LBA sizes 512/1024/2048/4096 respectively\n");
                options.error = true;
                return options;
            }
            continue;
        }

        if (!strcmp(argv[i], "-es") ||
            !strcmp(argv[i], "--esp-size")) {
            // Set size of EFI System Partition in Megabytes (MiB)
            if (++i >= argc) {
                options.error = true;
                return options;
            }

            // Enforce minimum size of ESP per LBA size
            options.esp_size = strtol(argv[i], NULL, 10);
            if ((options.lba_size == 512  && options.esp_size < 33)  ||
                (options.lba_size == 1024 && options.esp_size < 65)  ||
                (options.lba_size == 2048 && options.esp_size < 129) ||
                (options.lba_size == 4096 && options.esp_size < 257)) {

                fprintf(stderr, "Error: ESP Must be a minimum of 33/65/129/257 MiB for "
                                "LBA sizes 512/1024/2048/4096 respectively\n");
                options.error = true;
                return options;
            }

            continue;
        }

        if (!strcmp(argv[i], "-ds") ||
            !strcmp(argv[i], "--data-size")) {
            // Set size of EFI System Partition in Megabytes (MiB)
            if (++i >= argc) {
                options.error = true;
                return options;
            }

            options.data_size = strtol(argv[i], NULL, 10);
            continue;
        }

        if (!strcmp(argv[i], "-ae") ||
            !strcmp(argv[i], "--add-esp-files")) {
            // Add files to the EFI System Partition
            if (i + 2 >= argc) {
                // Need at least 2 more args for path & file, for this to work
                fprintf(stderr, "Error: Must include at least 1 path and 1 file to add to ESP\n");
                options.error = true;
                return options;
            }

            // Allocate memory for file paths & File pointers
            const uint32_t MAX_FILES = 10;
            options.esp_file_paths = malloc(MAX_FILES * sizeof(char *));
            options.esp_files = malloc(MAX_FILES * sizeof(FILE *));

            for (i += 1; i < argc && argv[i][0] != '-'; i++) {
                // Grab next 2 args, 1st will be path to add, 2nd will be file to add to path
                const int MAX_LEN = 256;
                options.esp_file_paths[options.num_esp_file_paths] = calloc(1, MAX_LEN);

                // Get path to add
                strncpy(options.esp_file_paths[options.num_esp_file_paths], 
                        argv[i], 
                        MAX_LEN-1);

                // Ensure path starts and ends with a slash '/'
                if ((argv[i][0] != '/') ||
                    (argv[i][strlen(argv[i]) - 1] != '/')) {
                    fprintf(stderr, 
                            "Error: All file paths to add to ESP must start and end with slash '/'\n");
                    options.error = true;
                    return options;
                }

                // Get FILE * for file to add to path
                i++;
                options.esp_files[options.num_esp_file_paths] = fopen(argv[i], "rb");
                if (!options.esp_files[options.num_esp_file_paths]) {
                    fprintf(stderr, "Error: Could not fopen file '%s'\n", argv[i]);
                    options.error = true;
                    return options;
                }

                // Concat file to add to path 
                char *slash = strrchr(argv[i], '/');
                if (!slash) {
                    // Plain file name, no folder path
                    strncat(options.esp_file_paths[options.num_esp_file_paths], 
                            argv[i], 
                            MAX_LEN-1);
                } else {
                    // Get only last name in path, no folders 
                    strncat(options.esp_file_paths[options.num_esp_file_paths], 
                            slash + 1,  // File name starts after final slash
                            MAX_LEN-1);
                }

                if (++options.num_esp_file_paths == MAX_FILES) {
                    fprintf(stderr, 
                            "Error: Number of ESP files to add must be <= %d\n",
                            MAX_FILES);
                    options.error = true;
                    return options;
                }
            }

            // Overall for loop will increment i; in order to get next option, decrement here
            i--;    
            continue;
        }

        if (!strcmp(argv[i], "-ad") ||
            !strcmp(argv[i], "--add-data-files")) {
            // Add files to the Basic Data Partition
            // Allocate memory for file paths
            const uint32_t MAX_FILES = 10;
            options.data_files = malloc(MAX_FILES * sizeof(char *));

            for (i += 1; i < argc && argv[i][0] != '-'; i++) {
                // Grab next 2 args, 1st will be path to add, 2nd will be file to add to path
                const int MAX_LEN = 256;
                options.data_files[options.num_data_files] = calloc(1, MAX_LEN);

                // Get path to add
                strncpy(options.data_files[options.num_data_files], 
                        argv[i], 
                        MAX_LEN-1);

                if (++options.num_data_files == MAX_FILES) {
                    fprintf(stderr, 
                            "Error: Number of Data Parition files to add must be <= %d\n",
                            MAX_FILES);

                    options.error = true;
                    return options;
                }
            }

            // Overall for loop will increment i; in order to get next option, decrement here
            i--;    
            continue;
        }

        if (!strcmp(argv[i], "-v") ||
            !strcmp(argv[i], "--vhd")) {
            // Add a fixed Virtual Hard Disk Footer to the disk image;
            //   will also change the suffix to .vhd

            options.vhd = true; 
            continue;
        }
    }

    return options;
}

// =============================
// Add a fixed Virtual Hard Disk footer to the disk image
// =============================
void add_fixed_vhd_footer(FILE *image) {
    // Fill out VHD footer info
    Vhd vhd = {
        .cookie = { "conectix" },
        .features = { 0 },
        .version = { 0x00, 0x01, 0x00, 0x00 },
        .data_offset = -1,
        .timestamp = { 0 }, // # of seconds since 01/01/2000
        .creator_app = { "qfic" },
        .creator_ver = { 0x00, 0x01, 0x00, 0x00},
        .creator_OS = { "MYOS" },
        .original_size = { 0 },
        .current_size = { 0 },
        .disk_geometry = { 0 },
        .disk_type = { 0x00, 0x00, 0x00, 0x02 }, // 2 = Fixed hard disk
        .checksum = { 0 },
        .unique_id = new_guid(),
        .saved_state = 0,
        .reserved = { 0 },
    };

    // Unix epoch for 01/01/2000 = 946684800,
    //  subtract this value from epoch 01/01/1970 to translate
    //  to correct timestamp
    uint32_t time_u32 = (uint32_t)time(NULL) - 946684800; 
    vhd.timestamp[0] = (time_u32 >> 24) & 0xFF;
    vhd.timestamp[1] = (time_u32 >> 16) & 0xFF;
    vhd.timestamp[2] = (time_u32 >>  8) & 0xFF;
    vhd.timestamp[3] = time_u32 & 0xFF;

    // Get current image size (should be 4KiB aligned - 512 bytes)
    //   and use 4KiB aligned size for vhd footer to not have "corrupted" image
    fseek(image, 0, SEEK_END); 
    const uint64_t vhd_image_size = ftell(image);

    vhd.original_size[0] = (vhd_image_size >> 56) & 0xFF;
    vhd.original_size[1] = (vhd_image_size >> 48) & 0xFF;
    vhd.original_size[2] = (vhd_image_size >> 40) & 0xFF;
    vhd.original_size[3] = (vhd_image_size >> 32) & 0xFF;
    vhd.original_size[4] = (vhd_image_size >> 24) & 0xFF;
    vhd.original_size[5] = (vhd_image_size >> 16) & 0xFF;
    vhd.original_size[6] = (vhd_image_size >>  8) & 0xFF;
    vhd.original_size[7] = vhd_image_size & 0xFF;

    memcpy(vhd.current_size, vhd.original_size, sizeof vhd.original_size); 

    // Fill out disk geometry (CHS values)
    // Code Taken from Microsoft VHD documentation
    uint32_t totalSectors;
    uint16_t cylinders;
    uint8_t heads, sectorsPerTrack;
    uint32_t cylinderTimesHeads;

    totalSectors = image_size_lbas;
    //                  C      H     S
    if (totalSectors > 65535 * 16 * 255)
        totalSectors = 65535 * 16 * 255;

    if (totalSectors >= 65535 * 16 * 63) {
        sectorsPerTrack = 255;
        heads = 16;
        cylinderTimesHeads = totalSectors / sectorsPerTrack;
    } else {
        sectorsPerTrack = 17;
        cylinderTimesHeads = totalSectors / sectorsPerTrack;

        heads = (cylinderTimesHeads + 1023) / 1024;

        if (heads < 4) heads = 4;

        if (cylinderTimesHeads >= (heads * 1024) || heads > 16) {
            sectorsPerTrack = 31;
            heads = 16;
            cylinderTimesHeads = totalSectors / sectorsPerTrack;
        }

        if (cylinderTimesHeads >= (heads * 1024)) {
            sectorsPerTrack = 63;
            heads = 16;
            cylinderTimesHeads = totalSectors / sectorsPerTrack;
        }
    }
    cylinders = cylinderTimesHeads / heads;

    // CHS values for disk geometry: Cylinders 2 bytes, heads 1 byte, sectorsPerTrack 1 byte
    vhd.disk_geometry[0] = (cylinders >> 8) & 0xFF;
    vhd.disk_geometry[1] = cylinders & 0xFF;
    vhd.disk_geometry[2] = heads;
    vhd.disk_geometry[3] = sectorsPerTrack;

    // Fill out checksum
    // Code Taken from Microsoft VHD documentation
    uint32_t checksum = 0;
    uint8_t *vhd_p = (uint8_t *)&vhd;
    for (uint32_t counter = 0; counter < sizeof vhd; counter++) 
        checksum += vhd_p[counter];

    checksum = ~checksum;

    vhd.checksum[0] = (checksum >> 24) & 0xFF;
    vhd.checksum[1] = (checksum >> 16) & 0xFF;
    vhd.checksum[2] = (checksum >>  8) & 0xFF;
    vhd.checksum[3] = checksum & 0xFF;

    // Write footer to end of file
    fseek(image, 0, SEEK_END); 
    fwrite(&vhd, 1, sizeof vhd, image);
}

// =============================
// MAIN
// =============================
int main(int argc, char *argv[]) {
    FILE *image = NULL, *fp = NULL;

    // Get options passed in from command line
    Options options = get_opts(argc, argv);
    if (options.error) return EXIT_FAILURE;

    // Set/evaluate values from options
    if (options.help) {
        // Print help/usage text
        fprintf(stderr,
                "%s [options]\n"
                "\n"
                "options:\n"
                "-ad --add-data-files   Add local files to the basic data partition, and create\n"
                "                       a <FILE.TXT> file in directory '/EFI/BOOT/' in the \n"
                "                       ESP. This INF file will hold info for each file added\n"
                "                       ex: '-ad info.txt ../folderA/kernel.bin'.\n"
                "-ae --add-esp-files    Add local files to the generated EFI System Partition.\n"
                "                       File paths must start under root '/' and end with a \n"
                "                       slash '/', and all dir/file names are limited to FAT 8.3\n"
                "                       naming. Each file is added in 2 parts; The 1st arg for\n"
                "                       the path, and the 2nd arg for the file to add to that\n"
                "                       path. ex: '-ae /EFI/BOOT/ file1.txt' will add the local\n"
                "                       file 'file1.txt' to the ESP under the path '/EFI/BOOT/'.\n"
                "                       To add multiple files (up to 10), use multiple\n"
                "                       <path> <file> args.\n"
                "                       ex: '-ae /DIR1/ FILE1.TXT /DIR2/ FILE2.TXT'.\n"
                "-ds --data-size        Set the size of the Basic Data Partition in MiB; Minimum\n" 
                "                       size is 1 MiB\n" 
                "-es --esp-size         Set the size of the EFI System Partition in MiB\n"
                "-h  --help             Print this help text\n"
                "-i  --image-name       Set the image name. Default name is 'test.hdd'\n"
                "-l  --lba-size         Set the lba (sector) size in bytes; This is \n"
                "                       experimental, as tools are lacking for proper testing.\n"
                "                       Valid sizes: 512/1024/2048/4096\n" 
                "-v  --vhd              Create a fixed vhd footer and add it to the end of the\n" 
                "                       disk image. The image name will have a .vhd suffix.\n",
                argv[0]);
        return EXIT_SUCCESS;
    }

    // Using .hdd to ensure this also works by default in e.g. VirtualBox or other programs
    char *image_name = "test.hdd";  

    if (options.image_name) image_name = options.image_name;

    if (options.lba_size) lba_size = options.lba_size;

    if (options.esp_size) {
        // Enforce minimum sizes for ESP according to LBA size
        if ((lba_size == 512  && options.esp_size < 33)  ||
            (lba_size == 1024 && options.esp_size < 65)  ||
            (lba_size == 2048 && options.esp_size < 129) ||
            (lba_size == 4096 && options.esp_size < 257)) {

            fprintf(stderr, "Error: ESP Must be a minimum of 33/65/129/257 MiB for "
                            "LBA sizes 512/1024/2048/4096 respectively\n");
            return EXIT_FAILURE;
        }

        esp_size = options.esp_size * ALIGNMENT; 
    }

    // NOTE: Data partition will always be at least 1 MiB in size
    if (options.data_size) data_size = options.data_size * ALIGNMENT;

    // Set sizes & LBA values
    gpt_table_lbas = GPT_TABLE_SIZE / lba_size;

    // Add extra padding for:
    //   2 aligned partitions
    //   2 GPT tables
    //   MBR
    //   GPT headers
    const uint64_t padding = (ALIGNMENT*2 + (lba_size * ((gpt_table_lbas*2) + 1 + 2))); 
    image_size = esp_size + data_size + padding; 
    image_size_lbas = bytes_to_lbas(image_size);
    align_lba = ALIGNMENT / lba_size;
    esp_lba = align_lba;
    esp_size_lbas = bytes_to_lbas(esp_size);
    data_size_lbas = bytes_to_lbas(data_size);
    data_lba = next_aligned_lba(esp_lba + esp_size_lbas - 1);   // Use 0-based index size in lbas

    if (options.vhd) {
        // Only allow lba_size = 512 for vhd,
        //   the spec says it only uses 512 byte disk sectors
        if (lba_size > 512) {
            fprintf(stderr, "Error: VHD only allows disk sector size (LBA) = 512 bytes\n");
            return EXIT_FAILURE;
        }

        // Add VHD suffix to image name
        char *buf = calloc(1, strlen(image_name) + 5);
        strcpy(buf, image_name);

        char *dot_pos = strrchr(buf, '.');
        if (!dot_pos) dot_pos = buf + strlen(buf); 
        strcpy(dot_pos, ".vhd");
        image_name = buf;
    }

    // Open image file
    image = fopen(image_name, "wb+");
    if (!image) {
        fprintf(stderr, "Error: could not open file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Print info on sizes and image for user
    printf("IMAGE NAME: %s\n"
           "LBA SIZE: %"PRIu64"\n"
           "ESP SIZE: %"PRIu64"MiB\n"
           "DATA SIZE: %"PRIu64"MiB\n"
           "PADDING: %"PRIu64"MiB\n"
           "IMAGE SIZE: %"PRIu64"MiB\n",

           image_name,
           lba_size,
           esp_size / ALIGNMENT,
           data_size / ALIGNMENT,
           padding / ALIGNMENT,
           image_size / ALIGNMENT);

    // Seed random number generation
    srand(time(NULL));

    // Write protective MBR
    if (!write_mbr(image)) {
        fprintf(stderr, "Error: could not write protective MBR for file %s\n", image_name);
        fclose(image);
        return EXIT_FAILURE;
    }

    // Write GPT headers & tables
    if (!write_gpts(image)) {
        fprintf(stderr, "Error: could not write GPT headers & tables for file %s\n", image_name);
        fclose(image);
        return EXIT_FAILURE;
    }

    // Write EFI System Partition w/FAT32 filesystem
    if (!write_esp(image)) {
        fprintf(stderr, "Error: could not write ESP for file %s\n", image_name);
        fclose(image);
        return EXIT_FAILURE;
    }

    // Check if "BOOTX64.EFI" file exists in current directory, if so automatically
    //   add it to the ESP
    fp = fopen("BOOTX64.EFI", "rb"); 
    if (fp) {
        char path[25] = { 0 };
        strcpy(path, "/EFI/BOOT/BOOTX64.EFI");
        if (!add_path_to_esp(path, fp, image)) 
            fprintf(stderr, "Error: Could not add file '%s'\n", path);

        fclose(fp);
    }

    if (options.num_esp_file_paths > 0) {
        // Add file paths to EFI System Partition
        for (uint32_t i = 0; i < options.num_esp_file_paths; i++) {
            if (!add_path_to_esp(options.esp_file_paths[i], options.esp_files[i], image)) {
                fprintf(stderr,
                        "ERROR: Could not add '%s' to ESP\n",
                        options.esp_file_paths[i]);
            }
            free(options.esp_file_paths[i]);
            fclose(options.esp_files[i]);
        }
        free(options.esp_file_paths);
        free(options.esp_files);
    }

    if (options.num_data_files > 0) {
        // Add file paths to Basic Data Partition
        for (uint32_t i = 0; i < options.num_data_files; i++) {
            if (!add_file_to_data_partition(options.data_files[i], image)) {
                fprintf(stderr,
                        "ERROR: Could not add file '%s' to data partition\n",
                        options.data_files[i]);
            }
            free(options.data_files[i]);
        }
        free(options.data_files);
    }

    // Pad file to next 4KiB aligned size
    fseek(image, 0, SEEK_END);
    uint64_t current_size = ftell(image);
    uint64_t new_size = current_size - (current_size % 4096) + 4096;
    uint8_t byte = 0;

    if (options.vhd) {
        fseek(image, new_size - (sizeof(Vhd) + 1), SEEK_SET);
        fwrite(&byte, 1, 1, image);

        // Add a fixed Virtual Hard Disk footer to the disk image
        add_fixed_vhd_footer(image);
        printf("Added VHD footer\n");

        // Image_name had .vhd concat-ed on in a separate buffer
        free(image_name);   
    } else {
        // No vhd footer
        fseek(image, new_size - 1, SEEK_SET);
        fwrite(&byte, 1, 1, image);
    }

    // Add disk image info file to hold at minimum the size of this disk image;
    //   this could be used in an EFI application later as part of an installer, for example
    image_size = new_size; // Image size is used to write info file
    if (!add_disk_image_info_file(image)) 
        fprintf(stderr, "Error: Could not add disk image info file to '%s'\n", image_name);

    // File cleanup
    fclose(image);

    return EXIT_SUCCESS;
}

