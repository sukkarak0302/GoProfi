/*
 * Camera.h
 *
 *  Created on: 2021. 11. 14.
 *      Author: Gyuhyun_Cho
 */

#ifndef MAIN_GOPROFI_CAMERA_H_
#define MAIN_GOPROFI_CAMERA_H_

#include "esp_system.h"

int camera_init();
void camera_capture_task();
int camera_capture();

esp_err_t init_sd();
int sd_unmount();

int camera_filegen();
void filename_gen(char*);
void filename_gen_meta(char*);
int camera_fileclose();

int cam_config_fps(int);
int cam_config_quality(int);
int cam_config_size(int);
int get_cam_config_fps();
int get_cam_config_size();
int get_cam_config_quality();

extern unsigned int tot_kb;
extern unsigned int free_kb;

void intToChar_ret(int, int, char*);

#endif /* MAIN_GOPROFI_CAMERA_H_ */
