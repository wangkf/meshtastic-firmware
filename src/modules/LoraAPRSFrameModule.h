#pragma once
#include "PositionModule.h"
#include "OLEDDisplay.h"
#include "OLEDDisplayUi.h"
#include "Observer.h"
#include "input/InputBroker.h"

/**
 * Lora APRS frame module for displaying position data with Lora APRS label
 */
class LoraAPRSFrameModule : public PositionModule, public Observable<const UIFrameEvent *>
{
  private:
    bool enabled; // 启用状态标志
  
  public:
    /** Constructor */
    LoraAPRSFrameModule();
    
    // 菜单控制方法
    bool isEnabled() const { return enabled; }
    void toggleEnabled() { 
        enabled = !enabled; 
    }

#if HAS_SCREEN
    // UI相关方法
    virtual bool wantUIFrame() override { return true; }
    virtual Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    virtual bool isRequestingFocus() override { return false; }
    virtual bool interceptingKeyboardInput() override { return false; }
    // 处理UI框架事件
    virtual void handleUIFrameEvent(const UIFrameEvent *evt);
    // 实现Observer接口 - 处理输入事件
    bool onNotify(const InputEvent *event, const char *eventContext);
    // 实现Observer接口 - 处理UI框架事件
    virtual int onNotify(const UIFrameEvent *evt);
#endif
};

extern LoraAPRSFrameModule *loraAPRSFrameModule;