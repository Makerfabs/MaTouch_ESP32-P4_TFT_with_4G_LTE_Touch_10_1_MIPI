#include "LcdTest.hpp"

// Declare external image resource
LV_IMG_DECLARE(img_app_lcd_test);

static const char *TAG = "LcdTest";

// 定义测试颜色数组
const LcdTest::color_item_t LcdTest::_colors[] = {
    {"White",   lv_color_hex(0xFFFFFF)},
    {"Red",     lv_color_hex(0xFF0000)},
    {"Green",   lv_color_hex(0x00FF00)},
    {"Blue",    lv_color_hex(0x0000FF)},
    {"Yellow",  lv_color_hex(0xFFFF00)},
    {"Cyan",    lv_color_hex(0x00FFFF)},
    {"Magenta", lv_color_hex(0xFF00FF)},
    {"Black",   lv_color_hex(0x000000)},
    {"Gray",    lv_color_hex(0x808080)},
    {"Orange",  lv_color_hex(0xFF8000)},
    {"Purple",  lv_color_hex(0x8000FF)},
    {"Pink",    lv_color_hex(0xFF80C0)}
};

const int LcdTest::_color_count = sizeof(LcdTest::_colors) / sizeof(LcdTest::_colors[0]);

LcdTest::LcdTest():
    ESP_Brookesia_PhoneApp("Lcd Test", &img_app_lcd_test, true),
    _screen(nullptr),
    _color_panel(nullptr),
    _left_button(nullptr),
    _right_button(nullptr),
    _color_label(nullptr),
    _info_label(nullptr),
    _current_color_index(0)  // 默认从白色开始
{

}

LcdTest::~LcdTest()
{
    // 析构函数中清理资源
}

bool LcdTest::run(void)
{
    _screen = lv_scr_act();
    lv_obj_clean(_screen);

    // 创建全屏颜色面板
    _color_panel = lv_obj_create(_screen);
    lv_obj_set_size(_color_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(_color_panel, 0, 0);
    lv_obj_set_style_border_width(_color_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_color_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_color_panel, 0, LV_PART_MAIN);

    // 创建左切换按钮
    _left_button = lv_btn_create(_screen);
    lv_obj_set_size(_left_button, 60, 100);
    lv_obj_align(_left_button, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_bg_color(_left_button, lv_color_hex(0x2C3E50), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_left_button, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(_left_button, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_left_button, lv_color_hex(0x34495E), LV_PART_MAIN);
    lv_obj_add_event_cb(_left_button, _left_button_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *left_label = lv_label_create(_left_button);
    lv_label_set_text(left_label, "<");
    lv_obj_set_style_text_color(left_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(left_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_center(left_label);

    // 创建右切换按钮
    _right_button = lv_btn_create(_screen);
    lv_obj_set_size(_right_button, 60, 100);
    lv_obj_align(_right_button, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(_right_button, lv_color_hex(0x2C3E50), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_right_button, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(_right_button, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_right_button, lv_color_hex(0x34495E), LV_PART_MAIN);
    lv_obj_add_event_cb(_right_button, _right_button_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *right_label = lv_label_create(_right_button);
    lv_label_set_text(right_label, ">");
    lv_obj_set_style_text_color(right_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(right_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_center(right_label);

    // 创建颜色名称标签
    _color_label = lv_label_create(_screen);
    lv_obj_align(_color_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_font(_color_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_color_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_color_label, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(_color_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(_color_label, 15, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(_color_label, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(_color_label, 8, LV_PART_MAIN);

    // 创建信息标签
    _info_label = lv_label_create(_screen);
    lv_obj_align(_info_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_text_font(_info_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_info_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_info_label, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_text_color(_info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(_info_label, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(_info_label, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(_info_label, 6, LV_PART_MAIN);

    // 初始化显示
    _update_display();
    
    ESP_LOGD(TAG, "LCD Test initialized with %d colors", _color_count);

    return true;
}

bool LcdTest::init(void)
{
    _current_color_index = 0;  // 重置到白色
    return true;
}

bool LcdTest::back(void)
{
    notifyCoreClosed();
    return true;
}

bool LcdTest::close(void)
{
    return true;
}

void LcdTest::_left_button_cb(lv_event_t *e)
{
    LcdTest *instance = static_cast<LcdTest *>(lv_event_get_user_data(e));
    if (instance == nullptr) return;

    if (instance->_current_color_index > 0) {
        instance->_current_color_index--;
        instance->_switch_to_color(instance->_current_color_index);
        ESP_LOGD(TAG, "Switched to previous color: %s", 
                instance->_colors[instance->_current_color_index].name);
    }
}
void LcdTest::_right_button_cb(lv_event_t *e)
{
    LcdTest *instance = static_cast<LcdTest *>(lv_event_get_user_data(e));
    if (instance == nullptr) return;

    if (instance->_current_color_index < instance->_color_count - 1) {
        instance->_current_color_index++;
        instance->_switch_to_color(instance->_current_color_index);
        ESP_LOGD(TAG, "Switched to next color: %s", 
                instance->_colors[instance->_current_color_index].name);
    }
}

void LcdTest::_switch_to_color(int index)
{
    if (index < 0 || index >= _color_count) {
        return;
    }
    
    _current_color_index = index;
    _update_display();
}

void LcdTest::_update_display()
{
    if (!_color_panel || !_color_label || !_info_label) {
        return;
    }
    
    const color_item_t &current_color = _colors[_current_color_index];
    
    // 设置背景颜色
    lv_obj_set_style_bg_color(_color_panel, current_color.color, LV_PART_MAIN);
    
    // 更新颜色名称标签
    lv_label_set_text(_color_label, current_color.name);
    
    // 更新信息标签
    char info_text[64];
    snprintf(info_text, sizeof(info_text), "%d/%d - RGB: 0x%06X", 
            _current_color_index + 1, _color_count, 
            lv_color_to32(current_color.color) & 0xFFFFFF);
    lv_label_set_text(_info_label, info_text);
    
    // 根据背景颜色调整文字颜色以保证可读性
    lv_color_t text_color;
    if (current_color.color.full == lv_color_hex(0x000000).full || 
        current_color.color.full == lv_color_hex(0x0000FF).full ||
        current_color.color.full == lv_color_hex(0x8000FF).full) {
        // 深色背景使用白色文字
        text_color = lv_color_hex(0xFFFFFF);
    } else {
        // 浅色背景使用黑色文字
        text_color = lv_color_hex(0x000000);
    }
    
    // 更新标签背景和文字颜色
    lv_obj_set_style_text_color(_color_label, text_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(_info_label, text_color, LV_PART_MAIN);
    
    // 根据背景调整标签背景透明度
    uint8_t bg_opa = (text_color.full == lv_color_hex(0xFFFFFF).full) ? LV_OPA_70 : LV_OPA_50;
    lv_obj_set_style_bg_opa(_color_label, bg_opa, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_info_label, bg_opa, LV_PART_MAIN);
    
    _update_buttons();
}

void LcdTest::_update_buttons()
{
    if (!_left_button || !_right_button) {
        return;
    }
    
    // 更新左按钮状态
    if (_current_color_index <= 0) {
        // 已经是第一个颜色，禁用左按钮
        lv_obj_set_style_bg_opa(_left_button, LV_OPA_30, LV_PART_MAIN);
        lv_obj_add_state(_left_button, LV_STATE_DISABLED);
    } else {
        lv_obj_set_style_bg_opa(_left_button, LV_OPA_80, LV_PART_MAIN);
        lv_obj_clear_state(_left_button, LV_STATE_DISABLED);
    }
    
    // 更新右按钮状态
    if (_current_color_index >= _color_count - 1) {
        // 已经是最后一个颜色，禁用右按钮
        lv_obj_set_style_bg_opa(_right_button, LV_OPA_30, LV_PART_MAIN);
        lv_obj_add_state(_right_button, LV_STATE_DISABLED);
    } else {
        lv_obj_set_style_bg_opa(_right_button, LV_OPA_80, LV_PART_MAIN);
        lv_obj_clear_state(_right_button, LV_STATE_DISABLED);
    }
}