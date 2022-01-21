/*
 * MainParameter.h
 *
 *  Created on: 2021. 11. 14.
 *      Author: Gyuhyun_Cho
 */

#ifndef MAIN_MAINPARAMETER_H_
#define MAIN_MAINPARAMETER_H_

// Button GPIO
//#define BUTTON_INPUT_PIN 13 // Button GPIO16
//#define BUTTON_INPUT_PIN_SEL (1ULL<<BUTTON_INPUT_PIN)

#define STACK_SIZE 8096

#define SCRATCH_BUFSIZE 1024

#define ESP_WIFI_SSID "GO_PROFI"
#define ESP_WIFI_PASS "12345678"

extern int State;

#endif /* MAIN_MAINPARAMETER_H_ */
