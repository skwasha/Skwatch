#include <pebble.h>
#include "main.h"

#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1
#define KEY_BACKGROUND_COLOR 2

static Window *s_main_window;
static TextLayer *s_time_layer, *s_date_layer;
static TextLayer *s_weather_layer;
static TextLayer *s_battery_layer;
static TextLayer *s_step_layer;

static GFont s_time_font;
static GFont s_date_font;
static GFont s_weather_font;

static char s_current_steps_buffer[16];
static int s_step_count = 0, s_step_goal = 0, s_step_average = 0;

// A struct for our specific settings (see main.h)
ClaySettings settings;

// Initialize the default settings
static void prv_default_settings() {
  settings.BackgroundColor = GColorBlack;
//   settings.ForegroundColor = GColorWhite;
}

// Read settings from persistent storage
static void prv_load_settings() {
  // Load the default settings
  prv_default_settings();
  // Read settings from persistent storage, if they exist
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

// Save the settings to persistent storage
static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
  // Update the display based on new settings
  prv_update_display();
}

// Is step data available?
bool step_data_is_available() {
  return HealthServiceAccessibilityMaskAvailable &
    health_service_metric_accessible(HealthMetricStepCount,
      time_start_of_today(), time(NULL));
}

// Daily step goal
static void get_step_goal() {
  const time_t start = time_start_of_today();
  const time_t end = start + SECONDS_PER_DAY;
  s_step_goal = (int)health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
  APP_LOG(APP_LOG_LEVEL_ERROR, "Step Goal: %d", s_step_goal);
}

// Todays current step count
static void get_step_count() {
  s_step_count = (int)health_service_sum_today(HealthMetricStepCount);
}

// Average daily step count for this time of day
static void get_step_average() {
  const time_t start = time_start_of_today();
  const time_t end = time(NULL);
  s_step_average = (int)health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
}

static void display_step_count() {
  static char s_emoji[5];

  if(s_step_count >= s_step_goal) {
    text_layer_set_text_color(s_step_layer, GColorJaegerGreen);
    snprintf(s_emoji, sizeof(s_emoji), "\U0001F60C");
  } else {
    text_layer_set_text_color(s_step_layer, GColorPictonBlue);
    snprintf(s_emoji, sizeof(s_emoji), "\U0001F620");
  }

  snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer),
      "%d%s", s_step_count, s_emoji);
  text_layer_set_text(s_step_layer, s_current_steps_buffer);
}

static void health_handler(HealthEventType event, void *context) {
  if(event == HealthEventSignificantUpdate) {
    get_step_goal();
  }
  if(event != HealthEventSleepUpdate) {
    get_step_count();
    display_step_count();
  }
}

static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[] = "100%";

  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "charging");
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(s_battery_layer, battery_text);
}

static void set_background_and_text_color(int color) {
  GColor background_color = GColorFromHEX(color);
  window_set_background_color(s_main_window, background_color);
  text_layer_set_text_color(s_time_layer, gcolor_legible_over(background_color));
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char s_date_buffer[16];

  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);

  strftime(s_date_buffer, sizeof(s_date_buffer), "%m/%d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
}

// Update the display elements
static void prv_update_display() {
  // Background color
  window_set_background_color(s_main_window, settings.BackgroundColor);

  // Foreground Color
//   text_layer_set_text_color(s_label_secondtick, settings.ForegroundColor);
//   text_layer_set_text_color(s_label_animations, settings.ForegroundColor);

  // TwentyFourHourFormat
//   if (settings.TwentyFourHourFormat) {
//     text_layer_set_text(s_label_secondtick, "seconds: enabled");
//   } else {
//     text_layer_set_text(s_label_secondtick, "seconds: disabled");
//   }
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];
  int c = 0;

  // Read color preferences
  Tuple *background_color_t = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  if(background_color_t) {
    settings.BackgroundColor = GColorFromHEX(background_color_t->value->int32); 
//     GColor bg_color = GColorFromHEX(background_color_t->value->int32);
//     int background_color = background_color_t->value->int32;
//     set_background_and_text_color(background_color);
  }

  // Read tuples for data
  Tuple *temp_tuple = dict_find(iter, MESSAGE_KEY_Temperature);
  Tuple *conditions_tuple = dict_find(iter, MESSAGE_KEY_Conditions);

  // If all data is available, use it
  if(temp_tuple && conditions_tuple) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°F", (int)temp_tuple->value->int32);
    c = (int)conditions_tuple->value->int32;
    if(c < 300) {
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F329");
    } else if(c < 400){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F326");
    } else if(c < 600){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F327");
    } else if(c < 700){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F328");
    } else if(c < 800){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F32B");
    } else if(c == 800){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F323");
    } else if(c < 804){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F324");
    } else if(c < 900){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F325");
    } else if(c < 910){
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F32A");
    } else {
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", "\U0001F321");
    }
    // Assemble full string and display
    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s%s", temperature_buffer, conditions_buffer);
    text_layer_set_text(s_weather_layer, weather_layer_buffer);
  }

  // Save the new settings to persistent storage
  prv_save_settings();
}

static void prv_inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void prv_outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void prv_outbox_sent_handler(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();

  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) {
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);

    // Send the message!
    app_message_outbox_send();
  }
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_unobstructed_bounds(window_layer);

//   if (persist_read_int(KEY_BACKGROUND_COLOR)) {
//     int background_color = persist_read_int(KEY_BACKGROUND_COLOR);
//     set_background_and_text_color(background_color);
//   }

//   if (persist_read_bool(KEY_TWENTY_FOUR_HOUR_FORMAT)) {
//     twenty_four_hour_format = persist_read_bool(KEY_TWENTY_FOUR_HOUR_FORMAT);
//   }
  
  // Create a layer to hold the current step count
  s_step_layer = text_layer_create(
      GRect(0, 133, bounds.size.w, 35));
  text_layer_set_text_color(s_step_layer, GColorLightGray);
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_font(s_step_layer,
                      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SKWATCH_35)));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentRight);
  
  layer_add_child(window_layer, text_layer_get_layer(s_step_layer));

  // Subscribe to health events if we can
  if(step_data_is_available()) {
    health_service_events_subscribe(health_handler, NULL);
  }

  s_battery_layer = text_layer_create(
      GRect(0, 100, bounds.size.w, 35));
  text_layer_set_text_color(s_battery_layer, GColorLightGray);
  text_layer_set_background_color(s_battery_layer, GColorClear);
  text_layer_set_font(s_battery_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SKWATCH_35)));
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);
  text_layer_set_text(s_battery_layer, "100%");

  battery_state_service_subscribe(handle_battery);

  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));

  handle_battery(battery_state_service_peek());

  // Create date TextLayer
  s_date_layer = text_layer_create(
      GRect(0, 68, bounds.size.w, 35));
  text_layer_set_text_color(s_date_layer, GColorLightGray );
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);

  // Create GFont
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SKWATCH_35));

  // Apply to TextLayer
  text_layer_set_font(s_date_layer, s_date_font);

  // Add to Window
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Create the TextLayer with specific bounds
  s_time_layer = text_layer_create(
      GRect(0, 34, bounds.size.w, 35));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);

  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SKWATCH_35));

  // Apply to TextLayer
  text_layer_set_font(s_time_layer, s_time_font);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Create temperature Layer
  s_weather_layer = text_layer_create(
      GRect(0, 0, bounds.size.w, 35));

  // Style the text
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorSunsetOrange );
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentRight);
  text_layer_set_text(s_weather_layer, "Loading...");

  // Create second custom font, apply it and add to Window
  s_weather_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SKWATCH_35));
  text_layer_set_font(s_weather_layer, s_weather_font);
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
  
  prv_update_display(); 
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  layer_destroy(text_layer_get_layer(s_time_layer));
  layer_destroy(text_layer_get_layer(s_date_layer));
  layer_destroy(text_layer_get_layer(s_weather_layer));
  layer_destroy(text_layer_get_layer(s_battery_layer));
  layer_destroy(text_layer_get_layer(s_step_layer));

  // Unload GFont
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_date_font);
  fonts_unload_custom_font(s_weather_font);

  battery_state_service_unsubscribe();
}

static void init() {
  prv_load_settings();
 
  // Register handlers
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_register_inbox_dropped(prv_inbox_dropped_handler);
  app_message_register_outbox_failed(prv_outbox_failed_handler);
  app_message_register_outbox_sent(prv_outbox_sent_handler);

  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set the background color
//   window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start
  update_time();

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit() {
  // Destroy Window
  if (s_main_window) {
    window_destroy(s_main_window);
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
