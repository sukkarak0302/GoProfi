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

static int State = 0;

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
		//mainRoutine();
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

    StaticTask_t xTaskBuffer;
    StackType_t *xStack;

    button_State = xTaskCreate( button_main, "button_main", STACK_SIZE, ( void * ) 1, tskIDLE_PRIORITY, &button_Handle );

    int check = 0;
	int cam_check = 0;
	int wifi_check = 0;

	for( ; ; )
	{
		if ( ( Button_Change == 1 || State == 0 ) )
		{
			if ( State == 0 ) State = 2;
			else if ( State == 3 ) State = 2;
			else if ( State == 2 ) State = 3;

			ESP_LOGI(TAG, "Button : %d, State : %d", Button_Change, State);

			switch (State)
			{
				case 2 :
					cam_check = 0;
					wifi_check = 0;
					if( camera_Handle != NULL )
					{
						vTaskDelete( camera_Handle );
						ESP_LOGI(TAG, "CamHandle deleted1");
						vTaskDelay(100);
						ESP_LOGI(TAG, "Task delay!");
						camera_Handle = NULL;
						camera_fileclose();
						ESP_LOGI(TAG, "Cam task deleted");
						cam_check = 1;
					}
					else
					{
						cam_check = 1;
					}

					if ( wifi_Handle == NULL )
					{
						wifi_State = xTaskCreate( Wifi_main, "wifi_main", STACK_SIZE, ( void * ) 1, tskIDLE_PRIORITY, &wifi_Handle );
						ESP_LOGI(TAG, "WIFI_STATUS : %d", wifi_State);
						wifi_check = 1;
					}

					if ( ( cam_check == 0 || wifi_check == 0 ) )
					{
						State = 4;
					}

					break;

				case 3 :
					cam_check = 0;
					wifi_check = 0;
					ESP_LOGI(TAG, "Cam Started");
					if( wifi_Handle != NULL )
					{
						int wifi_status = Wifi_stop();
						vTaskDelete( wifi_Handle );
						wifi_Handle = NULL;
						ESP_LOGI(TAG, "Wifi task deleted. status : %d", wifi_status);
						wifi_check = 1;
					}
					else
					{
						wifi_check = 1;
					}

					if( camera_filegen() == 1 && camera_Handle == NULL )
					{
						//xStack = (uint8_t*)heap_caps_calloc(1, 10*1024, MALLOC_CAP_SPIRAM);
						//camera_State = xTaskCreateStatic( camera_capture_task, "camera_main", 10*1024, ( void * ) 1, tskIDLE_PRIORITY, xStack, &xTaskBuffer );
						camera_State = xTaskCreate( camera_capture_task, "camera_main", STACK_SIZE, ( void * ) 1, ( UBaseType_t ) 1U, &camera_Handle );
						//camera_capture_task();
						cam_check = 1;
					}

					if ( cam_check == 0 || wifi_check == 0 )
					{
						State = 4;
						ESP_LOGI(TAG, "ERROR to State 4");
						esp_restart();
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


	ESP_LOGI(TAG, "Largest Free block SPIRAM : %d", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "Largest Free block INTERNAL : %d", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

	// debug
	ESP_LOGI(TAG, "1Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "1Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

	status_ind[1] = Wifi_init();

	ESP_LOGI(TAG, "Largest Free block SPIRAM : %d", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "Largest Free block INTERNAL : %d", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

	// debug
	ESP_LOGI(TAG, "2Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "2Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
	status_ind[0] = camera_init();

	//TEMP
	status_ind[0] = 0;

	// debug
	ESP_LOGI(TAG, "3Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "3Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
	status_ind[2] = button_init();

	ESP_LOGI(TAG,"Initialization Status : %d , %d, %d", status_ind[0], status_ind[1], status_ind[2] );
	if ( status_ind[0] + status_ind[1] + status_ind[2] == 0 ) { status = true; };

	return status;
}
