#pragma once

#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class LcdTest: public ESP_Brookesia_PhoneApp {
    public:
        LcdTest();
        ~LcdTest();

        bool init(void);
        bool close(void);
        bool back(void);

        bool run(void) override;
        
    private:
        typedef struct {
            const char* name;
            lv_color_t color;
        } color_item_t;
        
        lv_obj_t *_screen;
        lv_obj_t *_color_panel;
        lv_obj_t *_left_button;
        lv_obj_t *_right_button;
        lv_obj_t *_color_label;
        lv_obj_t *_info_label;
        
        static const color_item_t _colors[];
        static const int _color_count;
        int _current_color_index;

        static void _left_button_cb(lv_event_t *e);
        static void _right_button_cb(lv_event_t *e);
        
        void _update_display();
        void _update_buttons();
        void _switch_to_color(int index);
};