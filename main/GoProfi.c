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
#include "MainParameter.h"

void mainRoutine();
bool Initialize();

extern int State = 0;

static const char *TAG = "Main";

void app_main(void)
{
    TaskHandle_t wifi_Control_Handle = NULL;

	if( Initialize() )
	{
		xTaskCreate( Wifi_control_main, "wifi_control_main", STACK_SIZE, ( void * ) 1, tskIDLE_PRIORITY, &wifi_Control_Handle );
		mainRoutine();
	}
	else
	{
		ESP_LOGE(TAG, "Initialize Fail, Restart");
		esp_restart();
	}
}

void mainRoutine()
{
    TaskHandle_t camera_Handle = NULL;

    vTaskDelay(100);

	for( ; ; )
	{
		if ( State != State_Read() )
		{

#ifdef DEBUG
			ESP_LOGI(TAG, "State Req : %d, State : %d", State_Request, State);
#endif
			State = State_Read();

			switch (State)
			{
				case 0 :
					// Initial phase, nothing
					break;

				case 2 :
					if( camera_Handle != NULL )
					{
						if ( camera_fileclose() == 0 )
						{
							State = 4;
						}
						vTaskDelete( camera_Handle );
						camera_Handle = NULL;
					}
					break;

				case 3 :
					if( camera_filegen() == 1 && camera_Handle == NULL )
					{
						xTaskCreate( camera_capture_task, "camera_main", STACK_SIZE, ( void * ) 1, ( UBaseType_t ) 1U, &camera_Handle );
					}
					else
					{
						State = 5;
					}
					break;

				default :
					//Do nothing
					break;
			}

			if ( State >= 4 )
			{
				esp_restart();
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
	int status_ind[2] = {1,1};

#ifdef DEBUG
	ESP_LOGI(TAG, "Largest Free block SPIRAM : %d", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "Largest Free block INTERNAL : %d", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
	ESP_LOGI(TAG, "1Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "1Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#endif

	status_ind[1] = Wifi_init();

#ifdef DEBUG
	ESP_LOGI(TAG, "Largest Free block SPIRAM : %d", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "Largest Free block INTERNAL : %d", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
	ESP_LOGI(TAG, "2Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "2Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#endif

	status_ind[0] = camera_init();

#ifdef DEBUG
	ESP_LOGI(TAG, "3Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI(TAG, "3Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
	ESP_LOGI(TAG, "Initialize number : %d", status_ind[0] + status_ind[1]);
#endif

	if ( status_ind[0] + status_ind[1] == 0 ) { status = true; };

	return status;
}
