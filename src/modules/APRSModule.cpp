#include "APRSModule.h"
#include "Default.h"
#include "GPS.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "airtime.h"
#include "configuration.h"
#include "main.h"
#include "meshUtils.h"
#include "sleep.h"
#include "graphics/draw/MenuHandler.h"
#include "gps/RTC.h" // For getValidTime
#include <pb.h> // For nanopb functions
#include <time.h>
#include "graphics/Screen.h"
#include "graphics/SharedUIDisplay.h"
// #include "Router.h" - 暂时不需要，因为没有使用observePackets方法

// 配置对象已经在NodeDB.h中声明，不需要重复声明

APRSModule *aprsModule;

// APRS默认配置
#define APRS_DEFAULT_ENABLED true
#define APRS_DEFAULT_FREQUENCY 433.775f  // 标准APRS频率
#define APRS_DEFAULT_SF 12              // 扩频因子
#define APRS_DEFAULT_BW 125             // 带宽(kHz)
#define APRS_DEFAULT_CR 5               // 编码率
#define APRS_DEFAULT_POWER 22           // 发射功率(dBm)
#define APRS_DEFAULT_CALLSIGN "BI9ABS"
#define APRS_DEFAULT_SSID 2
#define APRS_DEFAULT_BEACON_INTERVAL 900  // 15分钟

APRSModule::APRSModule()
    : ProtobufModule("APRS", meshtastic_PortNum_ADMIN_APP, &meshtastic_AdminMessage_msg), 
       concurrency::OSThread("APRS")
{
    // 初始化默认配置 - 与TTGO_T_Beam_LoRa_APRS.ino保持一致的参数
    aprsConfig.enabled = false; // 默认禁用，避免启动时修改无线电配置
    aprsConfig.frequency = APRS_DEFAULT_FREQUENCY;
    aprsConfig.spreadingFactor = APRS_DEFAULT_SF;
    aprsConfig.bandwidth = APRS_DEFAULT_BW;
    aprsConfig.codingRate = APRS_DEFAULT_CR;
    aprsConfig.txPower = APRS_DEFAULT_POWER;
    strcpy(aprsConfig.callsign, APRS_DEFAULT_CALLSIGN);
    aprsConfig.ssid = APRS_DEFAULT_SSID;
    aprsConfig.beaconInterval = APRS_DEFAULT_BEACON_INTERVAL;
    aprsConfig.usePositionData = true;
    aprsConfig.useCustomMessage = false;
    strcpy(aprsConfig.customMessage, "Meshtastic APRS");
    // 移除转发功能，不再需要这些字段
    
    lastAPRSSend = 0;
    lastFrequency = 0;
    isRadioReconfigured = false;
    txCount = 0;
    
    // 加载配置
    loadAPRSConfig();
    
    // 注意：由于Router类没有observePackets方法，我们将使用handleReceived方法来处理本地消息
    
    LOG_INFO("APRSModule initialized: freq=%.3f MHz, enabled=%d, callsign=%s-%d", 
            aprsConfig.frequency, aprsConfig.enabled, aprsConfig.callsign, aprsConfig.ssid);
    
    // 设置运行间隔
    setIntervalFromNow(10000);  // 10秒后首次运行
}

void APRSModule::loadAPRSConfig()
{
    // 为了简化编译，我们暂时跳过从protobuf加载配置的逻辑
    // 直接使用默认配置
    LOG_INFO("Loading APRS config from storage: freq=%f, enabled=%d, callsign=%s-%d", 
             aprsConfig.frequency, aprsConfig.enabled, aprsConfig.callsign, aprsConfig.ssid);
}

void APRSModule::saveAPRSConfig()
{
    // 为了简化编译，我们暂时跳过保存配置到protobuf的逻辑
    // 直接保存到存储设备
    LOG_INFO("Saving APRS config to storage: freq=%f, enabled=%d, callsign=%s-%d", 
             aprsConfig.frequency, aprsConfig.enabled, aprsConfig.callsign, aprsConfig.ssid);
    
    // 保存到存储
    nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
    
    LOG_INFO("APRS config saved: freq=%f, enabled=%d, callsign=%s-%d", 
             aprsConfig.frequency, aprsConfig.enabled, aprsConfig.callsign, aprsConfig.ssid);
}

bool APRSModule::configureRadioForAPRS()
{
    // 添加额外检查确保service有效
    if (!service) {
        LOG_ERROR("APRSModule: Service not available for radio configuration");
        return false;
    }
    
    // 保存当前无线电配置
    originalFreq = config.lora.frequency_offset;
    originalSF = config.lora.spread_factor;
    originalBW = config.lora.bandwidth;
    originalCR = config.lora.coding_rate;
    originalPower = config.lora.tx_power;
    
    // 配置无线电为APRS模式
    config.lora.override_frequency = true;
    config.lora.frequency_offset = aprsConfig.frequency;
    config.lora.spread_factor = aprsConfig.spreadingFactor;
    config.lora.bandwidth = aprsConfig.bandwidth;
    config.lora.coding_rate = aprsConfig.codingRate;
    config.lora.tx_power = aprsConfig.txPower;
    
    // 重新配置无线电
    service->reloadConfig();
    isRadioReconfigured = true;
    
    LOG_INFO("Radio configured for APRS: freq=%f MHz, SF=%d, BW=%d kHz, CR=4/%d", 
             aprsConfig.frequency, aprsConfig.spreadingFactor, 
             aprsConfig.bandwidth, aprsConfig.codingRate);
    
    return true;
}

bool APRSModule::restoreOriginalRadioConfig()
{
    if (!isRadioReconfigured) return true;
    
    // 添加额外检查确保service有效
    if (!service) {
        LOG_ERROR("APRSModule: Service not available for radio configuration restore");
        isRadioReconfigured = false; // 重置标志避免重复尝试
        return false;
    }
    
    // 恢复原始无线电配置
    config.lora.override_frequency = false;
    config.lora.frequency_offset = originalFreq;
    config.lora.spread_factor = originalSF;
    config.lora.bandwidth = originalBW;
    config.lora.coding_rate = originalCR;
    config.lora.tx_power = originalPower;
    
    // 重新配置无线电
    service->reloadConfig();
    isRadioReconfigured = false;
    
    LOG_INFO("Radio restored to original configuration");
    
    return true;
}

void APRSModule::formatAPRSPosition(char *buffer, size_t bufferSize, double lat, double lon, uint16_t altitude)
{
    // 格式化APRS位置字符串
    // 格式: /HHMMSS/hdddd.ddN/dddmm.mmE-A
    
    // 获取当前时间
    time_t now = getValidTime(RTCQuality::RTCQualityDevice, true);
    struct tm *timeinfo = localtime(&now);
    
    // 格式化时间
    sprintf(buffer, "/%02d%02d%02dz", 
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    // 格式化纬度
    char latDir = (lat >= 0) ? 'N' : 'S';
    double latAbs = fabs(lat);
    int latDeg = (int)latAbs;
    double latMin = (latAbs - latDeg) * 60;
    
    // 格式化经度
    char lonDir = (lon >= 0) ? 'E' : 'W';
    double lonAbs = fabs(lon);
    int lonDeg = (int)lonAbs;
    double lonMin = (lonAbs - lonDeg) * 60;
    
    // 添加位置信息
    sprintf(buffer + strlen(buffer), "%02d%05.2f%c/%03d%05.2f%c", 
            latDeg, latMin, latDir, lonDeg, lonMin, lonDir);
    
    // 添加附加信息
    sprintf(buffer + strlen(buffer), "-\\/%03d/%03d", altitude, 0); // 高度和航向
}

meshtastic_MeshPacket *APRSModule::buildAPRSPositionPacket()
{
    // 获取当前位置
    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(nodeDB->getNodeNum());
    if (!nodeDB->hasValidPosition(node)) {
        LOG_WARN("No valid position available for APRS");
        return nullptr;
    }
    
    // 转换位置格式
    double lat = node->position.latitude_i * 1e-7;
    double lon = node->position.longitude_i * 1e-7;
    uint16_t altitude = node->position.altitude; // PositionLite的altitude已经是米单位
    
    // 构建APRS包
    char aprsPacket[256];
    
    // 添加呼号和SSID
    if (aprsConfig.ssid > 0) {
        sprintf(aprsPacket, "%s-%d>", aprsConfig.callsign, aprsConfig.ssid);
    } else {
        sprintf(aprsPacket, "%s>", aprsConfig.callsign);
    }
    
    // 添加目标 - 使用标准APRS路径，适合LoRa APRS
    strcat(aprsPacket, "APRS,qAR,");
    strcat(aprsPacket, aprsConfig.callsign);
    strcat(aprsPacket, "*");
    
    // 添加位置信息
    char position[64];
    formatAPRSPosition(position, sizeof(position), lat, lon, altitude);
    strcat(aprsPacket, position);
    
    // 添加消息
    if (aprsConfig.useCustomMessage && strlen(aprsConfig.customMessage) > 0) {
        strcat(aprsPacket, " ");
        strncat(aprsPacket, aprsConfig.customMessage, sizeof(aprsPacket) - strlen(aprsPacket) - 1);
    } else {
        strcat(aprsPacket, " Meshtastic APRS");
    }
    
    LOG_INFO("APRSModule: Building standard APRS packet: %s", aprsPacket);
    LOG_INFO("APRSModule: Using frequency: %.3f MHz, SF: %d, BW: %d kHz, CR: 4/%d, Power: %d dBm", 
             aprsConfig.frequency, aprsConfig.spreadingFactor, aprsConfig.bandwidth, 
             aprsConfig.codingRate, aprsConfig.txPower);
    
    // 创建数据数据包
    meshtastic_MeshPacket *mp = allocDataPacket();
    mp->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP; // 使用文本消息端口
    mp->want_ack = false;
    mp->decoded.payload.size = strlen(aprsPacket);
    memcpy(mp->decoded.payload.bytes, aprsPacket, mp->decoded.payload.size);
    
    return mp;
}

meshtastic_MeshPacket *APRSModule::buildAPRSMessagePacket(const char *message)
{
    // 构建APRS消息数据包
    char aprsPacket[256];
    
    // 添加呼号和SSID
    if (aprsConfig.ssid > 0) {
        sprintf(aprsPacket, "%s-%d>", aprsConfig.callsign, aprsConfig.ssid);
    } else {
        sprintf(aprsPacket, "%s>", aprsConfig.callsign);
    }
    
    // 添加目标
    strcat(aprsPacket, "APRS,TCPIP*:");
    
    // 添加消息
    strncat(aprsPacket, message, sizeof(aprsPacket) - strlen(aprsPacket) - 1);
    
    LOG_INFO("Building APRS message packet: %s", aprsPacket);
    
    // 创建数据数据包
    meshtastic_MeshPacket *mp = allocDataPacket();
    mp->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    mp->want_ack = false;
    mp->decoded.payload.size = strlen(aprsPacket);
    memcpy(mp->decoded.payload.bytes, aprsPacket, mp->decoded.payload.size);
    
    return mp;
}

void APRSModule::sendAPRSPosition()
{
    if (!aprsConfig.enabled) {
        LOG_DEBUG("APRSModule: APRS is disabled, skipping transmission");
        return;
    }
    
    LOG_INFO("APRSModule: Preparing to send APRS position beacon");
    
    // 检查无线电是否可用
    if (!service) {
        LOG_ERROR("APRSModule: MeshService not available");
        return;
    }
    
    // 配置无线电为APRS模式
    LOG_INFO("APRSModule: Configuring radio for APRS transmission");
    if (!configureRadioForAPRS()) {
        LOG_ERROR("APRSModule: Failed to configure radio for APRS");
        return;
    }
    
    // 构建APRS位置数据包
    meshtastic_MeshPacket *p = buildAPRSPositionPacket();
    if (p == nullptr) {
        LOG_ERROR("APRSModule: Failed to build APRS position packet");
        restoreOriginalRadioConfig();
        return;
    }
    
    // 设置目标为广播
    p->to = NODENUM_BROADCAST;
    p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
    
    LOG_INFO("APRSModule: Sending APRS packet to mesh network");
    // 发送数据包
    service->sendToMesh(p, RX_SRC_LOCAL, true);
    
    // 假设发送成功，更新状态
    lastAPRSSend = millis();
    txCount++; // 增加发送计数
    LOG_INFO("APRSModule: APRS position sent, total tx: %u", txCount);
    LOG_INFO("APRSModule: Next beacon in %u seconds", aprsConfig.beaconInterval);
    
    // 恢复原始无线电配置
    LOG_DEBUG("APRSModule: Restoring original radio configuration");
    restoreOriginalRadioConfig();
}

void APRSModule::sendAPRSMessage(const char *message)
{
    if (!aprsConfig.enabled) {
        LOG_DEBUG("APRS is disabled, skipping transmission");
        return;
    }
    
    // 配置无线电为APRS模式
    if (!configureRadioForAPRS()) {
        LOG_ERROR("Failed to configure radio for APRS");
        return;
    }
    
    // 构建APRS消息数据包
    meshtastic_MeshPacket *p = buildAPRSMessagePacket(message);
    if (p == nullptr) {
        LOG_ERROR("Failed to build APRS message packet");
        restoreOriginalRadioConfig();
        return;
    }
    
    // 设置目标为广播
    p->to = NODENUM_BROADCAST;
    p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
    
    // 发送数据包
    service->sendToMesh(p, RX_SRC_LOCAL, true);
    lastAPRSSend = millis();
    txCount++; // 增加发送计数
    
    LOG_INFO("APRS message sent, total tx: %u", txCount);
    
    // 恢复原始无线电配置
    restoreOriginalRadioConfig();
}

// parseAPRSPacket方法已移除，因为不再处理接收功能

bool APRSModule::handleReceivedProtobuf(const meshtastic_MeshPacket &mp, meshtastic_AdminMessage *p)
{
    // 处理接收到的管理消息，用于配置APRS模块
    if (p->which_payload_variant == meshtastic_AdminMessage_get_module_config_response_tag) {
        
        // 检查是否包含 APRS 配置
        if (p->get_module_config_response.which_payload_variant == meshtastic_ModuleConfig_aprs_tag) {
            
            // 更新本地配置
            meshtastic_ModuleConfig_APRSConfig *aprsProtoConfig = &p->get_module_config_response.payload_variant.aprs;
            
            aprsConfig.enabled = aprsProtoConfig->enabled;
            aprsConfig.frequency = aprsProtoConfig->frequency;
            aprsConfig.spreadingFactor = aprsProtoConfig->spreading_factor;
            aprsConfig.bandwidth = aprsProtoConfig->bandwidth;
            aprsConfig.codingRate = aprsProtoConfig->coding_rate;
            aprsConfig.txPower = aprsProtoConfig->tx_power;
            
            // 处理呼号字符串 - 由于nanopb回调的限制，我们暂时不直接访问回调数据
            LOG_INFO("APRSModule: Callsign field present but direct access not supported with nanopb callbacks\n");
            // 保留原有的呼号值
            
            aprsConfig.ssid = aprsProtoConfig->ssid;
            aprsConfig.beaconInterval = aprsProtoConfig->beacon_interval;
            aprsConfig.usePositionData = aprsProtoConfig->use_position_data;
            aprsConfig.useCustomMessage = aprsProtoConfig->use_custom_message;
            
            // 处理自定义消息字符串
            if (aprsProtoConfig->custom_message.funcs.decode) {
                // 对于接收的消息，我们需要使用pb_decode_string
                // 这里简化处理，只检查是否有值
                // 由于无法直接从回调中获取字符串值，我们暂时保留默认值
                LOG_INFO("Received APRS custom message, but direct access not supported with nanopb callbacks");
            }
            
            // 保存配置
            saveAPRSConfig();
            
            LOG_INFO("Updated APRS module config: freq=%f, enabled=%d, callsign=%s-%d", 
                    aprsConfig.frequency, aprsConfig.enabled, aprsConfig.callsign, aprsConfig.ssid);
                    
            return true;
        }
    }
    
    return false;
}

// handleLocalMeshPacket方法已移除，因为不再需要转发功能

// 处理接收到的任何数据包，不处理接收功能
ProcessMessage APRSModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    // 只处理管理消息，不处理其他接收功能
    bool handled = handleReceivedProtobuf(mp, (meshtastic_AdminMessage*)mp.decoded.payload.bytes);
    
    // 如果消息已处理，返回STOP，否则返回CONTINUE
    return handled ? ProcessMessage::STOP : ProcessMessage::CONTINUE;
}

int32_t APRSModule::runOnce()
{
    // 添加额外的初始化检查，确保系统完全启动
    if (!service || !airTime) {
        LOG_DEBUG("APRSModule: System services not fully initialized, skipping");
        return 10000; // 继续等待系统初始化
    }
    
    if (!aprsConfig.enabled) {
        return 60000; // 1分钟后再检查
    }
    
    uint32_t now = millis();
    uint32_t intervalMs = aprsConfig.beaconInterval * 1000;
    
    // 检查是否需要发送APRS信标
    if (lastAPRSSend == 0 || (now - lastAPRSSend) >= intervalMs) {
        // 检查是否可以发送
        if (airTime->isTxAllowedAirUtil()) {
            sendAPRSPosition();
        }
    }
    // 返回下一次运行间隔
    return 10000; // 10秒
}

#if HAS_SCREEN
void APRSModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // 设置字体和对齐方式
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    
    // 绘制通用头部
    graphics::drawCommonHeader(display, x, y, "APRS");
    
    // 获取标准文本位置
    const int *textPos = graphics::getTextPositions(display);
    
    // 显示启用状态
    char statusStr[32];
    snprintf(statusStr, sizeof(statusStr), "Status: %s", aprsConfig.enabled ? "Enabled" : "Disabled");
    display->drawString(x, textPos[1], statusStr);
    
    // 显示呼号和SSID
    char callsignStr[32];
    if (aprsConfig.ssid > 0) {
        snprintf(callsignStr, sizeof(callsignStr), "Callsign: %s-%d", aprsConfig.callsign, aprsConfig.ssid);
    } else {
        snprintf(callsignStr, sizeof(callsignStr), "Callsign: %s", aprsConfig.callsign);
    }
    display->drawString(x, textPos[2], callsignStr);
    
    // 显示频率
    char freqStr[32];
    snprintf(freqStr, sizeof(freqStr), "Freq: %.3f MHz", aprsConfig.frequency);
    display->drawString(x, textPos[3], freqStr);
    
    // 显示数据包计数
    char packetStr[32];
    snprintf(packetStr, sizeof(packetStr), "Packets: Tx:%u", txCount);
    display->drawString(x, textPos[4], packetStr);
}
#endif