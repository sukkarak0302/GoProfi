/*
 * Camera.h
 *
 *  Created on: 2021. 11. 14.
 *      Author: Gyuhyun_Cho
 */

#ifndef MAIN_GOPROFI_CAMERA_H_
#define MAIN_GOPROFI_CAMERA_H_

#include "esp_system.h"

esp_err_t camera_init();
void camera_capture_task();
int camera_capture();

esp_err_t init_sd();
int sd_unmount();

int camera_filegen();
void filename_gen(char*);
void camera_fileclose();

int cam_config_fps(int);
int cam_config_quality(int);
int cam_config_size(int);
int get_cam_config_fps();
int get_cam_config_size();
int get_cam_config_quality();

extern unsigned int tot_kb;
extern unsigned int free_kb;

#endif /* MAIN_GOPROFI_CAMERA_H_ */
