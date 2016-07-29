#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

static void
_e_gesture_swipe_cancel(void)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;

   if (swipes->start_timer)
     {
        ecore_timer_del(swipes->start_timer);
        swipes->start_timer = NULL;
     }
   if (swipes->done_timer)
     {
        ecore_timer_del(swipes->done_timer);
        swipes->done_timer = NULL;
     }

   swipes->enabled_finger = 0x0;
   swipes->direction = TIZEN_GESTURE_DIRECTION_NONE;

   gesture->gesture_filter |= TIZEN_GESTURE_TYPE_SWIPE;
}

static void
_e_gesture_keyevent_free(void *data EINA_UNUSED, void *ev)
{
   Ecore_Event_Key *e = ev;

   eina_stringshare_del(e->keyname);
   eina_stringshare_del(e->key);
   eina_stringshare_del(e->compose);

   E_FREE(e);
}

/* Optional: This function is currently used to generate back key.
 *           But how about change this function to generate every key?
 *           _e_gesture_send_key(char *keyname, Eina_Bool pressed)
 */
static void
_e_gesture_send_back_key(Eina_Bool pressed)
{
   Ecore_Event_Key *ev;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   EINA_SAFETY_ON_NULL_RETURN(e_comp_wl->xkb.keymap);

   ev = E_NEW(Ecore_Event_Key, 1);
   EINA_SAFETY_ON_NULL_RETURN(ev);

   ev->key = (char *)eina_stringshare_add("XF86Back");
   ev->keyname = (char *)eina_stringshare_add(ev->key);
   ev->compose = (char *)eina_stringshare_add(ev->key);
   ev->timestamp = (int)(ecore_time_get()*1000);
   ev->same_screen = 1;
   ev->keycode = conf->swipe.back_key;
   ev->dev = gesture->device.kbd_device;

   if (pressed)
     ecore_event_add(ECORE_EVENT_KEY_DOWN, ev, _e_gesture_keyevent_free, NULL);
   else
     ecore_event_add(ECORE_EVENT_KEY_UP, ev, _e_gesture_keyevent_free, NULL);
}

static void
_e_gesture_send_swipe(int fingers, int x, int y, int direction)
{
   Eina_List *l;
   Ecore_Event_Mouse_Button *ev_cancel;
   E_Gesture_Event_Swipe_Finger_Direction *ddata;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;
   int base_point = -1;

   ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
   EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

   ev_cancel->timestamp = (int)(ecore_time_get()*1000);
   ev_cancel->same_screen = 1;

   ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);

   GTINF("Send swipe gesture (fingers: %d))(direction: %d)\n", fingers, direction);

   if (conf->swipe.default_enable_back &&
       direction == TIZEN_GESTURE_DIRECTION_DOWN)
     {
        _e_gesture_send_back_key(EINA_TRUE);
        _e_gesture_send_back_key(EINA_FALSE);
        goto finish;
     }

   if (direction == TIZEN_GESTURE_DIRECTION_DOWN ||
       direction == TIZEN_GESTURE_DIRECTION_UP)
     {
        base_point = x;
     }
   else if (direction == TIZEN_GESTURE_DIRECTION_RIGHT ||
            direction == TIZEN_GESTURE_DIRECTION_LEFT)
     {
        base_point = y;
     }

   EINA_LIST_FOREACH(swipes->fingers[fingers].direction[direction], l, ddata)
     {
        if (base_point >= ddata->start_point &&
            base_point <= ddata->end_point)
          {
             GTINF("Send swipe gesture (fingers: %d))(direction: %d) to client: %p\n", fingers, direction, wl_resource_get_client(ddata->res));
             tizen_gesture_send_swipe(ddata->res, TIZEN_GESTURE_MODE_DONE, fingers, x, y, direction);
             break;
          }
     }

finish:
   _e_gesture_swipe_cancel();
   gesture->gesture_events.recognized_gesture |= TIZEN_GESTURE_TYPE_SWIPE;
}

static Eina_Bool
_e_gesture_process_device_add(void *event)
{
   return e_gesture_device_add(event);
}

static Eina_Bool
_e_gesture_process_device_del(void *event)
{
   return e_gesture_device_del(event);
}

static Eina_Bool
_e_gesture_event_swipe_direction_check(unsigned int direction)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;
   int idx = gesture->gesture_events.num_pressed;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   Eina_List *l;
   E_Gesture_Event_Swipe_Finger_Direction *ddata;
   Coords coords;

   if ((conf->swipe.default_enable_back) &&
       (direction == TIZEN_GESTURE_DIRECTION_DOWN  ||
       ((swipes->combined_keycode == conf->swipe.compose_key) &&
       (swipes->direction == TIZEN_GESTURE_DIRECTION_RIGHT))))
     {
        return EINA_TRUE;
     }

   coords.x = swipes->fingers[idx].start.x;
   coords.y = swipes->fingers[idx].start.y;

   EINA_LIST_FOREACH(swipes->fingers[idx].direction[direction], l, ddata)
     {
        if (direction == TIZEN_GESTURE_DIRECTION_DOWN ||
            direction == TIZEN_GESTURE_DIRECTION_UP)
          {
             if (coords.x >= ddata->start_point &&
                 coords.x <= ddata->end_point)
               {
                  return EINA_TRUE;
               }
          }
        else if (direction == TIZEN_GESTURE_DIRECTION_RIGHT ||
                 direction == TIZEN_GESTURE_DIRECTION_LEFT)
          {
             if (coords.y >= ddata->start_point &&
                 coords.y <= ddata->end_point)
               {
                  return EINA_TRUE;
               }
          }
     }

   return EINA_FALSE;
}

static Eina_Bool
_e_gesture_timer_swipe_start(void *data)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;
   int idx = gesture->gesture_events.num_pressed;
   int i;

   GTDBG("Swipe start timer is expired. Currently alived swipe fingers: 0x%x\n", swipes->enabled_finger);

   ecore_timer_del(swipes->start_timer);
   swipes->start_timer = NULL;

   for (i = E_GESTURE_FINGER_MAX; i > idx; i--)
     {
        swipes->enabled_finger &= ~(1 << i);
     }
   if (swipes->enabled_finger == 0x0 ||
       _e_gesture_event_swipe_direction_check(swipes->direction) == EINA_FALSE)
     {
        _e_gesture_swipe_cancel();
     }

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_gesture_timer_swipe_done(void *data)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;

   GTDBG("Swipe done timer is expired. Currently alived swipe fingers: 0x%x\n", swipes->enabled_finger);

   ecore_timer_del(swipes->done_timer);
   swipes->done_timer = NULL;

   _e_gesture_swipe_cancel();

   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_swipe_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   int i;
   unsigned int idx = ev->multi.device+1;

   if (gesture->gesture_events.num_pressed == 1)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             if (swipes->fingers[i].enabled)
               {
                  swipes->enabled_finger |= (1 << i);
               }
          }

        if (ev->y < conf->swipe.area_offset)
          swipes->direction = TIZEN_GESTURE_DIRECTION_DOWN;
        else if (ev->y > e_comp->h - conf->swipe.area_offset)
          swipes->direction = TIZEN_GESTURE_DIRECTION_UP;
        else if (ev->x < conf->swipe.area_offset)
          swipes->direction = TIZEN_GESTURE_DIRECTION_RIGHT;
        else if (ev->x > e_comp->w - conf->swipe.area_offset)
          swipes->direction = TIZEN_GESTURE_DIRECTION_LEFT;

        if (conf->swipe.default_enable_back && (swipes->direction != TIZEN_GESTURE_DIRECTION_DOWN &&
       !((swipes->combined_keycode == conf->swipe.compose_key) && swipes->direction == TIZEN_GESTURE_DIRECTION_RIGHT)))
          {
             _e_gesture_swipe_cancel();
          }
        else
          {
             swipes->fingers[idx].start.x = ev->x;
             swipes->fingers[idx].start.y = ev->y;
             swipes->start_timer = ecore_timer_add(conf->swipe.time_begin, _e_gesture_timer_swipe_start, NULL);
             swipes->done_timer = ecore_timer_add(conf->swipe.time_done, _e_gesture_timer_swipe_done, NULL);
          }
     }
   else
     {
        swipes->enabled_finger &= ~(1 << (gesture->gesture_events.num_pressed - 1));
        if (swipes->start_timer == NULL)
          {
             _e_gesture_swipe_cancel();
          }
     }
}

static void
_e_gesture_process_swipe_move(Ecore_Event_Mouse_Move *ev)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;
   Coords diff;
   unsigned int idx = ev->multi.device+1;

   if (!(swipes->enabled_finger & (1 << idx)))
     return;

   diff.x = ABS(swipes->fingers[idx].start.x - ev->x);
   diff.y = ABS(swipes->fingers[idx].start.y - ev->y);

   switch(swipes->direction)
     {
        case TIZEN_GESTURE_DIRECTION_DOWN:
           if (diff.x > conf->swipe.min_length)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.y > conf->swipe.max_length)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction);
             }
           break;
        case TIZEN_GESTURE_DIRECTION_LEFT:
           if (diff.y > conf->swipe.min_length)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.x > conf->swipe.max_length)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction);
             }
           break;
        case TIZEN_GESTURE_DIRECTION_UP:
           if (diff.x > conf->swipe.min_length)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.y > conf->swipe.max_length)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction);
             }
           break;
        case TIZEN_GESTURE_DIRECTION_RIGHT:
           if (diff.y > conf->swipe.min_length)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.x > conf->swipe.max_length)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction);
             }
           break;
        default:
           GTWRN("Invalid direction(%d)\n", swipes->direction);
           break;
     }
}

static void
_e_gesture_process_swipe_up(Ecore_Event_Mouse_Button *ev)
{
   _e_gesture_swipe_cancel();
}

static Eina_Bool
_e_gesture_process_mouse_button_down(void *event)
{
   Ecore_Event_Mouse_Button *ev = event;

   gesture->gesture_events.num_pressed++;

   if (!gesture->grabbed_gesture)
     {
        return EINA_TRUE;
     }
   if (e_gesture_is_touch_device(ev->dev) == EINA_FALSE)
     {
        return EINA_TRUE;
     }
   if (ev->multi.device > E_GESTURE_FINGER_MAX)
     {
        return EINA_TRUE;
     }

   if (gesture->gesture_events.recognized_gesture)
     {
        return EINA_FALSE;
     }

   if (gesture->gesture_events.num_pressed == 1)
     {
        gesture->gesture_filter = E_GESTURE_TYPE_ALL & ~gesture->grabbed_gesture;
     }

   if (!(gesture->gesture_filter & TIZEN_GESTURE_TYPE_SWIPE))
     {
        _e_gesture_process_swipe_down(ev);
     }

   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_process_mouse_button_up(void *event)
{
   Ecore_Event_Mouse_Button *ev = event;

   gesture->gesture_events.num_pressed--;

   if (!gesture->grabbed_gesture)
     {
        return EINA_TRUE;
     }
   if (e_gesture_is_touch_device(ev->dev) == EINA_FALSE)
     {
        return EINA_TRUE;
     }

   if (gesture->gesture_events.recognized_gesture)
     {
        if (gesture->gesture_events.num_pressed == 0)
          {
             gesture->gesture_events.recognized_gesture = 0x0;
          }
        return EINA_FALSE;
     }

   if (!(gesture->gesture_filter & TIZEN_GESTURE_TYPE_SWIPE))
     {
        _e_gesture_process_swipe_up(ev);
     }

   return EINA_TRUE;
}


static Eina_Bool
_e_gesture_process_mouse_move(void *event)
{
   Ecore_Event_Mouse_Move *ev = event;

   if (!gesture->grabbed_gesture)
     {
        return EINA_TRUE;
     }
   if (e_gesture_is_touch_device(ev->dev) == EINA_FALSE)
     {
        return EINA_TRUE;
     }

   if (gesture->gesture_events.recognized_gesture)
     {
        return EINA_FALSE;
     }

   if (!(gesture->gesture_filter & TIZEN_GESTURE_TYPE_SWIPE))
     {
        _e_gesture_process_swipe_move(ev);
     }

   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_process_key_down(void *event)
{
   Ecore_Event_Key *ev = event;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   if (ev->keycode == conf->swipe.compose_key)
     {
        gesture->gesture_events.swipes.combined_keycode = conf->swipe.compose_key;
     }

   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_process_key_up(void *event)
{
   Ecore_Event_Key *ev = event;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   if (ev->keycode == conf->swipe.compose_key)
     {
        gesture->gesture_events.swipes.combined_keycode = 0;
     }

   return EINA_TRUE;
}

/* Function for checking the existing grab for a key and sending key event(s) */
Eina_Bool
e_gesture_process_events(void *event, int type)
{
   Eina_Bool res = EINA_TRUE;

   if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN)
     res = _e_gesture_process_mouse_button_down(event);
   else if (type == ECORE_EVENT_MOUSE_BUTTON_UP)
     res = _e_gesture_process_mouse_button_up(event);
   else if (type == ECORE_EVENT_MOUSE_MOVE)
     res = _e_gesture_process_mouse_move(event);
   else if (type == ECORE_EVENT_KEY_DOWN)
     res = _e_gesture_process_key_down(event);
   else if (type == ECORE_EVENT_KEY_UP)
     res = _e_gesture_process_key_up(event);
   else if (type == ECORE_EVENT_DEVICE_ADD)
     res = _e_gesture_process_device_add(event);
   else if (type == ECORE_EVENT_DEVICE_DEL)
     res = _e_gesture_process_device_del(event);

   return res;
}
