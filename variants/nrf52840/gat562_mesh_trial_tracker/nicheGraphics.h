#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "configuration.h"

#include "graphics/niche/InkHUD/InkHUD.h"
#include "graphics/niche/InkHUD/WindowManager.h"

// 由于这个板没有E-Ink显示屏，我们使用一个虚拟驱动
// 在实际应用中，这里应该初始化真实的E-Ink驱动
class DummyEInkDriver : public NicheGraphics::Drivers::EInk {
public:
    DummyEInkDriver() : EInk(200, 200) {} // 虚拟分辨率
    bool begin() override { return true; }
    void update(uint8_t* buffer, UpdateTypes type = UpdateTypes::UNSPECIFIED) override {} // 不做实际更新
    void sleep() override {} // 不做实际睡眠
};

static DummyEInkDriver* dummyDriver = nullptr;

void setupNicheGraphics() {
    // 获取InkHUD实例
    auto inkhud = NicheGraphics::InkHUD::InkHUD::getInstance();
    
    // 创建并设置虚拟驱动
    dummyDriver = new DummyEInkDriver();
    inkhud->setDriver(dummyDriver);
    
    // 配置显示弹性参数（可选）
    inkhud->setDisplayResilience(5, 2.0);
    
    // 启动InkHUD
    inkhud->begin();
}

#endif // MESHTASTIC_INCLUDE_INKHUD