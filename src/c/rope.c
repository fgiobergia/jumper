#include <string.h>
#include <pebble.h>

/* 1 + (25 * 7) + (25 * 17) + 7 + 4 => 637 (round up to 650)
* 18: string format: "-xxxx,-xxxx,-xxxx\0"
* 7+4: format for timestamp prefix
*/
#define DICT_SIZE 650

#define COLOR_GREEN GColorMintGreen
#define COLOR_YELLOW GColorIcterine
#define COLOR_RED GColorMelon

static bool s_js_ready;

static Window *s_window;
static TextLayer *text_status, *text_packets_sent, *text_packets_lost, *text_count;

static int packets_sent = 0, packets_lost = 0;

char packets_sent_buf[18], packets_lost_buf[18];

bool comm_is_js_ready() {
	return s_js_ready;
}

static void outbox_sent_callback(DictionaryIterator *it, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Message sent successfully");
	packets_sent++;
	snprintf(packets_sent_buf, sizeof(packets_sent_buf), "%d", packets_sent);
	text_layer_set_text(text_packets_sent, packets_sent_buf);
}

static void outbox_failed_callback(DictionaryIterator *it, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send message. Reason %d", reason);
	packets_lost++;
	snprintf(packets_lost_buf, sizeof(packets_lost_buf), "%d", packets_lost);
	text_layer_set_text(text_packets_lost, packets_lost_buf);
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "MSG RECEIVED");
	Tuple *ready_tuple = dict_find(iter, MESSAGE_KEY_JSReady);
	if (ready_tuple) {
		APP_LOG(APP_LOG_LEVEL_INFO, "JS is ready");
		s_js_ready = true;
		text_layer_set_text(text_status, "Connected!");
        text_layer_set_background_color(text_status, COLOR_GREEN);
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

		dict_write_end(iter);
		app_message_outbox_send();
	}
}


static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // bottom text (connecting... connected... failed)
  text_status = text_layer_create(GRect(0, bounds.size.h - 28, bounds.size.w, 24));
  text_layer_set_text_alignment(text_status, GTextAlignmentCenter);
  text_layer_set_font(text_status, fonts_get_system_font(FONT_KEY_GOTHIC_18));

  // top left text  (# packets sent successfully)
  text_packets_sent = text_layer_create(GRect(0, 0, bounds.size.w / 2, 28));
  text_layer_set_text_alignment(text_packets_sent, GTextAlignmentCenter);
  text_layer_set_font(text_packets_sent, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

  // top right text (# packets failed)
  text_packets_lost = text_layer_create(GRect(bounds.size.w / 2, 0, bounds.size.w / 2, 28));
  text_layer_set_text_alignment(text_packets_lost, GTextAlignmentCenter);
  text_layer_set_font(text_packets_lost, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));

  // center text (skips count)
  int x = 52;
  text_count = text_layer_create(GRect(0, (bounds.size.h - x)/2, bounds.size.w, x));
  text_layer_set_text_alignment(text_count, GTextAlignmentCenter);
  text_layer_set_font(text_count, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));


  text_layer_set_text(text_status, "Connecting...");
  text_layer_set_text(text_packets_sent, "0");
  text_layer_set_text(text_packets_lost, "0");
  text_layer_set_text(text_count, "1234"); // placeholder text

  text_layer_set_background_color(text_status, COLOR_YELLOW);
  text_layer_set_background_color(text_packets_sent, COLOR_GREEN);
  text_layer_set_background_color(text_packets_lost, COLOR_RED);
  
  layer_add_child(window_layer, text_layer_get_layer(text_status));
  layer_add_child(window_layer, text_layer_get_layer(text_packets_sent));
  layer_add_child(window_layer, text_layer_get_layer(text_packets_lost));
  layer_add_child(window_layer, text_layer_get_layer(text_count));

}

static void prv_window_unload(Window *window) {
  text_layer_destroy(text_status);
  text_layer_destroy(text_packets_sent);
  text_layer_destroy(text_packets_lost);
}

static void prv_init(void) {
  s_window = window_create();

  /* register callbacks for sent/failed messages */
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_inbox_received(inbox_received_callback);

  accel_data_service_subscribe(25, accel_data_handler);

  // _set_sampling_fails if called before _service_subscribe
  // Reasonable jumping speed: < 200 bpm => 4 Hz. 25Hz should suffice
  // (25 is also the defult sampling frequency, but we're setting it
  // anyway, just in case we need to change it afterwards
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  app_message_open(16, DICT_SIZE);
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
