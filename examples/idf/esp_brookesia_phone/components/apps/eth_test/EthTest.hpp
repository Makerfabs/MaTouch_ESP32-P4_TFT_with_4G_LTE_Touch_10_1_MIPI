#pragma once

#include "esp_brookesia.hpp"
#include "lvgl.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <string>

class NetworkApp : public ESP_Brookesia_PhoneApp {
public:
    NetworkApp();
    ~NetworkApp();

    bool init(void) override;
    bool close(void) override;
    bool run(void) override;
    bool back(void) override;

private:
    // --- UI 相关 ---
    lv_obj_t *_screen;
    lv_obj_t *_main_cont;
    lv_obj_t *_status_panel;
    
    // 显示标签
    lv_obj_t *_lbl_status_val;
    lv_obj_t *_lbl_ip_val;
    lv_obj_t *_lbl_mask_val;
    lv_obj_t *_lbl_gw_val;
    lv_obj_t *_lbl_mac_val;

    lv_timer_t *_ui_timer;

    // --- 网络相关 ---
    esp_netif_t *_eth_netif;
    esp_eth_handle_t _eth_handle;
    
    // 数据缓存（用于从事件回调传递给UI定时器）
    char _ip_str[64];
    char _mask_str[64];
    char _gw_str[64];
    char _status_str[32];
    bool _data_changed; // 数据更新标志

    // --- 内部函数 ---
    void _create_ui();
    void _update_ui();
    static void _ui_timer_cb(lv_timer_t *timer);

    // --- 事件回调 ---
    static void _eth_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data);
    static void _got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data);
};