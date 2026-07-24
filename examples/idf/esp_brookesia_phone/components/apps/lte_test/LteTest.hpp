#pragma once

#include "esp_brookesia.hpp"
#include "lvgl.h"
#include "driver/uart.h"
#include "driver/gpio.h"

class LteTest : public ESP_Brookesia_PhoneApp {
public:
    LteTest();
    ~LteTest();

    bool init(void) override;
    bool close(void) override;
    bool run(void) override;
    bool back(void) override;

private:
    lv_obj_t *_screen;
    lv_obj_t *_main_cont;

    lv_obj_t *_lbl_status_val;
    lv_obj_t *_lbl_module_val;
    lv_obj_t *_lbl_version_val;
    lv_obj_t *_lbl_sim_val;
    lv_obj_t *_lbl_signal_val;
    lv_obj_t *_lbl_reg_val;
    lv_obj_t *_lbl_operator_val;
    lv_obj_t *_lbl_attach_val;
    lv_obj_t *_lbl_http_val;

    lv_timer_t *_ui_timer;
    TaskHandle_t _test_task;

    char _status[64];
    char _module[96];
    char _version[256];
    char _sim[64];
    char _signal[64];
    char _reg[64];
    char _operator[96];
    char _attach[64];
    char _http_result[128];

    bool _data_changed;
    bool _running;

    void _create_ui();
    void _update_ui();

    void _power_on();
    bool _uart_init();
    bool _send_at(const char *cmd, char *response, size_t response_size, uint32_t timeout_ms);
    bool _send_at_wait(const char *cmd, const char *expect, char *response, size_t response_size, uint32_t timeout_ms);
    void _run_test();

    static void _ui_timer_cb(lv_timer_t *timer);
    static void _test_task_cb(void *arg);
};