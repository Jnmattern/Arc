#include <pebble.h>

#define OUTER_CIRCLE_THICKNESS 15
#define INNER_CIRCLE_THICKNESS 15
#define SPACE 5

#define INFO_DURATION 1000

static Window *window;
static Layer *rootLayer;
static Layer *layer;

static int outerCircleOuterRadius = 71, outerCircleInnerRadius;
static int innerCircleOuterRadius, innerCircleInnerRadius ;

static int angle_180 = TRIG_MAX_ANGLE / 2;
static int angle_90 = TRIG_MAX_ANGLE / 4;

static int32_t min_a, min_a1, min_a2, hour_a, hour_a1, hour_a2;
static int32_t minutesWidth = TRIG_MAX_ANGLE / 50;
static int32_t hourWidth = TRIG_MAX_ANGLE / 24;

static GRect textFrame;
static TextLayer *textLayer;
static char date[20], info[20];
static GFont font;

static int step = 0;

static const GPoint center = { 72, 84 };

const char weekDay[7][4] = {
	"dim", "lun", "mar", "mer", "jeu", "ven", "sam"
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
	snprintf(date, 10, "%s %d/%d", weekDay[t->tm_wday], t->tm_mday, t->tm_mon+1);
	text_layer_set_text(textLayer, date);
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
			// First show Date, already set in textLayer
			light_enable(true);
			centerTextLayer(date);
			layer_set_hidden(text_layer_get_layer(textLayer), false);
			app_timer_register(INFO_DURATION, timeHandler, NULL);
			break;

		case 2:
			// Show battery percentage
			charge = battery_state_service_peek();
			snprintf(info, 10, "batt %d%%", (int)charge.charge_percent);
			centerTextLayer(info);
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
			// Show hour
			clock_copy_time_string(info, 20);
			centerTextLayer(info);
			app_timer_register(INFO_DURATION, timeHandler, NULL);
			break;
			
		case 5:
			// Hide textLayer, reset it to Date
			light_enable(false);
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

static void init(void) {
	time_t t;
	struct tm *tm;
	
	initRadiuses();
	
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
