#include <string.h>
#include <pebble.h>

static Window *s_window;
static TextLayer *s_text_layer;
static int *count_x, *count_y, *count_z;
static char buf[32];


static uint16_t count_raises(AccelData *data, char axis, uint32_t samples) {
	uint32_t i, value = 0;
	uint16_t was_low = 0, was_high = 0, prev_low = 0, raising_edge = 0, falling_edge = 0;
	for (i = 0; i < samples; i++) {
		switch (axis) {
			case 'x':
				value = data[i].x;
				break;
			case 'y': 
				value = data[i].y;
				break;
			case 'z':
				value = data[i].z;
				break;
			default:
				break;
		}
		if (value > 2000000000) {
			was_high++;
			if (prev_low == 1) {
				// was low, how is high -- keep track!
				if (was_low > 0 && was_low <= 12) { // was low for less than 1/2 second! count it as a raising edge
					raising_edge++;
				}
			}
			prev_low = 0;
			was_low = 0;
		}
		else {
			was_low++;
			if (prev_low == 0) { // was previously high, now going back to low!
				if (was_high > 0 && was_high <= 12) { // was high for less than 1/2 second, count as lowering edge
					falling_edge++;
				}
			}
			prev_low = 1;
			was_high = 0;
		}
	}
	return (raising_edge + falling_edge) / 2;
}


static void accel_data_handler(AccelData *data,uint32_t num_samples) {
	uint16_t a = count_raises(data, 'x', num_samples);
	uint16_t b  = count_raises(data, 'y', num_samples);
	uint16_t c = count_raises(data, 'z', num_samples);
	uint32_t i;
	APP_LOG(APP_LOG_LEVEL_INFO, "=========");
	for (i = 0; i < num_samples; i++) {
		APP_LOG(APP_LOG_LEVEL_INFO, "(%d,%d,%d)", data[i].x, data[i].y, data[i].z);
	}
	(*count_x) += a;
	(*count_y) += b;
	(*count_z) += c;
	snprintf(buf, sizeof(buf), "%d %d %d", *count_x, *count_y, *count_z);
	text_layer_set_text(s_text_layer, buf);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_text_layer, "Select");
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_text_layer, "Up");
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_text_layer, "Down");
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
  text_layer_set_text(s_text_layer, "Press a button");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
  accel_data_service_subscribe(25, accel_data_handler);
  int a = 0, b= 0, c = 0;
  count_x = &a;
  count_y = &b;
  count_z = &c;
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
