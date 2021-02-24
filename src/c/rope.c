#include <string.h>
#include <pebble.h>

/* 1 + (25 * 7) + (25 * 17) + 7 + 4 => 637 (round up to 650)
* 18: string format: "-xxxx,-xxxx,-xxxx\0"
* 7+4: format for timestamp prefix
*/
#define NUM_SAMPLES 25 // number of samples per update (max 25 allowed)
#define DICT_SIZE 1 + (NUM_SAMPLES * 7) + (NUM_SAMPLES * 18) + 7 + 4 + 20 // + 20 for good measure

#define COLOR_GREEN GColorMintGreen
#define COLOR_YELLOW GColorIcterine
#define COLOR_RED GColorMelon

// time used for timer (1 second). Intuitively,
// this should be 1000 ms (duh), but there is some
// delay between when one timer fires and the other
// one is set. Empirically, we observed that the watch
// is ~ 3 s behind in a 10 minutes period (9:57 vs 10:00).
// A value of 997 has been observed to work quite well 
#define TIME_UPDATE_INTERVAL 1000

static bool s_js_ready;

static Window *s_window;
static TextLayer *text_status, *text_packets_sent, *text_packets_lost, *text_count, *text_elapsed;

static int packets_sent = 0, packets_lost = 0, count = 0;

time_t start_time;

char packets_sent_buf[8], packets_lost_buf[8], count_buf[8], elapsed_buf[16];

void update_timer() {
    time_t curr_time, elapsed;
    unsigned int h, m, s;
    app_timer_register(TIME_UPDATE_INTERVAL, update_timer, NULL);

    curr_time = time(NULL);
    elapsed = curr_time - start_time;

    h = elapsed / 3600;
    m = (elapsed % 3600) / 60;
    s = (elapsed % 3600) % 60;

    if (h) { // now handling hours
        snprintf(elapsed_buf, sizeof(elapsed_buf), "%02u:%02u:%02u", h, m, s); 
    }
    else {
        snprintf(elapsed_buf, sizeof(elapsed_buf), "%02u:%02u", m, s); 
    }
    text_layer_set_text(text_elapsed, elapsed_buf);
}



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
	Tuple *count_tuple = dict_find(iter, MESSAGE_KEY_Count);
	Tuple *t = dict_read_first(iter);
	APP_LOG(APP_LOG_LEVEL_INFO, "%p", t);
	if (ready_tuple) {
		APP_LOG(APP_LOG_LEVEL_INFO, "JS is ready");
		s_js_ready = true;
		text_layer_set_text(text_status, "Connected!");
        text_layer_set_background_color(text_status, COLOR_GREEN);
	}
	else if (count_tuple) {
	    APP_LOG(APP_LOG_LEVEL_INFO, "Count update received: +%ld", count_tuple->value->int32);
	    count += count_tuple->value->int32;
        snprintf(count_buf, sizeof(count_buf), "%d", count);
        text_layer_set_text(text_count, count_buf);

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

  // center text, above skips count (elapsed time)
  int y = 32;
  text_elapsed = text_layer_create(GRect(0, (bounds.size.h - x)/2 - y, bounds.size.w, y));
  text_layer_set_text_alignment(text_elapsed, GTextAlignmentCenter);
  text_layer_set_font(text_elapsed, fonts_get_system_font(FONT_KEY_GOTHIC_28));


  text_layer_set_text(text_status, "Connecting...");
  text_layer_set_text(text_packets_sent, "0");
  text_layer_set_text(text_packets_lost, "0");
  text_layer_set_text(text_count, "0"); // placeholder text
  text_layer_set_text(text_elapsed, "00:00"); // placeholder text

  text_layer_set_background_color(text_status, COLOR_YELLOW);
  text_layer_set_background_color(text_packets_sent, COLOR_GREEN);
  text_layer_set_background_color(text_packets_lost, COLOR_RED);
  
  layer_add_child(window_layer, text_layer_get_layer(text_status));
  layer_add_child(window_layer, text_layer_get_layer(text_packets_sent));
  layer_add_child(window_layer, text_layer_get_layer(text_packets_lost));
  layer_add_child(window_layer, text_layer_get_layer(text_count));
  layer_add_child(window_layer, text_layer_get_layer(text_elapsed));

}

static void prv_window_unload(Window *window) {
  text_layer_destroy(text_status);
  text_layer_destroy(text_packets_sent);
  text_layer_destroy(text_packets_lost);
  text_layer_destroy(text_count);
  text_layer_destroy(text_elapsed);
}

static void prv_init(void) {
  s_window = window_create();

  /* register callbacks for sent/failed messages */
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_inbox_received(inbox_received_callback);

  accel_data_service_subscribe(NUM_SAMPLES, accel_data_handler);

  // _set_sampling_fails if called before _service_subscribe

  // Reasonable jumping speed: < 200 bpm => 4 Hz. 25Hz should suffice
  // (25 is also the defult sampling frequency, but we're setting it
  // anyway, just in case we need to change it afterwards
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  start_time = time(NULL);
  app_timer_register(TIME_UPDATE_INTERVAL, update_timer, NULL);

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
