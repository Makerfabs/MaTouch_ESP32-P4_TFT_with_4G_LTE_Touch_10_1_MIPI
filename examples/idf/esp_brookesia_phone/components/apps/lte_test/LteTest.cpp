#include "LteTest.hpp"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

LV_IMG_DECLARE(eth);

static const char *TAG = "LteTest";

#define LTE_UART_NUM UART_NUM_1
#define LTE_UART_TX_GPIO GPIO_NUM_48
#define LTE_UART_RX_GPIO GPIO_NUM_47
#define LTE_PWRKEY_GPIO GPIO_NUM_46
#define LTE_UART_BAUDRATE 115200
#define LTE_UART_BUF_SIZE 2048
#define LTE_APN "ctnet"

LteTest::LteTest() : ESP_Brookesia_PhoneApp("4G", &eth, true),
                     _screen(nullptr),
                     _main_cont(nullptr),
                     _ui_timer(nullptr),
                     _test_task(nullptr),
                     _data_changed(false),
                     _running(false)
{
    strcpy(_status, "Init...");
    strcpy(_module, "---");
    strcpy(_version, "---");
    strcpy(_sim, "---");
    strcpy(_signal, "---");
    strcpy(_reg, "---");
    strcpy(_operator, "---");
    strcpy(_attach, "---");
    strcpy(_http_result, "---");
}

LteTest::~LteTest()
{
    close();
}

bool LteTest::init(void)
{
    _power_on();

    if (!_uart_init())
    {
        strcpy(_status, "UART init failed");
        return false;
    }

    strcpy(_status, "Ready");
    return true;
}

bool LteTest::close(void)
{
    _running = false;

    if (_ui_timer)
    {
        lv_timer_del(_ui_timer);
        _ui_timer = nullptr;
    }

    return true;
}

bool LteTest::run(void)
{
    _create_ui();

    _running = true;
    _ui_timer = lv_timer_create(_ui_timer_cb, 500, this);

    if (_test_task == nullptr)
    {
        xTaskCreate(_test_task_cb, "lte_test", 4096, this, 5, &_test_task);
    }
    return true;
}

bool LteTest::back(void)
{
    close();
    notifyCoreClosed();
    return true;
}

void LteTest::_power_on()
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << LTE_PWRKEY_GPIO;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(LTE_PWRKEY_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    gpio_set_level(LTE_PWRKEY_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(1500));

    gpio_set_level(LTE_PWRKEY_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(8000));
}

bool LteTest::_uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = LTE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(LTE_UART_NUM, &uart_config);
    if (ret != ESP_OK)
    {
        return false;
    }

    ret = uart_set_pin(
        LTE_UART_NUM,
        LTE_UART_TX_GPIO,
        LTE_UART_RX_GPIO,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE);
    if (ret != ESP_OK)
    {
        return false;
    }

    ret = uart_driver_install(LTE_UART_NUM, LTE_UART_BUF_SIZE, LTE_UART_BUF_SIZE, 0, nullptr, 0);
    return ret == ESP_OK;
}

bool LteTest::_send_at(const char *cmd, char *response, size_t response_size, uint32_t timeout_ms)
{
    memset(response, 0, response_size);
    uart_flush_input(LTE_UART_NUM);

    uart_write_bytes(LTE_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(LTE_UART_NUM, "\r\n", 2);

    int offset = 0;
    int64_t start = esp_timer_get_time() / 1000;

    while (_running && ((esp_timer_get_time() / 1000) - start) < timeout_ms)
    {
        int len = uart_read_bytes(
            LTE_UART_NUM,
            (uint8_t *)response + offset,
            response_size - offset - 1,
            pdMS_TO_TICKS(100));

        if (len > 0)
        {
            offset += len;
            response[offset] = '\0';

            if (strstr(response, "OK") || strstr(response, "ERROR"))
            {
                ESP_LOGI(TAG, "%s -> %s", cmd, response);
                return strstr(response, "OK") != nullptr;
            }
        }

        if (offset >= (int)response_size - 1)
        {
            break;
        }
    }

    ESP_LOGW(TAG, "%s timeout: %s", cmd, response);
    return false;
}

bool LteTest::_send_at_wait(const char *cmd, const char *expect, char *response, size_t response_size, uint32_t timeout_ms)
{
    memset(response, 0, response_size);
    uart_flush_input(LTE_UART_NUM);

    uart_write_bytes(LTE_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(LTE_UART_NUM, "\r\n", 2);

    int offset = 0;
    int64_t start = esp_timer_get_time() / 1000;

    while (_running && ((esp_timer_get_time() / 1000) - start) < timeout_ms)
    {
        int len = uart_read_bytes(
            LTE_UART_NUM,
            (uint8_t *)response + offset,
            response_size - offset - 1,
            pdMS_TO_TICKS(200));

        if (len > 0)
        {
            offset += len;
            response[offset] = '\0';

            if (strstr(response, expect))
            {
                ESP_LOGI(TAG, "%s -> %s", cmd, response);
                return true;
            }

            if (strstr(response, "\r\nERROR\r\n") || strstr(response, "+CME ERROR"))
            {
                ESP_LOGW(TAG, "%s failed: %s", cmd, response);
                return false;
            }
        }

        if (offset >= (int)response_size - 1)
        {
            break;
        }
    }

    ESP_LOGW(TAG, "%s wait timeout: %s", cmd, response);
    return false;
}

void LteTest::_run_test()
{
    char resp[512];

    strcpy(_status, "Testing AT...");
    _data_changed = true;

    bool at_ok = false;
    for (int i = 0; i < 10 && _running; i++)
    {
        if (_send_at("AT", resp, sizeof(resp), 1000)) // 测试
        {
            at_ok = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!at_ok)
    {
        strcpy(_status, "No AT response");
        _data_changed = true;
        return;
    }

    strcpy(_status, "AT OK");
    _data_changed = true;

    _send_at("ATE0", resp, sizeof(resp), 1000);

    if (_send_at("ATI", resp, sizeof(resp), 2000))
    {
        snprintf(_module, sizeof(_module), "%s", resp);
    }
    if (_send_at("AT+SIMCOMATI", resp, sizeof(resp), 2000))
    {
        snprintf(_version, sizeof(_version), "%s", resp);
    }

    if (_send_at("AT+CPIN?", resp, sizeof(resp), 2000))
    {
        if (strstr(resp, "READY"))
        {
            strcpy(_sim, "READY");
        }
        else
        {
            snprintf(_sim, sizeof(_sim), "%s", resp);
        }
    }
    else
    {
        strcpy(_sim, "No SIM response");
    }

    if (_send_at("AT+CSQ", resp, sizeof(resp), 2000))
    {
        snprintf(_signal, sizeof(_signal), "%s", strstr(resp, "+CSQ:") ? strstr(resp, "+CSQ:") : resp);
    }

    if (_send_at("AT+CEREG?", resp, sizeof(resp), 2000))
    {
        snprintf(_reg, sizeof(_reg), "%s", strstr(resp, "+CEREG:") ? strstr(resp, "+CEREG:") : resp);
    }

    if (_send_at("AT+COPS?", resp, sizeof(resp), 5000))
    {
        snprintf(_operator, sizeof(_operator), "%s", strstr(resp, "+COPS:") ? strstr(resp, "+COPS:") : resp);
    }

    if (_send_at("AT+CGATT?", resp, sizeof(resp), 3000))
    {
        snprintf(_attach, sizeof(_attach), "%s", strstr(resp, "+CGATT:") ? strstr(resp, "+CGATT:") : resp);
    }

    char cmd[128];

    ESP_LOGI(TAG, "========== LTE INTERNET TEST START ==========");

    strcpy(_status, "Opening network...");
    _data_changed = true;

    ESP_LOGI(TAG, "Set APN: %s", LTE_APN);
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", LTE_APN);
    _send_at(cmd, resp, sizeof(resp), 3000);

    ESP_LOGI(TAG, "Activate PDP context");
    _send_at("AT+CGACT=1,1", resp, sizeof(resp), 10000);

    ESP_LOGI(TAG, "Close previous network if opened; ERROR here is normal if it was not opened");
    _send_at("AT+NETCLOSE", resp, sizeof(resp), 5000);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Open network");
    if (!_send_at_wait("AT+NETOPEN", "+NETOPEN:", resp, sizeof(resp), 20000))
    {
        ESP_LOGE(TAG, "NETOPEN failed");
        strcpy(_status, "NETOPEN failed");
        _data_changed = true;
        return;
    }

    if (!strstr(resp, "+NETOPEN: 0"))
    {
        ESP_LOGE(TAG, "NETOPEN returned unexpected result: %s", resp);
        strcpy(_status, "NETOPEN failed");
        _data_changed = true;
        return;
    }

    ESP_LOGI(TAG, "Get IP address");
    if (_send_at("AT+IPADDR", resp, sizeof(resp), 5000))
    {
        ESP_LOGI(TAG, "IPADDR response: %s", resp);
        snprintf(_attach, sizeof(_attach), "%s", strstr(resp, "+IPADDR:") ? strstr(resp, "+IPADDR:") : resp);
    }
    else
    {
        ESP_LOGW(TAG, "Get IP address failed");
    }

    ESP_LOGI(TAG, "Terminate old HTTP session if exists; ERROR here is normal if it was not initialized");
    _send_at("AT+HTTPTERM", resp, sizeof(resp), 1000);

    ESP_LOGI(TAG, "Init HTTP");
    if (!_send_at("AT+HTTPINIT", resp, sizeof(resp), 3000))
    {
        ESP_LOGE(TAG, "HTTPINIT failed");
        strcpy(_status, "HTTPINIT failed");
        _data_changed = true;
        return;
    }

    ESP_LOGI(TAG, "Set HTTP URL");
    _send_at("AT+HTTPPARA=\"URL\",\"http://www.baidu.com/\"", resp, sizeof(resp), 3000);

    ESP_LOGI(TAG, "Start HTTP GET");
    strcpy(_status, "HTTP testing...");
    _data_changed = true;

    if (_send_at_wait("AT+HTTPACTION=0", "+HTTPACTION:", resp, sizeof(resp), 30000))
    {
        ESP_LOGI(TAG, "HTTPACTION response: %s", resp);
        snprintf(_http_result, sizeof(_http_result), "%s", strstr(resp, "+HTTPACTION:") ? strstr(resp, "+HTTPACTION:") : resp);

        if (strstr(resp, ",200,") || strstr(resp, ",301,") || strstr(resp, ",302,"))
        {
            ESP_LOGI(TAG, "Internet test passed");
            strcpy(_status, "Internet OK");

            if (_send_at("AT+HTTPREAD=0,256", resp, sizeof(resp), 10000))
            {
                ESP_LOGI(TAG, "HTTPREAD first 256 bytes: %s", resp);
            }
            else
            {
                ESP_LOGW(TAG, "HTTPREAD failed, but HTTPACTION already proved internet access");
            }
        }
        else
        {
            ESP_LOGE(TAG, "HTTP request failed: %s", resp);
            strcpy(_status, "HTTP failed");
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTPACTION timeout");
        strcpy(_status, "HTTP timeout");
    }

    ESP_LOGI(TAG, "Terminate HTTP");
    _send_at("AT+HTTPTERM", resp, sizeof(resp), 3000);

    ESP_LOGI(TAG, "========== LTE INTERNET TEST END ==========");

    _data_changed = true;
}

void LteTest::_create_ui()
{
    _screen = lv_scr_act();
    lv_obj_clean(_screen);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x101010), LV_PART_MAIN);

    // 标题
    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "4G LTE Info");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    _main_cont = lv_obj_create(_screen);
    lv_obj_set_size(_main_cont, LV_PCT(90), LV_PCT(62));
    // lv_obj_align(_main_cont, LV_ALIGN_CENTER, 0, 20);
    lv_obj_align(_main_cont, LV_ALIGN_TOP_MID, 0, 135);
    lv_obj_set_style_bg_color(_main_cont, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(_main_cont, 0, 0);
    lv_obj_set_flex_flow(_main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(_main_cont, 20, 0);
    lv_obj_set_style_pad_gap(_main_cont, 15, 0);

    auto create_row = [this](const char *label_text, lv_obj_t **val_obj)
    {
        lv_obj_t *cont = lv_obj_create(_main_cont);
        // lv_obj_set_size(cont, LV_PCT(100), 40);
        lv_obj_set_size(cont, LV_PCT(100), 50);
        lv_obj_set_style_bg_opa(cont, 0, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_style_pad_all(cont, 0, 0);

        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, label_text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        *val_obj = lv_label_create(cont);
        lv_obj_set_width(*val_obj, LV_PCT(65));
        lv_label_set_long_mode(*val_obj, LV_LABEL_LONG_CLIP);
        lv_label_set_text(*val_obj, "---");
        lv_obj_set_style_text_color(*val_obj, lv_color_white(), 0);
        lv_obj_set_style_text_font(*val_obj, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(*val_obj, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(*val_obj, LV_ALIGN_RIGHT_MID, 0, 0);
    };

    create_row("Status", &_lbl_status_val);
    create_row("Module", &_lbl_module_val);
    create_row("Version", &_lbl_version_val);
    create_row("SIM", &_lbl_sim_val);
    create_row("Signal", &_lbl_signal_val);
    create_row("Register", &_lbl_reg_val);
    create_row("Operator", &_lbl_operator_val);
    create_row("Attach", &_lbl_attach_val);
    create_row("HTTP Result", &_lbl_http_val); // ✅ 这里新加一行！

    // 初始刷新
    _data_changed = true;
    _update_ui();
}

void LteTest::_update_ui()
{
    if (!_screen)
    {
        return;
    }

    lv_label_set_text(_lbl_status_val, _status);
    lv_label_set_text(_lbl_module_val, _module);
    lv_label_set_text(_lbl_version_val, _version);
    lv_label_set_text(_lbl_sim_val, _sim);
    lv_label_set_text(_lbl_signal_val, _signal);
    lv_label_set_text(_lbl_reg_val, _reg);
    lv_label_set_text(_lbl_operator_val, _operator);
    lv_label_set_text(_lbl_attach_val, _attach);
    lv_label_set_text(_lbl_http_val, _http_result); // ✅ 刷新新行
}

void LteTest::_ui_timer_cb(lv_timer_t *timer)
{
    LteTest *app = (LteTest *)timer->user_data;
    if (app->_data_changed)
    {
        app->_update_ui();
        app->_data_changed = false;
    }
}

void LteTest::_test_task_cb(void *arg)
{
    LteTest *app = (LteTest *)arg;
    app->_run_test();
    app->_test_task = nullptr;
    vTaskDelete(nullptr);
}
