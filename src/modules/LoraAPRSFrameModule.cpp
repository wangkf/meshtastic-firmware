#include "LoraAPRSFrameModule.h"
#include "NodeDB.h"
#include "RTC.h"
#include "configuration.h"
#include "graphics/Screen.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>
#include "graphics/SharedUIDisplay.h"

LoraAPRSFrameModule *loraAPRSFrameModule;

LoraAPRSFrameModule::LoraAPRSFrameModule()
    : PositionModule()
{
    LOG_INFO("LoraAPRSFrameModule initialized");
    enabled = true; // 默认启用
}

#if HAS_SCREEN
void LoraAPRSFrameModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->clear();
    // 设置字体和对齐方式
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    
    // 绘制通用头部
    graphics::drawCommonHeader(display, x, y, "Lora APRS");
    
    // 获取标准文本位置
    const int *textPos = graphics::getTextPositions(display);
    
    // 显示启用状态
    char statusStr[32];
    snprintf(statusStr, sizeof(statusStr), "Status: %s", enabled ? "Enabled" : "Disabled");
    display->drawString(x, graphics::getTextPositions(display)[1], statusStr);
    
    // 获取本地节点信息
    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(nodeDB->getNodeNum());
    
    // 如果有有效的位置信息，显示位置数据
    if (node && nodeDB->hasValidPosition(node)) {
        // 格式化并显示纬度
        char latStr[16];
        snprintf(latStr, sizeof(latStr), "Lat: %.5f", node->position.latitude_i * 1e-7);
        display->drawString(x, graphics::getTextPositions(display)[2], latStr);
        
        // 格式化并显示经度
        char lonStr[16];
        snprintf(lonStr, sizeof(lonStr), "Lon: %.5f", node->position.longitude_i * 1e-7);
        display->drawString(x, graphics::getTextPositions(display)[3], lonStr);
        
        // 格式化并显示高度
        char altStr[16];
        snprintf(altStr, sizeof(altStr), "Alt: %dm", node->position.altitude);
        display->drawString(x, graphics::getTextPositions(display)[4], altStr);
    } else {
        // 如果没有位置信息，显示提示
        display->drawString(x, graphics::getTextPositions(display)[2], "No position data");
        display->drawString(x, graphics::getTextPositions(display)[3], "available");
    }
    
    // 显示提示信息
    display->setFont(FONT_SMALL);
    display->drawString(x, graphics::getTextPositions(display)[5], "Long press for menu");
}

// 实现Observer接口的onNotify方法，处理从Observable通知来的事件
bool LoraAPRSFrameModule::onNotify(const InputEvent *event, const char *eventContext)
{
    // 处理UI框架事件
    if (event && eventContext && strcmp(eventContext, "ui_frame") == 0) {
        // 检查是否为长按事件
        if (event->inputEvent == INPUT_BROKER_SELECT_LONG) {
            handleUIFrameEvent(nullptr); // 简化处理，不传递具体事件
            return true;
        }
        // 检查是否为短按事件
        else if (event->inputEvent == INPUT_BROKER_SELECT) {
            // 切换启用状态
            toggleEnabled();
            // 通知UI重绘
            UIFrameEvent e;
            e.action = UIFrameEvent::Action::REDRAW_ONLY;
            notifyObservers(&e);
            return true;
        }
    }
    return false;
}

// 处理UI框架事件（如按钮长按）
void LoraAPRSFrameModule::handleUIFrameEvent(const UIFrameEvent *event)
{
    // 处理长按事件
    if (event) {
        LOG_INFO("LoraAPRSFrameModule: Received UIFrameEvent, action=%d", (int)event->action);
        if (event->action == UIFrameEvent::Action::LONG_PRESS) {
            LOG_INFO("LoraAPRSFrameModule: LONG_PRESS event detected, showing APRS menu");
            // 确保screen不为空
            if (screen) {
                screen->requestMenu(graphics::menuHandler::aprs_menu);
            } else {
                LOG_ERROR("LoraAPRSFrameModule: Screen is null, cannot show menu");
            }
        }
    }
}

// 实现Observer接口 - 处理UI框架事件
int LoraAPRSFrameModule::onNotify(const UIFrameEvent *evt)
{
    LOG_INFO("LoraAPRSFrameModule: onNotify(UIFrameEvent) called");
    handleUIFrameEvent(evt);
    return 0; // 返回0表示允许其他观察者继续处理
}
#endif