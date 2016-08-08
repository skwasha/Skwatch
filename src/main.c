#include <pebble.h>

#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1
#define KEY_BACKGROUND_COLOR 2
#define KEY_TWENTY_FOUR_HOUR_FORMAT 3

static Window *s_main_window;
static TextLayer *s_time_layer, *s_date_layer;
static TextLayer *s_weather_layer;
static TextLayer *s_battery_layer;
static TextLayer *s_step_layer;
static TextLayer *s_emoji_layer;

static GFont s_time_font;
static GFont s_date_font;
static GFont s_weather_font;

static char s_current_time_buffer[8], s_current_steps_buffer[16], s_emoji_buffer[5];
static int s_step_count = 0, s_step_goal = 0, s_step_average = 0;
static bool twenty_four_hour_format = false;

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
  s_step_goal = (int)health_service_sum_averaged(HealthMetricStepCount,
    start, end, HealthServiceTimeScopeDaily);
}

// Todays current step count
static void get_step_count() {
  s_step_count = (int)health_service_sum_today(HealthMetricStepCount);
}

// Average daily step count for this time of day
static void get_step_average() {
  const time_t start = time_start_of_today();
  const time_t end = time(NULL);
  s_step_average = (int)health_service_sum_averaged(HealthMetricStepCount,
    start, end, HealthServiceTimeScopeDaily);
}

static void display_step_count() {
  int thousands = s_step_count / 1000;
  int hundreds = s_step_count % 1000;
  static char s_emoji[5];

  if(s_step_count >= s_step_average) {
    text_layer_set_text_color(s_step_layer, GColorJaegerGreen);
    text_layer_set_text_color(s_emoji_layer, GColorJaegerGreen);
    text_layer_set_text(s_emoji_layer, "\U0001F60C"); 
//     snprintf(s_emoji, sizeof(s_emoji), "\U0001F60C");
  } else {
    text_layer_set_text_color(s_step_layer, GColorPictonBlue);
    text_layer_set_text_color(s_emoji_layer, GColorPictonBlue);
    text_layer_set_text(s_emoji_layer, "\U0001F4A9");
//     snprintf(s_emoji, sizeof(s_emoji), "\U0001F620");
  }

  if(thousands > 0) {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer),
      "%d,%03d ", thousands, hundreds);
  } else {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer),
      "%d ", hundreds);
  }
//   snprintf(s_emoji_buffer, sizeof(s_emoji_buffer), "%s", s_emoji);
//   text_layer_set_text(s_emoji_layer, s_emoji_buffer);
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

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char s_date_buffer[16];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() == twenty_four_hour_format ?
                                          "%H:%M" : "%I:%M", tick_time);

  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %m/%d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
}

static void set_background_and_text_color(int color) {
  GColor background_color = GColorFromHEX(color);
  window_set_background_color(s_main_window, background_color);
  text_layer_set_text_color(s_time_layer, gcolor_legible_over(background_color));
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];

  // Read tuples for data
  Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);
  Tuple *background_color_t = dict_find(iterator, KEY_BACKGROUND_COLOR);
  Tuple *twenty_four_hour_format_t = dict_find(iterator, KEY_TWENTY_FOUR_HOUR_FORMAT);

  // If all data is available, use it
  if(temp_tuple && conditions_tuple) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%dF", (int)temp_tuple->value->int32);
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);

    // Assemble full string and display
    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s, %s", temperature_buffer, conditions_buffer);
    text_layer_set_text(s_weather_layer, weather_layer_buffer);
  }

  if (background_color_t) {
    int background_color = background_color_t->value->int32;
    persist_write_int(KEY_BACKGROUND_COLOR, background_color);
    set_background_and_text_color(background_color);
  }

  if (twenty_four_hour_format_t) {
    twenty_four_hour_format = twenty_four_hour_format_t->value->int8;
    persist_write_int(KEY_TWENTY_FOUR_HOUR_FORMAT, twenty_four_hour_format);
    update_time();
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
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
  GRect bounds = layer_get_bounds(window_layer);

  if (persist_read_int(KEY_BACKGROUND_COLOR)) {
    int background_color = persist_read_int(KEY_BACKGROUND_COLOR);
    set_background_and_text_color(background_color);
  }

  if (persist_read_bool(KEY_TWENTY_FOUR_HOUR_FORMAT)) {
    twenty_four_hour_format = persist_read_bool(KEY_TWENTY_FOUR_HOUR_FORMAT);
  }

  // Create date TextLayer
  s_date_layer = text_layer_create(
      GRect(0, 68, bounds.size.w, 34));
  text_layer_set_text_color(s_date_layer, GColorLightGray );
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);

  // Create GFont
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BELFAST_34));

  // Apply to TextLayer
  text_layer_set_font(s_date_layer, s_date_font);

  // Add to Window
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Create the TextLayer with specific bounds
  s_time_layer = text_layer_create(
      GRect(0, 34, bounds.size.w, 36));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);

  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BELFAST_34));

  // Apply to TextLayer
  text_layer_set_font(s_time_layer, s_time_font);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Create temperature Layer
  s_weather_layer = text_layer_create(
      GRect(0, 0, bounds.size.w, 36));

  // Style the text
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorSunsetOrange );
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentRight);
  text_layer_set_text(s_weather_layer, "Loading...");

  // Create second custom font, apply it and add to Window
  s_weather_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BELFAST_34));
  text_layer_set_font(s_weather_layer, s_weather_font);
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));

  s_battery_layer = text_layer_create(
      GRect(0, 100, bounds.size.w, 34));
  text_layer_set_text_color(s_battery_layer, GColorLightGray);
  text_layer_set_background_color(s_battery_layer, GColorClear);
  text_layer_set_font(s_battery_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BELFAST_34)));
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);
  text_layer_set_text(s_battery_layer, "100%");

  battery_state_service_subscribe(handle_battery);

  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));

  handle_battery(battery_state_service_peek());
  
  // Create a layer to hold the current step count
  s_step_layer = text_layer_create(
      GRect(0, 134, bounds.size.w - 20, 34));
  s_emoji_layer = text_layer_create(
      GRect(bounds.size.w - 20, 140, 20, 30));
  text_layer_set_text_color(s_step_layer, GColorLightGray);
  text_layer_set_text_color(s_emoji_layer, GColorLightGray);
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_background_color(s_emoji_layer, GColorClear);
  text_layer_set_font(s_step_layer,
                      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BELFAST_34)));
  text_layer_set_font(s_emoji_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentRight);
  text_layer_set_text_alignment(s_emoji_layer, GTextAlignmentCenter);
  
  layer_add_child(window_layer, text_layer_get_layer(s_step_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_emoji_layer));

  // Subscribe to health events if we can
  if(step_data_is_available()) {
    health_service_events_subscribe(health_handler, NULL);
  }
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);

  // Unload GFont
  fonts_unload_custom_font(s_time_font);

  // Destroy weather elements
  text_layer_destroy(s_weather_layer);
  fonts_unload_custom_font(s_weather_font);
  battery_state_service_unsubscribe();
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_step_layer);
  text_layer_destroy(s_emoji_layer);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set the background color
  window_set_background_color(s_main_window, GColorBlack);

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

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
