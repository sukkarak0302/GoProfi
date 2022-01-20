/*
 * GoProfi_button.c
 *
 *  Created on: 2021. 11. 14.
 *      Author: Gyuhyun_Cho
 */

#include "GoProfi_Button.h"
#include "MainParameter.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#define LOG "Button"

gpio_config_t button_config = {
		.pin_bit_mask = GPIO_SEL_13,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.intr_type = GPIO_INTR_DISABLE
};

static int level = 0;
static int level_pre = 0;
extern int Button_Change = 2;

int button_init()
{
	if( gpio_config(&button_config) == ESP_OK && gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLDOWN_ONLY) == ESP_OK )
		return ESP_OK;
	else
		return 1;
}

void button_main()
{
	Button_Change = 2;

	for( ; ; )
	{
		level = gpio_get_level(GPIO_NUM_13);

		if( level != level_pre && level == 0 )
			Button_Change = 1;
		else
			Button_Change = 2;

		level_pre = level;
		vTaskDelay( 20 );
	}
}
