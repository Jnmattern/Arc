#include <pebble.h>

#define OUTER_CIRCLE_THICKNESS 15
#define INNER_CIRCLE_THICKNESS 15
#define SPACE 5

#define INFO_DURATION 1000
#define INFO_LONG_DURATION 2000

// Languages
#define LANG_DUTCH 0
#define LANG_ENGLISH 1
#define LANG_FRENCH 2
#define LANG_GERMAN 3
#define LANG_SPANISH 4
#define LANG_PORTUGUESE 5
#define LANG_SWEDISH 6
#define LANG_MAX 7

enum {
	CONFIG_KEY_DATEORDER = 1852,
	CONFIG_KEY_LANG = 1853,
	CONFIG_KEY_BACKLIGHT = 1854
};


static Window *window;
static Layer *rootLayer;
static Layer *layer;

static int outerCircleOuterRadius = 71, outerCircleInnerRadius;
static int innerCircleOuterRadius, innerCircleInnerRadius ;

static int angle_90 = TRIG_MAX_ANGLE / 4;
static int angle_180 = TRIG_MAX_ANGLE / 2;
static int angle_270 = 3 * TRIG_MAX_ANGLE / 4;

static int32_t min_a, min_a1, min_a2, hour_a, hour_a1, hour_a2;
static int32_t minutesWidth = TRIG_MAX_ANGLE / 50;
static int32_t hourWidth = TRIG_MAX_ANGLE / 24;

static GRect textFrame;
static TextLayer *textLayer;
static char date[20], info[20];
static GFont font;

static int step = 0;

static const GPoint center = { 72, 84 };

// Days of the week in all languages
static int curLang = LANG_ENGLISH;
static int USDate = 1;
static int backlight = 0;

const char weekDay[LANG_MAX][7][6] = {
	{ "zon", "maa", "din", "woe", "don", "vri", "zat" },	// Dutch
	{ "sun", "mon", "tue", "wed", "thu", "fri", "sat" },	// English
	{ "dim", "lun", "mar", "mer", "jeu", "ven", "sam" },	// French
	{ "son", "mon", "die", "mit", "don", "fre", "sam" },	// German
	{ "dom", "lun", "mar", "mie", "jue", "vie", "sab" },	// Spanish
	{ "dom", "seg", "ter", "qua", "qui", "sex", "sab" },	// Portuguese
	{ "sön", "mån", "Tis", "ons", "tor", "fre", "lör" }	// Swedish
};


/*\
|*| DrawArc function thanks to Cameron MacFarland (http://forums.getpebble.com/profile/12561/Cameron%20MacFarland)
\*/
static void graphics_draw_arc(GContext *ctx, GPoint center, int radius, int thickness, int start_angle, int end_angle, GColor c) {
	int32_t xmin = 65535000, xmax = -65535000, ymin = 65535000, ymax = -65535000;
	int32_t cosStart, sinStart, cosEnd, sinEnd;
	int32_t r, t;
	
	while (start_angle < 0) start_angle += TRIG_MAX_ANGLE;
	while (end_angle < 0) end_angle += TRIG_MAX_ANGLE;

	start_angle %= TRIG_MAX_ANGLE;
	end_angle %= TRIG_MAX_ANGLE;
	
	if (end_angle == 0) end_angle = TRIG_MAX_ANGLE;
	
	if (start_angle > end_angle) {
		graphics_draw_arc(ctx, center, radius, thickness, start_angle, TRIG_MAX_ANGLE, c);
		graphics_draw_arc(ctx, center, radius, thickness, 0, end_angle, c);
	} else {
		// Calculate bounding box for the arc to be drawn
		cosStart = cos_lookup(start_angle);
		sinStart = sin_lookup(start_angle);
		cosEnd = cos_lookup(end_angle);
		sinEnd = sin_lookup(end_angle);
		
		r = radius;
		// Point 1: radius & start_angle
		t = r * cosStart;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinStart;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;

		// Point 2: radius & end_angle
		t = r * cosEnd;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinEnd;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;
		
		r = radius - thickness;
		// Point 3: radius-thickness & start_angle
		t = r * cosStart;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinStart;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;

		// Point 4: radius-thickness & end_angle
		t = r * cosEnd;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinEnd;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;
		
		// Normalization
		xmin /= TRIG_MAX_RATIO;
		xmax /= TRIG_MAX_RATIO;
		ymin /= TRIG_MAX_RATIO;
		ymax /= TRIG_MAX_RATIO;
				
		// Corrections if arc crosses X or Y axis
		if ((start_angle < angle_90) && (end_angle > angle_90)) {
			ymax = radius;
		}
		
		if ((start_angle < angle_180) && (end_angle > angle_180)) {
			xmin = -radius;
		}
		
		if ((start_angle < angle_270) && (end_angle > angle_270)) {
			ymin = -radius;
		}
		
		// Slopes for the two sides of the arc
		float sslope = (float)cosStart/ (float)sinStart;
		float eslope = (float)cosEnd / (float)sinEnd;
	 
		if (end_angle == TRIG_MAX_ANGLE) eslope = -1000000;
	 
		int ir2 = (radius - thickness) * (radius - thickness);
		int or2 = radius * radius;
	 
		graphics_context_set_stroke_color(ctx, c);

		for (int x = xmin; x <= xmax; x++) {
			for (int y = ymin; y <= ymax; y++)
			{
				int x2 = x * x;
				int y2 = y * y;
	 
				if (
					(x2 + y2 < or2 && x2 + y2 >= ir2) && (
						(y > 0 && start_angle < angle_180 && x <= y * sslope) ||
						(y < 0 && start_angle > angle_180 && x >= y * sslope) ||
						(y < 0 && start_angle <= angle_180) ||
						(y == 0 && start_angle <= angle_180 && x < 0) ||
						(y == 0 && start_angle == 0 && x > 0)
					) && (
						(y > 0 && end_angle < angle_180 && x >= y * eslope) ||
						(y < 0 && end_angle > angle_180 && x <= y * eslope) ||
						(y > 0 && end_angle >= angle_180) ||
						(y == 0 && end_angle >= angle_180 && x < 0) ||
						(y == 0 && start_angle == 0 && x > 0)
					)
				)
				graphics_draw_pixel(ctx, GPoint(center.x+x, center.y+y));
			}
		}
	}
}

static void updateScreen(Layer *layer, GContext *ctx) {
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, center, outerCircleOuterRadius);
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, center, outerCircleInnerRadius);
	graphics_draw_arc(ctx, center, outerCircleOuterRadius+1, OUTER_CIRCLE_THICKNESS+2, min_a1, min_a2, GColorBlack);

	graphics_draw_arc(ctx, center, innerCircleOuterRadius, INNER_CIRCLE_THICKNESS, hour_a1, hour_a2, GColorWhite);
}

static void calcAngles(struct tm *t) {
	min_a = TRIG_MAX_ANGLE * t->tm_min / 60 - angle_90;
	min_a1 = min_a - minutesWidth;
	min_a2 = min_a + minutesWidth;
	
	hour_a = TRIG_MAX_ANGLE * (60*(t->tm_hour%12)+t->tm_min) / 720 - angle_90;
	hour_a1 = hour_a - hourWidth;
	hour_a2 = hour_a + hourWidth;
}

void setDate(struct tm *t) {
	if (USDate) {
		snprintf(date, 10, "%s\n%d/%d", weekDay[curLang][t->tm_wday], t->tm_mon+1, t->tm_mday);
	} else {
		snprintf(date, 10, "%s\n%d/%.2d", weekDay[curLang][t->tm_wday], t->tm_mday, t->tm_mon+1);
	}
	//text_layer_set_text(textLayer, date);
}

static void handleTick(struct tm *tick_time, TimeUnits units_changed) {
	calcAngles(tick_time);
	if (units_changed & DAY_UNIT) {
		setDate(tick_time);
	}
	layer_mark_dirty(layer);
}

static void centerTextLayer(const char *text) {
	GSize size;
	static GRect newFrame;
	static Layer *layer = NULL;

	if (layer == NULL) {
		layer = text_layer_get_layer(textLayer);
		newFrame = textFrame;
	}

	size = graphics_text_layout_get_content_size(text, font, textFrame, GTextOverflowModeWordWrap, GTextAlignmentCenter);
	newFrame.origin.y = 84-size.h/2;
	layer_set_frame(text_layer_get_layer(textLayer), newFrame);
	text_layer_set_text(textLayer, text);
}

static void timeHandler(void *data) {
	static BatteryChargeState charge;
	
	step++;
	switch (step) {
		case 1:
			APP_LOG(APP_LOG_LEVEL_DEBUG, "timeHandler: backlight=%d", backlight);
			
			if (backlight) {
				light_enable(true);
			}

			// Show hour
			clock_copy_time_string(info, 20);
			centerTextLayer(info);
			layer_set_hidden(text_layer_get_layer(textLayer), false);
			app_timer_register(INFO_DURATION, timeHandler, NULL);
			break;

		case 2:
			// Show Date
			centerTextLayer(date);
			app_timer_register(INFO_DURATION, timeHandler, NULL);
			break;
			
		case 3:
			// Show Phone connection status
			if (bluetooth_connection_service_peek()) {
				strcpy(info, "phone ok");
			} else {
				strcpy(info, "phone failed");
			}
			centerTextLayer(info);
			app_timer_register(INFO_DURATION, timeHandler, NULL);
			break;

		case 4:
			// Show battery percentage
			charge = battery_state_service_peek();
			snprintf(info, 10, "batt %d%%", (int)charge.charge_percent);
			centerTextLayer(info);
			app_timer_register(INFO_DURATION, timeHandler, NULL);
			break;
			
		case 5:
			// Hide textLayer, reset it to Date
			if (backlight) {
				light_enable(false);
			}
			layer_set_hidden(text_layer_get_layer(textLayer), true);
			centerTextLayer(date);
			step = 0;
			break;
			
		case 101:
			// Display config saved message
			if (backlight) {
				light_enable(true);
			}
			strcpy(info, "config saved");
			centerTextLayer(info);
			layer_set_hidden(text_layer_get_layer(textLayer), false);
			app_timer_register(INFO_LONG_DURATION, timeHandler, NULL);
			break;
			
		case 102:
			// Hide textLayer, reset it to Date
			if (backlight) {
				light_enable(false);
			}

			layer_set_hidden(text_layer_get_layer(textLayer), true);
			centerTextLayer(date);
			step = 0;
			break;
	}
}

static void tapHandler(AccelAxisType axis, int32_t direction) {
	if (step) return;
	
	timeHandler(NULL);
}

static inline void initRadiuses() {
	outerCircleInnerRadius = outerCircleOuterRadius - OUTER_CIRCLE_THICKNESS;
	innerCircleOuterRadius = outerCircleInnerRadius - SPACE;
	innerCircleInnerRadius = innerCircleOuterRadius - INNER_CIRCLE_THICKNESS;
}

static void applyConfig() {
	time_t t = time(NULL);
	setDate(localtime(&t));
	step = 100;
	timeHandler(NULL);
}

static void logVariables(const char *msg) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "MSG: %s\n\tUSDate=%d\n\tcurLang=%d\n\tbacklight=%d\n", msg, USDate, curLang, backlight);
}

static bool checkAndSaveInt(int *var, int val, int key) {
	status_t ret;
	
	APP_LOG(APP_LOG_LEVEL_DEBUG, "CheckAndSaveInt : var=%d, val=%d, key=%d", *var, val, key);
	
	if (*var != val) {
		*var = val;
		ret = persist_write_int(key, val);
		if (ret < 0) APP_LOG(APP_LOG_LEVEL_DEBUG, "ERROR: persist_write_int returned %d", (int)ret);
		return true;
	} else {
		return false;
	}
}

void in_dropped_handler(AppMessageResult reason, void *context) {
}

void in_received_handler(DictionaryIterator *received, void *context) {
	bool somethingChanged = false;

	Tuple *dateorder = dict_find(received, CONFIG_KEY_DATEORDER);
	Tuple *lang = dict_find(received, CONFIG_KEY_LANG);
	Tuple *light = dict_find(received, CONFIG_KEY_BACKLIGHT);
	
	if (dateorder && lang && light) {
		somethingChanged |= checkAndSaveInt(&USDate, dateorder->value->int32, CONFIG_KEY_DATEORDER);
		somethingChanged |= checkAndSaveInt(&curLang, lang->value->int32, CONFIG_KEY_LANG);
		somethingChanged |= checkAndSaveInt(&backlight, light->value->int32, CONFIG_KEY_BACKLIGHT);
		
		logVariables("ReceiveHandler");
		
		if (somethingChanged) {
			applyConfig();
		}
	}
}


void readConfig() {
	if (persist_exists(CONFIG_KEY_DATEORDER)) {
		USDate = persist_read_int(CONFIG_KEY_DATEORDER);
	} else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists(CONFIG_KEY_DATEORDER) returned false");
		USDate = 1;
	}

	if (persist_exists(CONFIG_KEY_LANG)) {
		curLang = persist_read_int(CONFIG_KEY_LANG);
	} else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists(CONFIG_KEY_LANG) returned false");
		curLang = LANG_ENGLISH;
	}
	
	if (persist_exists(CONFIG_KEY_BACKLIGHT)) {
		backlight = persist_read_int(CONFIG_KEY_BACKLIGHT);
	} else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists(CONFIG_KEY_BACKLIGHT) returned false");
		backlight = 0;
	}
	
	logVariables("readConfig");
}

static void app_message_init(void) {
	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_open(64, 64);
}


static void init(void) {
	time_t t;
	struct tm *tm;
	
	initRadiuses();

	readConfig();
	app_message_init();

	window = window_create();
	window_set_background_color(window, GColorBlack);
	window_stack_push(window, false);

	rootLayer = window_get_root_layer(window);

	t = time(NULL);
	tm = localtime(&t);
	
	layer = layer_create(GRect(0, 0, 144, 168));
	layer_set_update_proc(layer, updateScreen);
	layer_add_child(rootLayer, layer);
	
	font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIMEBURNER_20));
	
	textFrame = GRect(72-innerCircleInnerRadius, 60, 2*innerCircleInnerRadius, 60);
	textLayer = text_layer_create(textFrame);
	text_layer_set_text_alignment(textLayer, GTextAlignmentCenter);
	text_layer_set_overflow_mode(textLayer, GTextOverflowModeWordWrap);
	text_layer_set_background_color(textLayer, GColorClear);
	text_layer_set_text_color(textLayer, GColorWhite);
	text_layer_set_font(textLayer, font);
	layer_set_hidden(text_layer_get_layer(textLayer), true);
	layer_add_child(rootLayer, text_layer_get_layer(textLayer));
	
	setDate(tm);
	calcAngles(tm);

	tick_timer_service_subscribe(MINUTE_UNIT, handleTick);
	
	accel_tap_service_subscribe(tapHandler);
}

static void deinit(void) {
	accel_tap_service_unsubscribe();
	tick_timer_service_unsubscribe();
	text_layer_destroy(textLayer);
	fonts_unload_custom_font(font);
	layer_destroy(layer);
	window_destroy(window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
