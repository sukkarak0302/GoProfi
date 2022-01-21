/*
 * Camera.c
 *
 *  Created on: 2021. 11. 14.
 *      Author: Gyuhyun_Cho
 */

#include "GoProfi_Camera.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "freertos/FreeRTOS.h"

#include "esp_spiffs.h"

#include <math.h>

#include <stdio.h>
#include <time.h>
#include <string.h>

#define MOUNT_POINT "/sdcard"
static const char *TAG = "CAMERA";

#define HEADER "HDR"

//PIN MAP for ESP32-CAM
#define CAM_PIN_PWDN    32 // power down is not used
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#define MAX_FILE_NAME_LENGTH 50

static camera_fb_t * fb;
static sdmmc_card_t *card;

static struct tm *current_time;
static time_t raw_time;
static clock_t ms_0;
static clock_t ms_start;
static clock_t ms_end;

static int pic_delay_ms = 25;

static FILE *f = NULL;
static FILE *f_meta = NULL;
extern unsigned int tot_kb = 0;
extern unsigned int free_kb = 0;

static int can_close = 0;
static int f_close_complete = 0;

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 16000000,//EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_SVGA,//FRAMESIZE_QVGA,//FRAMESIZE_SXGA,//FRAMESIZE_SVGA,//FRAMESIZE_SVGA 12,//QQVGA-QXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1, //if more than one, i2s runs in continuous mode. Use only with JPEG
	.fb_location = CAMERA_FB_IN_DRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY //. Sets when buffers should be filled
};


/*
 * CAMERA Related functions
 */
esp_err_t camera_init()
{
    //initialize the camera
    esp_err_t err_cam = esp_camera_init(&camera_config);
	esp_err_t err_sd = init_sd();

    if (err_cam != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err_cam;
    }
    if (err_sd != ESP_OK)
    {
        ESP_LOGE(TAG, "SD Failed");
        return err_sd;
    }
    ESP_LOGI(TAG, "Initialize Successful!");
    return ESP_OK;
}

void camera_capture_task()
{
	int cur_frame = 1;

	char write_buf[9];
	char write_tot_buf[33];

	ms_0 = clock();

	while( 1 )
	{
		ms_start = clock();
		if( camera_capture()==1 )
		{
			//File No
			ESP_LOGI(TAG, "BEFORE");
			intToChar_ret(cur_frame,8,write_buf);
			for(int i = 0 ; i < 8 ; i++) write_tot_buf[i] = write_buf[i];
			memset(write_buf, 0, 9);

			//File Size
			intToChar_ret(fb->len,8,write_buf);
			for(int i = 8 ; i < 16 ; i++) write_tot_buf[i] = write_buf[i-8];
			memset(write_buf, 0, 9);

			//ms
			intToChar_ret((ms_start-ms_0),8,write_buf);
			for(int i = 16 ; i < 24 ; i++) write_tot_buf[i] = write_buf[i-16];
			memset(write_buf, 0, 9);

			intToChar_ret(0,6,write_buf);
			for(int i = 24 ; i < 30 ; i++) write_tot_buf[i] = '0';
			write_tot_buf[30] = 0x0D;
			write_tot_buf[31] = 0x0A;
			ESP_LOGI(TAG,"%s", write_tot_buf);

			can_close = 0;
			if( f != NULL && f_close_complete == 0 )
			{
				if( fwrite(fb->buf, fb->len, 1, f) != 1 )
					ESP_LOGI(TAG, "CAM_WRITE ERROR!");
				else
					ESP_LOGI(TAG,"frame %d!", cur_frame);
			}

			if( f_meta != NULL && f_close_complete == 0 )
			{
				if( fwrite(write_tot_buf, 32, 1, f_meta) != 1 )
					ESP_LOGI(TAG, "File_different!");
				else
					ESP_LOGI(TAG, "Meta_successful!");
			}
			memset(write_buf, 0, 9);
			memset(write_tot_buf, 0, 32);
			can_close = 1;
		}
		else
		{
			ESP_LOGE(TAG,"frame error : %d!", cur_frame);
		}

	cur_frame++;
	esp_camera_fb_return(fb);
	ms_end = clock();
	ESP_LOGE(TAG, "fps : %f", 1/((double)(ms_end-ms_start)/1000));

	vTaskDelay(pic_delay_ms);
	}
}

int camera_capture(){
    fb = esp_camera_fb_get();

    if (!fb) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        return 0;
    }

    return 1;
}


/*
 * SD Card related
 */
esp_err_t init_sd()
{
	esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 4,
        .allocation_unit_size = 16*1024
    };

    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // To use 1-line SD mode, change this to 1:
    slot_config.width = 4;

    // On chips where the GPIOs used for SD card can be configured, set them in
    // the slot_config structure:
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = GPIO_NUM_14;
    slot_config.cmd = GPIO_NUM_15;
    slot_config.d0 = GPIO_NUM_2;
    slot_config.d1 = GPIO_NUM_4;
    slot_config.d2 = GPIO_NUM_12;
    slot_config.d3 = GPIO_NUM_13;
#endif

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    //slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // sd card space
    FATFS *fs;
    size_t fre_clust;
    size_t tot_sect;
    size_t fre_sect;

    f_getfree("0:", &fre_clust, &fs);
    tot_sect = (fs->n_fatent -2) * fs->csize;
    fre_sect = (fre_clust * fs->csize);
    tot_kb = tot_sect * fs->ssize / 1024;
    free_kb = fre_sect * fs->ssize / 1024;

    ESP_LOGI(TAG, "%d KiB total drive space.\n%d KiB available.\n", tot_kb, free_kb);

    return ret;
}

int sd_unmount()
{
    if( esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card) == ESP_OK )
    {
    	ESP_LOGI(TAG, "Card unmounted");
    	return 1;
    }
    else
    {
    	ESP_LOGI(TAG, "Card unmounted unsuccessful!");
    	return 0;
    }
}


/*
 * Output file related
 */
int camera_filegen()
{
	char file_path[MAX_FILE_NAME_LENGTH];
	char file_path_meta[MAX_FILE_NAME_LENGTH];

	//DEBUG
	ESP_LOGI(TAG, "Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

	filename_gen(file_path);
	ESP_LOGE(TAG, "Fname : %s", file_path);

	ESP_LOGE(TAG,"Camera main started-before!");
	f = fopen(file_path, "w");
	if (f == NULL)
	{
	    ESP_LOGE(TAG, "Failed to open file for writing");
	    return 0;
	}

	filename_gen_meta(file_path_meta);
	f_meta = fopen(file_path_meta, "w");

	if (f_meta == NULL)
	{
	    ESP_LOGE(TAG, "Failed to open file for writing");
	    return 0;
	}

	f_close_complete = 0;
	ESP_LOGI(TAG, "File Opened!");

	return 1;
}

void filename_gen(char* f_name)
{
	raw_time = time(NULL);
	current_time = localtime(&raw_time);

	sprintf(f_name, "/sdcard/M%d_%d_%d_%d-%d-%d.mjpeg", current_time->tm_year+1900, current_time->tm_mon+1, current_time->tm_mday, current_time->tm_hour, current_time->tm_min, current_time->tm_sec);
	ESP_LOGI(TAG, "%s",f_name);
}

void filename_gen_meta(char* f_name)
{
	sprintf(f_name, "/sdcard/M%d_%d_%d_%d-%d-%d_m.txt", current_time->tm_year+1900, current_time->tm_mon+1, current_time->tm_mday, current_time->tm_hour, current_time->tm_min, current_time->tm_sec);
	ESP_LOGI(TAG, "%s",f_name);
}


int camera_fileclose()
{
	esp_camera_fb_return(fb);
	int trial = 0;

	while(1)
	{
		trial++;
		if( f != NULL )
		{
			ESP_LOGE(TAG, "F != NULL");

			if( fclose(f) == 0 )
			{
				ESP_LOGE(TAG, "file close successful. cam");
				f = NULL;
			}
		}

		if( f_meta != NULL )
		{
			ESP_LOGE(TAG, "f_meta != NULL");

			if( fclose(f_meta) == 0 )
			{
				ESP_LOGE(TAG, "file close successful. meta");
				f_meta = NULL;
			}
		}

		if ( ( f == NULL && f_meta == NULL ) )
		{
			ESP_LOGE(TAG, "Both file closed");
			return 1;
		}

		if ( trial > 3 )
		{
			ESP_LOGE(TAG, "Trial limit over");
			return 0;
		}
	}

	f_close_complete = 1;
	ESP_LOGE(TAG, "File close failed");
	return 0;
}


/*
 * Setter, getter functions
 */
int cam_config_size(int val)
{
	if( esp_camera_deinit() == ESP_OK )
	{
		framesize_t pic_size;
		switch (val)
		{
			case 1:
				pic_size = FRAMESIZE_QVGA;
				break;
			case 2:
				pic_size = FRAMESIZE_VGA;
				break;
			case 3:
				pic_size = FRAMESIZE_SVGA;
				break;
			case 4:
				pic_size = FRAMESIZE_XGA;
				break;
			case 5:
				pic_size = FRAMESIZE_SXGA;
				break;
			case 6:
				pic_size = FRAMESIZE_UXGA;
				break;
			default:
				pic_size = FRAMESIZE_QVGA;
		}
		camera_config.frame_size = pic_size;
		if ( esp_camera_init(&camera_config) == ESP_OK )
		{
			return pic_size;
		}
		else
		{
			camera_config.frame_size = FRAMESIZE_QVGA;
			if ( esp_camera_deinit() == ESP_OK )
			{
				if ( esp_camera_init(&camera_config) == ESP_OK )
					return FRAMESIZE_QVGA;
			}
		}
	}
	return 0;
}

int cam_config_fps(int val)
{
	pic_delay_ms = (int)(1000/val);
	return 1;
}

int cam_config_quality(int val)
{
	if( esp_camera_deinit() == ESP_OK )
	{
		camera_config.jpeg_quality = val;
		if ( esp_camera_init(&camera_config) == ESP_OK )
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
		return 0;
}

int get_cam_config_fps()
{
	return (int)(1000/pic_delay_ms);
}

int get_cam_config_quality()
{
	return camera_config.jpeg_quality;
}

int get_cam_config_size()
{
	switch (camera_config.frame_size)
	{
		case 5 :
			return 1;
		case 8 :
			return 2;
		case 9 :
			return 3;
		case 10 :
			return 4;
		case 12 :
			return 5;
		case 13 :
			return 6;
		default :
			return 1;
	}
}

/*
 * Helper function
 */
void intToChar_ret(int InVal, int size, char* OutVal)
{
	for ( int i = 0 ; i < size; i++ )
	{
		OutVal[i] = ( InVal / (int)(pow(10, (size-i-1) ) ) ) % 10 + '0';
	}
}
