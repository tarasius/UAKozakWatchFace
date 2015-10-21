/*
Ukrainian Kozak Watchface written by Tarasius ( http://tarasius.name )
*/

#include <pebble.h>
#include <time.h>

#define SCREENSHOT 1
#define DEBUG      0

#define ONE        TRIG_MAX_RATIO
#define THREESIXTY TRIG_MAX_ANGLE

#define CENTER_X    72
#define CENTER_Y    84
#define DOTS_RADIUS 67
#define DOTS_SIZE    1
#define HOUR_RADIUS 40
#define MIN_RADIUS  60

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

static Window *window;
static Layer *background_layer;
static Layer *hands_layer;
static Layer *date_layer;
static GBitmap *background_bitmap;
static BitmapLayer *bitmap_layer;


static GBitmap *battery_images[22];
static BitmapLayer *battery_layer;

static GBitmap *bluetooth_images[2];
static BitmapLayer *bluetooth_layer;

static struct tm *now = NULL;
static int date_wday = -1;
static int date_mday = -1;
static bool was_connected = true;

static GFont *font;
#define DATE_BUFFER_BYTES 32
static char date_buffer[DATE_BUFFER_BYTES];

#if DEBUG
static TextLayer *debug_layer;
#define DEBUG_BUFFER_BYTES 32
static char debug_buffer[DEBUG_BUFFER_BYTES];
#endif
	

const GPathInfo HOUR_POINTS = { 9,
  (GPoint []) {
    {-5,-1},
    {-7,-13},
    {-5,-24},
    {-5,-33},
    {-1,-40},
    {3,-32},
    {3,-22},
    {2,-13},
    {5,-1},
  }
};

static GPath *hour_path;

const GPathInfo MIN_POINTS = { 13,
  (GPoint []) {
    {-4,-1},
    {-7,-16},
	{-5,-24},	
    {-3,-30},
	{-3,-40},	
    {-5,-46},
	{-4,-54},	
    {-1,-60},
    {5,-45},
    {5,-39},
    {7,-27},
    {4,-14},
    {6,-1},
  }
};

static GPath *min_path;

const char WEEKDAY_NAMES[7][5] =  // 3 chars, 1 for utf-8, 1 for terminating 0
  {"Нд",  "Пн",  "Вт",  "Ср",  "Чт",  "Пт",  "Сб" };

void background_layer_update_callback(Layer *layer, GContext* ctx) {
	graphics_context_set_fill_color(ctx, GColorBlack);
 }


void hands_layer_update_callback(Layer *layer, GContext* ctx) {
#if SCREENSHOT
  now->tm_hour = 12;
  now->tm_min = 26;
  now->tm_sec = 36;
#endif

  GPoint center = GPoint(CENTER_X, CENTER_Y);

  // hours and minutes
  int32_t hour_angle = THREESIXTY * (now->tm_hour * 5 + now->tm_min / 12) / 60;
  int32_t min_angle = THREESIXTY * now->tm_min / 60;
  gpath_rotate_to(hour_path, hour_angle);
  gpath_rotate_to(min_path, min_angle);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, min_path);
  gpath_draw_outline(ctx, min_path);
  gpath_draw_filled(ctx, hour_path);
  gpath_draw_outline(ctx, hour_path);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_circle(ctx, center, DOTS_SIZE+4);
  graphics_fill_circle(ctx, center, DOTS_SIZE+3);

  // center dot
  //graphics_context_set_fill_color(ctx, GColorWhite);
  //graphics_fill_circle(ctx, center, DOTS_SIZE);
}

void date_layer_update_callback(Layer *layer, GContext* ctx) {

#if SCREENSHOT
  now->tm_wday = 2;
  now->tm_mday = 7;
#endif

  graphics_context_set_text_color(ctx, GColorBlack);

  // weekday
  graphics_draw_text(ctx,
    WEEKDAY_NAMES[now->tm_wday],
    font,
    GRect(0, 0, 56, 16),
    GTextOverflowModeWordWrap,
    GTextAlignmentLeft,
    NULL);

  // day of month
  strftime(date_buffer, DATE_BUFFER_BYTES, "%e", now);
  graphics_draw_text(ctx,
    date_buffer,
    font,
    GRect(0, 0, 56, 16),
    GTextOverflowModeWordWrap,
    GTextAlignmentRight,
    NULL);

  date_wday = now->tm_wday;
  date_mday = now->tm_mday;
}

void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  now = tick_time;
  layer_mark_dirty(hands_layer);
  if (now->tm_wday != date_wday || now->tm_mday != date_mday)
    layer_mark_dirty(date_layer);
}

void lost_connection_warning(void *);

void handle_bluetooth(bool connected) {
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_images[connected ? 1 : 0]);
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), false);
  if (was_connected && !connected)
      lost_connection_warning((void*) 0);
  was_connected = connected;
}

void lost_connection_warning(void *data) {
  int count = (int) data;
  bool on_off = count & 1;
  // blink icon
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_images[on_off ? 1 : 0]);
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), false);
  // buzz 3 times
  if (count < 6 && !on_off)
    vibes_short_pulse();
  if (count < 50) // blink for 15 seconds
    app_timer_register(300, lost_connection_warning, (void*) (count+1));
  else // restore bluetooth icon
    handle_bluetooth(bluetooth_connection_service_peek());
}

void handle_battery(BatteryChargeState charge_state) {
#if DEBUG
  snprintf(debug_buffer, DEBUG_BUFFER_BYTES, "%s%d%%",  charge_state.is_charging ? "+" : "", charge_state.charge_percent);
  text_layer_set_text(debug_layer, debug_buffer);
#endif
#if SCREENSHOT
  bitmap_layer_set_bitmap(battery_layer, battery_images[14]);
#else
  bitmap_layer_set_bitmap(battery_layer, battery_images[
    (charge_state.is_charging ? 11 : 0) + min(charge_state.charge_percent / 10, 10)]);
#endif
  layer_set_hidden(bitmap_layer_get_layer(battery_layer), false);
}

void handle_init() {
  time_t clock = time(NULL);
  now = localtime(&clock);
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorWhite);

  background_layer = layer_create(GRect(0, 0, 144, 168));
  layer_set_update_proc(background_layer, &background_layer_update_callback);
  layer_add_child(window_get_root_layer(window), background_layer);
	
  background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  bitmap_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(bitmap_layer, background_bitmap);
  layer_add_child(background_layer, bitmap_layer_get_layer(bitmap_layer));
	
  date_layer = layer_create(GRect(44, 10, 58, 18));
  layer_set_update_proc(date_layer, &date_layer_update_callback);
  layer_add_child(background_layer, date_layer);	
  	
  hands_layer = layer_create(layer_get_frame(background_layer));
  layer_set_update_proc(hands_layer, &hands_layer_update_callback);
  layer_add_child(background_layer, hands_layer);

  battery_images[0] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_0);  	
  battery_images[1] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_10);  	
  battery_images[2] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_20);  	
  battery_images[3] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_30);  	
  battery_images[4] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_40);  	
  battery_images[5] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_50);  	
  battery_images[6] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_60);  	
  battery_images[7] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_70);  	
  battery_images[8] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_80);  	
  battery_images[9] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_90);  	
  battery_images[10] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_100);  	
  battery_images[11] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_0);  	
  battery_images[12] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_10);  	
  battery_images[13] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_20);  	
  battery_images[14] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_30);  	
  battery_images[15] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_40);  	
  battery_images[16] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_50);  	
  battery_images[17] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_60);  	
  battery_images[18] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_70);  	
  battery_images[19] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_80);  	
  battery_images[20] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_90);  	
  battery_images[21] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING_100);  		
  	
  battery_layer = bitmap_layer_create(GRect(120, 10, 16, 10));
  layer_add_child(background_layer, bitmap_layer_get_layer(battery_layer));

  bluetooth_images[0] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_OFF);  
  bluetooth_images[1] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_ON);  	
  bluetooth_layer = bitmap_layer_create(GRect(8, 10, 13, 13));
  layer_add_child(background_layer, bitmap_layer_get_layer(bluetooth_layer));

#if DEBUG
  debug_layer = text_layer_create(GRect(0, 0, 32, 16));
  strcpy(debug_buffer, "!");
  text_layer_set_text(debug_layer, debug_buffer);
  text_layer_set_text_color(debug_layer, GColorBlack);
  text_layer_set_background_color(debug_layer, GColorWhite);
  layer_add_child(background_layer, text_layer_get_layer(debug_layer));
#endif
  
  hour_path = gpath_create(&HOUR_POINTS);
  gpath_move_to(hour_path, GPoint(CENTER_X, CENTER_Y));
  min_path = gpath_create(&MIN_POINTS);
  gpath_move_to(min_path, GPoint(CENTER_X, CENTER_Y));

  font = //fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD); 
	  fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_18));

  tick_timer_service_subscribe(MINUTE_UNIT, &handle_tick);
  battery_state_service_subscribe(&handle_battery);
  handle_battery(battery_state_service_peek());
  bluetooth_connection_service_subscribe(&handle_bluetooth);
  handle_bluetooth(bluetooth_connection_service_peek());
}

void handle_deinit() {
  app_message_deregister_callbacks();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  fonts_unload_custom_font(font);
  gpath_destroy(min_path);
  gpath_destroy(hour_path);
#if DEBUG
  text_layer_destroy(debug_layer);
#endif
  layer_destroy(hands_layer);
  bitmap_layer_destroy(battery_layer);
  for (int i = 0; i < 22; i++)
    gbitmap_destroy(battery_images[i]);
  bitmap_layer_destroy(bluetooth_layer);
  for (int i = 0; i < 2; i++)
    gbitmap_destroy(bluetooth_images[i]);
  layer_destroy(background_layer);
  layer_destroy(date_layer);
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
