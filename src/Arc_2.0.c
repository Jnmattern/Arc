#include <pebble.h>

#define OUTER_CIRCLE_THICKNESS 15
#define INNER_CIRCLE_THICKNESS 15
#define SPACE 5

#define STACK_MAX_ELEMENTS 2000

static Window *window;
static Layer *rootLayer;
static Layer *layer;

static int outerCircleOuterRadius, outerCircleInnerRadius;
static int innerCircleOuterRadius, innerCircleInnerRadius;
static int outerCirclePathRadius, innerCirclePathRadius;
static int innerCircleMeanRadius;

static int angle_180 = TRIG_MAX_ANGLE / 2;
static int angle_90 = TRIG_MAX_ANGLE / 4;

static int32_t min_a, min_a1, min_a2, hour_a, hour_a1, hour_a2;
static int32_t minutesWidth, hourWidth;

static TextLayer *textLayer;
static char text[20];
static GFont font;

static const GPoint center = { 72, 84 };

const char weekDay[7][4] = {
	"dim", "lun", "mar", "mer", "jeu", "ven", "sam"
};

static inline GColor graphics_get_pixel(GContext *ctx, GPoint p) {
	GBitmap *bmp = (GBitmap *)ctx;
    if (p.x >= bmp->bounds.size.w || p.y >= bmp->bounds.size.h || p.x < 0 || p.y < 0) return -1;
    int byteoffset = p.y*bmp->row_size_bytes + p.x/8;
    return ((((uint8_t *)bmp->addr)[byteoffset] & (1<<(p.x%8))) != 0);
}

/*\
|*| DrawArc function thanks to Cameron MacFarland (http://forums.getpebble.com/profile/12561/Cameron%20MacFarland)
\*/
static void graphics_draw_arc(GContext *ctx, GPoint center, int radius, int thickness, int start_angle, int end_angle, GColor c) {
	while (start_angle < 0) start_angle += TRIG_MAX_ANGLE;
	while (end_angle < 0) end_angle += TRIG_MAX_ANGLE;

	start_angle %= TRIG_MAX_ANGLE;
	end_angle %= TRIG_MAX_ANGLE;
	
	if (end_angle == 0) end_angle = TRIG_MAX_ANGLE;
	
	if (start_angle > end_angle) {
		graphics_draw_arc(ctx, center, radius, thickness, start_angle, TRIG_MAX_ANGLE, c);
		graphics_draw_arc(ctx, center, radius, thickness, 0, end_angle, c);
	} else {
		float sslope = (float)cos_lookup(start_angle) / (float)sin_lookup(start_angle);
		float eslope = (float)cos_lookup(end_angle) / (float)sin_lookup(end_angle);
	 
		if (end_angle == TRIG_MAX_ANGLE) eslope = -1000000;
	 
		int ir2 = (radius - thickness) * (radius - thickness);
		int or2 = radius * radius;
	 
		graphics_context_set_stroke_color(ctx, c);

		for (int x = -radius; x <= radius; x++) {
			for (int y = -radius; y <= radius; y++)
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
	snprintf(text, 10, "%s %d/%d", weekDay[t->tm_wday], t->tm_mday, t->tm_mon+1);
	text_layer_set_text(textLayer, text);
}

static void handleTick(struct tm *tick_time, TimeUnits units_changed) {
	calcAngles(tick_time);
	if (units_changed & DAY_UNIT) {
		setDate(tick_time);
	}
	layer_mark_dirty(layer);
}

static void initVars() {
	outerCircleOuterRadius = 71;
	outerCircleInnerRadius = outerCircleOuterRadius - OUTER_CIRCLE_THICKNESS;
	innerCircleOuterRadius = outerCircleInnerRadius - SPACE;
	innerCircleInnerRadius = innerCircleOuterRadius - INNER_CIRCLE_THICKNESS;
	
	outerCirclePathRadius = outerCircleOuterRadius+5;
	innerCirclePathRadius = outerCircleInnerRadius-2;
	
	innerCircleMeanRadius = (innerCircleOuterRadius + innerCircleInnerRadius) / 2;
	
	minutesWidth = TRIG_MAX_ANGLE / 50;
	hourWidth = TRIG_MAX_ANGLE / 30;
}

static void init(void) {
	time_t t;
	struct tm *tm;
	
	initVars();

	window = window_create();
	window_set_background_color(window, GColorBlack);
	window_stack_push(window, false);

	rootLayer = window_get_root_layer(window);

	t = time(NULL);
	tm = localtime(&t);
	
	layer = layer_create(GRect(0, 0, 144, 168));
	layer_set_update_proc(layer, updateScreen);
	layer_add_child(rootLayer, layer);
	
	font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WILL_20));
	
	textLayer = text_layer_create(GRect(72-innerCircleInnerRadius, 60, 2*innerCircleInnerRadius, 60));
	text_layer_set_text_alignment(textLayer, GTextAlignmentCenter);
	text_layer_set_overflow_mode(textLayer, GTextOverflowModeWordWrap);
	text_layer_set_background_color(textLayer, GColorClear);
	text_layer_set_text_color(textLayer, GColorWhite);
	text_layer_set_font(textLayer, font);
	layer_add_child(rootLayer, text_layer_get_layer(textLayer));
	
	setDate(tm);
	calcAngles(tm);

	tick_timer_service_subscribe(MINUTE_UNIT, handleTick);
}

static void deinit(void) {
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
