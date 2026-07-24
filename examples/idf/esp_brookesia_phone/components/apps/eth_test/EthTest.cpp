#include "EthTest.hpp"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"

LV_IMG_DECLARE(eth);

static const char *TAG = "NetworkApp";

#define ETH_PHY_ADDR 1
#define ETH_PHY_RST_GPIO 51
#define ETH_MDC_GPIO 31
#define ETH_MDIO_GPIO 52

NetworkApp::NetworkApp() : ESP_Brookesia_PhoneApp("Eth", &eth, true),
                           _screen(nullptr), _ui_timer(nullptr),
                           _eth_netif(nullptr), _eth_handle(nullptr),
                           _data_changed(false)
{
    // 初始化默认文本
    strcpy(_ip_str, "0.0.0.0");
    strcpy(_mask_str, "0.0.0.0");
    strcpy(_gw_str, "0.0.0.0");
    strcpy(_status_str, "Init...");
}

NetworkApp::~NetworkApp()
{
    close();
}

bool NetworkApp::init(void)
{
    esp_err_t ret = ESP_OK;

    // 1. 初始化 TCP/IP 堆栈 (如果系统已初始化，此调用会直接返回)
    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_netif_init failed");
        return false;
    }

    // 2. 创建默认事件循环 (如果系统已创建，需忽略错误)
    esp_event_loop_create_default();

    // 3. 创建默认的以太网 Netif
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    _eth_netif = esp_netif_new(&cfg);

    // 4. 配置以太网 MAC (ESP32-P4 Internal EMAC)
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 4096;

    // eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // esp32_emac_config.smi_mdc_gpio_num = ETH_MDC_GPIO;
    // esp32_emac_config.smi_mdio_gpio_num = ETH_MDIO_GPIO;

    eth_esp32_emac_config_t esp32_emac_config = {};
    esp32_emac_config.smi_gpio.mdc_num = ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = ETH_MDIO_GPIO;
    esp32_emac_config.interface = EMAC_DATA_INTERFACE_RMII;
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    esp32_emac_config.clock_config.rmii.clock_gpio = (emac_rmii_clock_gpio_t)50;
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_32;
    esp32_emac_config.intr_priority = 0;
    esp32_emac_config.emac_dataif_gpio.rmii.tx_en_num = 49;
    esp32_emac_config.emac_dataif_gpio.rmii.txd0_num = 34;
    esp32_emac_config.emac_dataif_gpio.rmii.txd1_num = 35;
    esp32_emac_config.emac_dataif_gpio.rmii.crs_dv_num = 28;
    esp32_emac_config.emac_dataif_gpio.rmii.rxd0_num = 29;
    esp32_emac_config.emac_dataif_gpio.rmii.rxd1_num = 30;
    esp32_emac_config.clock_config_out_in.rmii.clock_mode = EMAC_CLK_EXT_IN;
    esp32_emac_config.clock_config_out_in.rmii.clock_gpio = (emac_rmii_clock_gpio_t)-1;
    esp32_emac_config.mdc_freq_hz = 0;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

    // 5. 配置以太网 PHY (IP101)
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = ETH_PHY_ADDR;
    phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    // 6. 安装驱动
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&config, &_eth_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ETH driver install failed");
        return false;
    }

    // 7. 将以太网驱动附加到 Netif
    esp_netif_attach(_eth_netif, esp_eth_new_netif_glue(_eth_handle));

    // 8. 注册事件处理
    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &_eth_event_handler, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &_got_ip_event_handler, this);

    // 9. 启动以太网
    esp_eth_start(_eth_handle);

    strcpy(_status_str, "Starting...");
    return true;
}

bool NetworkApp::close(void)
{
    if (_ui_timer)
    {
        lv_timer_del(_ui_timer);
        _ui_timer = nullptr;
    }

    if (_eth_handle)
    {
        esp_eth_stop(_eth_handle);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &_got_ip_event_handler);
        esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &_eth_event_handler);
        // 注意：通常不建议在应用退出时完全销毁 Netif 和驱动，
        // 除非你确定下次进入会重新创建。为简化，这里只做停止。
    }

    return true;
}

bool NetworkApp::run(void)
{
    _create_ui();
    _ui_timer = lv_timer_create(_ui_timer_cb, 500, this);
    return true;
}

bool NetworkApp::back(void)
{
    close();
    notifyCoreClosed();
    return true;
}

// --- UI 创建与更新 ---

void NetworkApp::_create_ui()
{
    _screen = lv_scr_act();
    lv_obj_clean(_screen);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x101010), LV_PART_MAIN);

    // 标题
    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "Ethernet Info");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    _main_cont = lv_obj_create(_screen);
    lv_obj_set_size(_main_cont, LV_PCT(90), LV_PCT(70));
    lv_obj_align(_main_cont, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(_main_cont, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(_main_cont, 0, 0);
    lv_obj_set_flex_flow(_main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(_main_cont, 20, 0);
    lv_obj_set_style_pad_gap(_main_cont, 15, 0);

    auto create_row = [this](const char *label_text, lv_obj_t **val_obj)
    {
        lv_obj_t *cont = lv_obj_create(_main_cont);
        lv_obj_set_size(cont, LV_PCT(100), 40);
        lv_obj_set_style_bg_opa(cont, 0, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_style_pad_all(cont, 0, 0);

        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, label_text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        *val_obj = lv_label_create(cont);
        lv_label_set_text(*val_obj, "---");
        lv_obj_set_style_text_color(*val_obj, lv_color_white(), 0);
        lv_obj_set_style_text_font(*val_obj, &lv_font_montserrat_16, 0);
        lv_obj_align(*val_obj, LV_ALIGN_RIGHT_MID, 0, 0);
    };

    create_row("Status", &_lbl_status_val);
    create_row("IP Address", &_lbl_ip_val);
    create_row("Netmask", &_lbl_mask_val);
    create_row("Gateway", &_lbl_gw_val);

    // 初始刷新
    _data_changed = true;
    _update_ui();
}

void NetworkApp::_update_ui()
{
    if (!_screen)
        return;

    lv_label_set_text(_lbl_status_val, _status_str);

    // 根据状态改变颜色
    if (strcmp(_status_str, "Connected") == 0)
    {
        lv_obj_set_style_text_color(_lbl_status_val, lv_color_hex(0x00ff00), 0); // Green
    }
    else
    {
        lv_obj_set_style_text_color(_lbl_status_val, lv_color_hex(0xffaa00), 0); // Orange
    }

    lv_label_set_text(_lbl_ip_val, _ip_str);
    lv_label_set_text(_lbl_mask_val, _mask_str);
    lv_label_set_text(_lbl_gw_val, _gw_str);
}

void NetworkApp::_ui_timer_cb(lv_timer_t *timer)
{
    NetworkApp *app = (NetworkApp *)timer->user_data;
    if (app->_data_changed)
    {
        app->_update_ui();
        app->_data_changed = false;
    }
}

void NetworkApp::_eth_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    NetworkApp *app = (NetworkApp *)arg;

    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        strcpy(app->_status_str, "Link Up");
        ESP_LOGI(TAG, "Ethernet Link Up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        strcpy(app->_status_str, "Disconnected");
        strcpy(app->_ip_str, "0.0.0.0"); // 断开时清空 IP
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        strcpy(app->_status_str, "Started");
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        strcpy(app->_status_str, "Stopped");
        break;
    default:
        break;
    }
    app->_data_changed = true;
}

void NetworkApp::_got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
    NetworkApp *app = (NetworkApp *)arg;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    // 将 IP 转换为字符串并保存
    snprintf(app->_ip_str, sizeof(app->_ip_str), IPSTR, IP2STR(&ip_info->ip));
    snprintf(app->_mask_str, sizeof(app->_mask_str), IPSTR, IP2STR(&ip_info->netmask));
    snprintf(app->_gw_str, sizeof(app->_gw_str), IPSTR, IP2STR(&ip_info->gw));

    strcpy(app->_status_str, "Connected");

    ESP_LOGI(TAG, "Ethernet Got IP: %s", app->_ip_str);

    app->_data_changed = true;
}