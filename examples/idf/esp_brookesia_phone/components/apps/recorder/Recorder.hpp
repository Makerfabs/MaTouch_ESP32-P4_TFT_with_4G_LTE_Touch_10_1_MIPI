#pragma once

#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

// --- WAV 结构体定义 (本地定义) ---
typedef struct {
    uint8_t ChunkID[4];       // "RIFF"
    int32_t ChunkSize;        // FileSize - 8
    uint8_t Format[4];        // "WAVE"
    uint8_t Subchunk1ID[4];   // "fmt "
    int32_t Subchunk1Size;    // 16
    int16_t AudioFormat;      // 1 (PCM)
    int16_t NumChannels;      // Channels
    int32_t SampleRate;       // Sample Rate
    int32_t ByteRate;         // Byte Rate
    int16_t BlockAlign;       // Block Align
    int16_t BitsPerSample;    // Bits Per Sample
} wav_header_t;

typedef struct {
    uint8_t SubchunkID[4];    // "data"
    int32_t SubchunkSize;     // Data Size
} wav_subchunk_header_t;
// ----------------------------------------------

class Recorder: public ESP_Brookesia_PhoneApp {
    public:
        Recorder();
        ~Recorder();

        bool init(void);
        bool close(void);
        bool back(void);
        bool run(void) override;
        
    private:
        enum RecorderState {
            STATE_IDLE,
            STATE_RECORDING,
            STATE_PLAYING
        };
        
        // UI对象
        lv_obj_t *_screen;
        lv_obj_t *_btn_record;
        lv_obj_t *_label_record;
        lv_obj_t *_btn_play;
        lv_obj_t *_label_play;
        
        lv_timer_t *_monitor_timer; // 后台资源清理定时器

        // 内部状态
        FILE *_file;
        RecorderState _state;
        volatile bool _playback_finished; // 播放完成标志
        
        // 任务句柄
        TaskHandle_t _rec_task_handle;
        TaskHandle_t _play_task_handle;
        volatile bool _stop_task_flag;

        // 回调
        static void _btn_record_cb(lv_event_t *e);
        static void _btn_play_cb(lv_event_t *e);
        static void _timer_cb(lv_timer_t *timer);

        // 任务函数
        static void rec_task(void *arg);
        static void play_task(void *arg);

        // 核心逻辑
        void _start_recording();
        void _stop_recording();
        void _start_playing();     
        void _stop_playing();
        
        void _write_wav_header();
        void _update_wav_header(int total_bytes);
};