#include <string.h>
#include <pebble.h>

#define DICT_SIZE 650

static bool s_js_ready;

static Window *s_window;
static TextLayer *s_text_layer;

bool comm_is_js_ready() {
	return s_js_ready;
}

static void outbox_sent_callback(DictionaryIterator *it, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Message sent successfully");
}

static void outbox_failed_callback(DictionaryIterator *it, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send message. Reason %d", reason);
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "MSG RECEIVED");
	Tuple *ready_tuple = dict_find(iter, MESSAGE_KEY_JSReady);
	if (ready_tuple) {
		APP_LOG(APP_LOG_LEVEL_INFO, "JS is ready");
		s_js_ready = true;
	}
}

static void accel_data_handler(AccelData *data,uint32_t num_samples) {
	uint32_t i, ts, ts_prefix;
	char buf[18];
	if (comm_is_js_ready()) {
		DictionaryIterator *iter;

		app_message_outbox_begin(&iter);

		/*
		 * It is quite unlikely that the prefix will vary among
		 * samples in the same batch. A change in the upper 
		 * 32 bits of the timestamp occurs once every 2^32 milliseconds,
		 * hopefully we will not be there to record it (even so,
		 * it can be handled by the server and not by teeny-tiny pebble)
		 */
		ts_prefix = (uint32_t)(data[0].timestamp >> 32);
		dict_write_uint32(iter, MESSAGE_KEY_TimestampPrefix, ts_prefix);

		for (i = 0; i < num_samples; i++) {
			ts = (uint32_t) data[i].timestamp & 0xffffffff;
			snprintf(buf, sizeof(buf), "%d,%d,%d", data[i].x, data[i].y, data[i].z);
			dict_write_cstring(iter, ts, buf);
		}

		APP_LOG(APP_LOG_LEVEL_DEBUG, "%llu", data[num_samples-1].timestamp - data[0].timestamp);

		dict_write_end(iter);
		app_message_outbox_send();
	}
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

  /* register callbacks for sent/failed messages */
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_inbox_received(inbox_received_callback);

  accel_data_service_subscribe(25, accel_data_handler);
  // _set_sampling_fails if called before _service_subscribe
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  /* 1 + (25 * 7) + (25 * 17) + 7 + 4 => 637 (round up to 650)
   * 18: string format: "-xxxx,-xxxx,-xxxx\0"
   * 7+4: format for timestamp prefix
   */
  app_message_open(16, 650);
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
