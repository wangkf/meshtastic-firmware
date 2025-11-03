/* Copyright (C) 2025
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BUTTON_UTILS_H_
#define BUTTON_UTILS_H_

#include <Arduino.h>

namespace BUTTON_Utils {

    // 初始化按键功能
    void setup();
    
    // 按键检测循环 - 必须在主loop中调用
    void loop();
    
    // 单击处理函数
    void singlePress();
    
    // 长按处理函数
    void longPress();
    
    // 双击处理函数
    void doublePress();

} // namespace BUTTON_Utils

#endif // BUTTON_UTILS_H_