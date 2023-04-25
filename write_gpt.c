#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <uchar.h>
#include <inttypes.h>
#include <ctype.h>

// TODO: Add more/other error handling and conditions
// TODO: Maybe print more info text for user when running different options

// --------------------------------------------------------
// Global Typedefs
// --------------------------------------------------------
// EFI GUID
typedef struct {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t  clock_seq_hi_and_res;
    uint8_t  clock_seq_lo;
    uint8_t  node[6];
} __attribute__ ((packed)) EFI_GUID;

// FAT32 VBR (boot sector)
typedef struct {
    uint8_t BS_jmpBoot[3];
    uint8_t BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t BPB_Media;
    uint16_t BPB_FatSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FatSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t  BPB_Reserved[12];
    uint8_t BS_DrvNum;
    uint8_t BS_Reserved1;
    uint8_t BS_BootSig;
    uint32_t BS_VolID;
    uint8_t BS_VolLab[11];
    uint8_t BS_FilSysType[8];

    // Not in docs, but for padding and 0xAA55:
    uint8_t BS_Reserved2[510-90];
    uint16_t BS_BiosBootSignature;
} __attribute__ ((packed)) Volume_Boot_Record;

// FAT32 File System Info Sector
typedef struct {
    uint32_t FSI_LeadSig;
    uint8_t FSI_Reserved1[480];
    uint32_t FSI_StrucSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Nxt_Free;
    uint8_t FSI_Reserved2[12];
    uint32_t FSI_TrailSig;
} __attribute__ ((packed)) FSInfo_Sector;

// Short name dir entry
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
} __attribute__ ((packed)) FAT_Dir_Entry;

// Image creator internal options from commandline flags
typedef struct {
    uint32_t esp_size;
    uint32_t data_size;
    uint64_t lba_size;
    char image_name[BUFSIZ];
    char esp_path[BUFSIZ];
    char data_file_name[BUFSIZ];
    bool vhd;
    bool help;
    // Other options here ...
} Options;

// --------------------------------------------------------
// Global constants
// --------------------------------------------------------
// FAT directory entry File attributes
enum {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN    = 0x02,
    ATTR_SYSTEM    = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE   = 0x20,
    ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN |
                     ATTR_SYSTEM    | ATTR_VOLUME_ID,
};

enum {
    PARTITION_ENTRY_SIZE = 128,
    PARTITION_ENTRY_ARRAY_SIZE = 16384,
    NUM_PARTITION_ENTRIES = 
        PARTITION_ENTRY_ARRAY_SIZE / PARTITION_ENTRY_SIZE,
    PARTITION_ALIGNMENT = 1048576,  // 1MiB
    MEGABYTE_SIZE = 1048576,        // 1MiB
};

const EFI_GUID ESP_GUID = 
    { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B,
        { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } };

const EFI_GUID BASIC_DATA_GUID = 
    { 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 
        { 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };

// --------------------------------------------------------
// Global variables
// --------------------------------------------------------
uint64_t lba_size        = 512;
uint64_t esp_size        = 1024*1024*33;    // 33MiB  
uint64_t basic_data_size = 1024*1024*10;    // 10 MiB
uint64_t image_size      = 0;

// ========================================================
// Helper function to convert bytes to lbas
// ========================================================
inline uint64_t bytes_to_lbas(const uint64_t bytes) {
    return (bytes / lba_size) + (bytes % lba_size ? 1 : 0); 
}

// ========================================================
// Helper function to get next highest aligned lba value
// ========================================================
inline uint64_t next_aligned_lba(const uint64_t lba) {
    const uint64_t align_lba = 
        PARTITION_ALIGNMENT / lba_size;
    return lba - (lba % align_lba) + align_lba;
}

// ========================================================
// Helper function to pad out 0s to full lba size if
// lba_size > 512 bytes
// ========================================================
void pad_out_to_full_lba_size(FILE *fp) {
    uint8_t zero_sector[512] = {0};
    uint8_t i = (lba_size - sizeof zero_sector) / lba_size;
    while (i > 0) {
        fwrite(zero_sector, sizeof zero_sector, 1, fp);
        i--;
    }
}

// ========================================================
// Helper function to set FAT directory entry date/time
//   values from a timestamp
// ========================================================
void get_fat_dir_entry_date_time(uint16_t *write_time, uint16_t *write_date) {
    // Get current date/time
    time_t t = time(NULL);
    struct tm tm; 
    tm = *localtime(&t);

    uint8_t second = (tm.tm_sec % 58) >> 2;  // 2 sec count, 5 bits
    uint8_t minute = tm.tm_min;
    uint8_t hour   = tm.tm_hour;
    uint8_t day    = tm.tm_mday;
    uint8_t month  = tm.tm_mon + 1;     // 1-12
    uint8_t year   = (tm.tm_year - 80); // years since 1980

    *write_time = (hour << 11) | (minute << 5) | second;
    *write_date = (year << 9) | (month << 5) | day;
}

// ========================================================
// Generate CRC32 Table values
// ========================================================
// CRC32 Table
uint32_t crc_table[256];

void make_crc_table(void) {
    for (int32_t n = 0; n < 256; n++) {
        uint32_t c = (uint32_t)n;
        for (int32_t k = 0; k < 8; k++) {
            if (c & 1) 
                c = 0xedb88320L ^ (c >> 1);
            else 
                c >>= 1;
        }
        crc_table[n] = c;
    }
}

// ========================================================
// Calculate CRC32 value over range of data
// ========================================================
uint32_t calculate_crc32(uint8_t *buf, int32_t len) {
    static bool created_crc_table = false;

    if (!created_crc_table) {
        make_crc_table();
        created_crc_table = true;
    }

    uint32_t c = 0xffffffffL;
    for (int32_t n = 0; n < len; n++) 
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);

    // Return bits inverted (1's complement)
    return c ^ 0xffffffffL;
}

// ========================================================
// Generate a new pseudo-random version 4 variant 2 GUID
// ========================================================
EFI_GUID new_guid(void) {
    uint8_t rand_arr[16];

    for (uint8_t i = 0; i < sizeof rand_arr; i++)
        rand_arr[i] = rand() % (UINT8_MAX + 1);

    EFI_GUID result = {
        .time_low = (rand_arr[0] << 24) | 
                    (rand_arr[1] << 16) |
                    (rand_arr[2] << 8)  |
                     rand_arr[3],
        .time_mid = (rand_arr[4] << 8) |
                     rand_arr[5],
        .time_hi_and_version = (rand_arr[6] << 8) |
                                rand_arr[7],
        .clock_seq_hi_and_res = rand_arr[8],
        .clock_seq_lo = rand_arr[9],
        .node = { rand_arr[10], rand_arr[11],
                  rand_arr[12], rand_arr[13],
                  rand_arr[14], rand_arr[15] },
    };

    // Set variant 2 bits
    result.clock_seq_hi_and_res |= (1 << 7);   // 0b_1_0000000
    result.clock_seq_hi_and_res |= (1 << 6);   // 0b0_1_000000
    result.clock_seq_hi_and_res &= ~(1 << 5);  // 0b11_0_11111

    // Set version 4 bits
    result.time_hi_and_version &= ~(1 << 15); // 0b_0_1111111
    result.time_hi_and_version |=  (1 << 14); // 0b0_1_000000
    result.time_hi_and_version &= ~(1 << 13); // 0b11_0_11111
    result.time_hi_and_version &= ~(1 << 12); // 0b111_0_1111

    return result;
}

// ========================================================
// Write protective MBR for LBA 0
// ========================================================
bool write_protective_mbr(FILE *fp) {
    // MBR Partition entry
    typedef struct {
        uint8_t boot_indicator;
        uint8_t starting_chs[3];
        uint8_t os_type;
        uint8_t ending_chs[3];
        uint32_t starting_lba;
        uint32_t size_in_lba;
    } __attribute__ ((packed)) MBR_PARTITION_RECORD;

    // MBR Partition table
    typedef struct {
        uint8_t boot_code[440];
        uint32_t unique_mbr_signature;
        uint16_t unknown;
        MBR_PARTITION_RECORD partition[4];
        uint16_t boot_signature;
    } __attribute__ ((packed)) MASTER_BOOT_RECORD;

    uint64_t image_size_lbas = bytes_to_lbas(image_size); 

    if (image_size_lbas > 0xFFFFFFFF) 
        image_size_lbas = 0x100000000;

    MASTER_BOOT_RECORD mbr = {
        .boot_code = { 0 },
        .unique_mbr_signature = 0,
        .unknown = 0,
        .partition[0] = {
            .boot_indicator = 0x00,
            .starting_chs = { 0x00, 0x02, 0x00 },
            .os_type = 0xEE,        // GPT Protective
            .ending_chs = { 0xFF, 0xFF, 0xFF },
            .starting_lba = 0x00000001,
            .size_in_lba = image_size_lbas - 1,
        },
        .boot_signature = 0xAA55,
    };

    if (fwrite(&mbr, 1, sizeof mbr, fp) != sizeof mbr) {
        return false;
    }
    pad_out_to_full_lba_size(fp);

    return true;
}

// ========================================================
// Write GPT headers and partition entry arrays
// ========================================================
bool write_gpts(FILE *fp) {
    // GPT Header
    typedef struct {
        uint8_t  signature[8];
        uint32_t revision;
        uint32_t header_size;
        uint32_t header_crc32;
        uint32_t reserved;
        uint64_t my_lba;
        uint64_t alternate_lba;
        uint64_t first_usable_lba;
        uint64_t last_usable_lba;
        EFI_GUID disk_guid;
        uint64_t partition_entry_lba;
        uint32_t number_of_partition_entries;
        uint32_t size_of_partition_entry;
        uint32_t partition_entry_array_crc32;
        uint8_t  reserved_2[512-92];
    } __attribute__ ((packed)) GPT_HEADER;

    // GPT Partition Entry
    typedef struct {
        EFI_GUID partition_type_guid;
        EFI_GUID unique_partition_guid;
        uint64_t starting_lba;
        uint64_t ending_lba;
        uint64_t attributes;
        char16_t partition_name[36];
    } __attribute__ ((packed)) EFI_PARTITION_ENTRY;

    uint64_t image_size_lbas = bytes_to_lbas(image_size);

    uint64_t partition_entry_lbas = 
        bytes_to_lbas(PARTITION_ENTRY_ARRAY_SIZE);

    GPT_HEADER primary_gpt = {
        .signature = { "EFI PART" },
        .revision = 0x00010000,
        .header_size = 92,
        .header_crc32 = 0,
        .reserved = 0,
        .my_lba = 1,
        .alternate_lba = image_size_lbas - 1,
        .first_usable_lba = 1 + 1 + partition_entry_lbas,
        .last_usable_lba = 
            image_size_lbas - 1 - partition_entry_lbas,
        .disk_guid = new_guid(),
        .partition_entry_lba = 2,
        .number_of_partition_entries = NUM_PARTITION_ENTRIES,
        .size_of_partition_entry = PARTITION_ENTRY_SIZE,
        .partition_entry_array_crc32 = 0, 
        .reserved_2 = { 0 },
    };

    const uint64_t align_lba = PARTITION_ALIGNMENT / lba_size;
    const uint64_t esp_lbas = bytes_to_lbas(esp_size);
    const uint64_t basic_data_lbas = bytes_to_lbas(basic_data_size);

    EFI_PARTITION_ENTRY 
        gpt_entries[NUM_PARTITION_ENTRIES] = {
            {
                .partition_type_guid = ESP_GUID,
                .unique_partition_guid = new_guid(),
                .starting_lba = align_lba,
                .ending_lba = align_lba + esp_lbas,
                .attributes = 0,
                .partition_name = { u"EFI SYSTEM" }
            },

            {
                .partition_type_guid = BASIC_DATA_GUID,
                .unique_partition_guid = new_guid(),
                .starting_lba = 
                    next_aligned_lba(align_lba + esp_lbas),
                .ending_lba = 
                    next_aligned_lba(align_lba + esp_lbas) +
                    basic_data_lbas,
                .attributes = 0,
                .partition_name = { u"BASIC DATA" }
            },
        };

    // Calculate CRC32 values in primary Header
    primary_gpt.partition_entry_array_crc32 = 
        calculate_crc32(
                (uint8_t *)gpt_entries, 
                primary_gpt.number_of_partition_entries * 
                primary_gpt.size_of_partition_entry);

    primary_gpt.header_crc32 = 
        calculate_crc32((uint8_t *)&primary_gpt, 
                        primary_gpt.header_size);

    // Write primary GPT header and table
    if (fwrite(&primary_gpt, 1, sizeof(GPT_HEADER), fp) !=
            sizeof(GPT_HEADER)) {
        return false;
    }
    pad_out_to_full_lba_size(fp);

    if (fwrite(gpt_entries, 1, sizeof gpt_entries, fp) !=
            sizeof gpt_entries) {
        return false;
    }

    // Seek to location of secondary GPT table
    // NOTE: Assumes image_size is <= INT32_MAX
    fseek(fp, image_size, SEEK_SET);    // End of image
    fseek(fp, 
          -(partition_entry_lbas + 1) * lba_size, 
          SEEK_CUR);    // Back up to secondary table lba

    // Write Secondary table 
    if (fwrite(gpt_entries, 1, sizeof gpt_entries, fp) !=
            sizeof gpt_entries) {
        return false;
    }

    // Fill out Secondary GPT Header 
    GPT_HEADER secondary_gpt = primary_gpt;
    secondary_gpt.header_crc32 = 0;
    secondary_gpt.partition_entry_array_crc32 = 0;

    secondary_gpt.my_lba = primary_gpt.alternate_lba;
    secondary_gpt.alternate_lba = primary_gpt.my_lba;
    secondary_gpt.partition_entry_lba = 
        image_size_lbas - 1 - partition_entry_lbas,

    // Calculate CRC32 values in secondary header
    secondary_gpt.partition_entry_array_crc32 = 
        calculate_crc32(
                (uint8_t *)gpt_entries, 
                primary_gpt.number_of_partition_entries * 
                primary_gpt.size_of_partition_entry);

    secondary_gpt.header_crc32 = 
        calculate_crc32((uint8_t *)&secondary_gpt, 
                        secondary_gpt.header_size);

    // Write secondary GPT header
    if (fwrite(&secondary_gpt, 1, sizeof(GPT_HEADER), fp) !=
            sizeof(GPT_HEADER)) {
        return false;
    }
    pad_out_to_full_lba_size(fp);

    return true;
}

// ========================================================
// Write EFI System Partition (ESP) with FAT32 filesystem
//   for file/folder structure '/EFI/BOOT/'
// Sources: fatgen103.doc
// ========================================================
bool write_esp(FILE *fp, Volume_Boot_Record *vbr) {
    // NOTE: Could move this to main() and not duplicate?
    const uint64_t esp_lbas = bytes_to_lbas(esp_size);
    const uint16_t rsvd_sects = 32;
    const uint32_t align_lba = PARTITION_ALIGNMENT / lba_size;

    // Fill out VBR
    *vbr = (Volume_Boot_Record){
        .BS_jmpBoot = { 0xEB, 0x00, 0x90 },
        .BS_OEMName = { "TEST OS" },
        .BPB_BytsPerSec = (uint16_t)lba_size,
        .BPB_SecPerClus = 1,
        .BPB_RsvdSecCnt = rsvd_sects,
        .BPB_NumFATs = 2,
        .BPB_RootEntCnt = 0,
        .BPB_TotSec16 = 0,
        .BPB_Media = 0xF8,      // Fixed non-removable media
        .BPB_FatSz16 = 0,
        .BPB_SecPerTrk = 0,
        .BPB_NumHeads = 0,
        .BPB_HiddSec = align_lba - 1,
        .BPB_TotSec32 = esp_lbas,
        .BPB_FatSz32 = (align_lba - rsvd_sects) / 2,  // Align on 1MiB 
        .BPB_ExtFlags = 0,
        .BPB_FSVer = 0,
        .BPB_RootClus = 2,
        .BPB_FSInfo = 1,    // Right after VBR
        .BPB_BkBootSec = 6,
        .BPB_Reserved = {0},
        .BS_DrvNum = 0x80,  // Drive number: Hard disk 1
        .BS_Reserved1 = 0,
        .BS_BootSig = 0x29,
        .BS_VolID = 0x00C0FFEE,
        .BS_VolLab = { "NO NAME    " }, 
        .BS_FilSysType = { "FAT32   " },

        .BS_Reserved2 = {0}, 
        .BS_BiosBootSignature = 0xAA55,
    };

    const uint64_t ESP_lba = align_lba;

    // Write VBR
    fseek(fp, ESP_lba * lba_size, SEEK_SET);

    if (fwrite(vbr, 1, sizeof(Volume_Boot_Record), fp) != 
            sizeof(Volume_Boot_Record)) {
        fprintf(stderr, "Error: Could not write ESP vbr\n");
        return false;
    }
    pad_out_to_full_lba_size(fp);

    // Fill out FS Info (file system info) sector
    FSInfo_Sector fsinfo = {
        .FSI_LeadSig = 0x41615252,      // "AaRR"
        .FSI_Reserved1 = {0},
        .FSI_StrucSig = 0x61417272,     // "aArr"
        .FSI_Free_Count = 0xFFFFFFFF, 
        .FSI_Nxt_Free = 0xFFFFFFFF,  
        .FSI_Reserved2 = {0},
        .FSI_TrailSig = 0xAA550000,
    };

    // Write FS Info
    if (fwrite(&fsinfo, 1, sizeof fsinfo, fp) != 
            sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP "
                        "FS Info\n");
        return false;
    }
    pad_out_to_full_lba_size(fp);

    // Write backup boot sector and FS Info
    fseek(fp, 
          (ESP_lba + vbr->BPB_BkBootSec) * lba_size, 
          SEEK_SET);

    if (fwrite(vbr, 1, sizeof(Volume_Boot_Record), fp) != 
            sizeof(Volume_Boot_Record)) {
        fprintf(stderr, "Error: Could not write ESP "
                        "backup vbr\n");
        return false;
    }
    pad_out_to_full_lba_size(fp);

    if (fwrite(&fsinfo, 1, sizeof fsinfo, fp) 
            != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP "
                        "backup FS Info\n");
        return false;
    }
    pad_out_to_full_lba_size(fp);

    // Go to to Each FAT, and write the same clusters to mirror across all FATs 
    for (uint8_t i = 0; i < vbr->BPB_NumFATs; i++) {
        fseek(fp, 
              (ESP_lba + vbr->BPB_RsvdSecCnt + (i * vbr->BPB_FatSz32)) * lba_size,
              SEEK_SET);

        uint32_t cluster = 0x00000000;

        // FAT Cluster 0 = low 8 bits = media value, all
        //   other bits = 1
        cluster = 0x0FFFFF00 | vbr->BPB_Media;
        fwrite(&cluster, sizeof cluster, 1, fp);

        // Cluster 1 = EOC marker, all bits = 1
        cluster = 0x0FFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, fp);

        // Cluster 2 = root directory '/',
        //   only 1 cluster for <16 directory entries;
        //   Can use EOC marker
        cluster = 0x0FFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, fp);

        // Cluster 3 = '/EFI' subdirectory
        cluster = 0x0FFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, fp);

        // Cluster 4 = '/EFI/BOOT' subdirectory
        cluster = 0x0FFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, fp);

        // Cluster 5+ would be for other file/directory data
    }

    // Go to data region of FAT32, skip past rest of FATs
    const uint32_t data_lba = 
        (ESP_lba + vbr->BPB_RsvdSecCnt + 
        (vbr->BPB_NumFATs * vbr->BPB_FatSz32)); 

    fseek(fp, data_lba * lba_size, SEEK_SET);

    // Write data for root directory '/' ------------------------------
    // Directory entry for '/EFI'
    FAT_Dir_Entry dir_entry = {
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
        .DIR_FstClusLO = 3,     // EFI dir data is at clus 3
        .DIR_FileSize = 0,      // ATTR_DIRECTORY has file size = 0
    };

    uint16_t write_time = 0, write_date = 0;
    get_fat_dir_entry_date_time(&write_time, &write_date);

    dir_entry.DIR_CrtTime = write_time;
    dir_entry.DIR_CrtDate = write_date; 

    dir_entry.DIR_WrtTime = write_time;
    dir_entry.DIR_WrtDate = write_date;

    fwrite(&dir_entry, sizeof dir_entry, 1, fp);

    // Write data for '/EFI' directory  ------------------------------
    fseek(fp, (data_lba + 1) * lba_size, SEEK_SET);

    // '.' dir entry
    memcpy(dir_entry.DIR_Name, ".          ", 11);
    fwrite(&dir_entry, sizeof dir_entry, 1, fp);

    // '..' dir entry
    memcpy(dir_entry.DIR_Name, "..         ", 11);
    dir_entry.DIR_FstClusLO = 0;    // Parent is root '/' 
    fwrite(&dir_entry, sizeof dir_entry, 1, fp);

    // '/EFI/BOOT' dir entry
    memcpy(dir_entry.DIR_Name, "BOOT       ", 11);
    dir_entry.DIR_FstClusLO = 4;  
    fwrite(&dir_entry, sizeof dir_entry, 1, fp);

    // Write data for '/EFI/BOOT' directory ------------------------------
    fseek(fp, (data_lba + 2) * lba_size, SEEK_SET);

    // '.' dir entry
    memcpy(dir_entry.DIR_Name, ".          ", 11);
    fwrite(&dir_entry, sizeof dir_entry, 1, fp);

    // '..' dir entry
    memcpy(dir_entry.DIR_Name, "..         ", 11);
    dir_entry.DIR_FstClusLO = 3;    // Parent is '/EFI'
    fwrite(&dir_entry, sizeof dir_entry, 1, fp);

    return true;
}

// ========================================================
// Add input file to EFI System Partition
// ========================================================
// TODO: Create directories in path if not found?
bool add_file_to_esp(char *path, FILE *image, FILE *fp, Volume_Boot_Record vbr) {
    const uint64_t ESP_lba = PARTITION_ALIGNMENT / lba_size;    // Start of ESP
    const uint64_t FAT_lba = (ESP_lba + vbr.BPB_RsvdSecCnt);    // Start of FATs
    const uint32_t data_lba = (FAT_lba +                        // Start of Data region
                              (vbr.BPB_NumFATs * vbr.BPB_FatSz32)); 

    uint32_t last_cluster_found = 0;
    uint64_t file_size_lbas = 0;
    uint64_t file_size = 0; 
    
    // Get file size in sectors/clusters
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp); 
    file_size_lbas = bytes_to_lbas(file_size);
    rewind(fp);

    // Go to each FAT region in EFI System Partition
    for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++) {
        fseek(image, (FAT_lba + (i * vbr.BPB_FatSz32)) * lba_size, SEEK_SET);

        // Find first empty cluster, keeping a running total of # of clusters found
        uint32_t cluster = 0;
        last_cluster_found = 0; 
        fread(&cluster, sizeof cluster, 1, image);
        while (cluster != 0) {
            last_cluster_found++;
            fread(&cluster, sizeof cluster, 1, image);
        }

        // Back up before last cluster read (which is 0), in order to overwrite it
        fseek(image, -4, SEEK_CUR); 

        // Write new clusters for given file, each cluster until the EOC mark
        //   will contain the number of the next cluster of this file's data
        //  e.g. cluster 5 points to cluster 6, so it has the value 6
        cluster = last_cluster_found + 1;
        for (uint64_t j = 0; j < file_size_lbas; j++) { 
            fwrite(&cluster, sizeof cluster, 1, image);
            cluster++;
        }

        // Back up before last cluster read in order to write EOC mark
        fseek(image, -4, SEEK_CUR); 
        cluster = 0x0FFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);
    }

    // Parse input path, to get to correct directory for file in data region
    fseek(image, data_lba * lba_size, SEEK_SET);    // Go to data region

    if (path[0] != '/') return false;   // Path must start with root '/'

    // Get next directory if valid (ends with '/'), if no slash, then this is file to add
    char name[11];
    uint32_t i = 1; // Skip initial root '/' in path; path index
    uint32_t j = 0; // Name index
    bool found = false;
    FAT_Dir_Entry dir_entry = {0};
    while (true) {
        memset(name, ' ', sizeof name); // Clear next dir/file name
        for (j = 0; path[i] != '/' && path[i] != '\0'; i++, j++)
            name[j] = path[i];

        if (path[i] == '/') {
            // Found next directory name, look for this directory in
            //   the dir_entry's for the current directory
            fread(&dir_entry, sizeof dir_entry, 1, image);
            while (dir_entry.DIR_Name[0] != 0) {
                if (!memcmp(dir_entry.DIR_Name, name, sizeof dir_entry.DIR_Name)) {
                    // Found directory, go to its (FstClusHI | FstClusLO) value for data region's
                    //   sector on disk
                    uint32_t dir_lba = (dir_entry.DIR_FstClusHI << 16) | dir_entry.DIR_FstClusLO;
                    dir_lba -= 2;   // Translate to data sector, by removing first 2 reserved clusters

                    // Go to disk lba in data region for this directory to continue parsing path
                    fseek(image, (data_lba + dir_lba) * lba_size, SEEK_SET);
                    found = true;
                    break;
                } 
                fread(&dir_entry, sizeof dir_entry, 1, image);
            }
            if (!found) return false;   // Did not find next directory, error

        } else {
            // Found file name to add to this directory, done searching for directories
            break;
        }
        i++;    // Skip next '/' in path
    }

    // Write file data to data region for directory
    // Find next free spot in directory to write new dir_entry at
    fread(&dir_entry, sizeof dir_entry, 1, image);
    while (dir_entry.DIR_Name[0] != 0) {
        fread(&dir_entry, sizeof dir_entry, 1, image);
    }
    fseek(image, -32, SEEK_CUR);    // Back up, will overwrite this dir_entry

    // TODO: Rewrite this to be better & less bug prone for 8.3 file naming?
    // Write new dir_entry for file
    uint8_t k = 0, l = 0;   // k = dir entry name index, l = name index
    for (k = 0; k < 8 && name[k] != '.'; k++)
        dir_entry.DIR_Name[k] = name[k];

    l = k;  
    if (name[l] == '.') l++;

    // Pad out 8.3 name for (8) portion with spaces
    while (k < 8) dir_entry.DIR_Name[k++] = ' ';

    // Pad out rest of name with extension or spaces
    while (k < 11) 
        dir_entry.DIR_Name[k++] = name[l++];

    // Fill out rest of dir entry info for file
    dir_entry.DIR_Attr = 0;

    uint16_t write_time = 0, write_date = 0;
    get_fat_dir_entry_date_time(&write_time, &write_date);

    dir_entry.DIR_CrtTime = write_time;
    dir_entry.DIR_CrtDate = write_date; 

    dir_entry.DIR_WrtTime = write_time;
    dir_entry.DIR_WrtDate = write_date;

    dir_entry.DIR_FstClusHI = (last_cluster_found >> 16) & 0xFFFF; 
    dir_entry.DIR_FstClusLO = last_cluster_found & 0xFFFF; 
    dir_entry.DIR_FileSize = file_size;

    // Write new dir_entry for file
    fwrite(&dir_entry, sizeof dir_entry, 1, image);

    // Write file data to data region at specified cluster
    uint32_t file_lba = (dir_entry.DIR_FstClusHI << 16) | dir_entry.DIR_FstClusLO;
    file_lba -= 2;  // Remove first 2 reserved clusters
    fseek(image, (data_lba + file_lba) * lba_size, SEEK_SET);   // Go to sector/lba on disk

    uint8_t *file_data = malloc(lba_size);

    for (uint64_t lba = 0; lba < file_size_lbas; lba++) {
        uint64_t bytes_read = fread(file_data, 1, lba_size, fp);
        fwrite(file_data, 1, bytes_read, image);  
    }
    free(file_data);

    printf("Added file '%s'\n", path);

    return true;
}

// ========================================================
// Add input file to Basic Data Partition
// ========================================================
bool add_file_to_data_part(FILE *image, FILE *new_file) {
    const uint64_t align_lba = PARTITION_ALIGNMENT / lba_size;
    const uint64_t esp_size_lbas = bytes_to_lbas(esp_size);
    const uint64_t ESP_lba = align_lba;
    const uint64_t data_lba = next_aligned_lba(ESP_lba + esp_size_lbas);

    // Go to the data partition lba
    fseek(image, data_lba * lba_size, SEEK_SET);

    // Get file size
    uint64_t file_size_bytes, file_size_lbas;
    fseek(new_file, 0, SEEK_END);
    file_size_bytes = ftell(new_file);
    file_size_lbas = bytes_to_lbas(file_size_bytes);
    rewind(new_file);

    // Write file to data partition
    uint8_t *file_buf = calloc(1, lba_size);
    for (uint64_t i = 0; i < file_size_lbas; i++) {
        uint64_t bytes_read = fread(file_buf, 1, lba_size, new_file);   // Read from new file
        fwrite(file_buf, 1, bytes_read, image);                         // Write to disk image
    }
    free(file_buf);

    return true;
}

// ========================================================
// Get options/flags from user input command line flags
// ========================================================
bool getopts(int argc, char *argv[], Options *options) {
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-h",     strlen(argv[i])) ||
            !strncmp(argv[i], "--help", strlen(argv[i]))) {
            // Help text
            options->help = true;
            continue;
        }

        if (!strncmp(argv[i], "-es",        strlen(argv[i])) ||
            !strncmp(argv[i], "--esp-size", strlen(argv[i]))) {
            // Set efi system partition size in MiB
            i++;    // Get next arg
            options->esp_size = strtol(argv[i], NULL, 10);

            // Check against minimum FAT32 file system size 
            uint32_t min_size = 0;
            if (options->lba_size == 512)  min_size = 33;   
            if (options->lba_size == 1024) min_size = 65;
            if (options->lba_size == 2048) min_size = 129;
            if (options->lba_size == 4096) min_size = 257;

            if (options->esp_size < min_size) {
                fprintf(stderr, 
                        "Error: For LBA Size %" PRIu64 " EFI System Partition Must be "
                        "at least %uMiB\n",
                        lba_size, 
                        min_size);
                return false;
            }
            continue;
        }

        if (!strncmp(argv[i], "-ds",         strlen(argv[i])) ||
            !strncmp(argv[i], "--data-size", strlen(argv[i]))) {
            // Set basic data partition size in MiB
            i++;    // Get next arg
            options->data_size = strtol(argv[i], NULL, 10);

            if (options->data_size < 1) {
                fprintf(stderr, "Error: Basic Data Partition Must be at least 1MiB\n");
                return false;
            }
            continue;
        }

        if (!strncmp(argv[i], "-ls",        strlen(argv[i])) ||
            !strncmp(argv[i], "--lba-size", strlen(argv[i]))) {
            // Set lba size in bytes
            i++;    // Get next arg
            options->lba_size = strtol(argv[i], NULL, 10);

            if (options->lba_size != 512 &&
                options->lba_size != 1024 &&
                options->lba_size != 2048 &&
                options->lba_size != 4096) {

                fprintf(stderr, "Error: LBA Size must be 1 of 512/1024/2048/4096\n");
                return false;
            }

            // Check against minimum FAT32 file system size  
            if (options->esp_size != 0) {
                uint32_t min_size = 0;
                if (options->lba_size == 512)  min_size = 33;  
                if (options->lba_size == 1024) min_size = 65;
                if (options->lba_size == 2048) min_size = 129;
                if (options->lba_size == 4096) min_size = 257;

                if (options->esp_size < min_size) {
                    fprintf(stderr, 
                            "Error: For LBA Size %" PRIu64 " EFI System Partition Must be "
                            "at least %uMiB\n",
                            lba_size, 
                            min_size);
                    return false;
                }
            }
            continue;
        }

        if (!strncmp(argv[i], "-i",           strlen(argv[i])) ||
            !strncmp(argv[i], "--image-name", strlen(argv[i]))) {
            // Set image name
            i++;
            strncpy(options->image_name, argv[i], sizeof options->image_name - 1);
            continue;
        }

        if (!strncmp(argv[i], "-ae",           strlen(argv[i])) ||
            !strncmp(argv[i], "--add-esp-file", strlen(argv[i]))) {
            // Add file to the EFI System Partition
            i++;

            if (argv[i][0] == '\'' || argv[i][0] == '"') {
                // Copy between quotes, don't include them
                char *to_path = options->esp_path;
                char *from_path = &argv[i][1];
                while (*from_path != '\'' && *from_path != '"' && *from_path != '\0') 
                    *to_path++ = *from_path++;
            } else {
                strncpy(options->esp_path, argv[i], sizeof options->esp_path - 1);
            }
            continue;
        }

        if (!strncmp(argv[i], "-ad",             strlen(argv[i])) ||
            !strncmp(argv[i], "--add-data-file", strlen(argv[i]))) {
            // Add file to the Basic Data Partition
            i++;

            strncpy(options->data_file_name, argv[i], sizeof options->data_file_name - 1);
            continue;
        }

        if (!strncmp(argv[i], "-v",    strlen(argv[i])) ||
            !strncmp(argv[i], "--vhd", strlen(argv[i]))) {
            // Add fixed vhd footer
            options->vhd = true;
            continue;
        }
    }

    return true;
}

// ========================================================
// Add fixed vhd footer to disk image, making a .vhd file
// ========================================================
bool add_vhd(const uint64_t image_size, FILE *image) {
    // Everything is big endian (netword byte order),
    //   So I went the bad/lazy route and made it all
    //   byte arrays
    typedef struct {
        uint8_t cookie[8];
        uint8_t features[4];
        uint8_t file_format_version[4];
        uint8_t data_offset[8];
        uint8_t time_stamp[4];
        uint8_t creator_application[4];
        uint8_t creator_version[4];
        uint8_t creator_host_os[4];
        uint8_t original_size[8];
        uint8_t current_size[8];
        uint8_t disk_geometry[4];
        uint8_t disk_type[4];
        uint8_t checksum[4];
        EFI_GUID unique_id;
        uint8_t saved_state;
        uint8_t reserved[427];
    } __attribute__ ((packed)) Vhd;

    // Footer needs to be added to end of file
    fseek(image, image_size, SEEK_END);

    // Fill out VHD info
    Vhd vhd = {
        .cookie = { "QFWUZHER" },
        .features = { 0 },
        .file_format_version = { 0x00,0x01,0x00,0x00 },
        .data_offset = { 0xFF,0xFF,0xFF,0xFF },     // Fixed disk
        .time_stamp = { 0 },                        // # of seconds since jan 1st, 2000
        .creator_application = { "qfic" },
        .creator_version = { 0x00,0x01,0x00,0x00 },
        .creator_host_os = { 0x00,0xC0,0xFF, 0xEE },
        .original_size = { 0 },
        .current_size = { 0 },
        .disk_geometry = { 0 },                     // CHS value
        .disk_type = { 0x00, 0x00, 0x00, 0x02 },    // Fixed hard disk
        .checksum = { 0 },
        .unique_id = new_guid(),
        .saved_state = 0,                           // Not in saved state
        .reserved = {0},
    };

    // Magic constant for Unix epoch 2000: "946684800", subtract this number
    //   from time(NULL) value to get seconds since 2000, not 1970
    uint32_t time_2000 = time(NULL) - 946684800;

    vhd.time_stamp[0] = (time_2000 >> 24) & 0xFF;
    vhd.time_stamp[1] = (time_2000 >> 16) & 0xFF;
    vhd.time_stamp[2] = (time_2000 >>  8) & 0xFF;
    vhd.time_stamp[3] = time_2000 & 0xFF;

    vhd.original_size[0] = (image_size >> 56) & 0xFF;
    vhd.original_size[1] = (image_size >> 48) & 0xFF;
    vhd.original_size[2] = (image_size >> 40) & 0xFF;
    vhd.original_size[3] = (image_size >> 32) & 0xFF;
    vhd.original_size[4] = (image_size >> 24) & 0xFF;
    vhd.original_size[5] = (image_size >> 16) & 0xFF;
    vhd.original_size[6] = (image_size >>  8) & 0xFF;
    vhd.original_size[7] = image_size & 0xFF;

    memcpy(vhd.current_size, vhd.original_size, sizeof vhd.current_size);

    // Taken from vhd .doc
    uint32_t total_sectors = bytes_to_lbas(image_size);
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t cylinder_times_heads;

    if (total_sectors > 65535 * 16 * 255)
        total_sectors = 65535 * 16 * 255;

    if (total_sectors >= 65535 * 16 * 63) {
        sectors_per_track = 255;
        heads = 16;
        cylinder_times_heads = total_sectors / sectors_per_track;
    } else {
        sectors_per_track = 17;
        cylinder_times_heads = total_sectors / sectors_per_track;
        heads = (cylinder_times_heads + 1023) / 1024;

        if (heads > 4) heads = 4;

        if (cylinder_times_heads >= (heads * 1024) || heads > 16) {
            sectors_per_track = 31;
            heads = 16;
            cylinder_times_heads = total_sectors / sectors_per_track;
        }

        if (cylinder_times_heads >= (heads * 1024)) {
            sectors_per_track = 63;
            heads = 16;
            cylinder_times_heads = total_sectors / sectors_per_track;
        }
    }
    cylinders = cylinder_times_heads / heads;

    vhd.disk_geometry[0] = (cylinders >> 8) & 0xFF;
    vhd.disk_geometry[1] = cylinders & 0xFF;
    vhd.disk_geometry[2] = heads;
    vhd.disk_geometry[3] = sectors_per_track;

    // Calculate checksum
    uint32_t checksum = 0;
    uint8_t *footer = (uint8_t *)&vhd;
    for (uint32_t counter = 0; counter < sizeof vhd; counter++) {
        checksum += footer[counter];
    }
    checksum = ~checksum;

    vhd.checksum[0] = (checksum >> 24) & 0xFF;
    vhd.checksum[1] = (checksum >> 16) & 0xFF;
    vhd.checksum[2] = (checksum >>  8) & 0xFF;
    vhd.checksum[3] = checksum & 0xFF;

    fwrite(&vhd, sizeof vhd, 1, image);
    rewind(image);

    return true;
}

// ========================================================
// MAIN
// ========================================================
int main(int argc, char *argv[]) {
    char image_name[BUFSIZ] = "test.img";
    char file_name[BUFSIZ] = "BOOTX64.EFI";
    FILE *fp = NULL;

    // Get options/flags from user input command line flags
    Options options = {0};

    if (options.help) {
        // Print help text
        fprintf(stderr,
                "write_gpt [options]\n"
                "\n"
                "options:\n"
                "-h  --help             Print this help text\n"
                "-es --esp-size         Set the size of the EFI System Partition in MB\n"
                "-ds --data-size        Set the size of the Basic Data Partition in MiB; Minimum\n" 
                "                       size is 1MiB\n" 
                "-ls --lba-size         Set the lba (sector) size in bytes; This is considered\n"
                "                       experimental, as I lack tools for proper testing.\n"
                "                       Valid sizes: 512/1024/2048/4096\n" 
                "-i  --image-name       Set the image name. Default name is 'test.img'\n"
                "-ae --add-esp-file     Add a local file to the generated EFI System Partition.\n"
                "                       File path must be qualified and quoted, name length\n"
                "                       must be <= 8 characters, and file must be under root\n"
                "                       ('/'), '/EFI/', or '/EFI/BOOT/'\n"
                "                       example: -ae '/EFI/file1.txt' will add local file\n"
                "                       './file1.txt' as '/EFI/FILE1.TXT' in the ESP.\n"
                "-ad --add-data-file    Add a file to the basic data partition, and create a\n"
                "                       <FILENAME.INF> file under '/EFI/BOOT/' in the ESP.\n"
                "-v  --vhd              Create a fixed vhd footer, and add it to the end of the\n" 
                "                       disk image. The image name will have a .vhd suffix.\n");

        return EXIT_SUCCESS;
    }

    if (!getopts(argc, argv, &options)) 
        return EXIT_FAILURE;

    if (options.lba_size != 0) {
        lba_size = options.lba_size;
    } 

    if (options.esp_size != 0) {
        // Set efi_size in MiB
        esp_size = options.esp_size * MEGABYTE_SIZE;
    } else { 
        // Use minimum esp sizes
        if (lba_size == 512)  esp_size = 33 * MEGABYTE_SIZE;
        if (lba_size == 1024) esp_size = 65 * MEGABYTE_SIZE;
        if (lba_size == 2048) esp_size = 129 * MEGABYTE_SIZE;
        if (lba_size == 4096) esp_size = 257 * MEGABYTE_SIZE;
    }

    if (options.data_size != 0) {
        // Set basic_data_size in MiB, minimum is 1MiB
        basic_data_size = options.data_size * MEGABYTE_SIZE;
    }

    if (options.image_name[0] != '\0') {
        strcpy(image_name, options.image_name);
    }

    // Set image size; add padding at end of image to ensure
    //   data partition is not overwritten by 2nd GPT
    // Padding = 3 lbas for MBR & 2 GPT headers, 2 GPT tables, and 2MiB for 
    //   aligning the ESP and data partitions on 1MiB
    const uint64_t padding = 
        ((3 + ((PARTITION_ENTRY_ARRAY_SIZE / lba_size) * 2)) * lba_size) + 
        PARTITION_ALIGNMENT*2; 
    image_size = esp_size + basic_data_size + padding;  

    if (options.vhd) {
        // Add .vhd suffix to image name
        char *pos = strrchr(image_name, '.');
        if (!pos) {
            // Concat .inf to end of name
            pos = file_name + strlen(file_name);
        } 
         
        *pos = '\0';
        strcat(pos, ".vhd");    // Add new extension

        printf("Adding fixed VHD footer\n");

        // Add fixed vhd footer to end of image
        // Open file under new .vhd name
        fp = fopen(image_name, "wb+");
        if (!fp) {
            fprintf(stderr, "Error: Could not open file %s\n", 
                    image_name);
            return EXIT_FAILURE;
        }

        image_size += 512;
        if (!add_vhd(image_size, fp)) {
            fprintf(stderr, "Error: Could not add vhd footer to %s\n", image_name);
            return EXIT_FAILURE;
        }
    } else {
        // Open file under normal image name
        fp = fopen(image_name, "wb+");
        if (!fp) {
            fprintf(stderr, "Error: Could not open file %s\n", 
                    image_name);
            return EXIT_FAILURE;
        }
    }

    // Print image info 
    printf("Image Name: %s\n"
           "LBA Size: %" PRIu64 "\n"
           "EFI System Partition Size: %luMiB\n"
           "Basic Data Partition Size: %luMiB\n"
           "Extra padding for MBR/GPT Headers/GPT Tables: %luMiB\n" 
           "Total Image Size: %luMiB\n",
           image_name,
           lba_size,
           esp_size / MEGABYTE_SIZE, 
           basic_data_size / MEGABYTE_SIZE, 
           padding / MEGABYTE_SIZE,  
           image_size / MEGABYTE_SIZE
           );

    // Seed random number generation from current time
    srand(time(NULL));

    // Write protective MBR for LBA 0
    if (!write_protective_mbr(fp)) {
        fprintf(stderr, "Error: Could not write protective MBR\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    // Write GPT headers and partition entry arrays 
    if (!write_gpts(fp)) {
        fprintf(stderr, "Error: Could not write GPT headers and "
                "tables\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    // Write EFI System Partition (ESP) with 
    //   FAT32 filesystem for file/folder structure of
    //   '/EFI/BOOT/'
    Volume_Boot_Record vbr = {0};
    if (!write_esp(fp, &vbr)) {
        fprintf(stderr, "Error: Could not write EFI System " 
                        "Partition\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    // If BOOTX64.EFI file is found in current directory, then
    //  add it automatically to the ESP
    // '/EFI/BOOT/BOOTX64.EFI'
    FILE *new_file = fopen(file_name, "rb");
    char path[BUFSIZ] = "/EFI/BOOT/BOOTX64.EFI";
    if (new_file) {
        if (!add_file_to_esp(path, fp, new_file, vbr)) {
            fprintf(stderr, "Error: Could not auto add %s"
                            "to ESP\n", file_name);
            fclose(new_file);
            return EXIT_FAILURE;
        }
        fclose(new_file);
    }

    // Create a disk image info file, for now only include size of disk image
    strcpy(file_name, "dskimg.inf");
    new_file = fopen(file_name, "w+");
    strcpy(path, "/EFI/BOOT/DSKIMG.INF");
    char *buf = malloc(lba_size); 
    memset(buf, 0, sizeof lba_size);  
    
    sprintf(buf, "DISK_SIZE=%lu\n", image_size);
    fwrite(buf, 1, lba_size, new_file);
    rewind(new_file);
    free(buf);

    if (!add_file_to_esp(path, fp, new_file, vbr)) {
        fprintf(stderr, "Error: Could not add '%s'\n", file_name);
        fclose(new_file);
    }

    if (options.esp_path[0] != '\0') {
        // Add user specified file (in path) to EFI System Partition
        // Get file name from path
        strcpy(path, options.esp_path); 
        memset(file_name, 0, sizeof file_name);
        char *pos = strrchr(path, '/');
        if (path[0] != '/' || !pos) {
            fprintf(stderr, 
                    "Error: Path %s needs to have at least 1 starting slash '/'\n",
                    path);
            return EXIT_FAILURE;
        }
            
        pos++;   // Skip '/'
        strncpy(file_name, pos, sizeof file_name - 1);

        new_file = fopen(file_name, "rb");
        if (!new_file) {
            fprintf(stderr, "Error: Could not open file %s\n", file_name);
            return EXIT_FAILURE;
        }

        if (!add_file_to_esp(path, fp, new_file, vbr)) {
            fprintf(stderr, "Error: Could not add %s to ESP", file_name);
            fclose(new_file);
            return EXIT_FAILURE;
        }
        fclose(new_file);
    }

    if (options.data_file_name[0] != '\0') {
        // Add user specified file (in path) to EFI System Partition
        // Get file name from option path
        strncpy(file_name, options.data_file_name, sizeof file_name);

        new_file = fopen(file_name, "rb");
        if (!new_file) {
            fprintf(stderr, "Error: Could not open file %s\n", file_name);
            return EXIT_FAILURE;
        }

        // Get file size
        uint64_t file_size_bytes;
        fseek(new_file, 0, SEEK_END);
        file_size_bytes = ftell(new_file);
        rewind(new_file);

        printf("Adding file '%s' to data partition\n", file_name);

        if (!add_file_to_data_part(fp, new_file)) {
            fprintf(stderr, "Error: Could not add %s to Basic Data Partition", file_name);
            fclose(new_file);
            return EXIT_FAILURE;
        }
        fclose(new_file);

        // Add file info .inf file similar to dskimg.inf to hold information for
        //   EFI applications later
        // Replace file name extension with .inf 
        // TODO: Add check for filename length before extension <= 8
        char *pos = strrchr(file_name, '.');
        if (!pos) {
            // Concat .inf to end of name
            pos = file_name + strlen(file_name);
        } 
         
        *pos = '\0';
        strcat(pos, ".inf");    // Add new extension

        for (size_t i = 0; i < strlen(file_name); i++) 
            file_name[i] = toupper(file_name[i]);

        new_file = fopen(file_name, "w+");
        strcpy(path, "/EFI/BOOT/");
        strcat(path, file_name);

        char *buf = malloc(lba_size); 
        memset(buf, 0, sizeof lba_size);  

        const uint64_t align_lba = PARTITION_ALIGNMENT / lba_size;
        const uint64_t esp_size_lbas = bytes_to_lbas(esp_size);
        const uint64_t ESP_lba = align_lba;
        const uint64_t data_lba = next_aligned_lba(ESP_lba + esp_size_lbas);

        sprintf(buf, 
                "FILE_SIZE=%lu\n"
                "DISK_LBA=%lu\n",
                file_size_bytes,
                data_lba);

        fwrite(buf, 1, lba_size, new_file);
        rewind(new_file);
        free(buf);

        if (!add_file_to_esp(path, fp, new_file, vbr)) {
            fprintf(stderr, "Error: Could not add '%s'\n", file_name);
            fclose(new_file);
        }
        fclose(new_file);
    }

    // Cleanup
    fclose(fp);

    return EXIT_SUCCESS;
}
