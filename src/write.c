#include "write.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <zephyr/drivers/uart.h>

#include "heat_sensor.h"

LOG_MODULE_REGISTER(write_thread, LOG_LEVEL_INF);

#if defined(CONFIG_FAT_FILESYSTEM_ELM)

#include <ff.h>

/*
 *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
 *  in ffconf.h
 */
#if defined(CONFIG_DISK_DRIVER_MMC)
#define DISK_DRIVE_NAME "SD2"
#else
#define DISK_DRIVE_NAME "SD"
#endif

#define DISK_MOUNT_PT "/"DISK_DRIVE_NAME":"

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};

#elif defined(CONFIG_FILE_SYSTEM_EXT2)

#include <zephyr/fs/ext2.h>

#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/ext"

static struct fs_mount_t mp = {
    .type = FS_EXT2,
    .flags = FS_MOUNT_FLAG_NO_FORMAT,
    .storage_dev = (void *)DISK_DRIVE_NAME,
    .mnt_point = "/ext",
};

#endif

#if defined(CONFIG_FAT_FILESYSTEM_ELM)
#define FS_RET_OK FR_OK
#else
#define FS_RET_OK 0
#endif


#define MAX_PATH 128
#define SOME_FILE_NAME "some.dat"
#define SOME_DIR_NAME "some"
#define SOME_REQUIRED_LEN MAX(sizeof(SOME_FILE_NAME), sizeof(SOME_DIR_NAME))


static bool write_to_file(const char *base_path, struct heat_measure_t *data)
{
    char path[MAX_PATH];
    struct fs_file_t file;
    int base = strlen(base_path);

    fs_file_t_init(&file);


    strncpy(path, base_path, sizeof(path));

    path[base++] = '/';
    path[base] = 0;
    strcat(&path[base], SOME_FILE_NAME);


    if (fs_open(&file, path, FS_O_CREATE | FS_O_RDWR | FS_O_APPEND) != 0) {
        LOG_ERR("Failed to open file %s", path);
        return false;
    }
    char buf[128];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d - %"PRId32" uV\n",
            data->time.tm_year + 1900,
            data->time.tm_mon + 1,
            data->time.tm_mday,
            data->time.tm_hour,
            data->time.tm_min,
            data->time.tm_sec,
            data->uv);
    int ret = fs_write(&file, buf, strlen(buf));
    if (ret < 0) {
        LOG_ERR("FAIL: write %s: %d", path, ret);
    }

    if (fs_close(&file)) {
        LOG_ERR("Failed to open file %s", path);
        return false;
    }
    return true;
}


static const char *disk_mount_pt = DISK_MOUNT_PT;
void write_thread(void *p1, void *p2, void *p3) {

    struct k_fifo *ht_fifo = (struct k_fifo*) p1;

    k_sleep(K_SECONDS(1));
    /* raw disk i/o */
    do {
        static const char *disk_pdrv = DISK_DRIVE_NAME;
        uint64_t memory_size_mb;
        uint32_t block_count;
        uint32_t block_size;

        if (disk_access_ioctl(disk_pdrv,
            DISK_IOCTL_CTRL_INIT, NULL) != 0) {
            LOG_ERR("Storage init ERROR!");
        break;
            }

            if (disk_access_ioctl(disk_pdrv,
                DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
                LOG_ERR("Unable to get sector count");
            break;
                }
                LOG_INF("Block count %u", block_count);

                if (disk_access_ioctl(disk_pdrv,
                    DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
                    LOG_ERR("Unable to get sector size");
                break;
                    }
                    LOG_PRINTK("Sector size %u\n", block_size);

                    memory_size_mb = (uint64_t)block_count * block_size;
                    LOG_PRINTK("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));

                    if (disk_access_ioctl(disk_pdrv,
                        DISK_IOCTL_CTRL_DEINIT, NULL) != 0) {
                        LOG_ERR("Storage deinit ERROR!");
                    break;
                        }
    } while (0);

    mp.mnt_point = disk_mount_pt;

    int res = fs_mount(&mp);
    if (res != FS_RET_OK) {
        LOG_PRINTK("Error mounting disk.\n");
        k_sleep(K_SECONDS(1));
    }
    while (1) {
        /* Try to unmount and remount the disk */
        if (!k_fifo_is_empty(ht_fifo)) {
            struct heat_measure_t *data = NULL;
            data = k_fifo_get(ht_fifo, K_MSEC(100));
            if (data != NULL) {
                bool has_written = write_to_file(disk_mount_pt, data);
                if (!has_written) {
                    LOG_ERR("failed !");
                }
            }
        }
        k_sleep(K_MSEC(100));
    }

    fs_unmount(&mp);
}

