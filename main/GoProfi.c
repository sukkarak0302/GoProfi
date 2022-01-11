/* Go Profi main

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   Control flow
   Wifi (2) <--(Button input)--> Recording state(3)

   1. Wifi(Config/Download)(2) : Default state when power is on
   	   	   	   	   - Waiting for user input or searching for cellphone to be connected
   	   	   	   	   - Configuration of system/camera recording setting
   	   	   	   	   - Offer file explorer and player

   2. Recording state(3) : Recording according to configuration setting, Check the free space every 1 minute.
   	   	   	   	   	   	- LED Red (Blinking)
   	   	   	   	   	   	- Recording and save
*/
#include <stdio.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "GoProfi_Camera.h"
#include "GoProfi_Wifi.h"
#include "GoProfi_Button.h"
#include "MainParameter.h"

void mainRoutine();

bool Initialize();

static int State = 2;
static int Initial_Run = 1;

static const char *TAG = "Main";

void app_main(void)
{
	ESP_LOGV(TAG, "Start");

    BaseType_t main_State;
    TaskHandle_t main_Handle = NULL;

	if( Initialize() )
	{
		ESP_LOGV(TAG, "Initialize!!!");
		main_State = xTaskCreate( mainRoutine, "mainRoutine", STACK_SIZE, ( void * ) 1, tskIDLE_PRIORITY, &main_Handle );
		ESP_LOGV(TAG, "DONE");
	}
}

void mainRoutine()
{
    BaseType_t camera_State;
    TaskHandle_t camera_Handle = NULL;

    BaseType_t button_State;
    TaskHandle_t button_Handle = NULL;

    BaseType_t wifi_State;
    TaskHandle_t wifi_Handle = NULL;

    button_State = xTaskCreate( button_main, "button_main", STACK_SIZE, ( void * ) 1, tskIDLE_PRIORITY, &button_Handle );

	for( ; ; )
	{
		if ( ( Button_Change == 1 && Initial_Run == 0 ) || Initial_Run == 1 )
		{
			if ( Initial_Run == 0 )
			{
				if ( State == 2 ) State = 3;
				else if ( State == 3 ) State = 2;
			}
			ESP_LOGI(TAG, "Button : %d, State : %d", Button_Change, State);
			Initial_Run = 0;

			switch (State)
			{
				case 2 :
					if( camera_Handle != NULL )
					{
						camera_fileclose();
						vTaskDelete( camera_Handle );
						camera_Handle = NULL;
						ESP_LOGI(TAG, "Cam task deleted");
					}
					if ( wifi_Handle == NULL )
					{
						wifi_State = xTaskCreate( Wifi_main, "wifi_main", STACK_SIZE, ( void * ) 1, tskIDLE_PRIORITY, &wifi_Handle );
						ESP_LOGI(TAG, "WIFI_STATUS : %d", wifi_State);
					}
					break;

				case 3 :
					ESP_LOGI(TAG, "Cam Started");
					if( wifi_Handle != NULL )
					{
						int wifi_status = Wifi_stop();
						vTaskDelete( wifi_Handle );
						wifi_Handle = NULL;
						ESP_LOGI(TAG, "Wifi task deleted. status : %d", wifi_status);
					}
					if( camera_filegen() && camera_Handle == NULL )
					{
						camera_State = xTaskCreate( camera_capture_task, "camera_main", STACK_SIZE, ( void * ) 1, ( UBaseType_t ) 1U, &camera_Handle );
					}
					break;

				default :
					//Do nothing
					break;
			}
		}
		else
		{
			//Do nothing
		}
		vTaskDelay(20);
	}
}


/*
 * Initialize
 */
bool Initialize()
{
	bool status=false;
	int status_ind[3] = {1,1,1};

	status_ind[1] = Wifi_init();
	status_ind[0] = camera_init();
	status_ind[2] = button_init();

	ESP_LOGI(TAG,"Initialization Status : %d , %d, %d", status_ind[0], status_ind[1], status_ind[2] );
	if ( status_ind[0] + status_ind[1] + status_ind[2] == 0 ) { status = true; };

	return status;
}
