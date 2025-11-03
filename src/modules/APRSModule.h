#pragma once
#include "ProtobufModule.h"
#include "concurrency/OSThread.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/module_config.pb.h"
#include "configuration.h"
#include "Observer.h"
#include "OLEDDisplay.h"
#include "OLEDDisplayUi.h"

#include <Arduino.h>

//*** APRS模块用于发送/接收APRS格式的位置和信息数据包
class APRSModule : public ProtobufModule<meshtastic_AdminMessage>, private concurrency::OSThread, public Observable<const UIFrameEvent *>, public Observer<const UIFrameEvent *>
{
  private:
    // APRS配置结构体
    struct APRSConfig {
        bool enabled;              // APRS功能是否启用
        float frequency;           // APRS传输频率(MHz)
        uint8_t spreadingFactor;   // 扩频因子
        uint8_t bandwidth;         // 带宽(kHz)
        uint8_t codingRate;        // 编码率
        int8_t txPower;            // 发射功率(dBm)
        char callsign[10];         // 呼号(最大9字符)
        uint8_t ssid;              // SSID
        uint32_t beaconInterval;   // 信标间隔(秒)
        bool usePositionData;      // 是否使用位置数据
        bool useCustomMessage;     // 是否使用自定义消息
        char customMessage[80];    // 自定义消息(最大79字符)
    };

    APRSConfig aprsConfig;        // APRS配置
    uint32_t lastAPRSSend;        // 上次发送APRS包的时间戳
    uint32_t lastFrequency;       // 上次使用的频率
    bool isRadioReconfigured;     // 是否需要重新配置无线电
    uint32_t txCount;             // 发送的APRS数据包数量

    // 临时保存原始LoRa配置，以便在APRS传输后恢复
    float originalFreq;           // 原始频率
    uint8_t originalSF;           // 原始扩频因子
    uint8_t originalBW;           // 原始带宽
    uint8_t originalCR;           // 原始编码率
    int8_t originalPower;         // 原始功率
    //*** 配置无线电为APRS模式
    bool configureRadioForAPRS();
    //*** 恢复原始无线电配置
    bool restoreOriginalRadioConfig();
    //*** 构建APRS位置数据包
    meshtastic_MeshPacket *buildAPRSPositionPacket();
    //*** 构建APRS消息数据包
    meshtastic_MeshPacket *buildAPRSMessagePacket(const char *message);
    //*** 格式化APRS位置字符串
    void formatAPRSPosition(char *buffer, size_t bufferSize, double lat, double lon, uint16_t altitude, float course = 0.0, float speed = 0.0, float battVoltage = 0.0);
    //*** 格式化纬度为APRS格式
    String formatLatitudeAPRS(double lat);
    //*** 格式化经度为APRS格式
    String formatLongitudeAPRS(double lon);
  public:
    //** 构造函数* name是用于调试输出的名称
    APRSModule();
    //*** 获取发送的APRS数据包数量
    uint32_t getTxCount() const { return txCount; }
    //*** 发送APRS位置数据包
    void sendAPRSPosition();
    //*** 发送预设位置的APRS数据包
    void sendPresetLocationAPRS();
    //*** 发送APRS自定义消息
    void sendAPRSMessage(const char *message);
    //*** 保存APRS配置
    void saveAPRSConfig();
    //*** 加载APRS配置
    void loadAPRSConfig();
    // Methods for menu control - always public
    bool isEnabled() const { return aprsConfig.enabled; }
    void toggleEnabled() { 
        aprsConfig.enabled = !aprsConfig.enabled; 
        saveAPRSConfig(); 
    }

  protected:
    //** 处理接收到的特定消息 * @return true如果已保证处理此消息且不应考虑其他处理程序
    virtual bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_AdminMessage *p) override;
    /** 处理接收到的任何数据包，不处理接收功能 */
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    //** 执行周期性广播
    virtual int32_t runOnce() override;

    // UIFrameEvent is defined in MeshModule.h

#if HAS_SCREEN
    // UI相关方法
    virtual bool wantUIFrame() override { return true; }
    virtual Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    virtual bool isRequestingFocus() override { return false; }
    virtual bool interceptingKeyboardInput() override { return false; }
    // 处理UI框架事件
    virtual void handleUIFrameEvent(const UIFrameEvent *evt);
    // 实现Observer接口 - 处理UI框架事件
    virtual int onNotify(const UIFrameEvent *evt) override;
    // 显示APRS配置菜单
    void showAPRSConfigMenu();
#endif
};

extern APRSModule *aprsModule;