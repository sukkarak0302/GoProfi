/*
 * Wifi.h
 *
 *  Created on: 2021. 11. 14.
 *      Author: Gyuhyun_Cho
 */

#ifndef MAIN_GOPROFI_WIFI_H_
#define MAIN_GOPROFI_WIFI_H_

typedef struct  {
		int			 	sys_time_1;
		int			 	sys_time_2;
		int 			sys_size;
		int 			sys_fps;
		int				sys_quality;
		int				sys_vflip;
		int				sys_hflip;
}sys_config_struct;

extern int State_Request;

int Wifi_init();
void Wifi_main();
void Wifi_control_main();
int Wifi_stop();

int start_webserver();
int stop_webserver();

int start_control_server();
int stop_control_server();

int charToInt(char*, int);

int sys_config_time1(int val);
int sys_config_time2(int val);
void get_sys_configValue(sys_config_struct*);

void State_Write(int);
int State_Read();

#endif /* MAIN_GOPROFI_WIFI_H_ */
