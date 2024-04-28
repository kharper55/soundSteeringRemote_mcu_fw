#include "app_include/app_spi.h"


uint32_t knobColors[2] = {APP_COLOR_UNH_GOLD, APP_COLOR_RED}; // SWITCH RELEASED COLOR , SWITCH PRESSED COLOR
uint32_t textColors[2] = {APP_COLOR_BLACK, APP_COLOR_RED};    // STANDARD TEXT COLOR , HIGHLIGHTED TEXT COLOR
uint32_t batteryColors[3] = {APP_COLOR_RED, APP_COLOR_UNH_GOLD, APP_COLOR_PCB_GREEN_ALT};

const char app_icons[][4] = {
    "\xEF\x89\x83", /* LV_SYMBOL_BATTERY_1 */
    "\xEF\x89\x82", /* LV_SYMBOL_BATTERY_2 */
    "\xEF\x89\x80", /* LV_SYMBOL_BATTERY_FULL */
    "\xEF\x89\x84", /* LV_SYMBOL_BATTERY_EMPTY */
    "\xEF\x83\xA7", /* LV_SYMBOL_CHARGE */
    "\xEF\x8a\x93", /* LV_SYMBOL_BLUETOOTH */
    "\xEF\x82\x95", /* LV_SYMBOL_BELL */
    "\xEF\x83\xB3"  /* LV_SYMBOL_CALL */
};

const char * batteryStateNames[4] = {
    "BATT_EMPTY",
    "BATT_LOW",
    "BATT_MED",
    "BATT_FULL"
};

/*---------------------------------------------------------------
    LCD arc objects for encoders and potentiometers.
---------------------------------------------------------------*/
lv_obj_t * app_arc_create(lv_obj_t * comp_parent, app_arc_t arc_t, lv_coord_t r, 
                          lv_align_t align, lv_coord_t x_offs, lv_coord_t y_offs,
                          bool symmetric_mode) {

    lv_obj_t * newArc;
    const void * tempImg;
    newArc = lv_arc_create(comp_parent);

    lv_obj_set_width(newArc, r);
    lv_obj_set_height(newArc, r);
    lv_obj_align(newArc, align, x_offs, y_offs);
    lv_obj_clear_flag(newArc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);      /// Flags
    if (arc_t == FULL_ARC) {
        lv_arc_set_range(newArc, 0, 360);
        lv_arc_set_value(newArc, 160);
        lv_arc_set_bg_angles(newArc, 0, 360);
    }
    else {
        if (symmetric_mode) {
            lv_arc_set_range(newArc, -60, 60);
            lv_arc_set_mode(newArc, LV_ARC_MODE_SYMMETRICAL);
            lv_arc_set_value(newArc, 50);
        }
        else {
            lv_arc_set_value(newArc, 10);
        }
        /* Default bg_angles range is 120, 60 */
    }
    lv_obj_set_style_arc_width(newArc, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (arc_t == FULL_ARC) {
        lv_obj_set_style_bg_img_src(newArc, &tempImg, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_blend_mode(newArc, LV_BLEND_MODE_NORMAL, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(newArc, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_arc_color(newArc, lv_color_hex(APP_COLOR_UNH_NAVY_ALT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_opa(newArc, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_arc_width(newArc, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(newArc, lv_color_hex(APP_COLOR_UNH_GOLD), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(newArc, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(newArc, lv_color_hex(0x000000), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(newArc, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(newArc, 1, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(newArc, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(newArc, lv_color_hex(0x000000), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(newArc, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(newArc, 10, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(newArc, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);

    return newArc;
}