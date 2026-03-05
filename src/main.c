#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/printk.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/cbprintf.h>
#include <zephyr/drivers/spi.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define SLEEP_TIME_MS 5000

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

#define FS_RET_OK FR_OK

#endif

#define MAX_PATH 128
#define SOME_FILE_NAME "myfile.dat"
#define SOME_DIR_NAME "some"
#define SOME_REQUIRED_LEN MAX(sizeof(SOME_FILE_NAME), sizeof(SOME_DIR_NAME))

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

static const struct gpio_dt_spec led_ok = GPIO_DT_SPEC_GET(DT_ALIAS(ledok), gpios);
static const struct gpio_dt_spec led_busy = GPIO_DT_SPEC_GET(DT_ALIAS(ledbusy), gpios);

static const struct adc_dt_spec fluxsensor = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

//static const struct adc_dt_spec fluxsensor = //ADC_DT_SPEC_GET_BY_IDX(DT_ALIAS(flux_sensor), 0);

int init_leds() {
	int ret;

	if (!gpio_is_ready_dt(&led_ok) || !gpio_is_ready_dt(&led_busy)) {
		LOG_ERR("LEDS not ready");
		return -1;
	}
	ret = gpio_pin_configure_dt(&led_ok, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Cannot set led_ok");
		return ret;
	}
	ret = gpio_pin_configure_dt(&led_busy, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Cannot set led_busy");
		return ret;
	}
	LOG_INF("Leds configured");
	return 0;
}

int init_adc() {
	/* Configure channels individually prior to sampling. */
	if (!adc_is_ready_dt(&fluxsensor)) {
		LOG_ERR("ADC controller device %s not ready\n", fluxsensor.dev->name);
		return -1;
	}
	int err = adc_channel_setup_dt(&fluxsensor);
	if (err < 0) {
		LOG_ERR("Could not setup channel %d\n", err);
		return err;
	}
	return 0;
}

static const char *disk_mount_pt = DISK_MOUNT_PT;

int init_disk() {
	do {
		static const char *disk_pdrv = DISK_DRIVE_NAME;
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;
		LOG_PRINTK("PT1");
		k_sleep(K_SECONDS(1));

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_CTRL_INIT, NULL) != 0) {
			LOG_ERR("Storage init ERROR!");
			return -1;
		}

		LOG_PRINTK("PT2");
		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
			LOG_ERR("Unable to get sector count");
			break;
		}
		LOG_INF("Block count %u", block_count);

		LOG_PRINTK("PT3");
		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
			LOG_ERR("Unable to get sector size");
			break;
		}
		printk("Sector size %u\n", block_size);
		LOG_PRINTK("PT4");

		memory_size_mb = (uint64_t)block_count * block_size;
		printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
		LOG_PRINTK("PT5");

		if (disk_access_ioctl(disk_pdrv,
			DISK_IOCTL_CTRL_DEINIT, NULL) != 0) {
			LOG_ERR("Storage deinit ERROR!");
			break;
		}
	} while (0);
	return 0;
}

int main(void)
{
	int ret;
	bool led_state = true;

	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};
	ret = adc_sequence_init_dt(&fluxsensor, &sequence);

	LOG_INF("Initialization console");
	/* Poll if the DTR flag was set */
	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		LOG_INF("Initialization console in progress");
		/* Give CPU resources to low priority threads. */
		k_sleep(K_MSEC(100));
	}
	LOG_PRINTK("Console ready !\n");

	k_sleep(K_SECONDS(1));

	k_sleep(K_SECONDS(1));

	LOG_INF("Initalize disk");
	do {
		static const char *disk_pdrv = DISK_DRIVE_NAME;
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;
		LOG_PRINTK("PT1");
		k_sleep(K_SECONDS(1));

		if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_CTRL_INIT, NULL) != 0) {
			LOG_ERR("Storage init ERROR!");
			break;
		}

		LOG_PRINTK("PT2");
		if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT,
								&block_count)) {
			LOG_ERR("Unable to get sector count");
			break;
		}
		LOG_INF("Block count %u", block_count);

		LOG_PRINTK("PT3");
		if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE,
								&block_size)) {
			LOG_ERR("Unable to get sector size");
			break;
		}
		printk("Sector size %u\n", block_size);
		LOG_PRINTK("PT4");

		memory_size_mb = (uint64_t)block_count * block_size;
		printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
		LOG_PRINTK("PT5");

		if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_CTRL_DEINIT, NULL) != 0) {
			LOG_ERR("Storage deinit ERROR!");
			break;
		}
        } while (0);


	ret = init_disk();
	if (ret <0) {
		LOG_ERR("Error accessing disk");
		k_sleep(K_MSEC(1000));
		return 0;
	}
	printk("After init\n");
	k_sleep(K_SECONDS(2));
	mp.mnt_point = disk_mount_pt;
	ret = fs_mount(&mp);
	if (ret == FS_RET_OK) {
		LOG_INF("Disk mounted.\n");
	}
	else {
		LOG_ERR("Error mounting disk - aborting");
		k_sleep(K_MSEC(1000));
		//return 0;
	}

	LOG_PRINTK("ALL good");

	k_sleep(K_MSEC(100));

	if (init_adc() < 0) {
		LOG_ERR("ADC Not ready, aborting");
		//return 0;
	}

	if (init_leds() < 0) {
		LOG_ERR("LEDS Not ready, aborting");
		//return 0;
	}


	while (1) {
		ret = gpio_pin_toggle_dt(&led_ok);
		if (ret < 0) {
			return 0;
		}
		ret = gpio_pin_toggle_dt(&led_busy);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		//LOG_PRINTK("LED state: %s\n", led_state ? "ON" : "OFF");
		LOG_PRINTK("start read");
		ret = adc_read_dt(&fluxsensor, &sequence);
		if (ret < 0) {
			LOG_ERR("Could not read channel %d\n", ret);
		}
		int32_t val_uv = (int32_t)((int16_t)buf);
		ret = adc_raw_to_microvolts_dt(&fluxsensor, &val_uv);
		/* conversion to mV may not be supported, skip if not */
		if (ret < 0) {
			LOG_ERR(" (value in mV not available)\n");
		} else {
			LOG_PRINTK(" = %"PRId32" uV\n", val_uv);
		}

		char path[MAX_PATH];
		struct fs_file_t file;
		int base = strlen(disk_mount_pt);
		fs_file_t_init(&file);
		LOG_INF("Creating some dir entries in %s", disk_mount_pt);
		strncpy(path, disk_mount_pt, sizeof(path));
		path[base++] = '/';
		path[base] = 0;
		strcat(&path[base], SOME_FILE_NAME);

		char sbuf[128];
		if (fs_open(&file, path, FS_O_CREATE | FS_O_APPEND) != 0) {
			LOG_ERR("Failed to open file %s", path);
			return false;
		}
		snprintfcb(sbuf, 128, "uV = %"PRId32" uV\n", val_uv);
		fs_write(&file, &sbuf, strlen(sbuf));
		fs_close(&file);


		ret = gpio_pin_toggle_dt(&led_ok);
		if (ret < 0) {
			return 0;
		}
		ret = gpio_pin_toggle_dt(&led_busy);
		if (ret < 0) {
			return 0;
		}

		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
