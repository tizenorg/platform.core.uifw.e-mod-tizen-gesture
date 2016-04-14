#define E_COMP_WL
#include "e_mod_main.h"

void
_e_gesture_conf_value_check(E_Gesture_Config_Data* gconfig)
{
   E_Gesture_Conf_Edd *conf;
   if (!gconfig->conf) gconfig->conf = E_NEW(E_Gesture_Conf_Edd, 1);
   EINA_SAFETY_ON_NULL_RETURN(gconfig->conf);

   conf = gconfig->conf;
   if (!conf->keyboard_name) conf->keyboard_name = strdup(E_GESTURE_KEYBOARD_DEVICE);
   if (conf->swipe.time_done <= 0.0) conf->swipe.time_done = E_GESTURE_SWIPE_DONE_TIME;
   if (conf->swipe.time_start <= 0.0) conf->swipe.time_start = E_GESTURE_SWIPE_START_TIME;
   if (conf->swipe.area_start <= 0) conf->swipe.area_start = E_GESTURE_SWIPE_START_AREA;
   if (conf->swipe.diff_fail <= 0) conf->swipe.diff_fail = E_GESTURE_SWIPE_DIFF_FAIL;
   if (conf->swipe.diff_success <= 0) conf->swipe.diff_success = E_GESTURE_SWIPE_DIFF_SUCCESS;
   if (conf->swipe.combine_key <= 0) conf->swipe.combine_key = E_GESTURE_SWIPE_COMBINE_KEY;
   if (conf->swipe.back_key <= 0) conf->swipe.back_key = E_GESTURE_SWIPE_BACK_KEY;
}

void
e_gesture_conf_init(E_Gesture_Config_Data *gconfig)
{
   gconfig->conf_edd = E_CONFIG_DD_NEW("Gesture_Config", E_Gesture_Conf_Edd);
#undef T
#undef D
#define T E_Gesture_Conf_Edd
#define D gconfig->conf_edd
   E_CONFIG_VAL(D, T, keyboard_name, STR);
   E_CONFIG_VAL(D, T, swipe.time_done, DOUBLE);
   E_CONFIG_VAL(D, T, swipe.time_start, DOUBLE);
   E_CONFIG_VAL(D, T, swipe.area_start, INT);
   E_CONFIG_VAL(D, T, swipe.diff_fail, INT);
   E_CONFIG_VAL(D, T, swipe.diff_success, INT);
   E_CONFIG_VAL(D, T, swipe.combine_key, INT);
   E_CONFIG_VAL(D, T, swipe.back_key, INT);
   E_CONFIG_VAL(D, T, swipe.default_enable_back, CHAR);

#undef T
#undef D
   gconfig->conf = e_config_domain_load("module.gesture", gconfig->conf_edd);

   if (!gconfig->conf)
     {
        GTWRN("Failed to find module.keyrouter config file.\n");
     }
   _e_gesture_conf_value_check(gconfig);
}

void
e_gesture_conf_deinit(E_Gesture_Config_Data *gconfig)
{
   if (gconfig->conf)
     {
        E_FREE(gconfig->conf->keyboard_name);
        E_FREE(gconfig->conf);
     }
   E_CONFIG_DD_FREE(gconfig->conf_edd);
   E_FREE(gconfig);
}
