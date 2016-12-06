#pragma once
#include <pebble.h>

#define SETTINGS_KEY 1

// A structure containing our settings
typedef struct ClaySettings {
  GColor BackgroundColor;
//   GColor ForegroundColor;
} __attribute__((__packed__)) ClaySettings;

static void prv_default_settings();
static void prv_load_settings();
static void prv_save_settings();
static void prv_update_display();
static void prv_inbox_received_handler(DictionaryIterator *iter, void *context);
static void prv_inbox_dropped_handler(AppMessageResult reason, void *context);
static void prv_outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context);
static void prv_outbox_sent_handler(DictionaryIterator *iter, void *context);
static void get_step_goal();
static void get_step_count();
static void get_step_average();
static void display_step_count();
static void health_handler(HealthEventType event, void *context);
static void handle_battery(BatteryChargeState charge_state);
static void set_background_and_text_color(int color);
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void update_time();
static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void init(void);
static void deinit(void);