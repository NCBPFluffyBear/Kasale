//
// Created by ianzh on 1/23/2025.
//

#ifndef KASALE_H
#define KASALE_H
#include <GroupUtils.h>
#include <Preferences.h>
#include <KASAUtil.h>

// Mode selection
#define SEL_A 34
#define SEL_B 35
#define SEL_SW 32
// Brightness selection
#define BRI_A 33
#define BRI_B 25
#define BRI_SW 26

#define SERIAL_TOGGLE 27
#define PANIC 12

// Fixed pins
#define LED 2
/*
 *Reserved pins for screen:x
 *SCL 22
 *SDA 21
 */

#define DEVICE_NAME "Kasale"
#define CONSOLE_PROMPT "Kasale>"
#define selGroup groupUtils.groups[selection]

#define MAX_IP_LEN 16
#define CHAR_HEIGHT 8

class KASASmartPlug;

extern Preferences preferences;
extern GroupUtils groupUtils;
extern KASAUtil kasaUtil;

extern int selection;
extern bool hasSerial;
extern int brightnessStep;
extern int defaultBrightness;
extern int idleTimeout;
extern bool dimDisplay;

void relay_isr();
int update_brightness(int);
void select_group(int);
void startWifi();

#endif //KASALE_H
