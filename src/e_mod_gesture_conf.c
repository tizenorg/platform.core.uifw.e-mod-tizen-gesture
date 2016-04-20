#define E_COMP_WL
#include "e_mod_main.h"

void
_e_gesture_conf_value_check(E_Gesture_Config_Data* gconfig)
{
   E_Gesture_Conf_Edd *conf;
   if (!gconfig->conf) gconfig->conf = E_NEW(E_Gesture_Conf_Edd, 1);
   EINA_SAFETY_ON_NULL_RETURN(gconfig->conf);

   conf = gconfig->conf;
   if (!conf->key_device_name) conf->key_device_name = strdup(E_GESTURE_KEYBOARD_DEVICE);
   if (conf->swipe.time_done <= 0.0) conf->swipe.time_done = E_GESTURE_SWIPE_DONE_TIME;
   if (conf->swipe.time_begin <= 0.0) conf->swipe.time_begin = E_GESTURE_SWIPE_START_TIME;
   if (conf->swipe.area_offset <= 0) conf->swipe.area_offset = E_GESTURE_SWIPE_START_AREA;
   if (conf->swipe.min_length <= 0) conf->swipe.min_length = E_GESTURE_SWIPE_DIFF_FAIL;
   if (conf->swipe.max_length <= 0) conf->swipe.max_length = E_GESTURE_SWIPE_DIFF_SUCCESS;
   if (conf->swipe.compose_key <= 0) conf->swipe.compose_key = E_GESTURE_SWIPE_COMBINE_KEY;
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
   E_CONFIG_VAL(D, T, key_device_name, STR);
   E_CONFIG_VAL(D, T, swipe.time_done, DOUBLE);
   E_CONFIG_VAL(D, T, swipe.time_begin, DOUBLE);
   E_CONFIG_VAL(D, T, swipe.area_offset, INT);
   E_CONFIG_VAL(D, T, swipe.min_length, INT);
   E_CONFIG_VAL(D, T, swipe.max_length, INT);
   E_CONFIG_VAL(D, T, swipe.compose_key, INT);
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
        E_FREE(gconfig->conf->key_device_name);
        E_FREE(gconfig->conf);
     }
   E_CONFIG_DD_FREE(gconfig->conf_edd);
   E_FREE(gconfig);
}
