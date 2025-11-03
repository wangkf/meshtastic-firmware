/* Copyright (C) 2025
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <Arduino.h>
#include <OneButton.h>
#include "TTGO_T-Beam_LoRa_APRS.h"

// 声明外部函数
void sendpacketWithPresetLocation(bool isTest = false);
void batt_read();

#ifdef BUTTON

// 声明外部变量
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

namespace BUTTON_Utils {
    
    // 创建OneButton对象，第三个参数true表示启用内部上拉电阻
    OneButton userButton = OneButton(BUTTON, true, true);
    
    // 单击处理函数 - 发送数据包
    void singlePress() {
        Serial.println("[BUTTON] Single click detected - Manual transmission");
        
        // 读取电池电压
        batt_read();
        
        // 显示短按响应信息
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextWrap(false);
        
        // 显示内容
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("Manual TX");
        display.setCursor(0, 16);
        display.print("BAT: " + String(BattVolts,1) + "V");
        display.setCursor(0, 32);
        display.print("Sending packet...");
        display.display();
        
        // 闪烁LED指示操作
        digitalWrite(TXLED, HIGH);
        
        // 使用主程序中的函数发送数据包
        sendpacketWithPresetLocation();
        
        // 更新最后发送时间
        lastTX = millis();
        
        // 显示完成信息
        digitalWrite(TXLED, LOW);
        
        batt_read(); // 重新读取最新电压
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Packet sent");
        display.setCursor(0, 16);
        display.print("BAT: " + String(BattVolts,1) + "V");
        display.setCursor(0, 32);
        display.print("TX Complete!");
        display.display();
        
        Serial.println("[BUTTON] Transmission complete!");
    }
    
    // 长按处理函数 - 可以扩展其他功能
    void longPress() {
        Serial.println("[BUTTON] Long press detected");
        
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("Long Press");
        display.setCursor(0, 16);
        display.print("Function reserved");
        display.display();
        
        delay(1000);
    }
    
    // 双击处理函数 - 可以扩展其他功能
    void doublePress() {
        Serial.println("[BUTTON] Double click detected");
        
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("Double Press");
        display.setCursor(0, 16);
        display.print("Function reserved");
        display.display();
        
        delay(1000);
    }
    
    // 按键检测循环 - 在主loop中调用
    void loop() {
        // 必须在每次循环中调用tick()来检测按键状态
        userButton.tick();
    }
    
    // 按键初始化函数 - 在setup中调用
    void setup() {
        Serial.println("[BUTTON] Initializing button");
        
        // 设置LED引脚为输出模式
        pinMode(TXLED, OUTPUT);
        digitalWrite(TXLED, LOW);
        
        // 配置按键参数 - 根据doc目录中的标准实现
        // 设置去抖时间 - 50ms提供良好的稳定性
        userButton.setDebounceMs(50);
        
        // 设置单击时间窗口 - 300ms是更稳定的设置
        userButton.setClickMs(300);
        
        // 设置长按时间阈值
        userButton.setPressMs(800);
        
        // 绑定按键事件处理函数
        userButton.attachClick(singlePress);        // 单击发送数据包
        userButton.attachLongPressStart(longPress); // 长按保留功能
        userButton.attachDoubleClick(doublePress);  // 双击保留功能
        
        Serial.println("[BUTTON] Button initialized successfully");
    }

} // namespace BUTTON_Utils

#endif // BUTTON