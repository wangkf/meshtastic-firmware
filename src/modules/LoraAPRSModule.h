#pragma once
#include "Default.h"
#include "ProtobufModule.h"
#include "concurrency/OSThread.h"
#include "meshtastic/mesh.pb.h"
#include "Observer.h"
#include "OLEDDisplay.h"
#include "OLEDDisplayUi.h"

/**
 * Lora APRS module for sending/receiving APRS format packets via LoRa
 */
class LoraAPRSModule : public ProtobufModule<meshtastic_Position>, private concurrency::OSThread, public Observable<const UIFrameEvent *>, public Observer<const UIFrameEvent *>
{
  private:
    // Lora APRS配置结构体
    struct LoraAPRSConfig {
        bool enabled;              // APRS功能是否启用
        float frequency;           // APRS传输频率(MHz)
        uint8_t spreadingFactor;   // 扩频因子
        uint8_t bandwidth;         // 带宽(kHz)
        uint8_t codingRate;        // 编码率
        int8_t txPower;            // 发射功率(dBm)
        char callsign[10];         // 呼号(最大9字符)
        uint8_t ssid;              // SSID
        uint32_t beaconInterval;   // 信标间隔(秒)
    };

    LoraAPRSConfig loraAPRSConfig;
    uint32_t lastLoraAPRSSend = 0;
    uint32_t lastFrequency = 0;
    bool isRadioReconfigured = false;
    uint32_t txCount = 0;

    // 临时保存原始LoRa配置，以便在APRS传输后恢复
    float originalFreq;
    uint8_t originalSF;
    uint8_t originalBW;
    uint8_t originalCR;
    int8_t originalPower;

    // APRS默认配置
    static constexpr bool APRS_DEFAULT_ENABLED = false;
    static constexpr float APRS_DEFAULT_FREQUENCY = 433.775f;
    static constexpr uint8_t APRS_DEFAULT_SF = 12;
    static constexpr uint8_t APRS_DEFAULT_BW = 125;
    static constexpr uint8_t APRS_DEFAULT_CR = 5;
    static constexpr int8_t APRS_DEFAULT_POWER = 22;
    static constexpr const char* APRS_DEFAULT_CALLSIGN = "BI9ABS";
    static constexpr uint8_t APRS_DEFAULT_SSID = 2;
    static constexpr uint32_t APRS_DEFAULT_BEACON_INTERVAL = 900; // 15分钟

    // 格式化APRS位置字符串
    void formatAPRSPosition(char *buffer, size_t bufferSize, double lat, double lon, uint16_t altitude);
    // 格式化纬度为APRS格式
    String formatLatitudeAPRS(double lat);
    // 格式化经度为APRS格式
    String formatLongitudeAPRS(double lon);
    // 配置无线电为APRS模式
    bool configureRadioForAPRS();
    // 恢复原始无线电配置
    bool restoreOriginalRadioConfig();
    // 构建并发送APRS数据包
    void sendAPRSPacket();

  public:
    /** Constructor
     * name is for debugging output
     */
    LoraAPRSModule();

    // Methods for menu control - always public
    bool isEnabled() const { return loraAPRSConfig.enabled; }
    void toggleEnabled() { 
        loraAPRSConfig.enabled = !loraAPRSConfig.enabled; 
        saveLoraAPRSConfig(); 
    }
    void saveLoraAPRSConfig();
    void loadLoraAPRSConfig();

  protected:
    /** Called to handle a particular incoming message

    @return true if you've guaranteed you've handled this message and no other handlers should be considered for it
    */
    virtual bool handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_Position *p) override;

    /** Messages can be received that have the want_response bit set.  If set, this callback will be invoked
     * so that subclasses can (optionally) send a response back to the original sender.  */
    virtual meshtastic_MeshPacket *allocReply() override;

    /** Does our periodic broadcast */
    virtual int32_t runOnce() override;

#if HAS_SCREEN
    // UI相关方法
    virtual bool wantUIFrame() override { return true; }
    virtual Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    virtual bool isRequestingFocus() override { return false; }
    virtual bool interceptingKeyboardInput() override { return false; }
    // 处理UI框架事件
    virtual void handleUIFrameEvent(const UIFrameEvent *evt);
    // 实现Observer接口的onNotify方法，处理长按事件
    virtual int onNotify(const UIFrameEvent *evt) override;
    // 显示Lora APRS操作菜单
    void showLoraAPRSMenu();
#endif
};

extern LoraAPRSModule *loraAPRSModule;