/* Copyright (C) 2025
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef TTGO_T_BEAM_LORA_APRS_H_
#define TTGO_T_BEAM_LORA_APRS_H_

#include <Arduino.h>
#include <BG_RF95.h>
#include <Adafruit_SSD1306.h>

// 外部变量声明
extern String CALLSIGN;
extern String LATITUDE_PRESET;
extern String LONGITUDE_PRESET;
extern String APRS_SYMBOL;
extern String MY_COMMENT;
extern String sTable;
extern float BattVolts;
extern unsigned long lastTX;
extern BG_RF95 rf95;
extern float TXFREQ;
extern byte TXdbmW;
extern Adafruit_SSD1306 display;

// 引脚定义
#ifndef BUTTON
#if (defined(T_BEAM_V1_0) || defined(T_BEAM_V1_1)|| defined(T_BEAM_V1_2)|| defined(T_BEAM_V2_0))
  #define BUTTON 38                         // 按钮
#else
  #define BUTTON 39                         // 按钮
#endif
#endif

#ifndef TXLED
#if (defined(T_BEAM_V1_0) || defined(T_BEAM_V1_1)|| defined(T_BEAM_V1_2)|| defined(T_BEAM_V2_0))
  #define TXLED 4                           // 发送指示灯
#else
  #define TXLED 14                          // 发送指示灯
#endif
#endif

#ifndef BATTERY_PIN
#define BATTERY_PIN 35                    // 电池电压检测
#endif

#endif // TTGO_T_BEAM_LORA_APRS_H_