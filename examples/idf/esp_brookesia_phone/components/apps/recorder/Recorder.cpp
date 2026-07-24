#include "Recorder.hpp"
#include "bsp_board_extra.h"
#include <sys/unistd.h>
#include <sys/stat.h>

LV_IMG_DECLARE(img_app_recorder);

static const char *TAG = "Recorder";

#define SAMPLE_RATE     16000
#define BIT_DEPTH       16
#define CHANNELS        1
#define RECORD_FILE     "/sdcard/record/record.wav"
#define RECORD_DIR      "/sdcard/record"
#define I2S_BUF_SIZE    2048

Recorder::Recorder():
    ESP_Brookesia_PhoneApp("Recorder", &img_app_recorder, true),
    _screen(nullptr), _btn_record(nullptr), _label_record(nullptr),
    _btn_play(nullptr), _label_play(nullptr), _monitor_timer(nullptr),
    _file(nullptr), _state(STATE_IDLE), _playback_finished(false),
    _rec_task_handle(nullptr), _play_task_handle(nullptr), _stop_task_flag(false)
{
}

Recorder::~Recorder()
{
    close();
}

bool Recorder::init(void)
{
    bsp_extra_codec_init();
    
    struct stat st = {0};
    if (stat(RECORD_DIR, &st) == -1) {
        mkdir(RECORD_DIR, 0775);
    }
    return true; 
}

bool Recorder::close(void)
{
    if (_monitor_timer) {
        lv_timer_del(_monitor_timer);
        _monitor_timer = nullptr;
    }
    if (_state == STATE_RECORDING) _stop_recording();
    if (_state == STATE_PLAYING) _stop_playing();
    
    bsp_extra_codec_dev_stop();
    return true;
}

bool Recorder::back(void)
{
    close();
    notifyCoreClosed();
    return true;
}

bool Recorder::run(void)
{
    _screen = lv_scr_act();
    lv_obj_clean(_screen);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x000000), LV_PART_MAIN);

    // 创建一个垂直布局的容器
    lv_obj_t *cont = lv_obj_create(_screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_row(cont, 40, 0);

    // 1. 录音按钮
    _btn_record = lv_btn_create(cont);
    lv_obj_set_size(_btn_record, 200, 80);
    lv_obj_set_style_bg_color(_btn_record, lv_color_hex(0xff0000), 0); // 红色
    lv_obj_add_event_cb(_btn_record, _btn_record_cb, LV_EVENT_CLICKED, this);

    _label_record = lv_label_create(_btn_record);
    lv_label_set_text(_label_record, "RECORD"); 
    lv_obj_set_style_text_font(_label_record, &lv_font_montserrat_24, 0);
    lv_obj_center(_label_record);

    // 2. 播放按钮
    _btn_play = lv_btn_create(cont);
    lv_obj_set_size(_btn_play, 200, 80);
    lv_obj_set_style_bg_color(_btn_play, lv_color_hex(0x00ff00), 0); // 绿色
    lv_obj_add_event_cb(_btn_play, _btn_play_cb, LV_EVENT_CLICKED, this);

    _label_play = lv_label_create(_btn_play);
    lv_label_set_text(_label_play, "PLAY"); // 始终显示 PLAY
    lv_obj_set_style_text_font(_label_play, &lv_font_montserrat_24, 0);
    lv_obj_center(_label_play);

    // 启动定时器，仅用于后台资源清理，不操作UI
    _monitor_timer = lv_timer_create(_timer_cb, 200, this);

    return true;
}

void Recorder::_timer_cb(lv_timer_t *timer)
{
    Recorder *app = (Recorder *)timer->user_data;
    
    if (app->_playback_finished) {
        app->_stop_playing(); 
        app->_playback_finished = false;
    }
}

void Recorder::_btn_record_cb(lv_event_t *e)
{
    Recorder *app = (Recorder *)lv_event_get_user_data(e);
    if (!app) return;

    if (app->_state == STATE_IDLE) {
        app->_start_recording();
        lv_label_set_text(app->_label_record, "STOP");
        
        lv_obj_add_state(app->_btn_play, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(app->_btn_play, 100, 0);
    } 
    else if (app->_state == STATE_RECORDING) {
        app->_stop_recording();
        lv_label_set_text(app->_label_record, "RECORD");
        
        lv_obj_clear_state(app->_btn_play, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(app->_btn_play, 255, 0);
    }
}

void Recorder::_btn_play_cb(lv_event_t *e)
{
    Recorder *app = (Recorder *)lv_event_get_user_data(e);
    if (!app) return;

    if (app->_state == STATE_IDLE) {
        app->_start_playing();

        if (app->_state == STATE_PLAYING) {
            lv_obj_add_state(app->_btn_record, LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(app->_btn_record, 100, 0);
        }
    } 
    else if (app->_state == STATE_PLAYING) {
        app->_stop_playing();
        
        lv_obj_clear_state(app->_btn_record, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(app->_btn_record, 255, 0);
    }
}

void Recorder::_start_recording()
{
    _file = fopen(RECORD_FILE, "wb");
    if (!_file) {
        ESP_LOGE(TAG, "File open failed");
        return;
    }
    
    _write_wav_header(); 
    _stop_task_flag = false;
    
    bsp_extra_codec_mute_set(false);
    bsp_extra_codec_set_fs(SAMPLE_RATE, BIT_DEPTH, I2S_SLOT_MODE_MONO);

    _state = STATE_RECORDING;
    xTaskCreatePinnedToCore(rec_task, "rec_task", 4096, this, 5, &_rec_task_handle, 1);
}

void Recorder::_stop_recording()
{
    _stop_task_flag = true;
    while (_rec_task_handle != nullptr) vTaskDelay(1); 

    if (_file) {
        fseek(_file, 0, SEEK_END);
        int total = ftell(_file) - 44; 
        _update_wav_header(total);
        fclose(_file);
        _file = nullptr;
    }
    _state = STATE_IDLE;
}

void Recorder::_start_playing()
{
    _file = fopen(RECORD_FILE, "rb");
    if (!_file) return;

    // wav_header_t h;
    // fread(&h, 1, sizeof(h), _file); 
    // fseek(_file, 44, SEEK_SET);     

    // i2s_slot_mode_t mode = (h.NumChannels == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
    // bsp_extra_codec_mute_set(false);
    // bsp_extra_codec_set_fs(h.SampleRate, h.BitsPerSample, mode);

    _stop_task_flag = false;
    _playback_finished = false;
    _state = STATE_PLAYING;
    xTaskCreatePinnedToCore(play_task, "play_task", 4096, this, 5, &_play_task_handle, 1);
}

void Recorder::_stop_playing()
{
    _stop_task_flag = true;
    if (_play_task_handle) {
        vTaskDelete(_play_task_handle);
        _play_task_handle = nullptr;
    }

    if (_file) {
        fclose(_file);
        _file = nullptr;
    }
    
    _state = STATE_IDLE;

    if (_btn_record) {
        lv_obj_clear_state(_btn_record, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(_btn_record, 255, 0);
    }
}

void Recorder::rec_task(void *arg)
{
    Recorder *app = (Recorder *)arg;
    uint8_t *buf = (uint8_t *)malloc(I2S_BUF_SIZE);
    size_t bytes_read;

    while (!app->_stop_task_flag) {
        if (bsp_extra_i2s_read(buf, I2S_BUF_SIZE, &bytes_read, 100) == ESP_OK) {
            if (bytes_read > 0) fwrite(buf, 1, bytes_read, app->_file);
        }
    }
    free(buf);
    app->_rec_task_handle = nullptr;
    vTaskDelete(NULL);
}

void Recorder::play_task(void *arg)
{
    Recorder *app = (Recorder *)arg;
    uint8_t *buf = (uint8_t *)malloc(I2S_BUF_SIZE);
    size_t read_len, written;

    while (!app->_stop_task_flag) {
        read_len = fread(buf, 1, I2S_BUF_SIZE, app->_file);
        if (read_len > 0) {
            bsp_extra_i2s_write(buf, read_len, &written, 100);
        } else {
            app->_playback_finished = true;
            break;
        }
    }
    free(buf);
    vTaskSuspend(NULL);
}

// --- WAV 辅助函数 ---

void Recorder::_write_wav_header()
{
    wav_header_t h = {{'R','I','F','F'}, 36, {'W','A','V','E'}, {'f','m','t',' '}, 16, 1, CHANNELS, SAMPLE_RATE, SAMPLE_RATE*CHANNELS*BIT_DEPTH/8, CHANNELS*BIT_DEPTH/8, BIT_DEPTH};
    wav_subchunk_header_t d = {{'d','a','t','a'}, 0};
    fwrite(&h, sizeof(h), 1, _file);
    fwrite(&d, sizeof(d), 1, _file);
}

void Recorder::_update_wav_header(int total_bytes)
{
    int32_t val = total_bytes + 36;
    fseek(_file, 4, SEEK_SET);
    fwrite(&val, 4, 1, _file); 
    val = total_bytes;
    fseek(_file, 40, SEEK_SET);
    fwrite(&val, 4, 1, _file); 
}