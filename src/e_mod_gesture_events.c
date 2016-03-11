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
   swipes->direction = E_GESTURE_DIRECTION_NONE;

   gesture->gesture_filter |= TIZEN_GESTURE_TYPE_SWIPE;
}

static Eina_Bool
_e_gesture_is_touch_device(const char *identifier)
{
   Eina_List *l;
   char *data;

   EINA_LIST_FOREACH(gesture->touch_devices, l, data)
     {
        if (!strncmp(data, identifier, strlen(identifier)))
          {
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

static void
_e_gesture_send_swipe(int fingers, int x, int y, int direction, struct wl_client *client, struct wl_resource *res)
{
   enum tizen_gesture_direction dir = 0;
   Ecore_Event_Mouse_Button *ev_cancel;
   switch (direction)
     {
        case E_GESTURE_DIRECTION_DOWN:
           dir = TIZEN_GESTURE_DIRECTION_DOWN;
           break;
        case E_GESTURE_DIRECTION_LEFT:
           dir = TIZEN_GESTURE_DIRECTION_LEFT;
           break;
        case E_GESTURE_DIRECTION_UP:
           dir = TIZEN_GESTURE_DIRECTION_UP;
           break;
        case E_GESTURE_DIRECTION_RIGHT:
           dir = TIZEN_GESTURE_DIRECTION_RIGHT;
           break;
     }

   ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
   EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

   ev_cancel->timestamp = (int)(ecore_time_get()*1000);
   ev_cancel->same_screen = 1;

   ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);

   GTINF("Send swipe gesture (direction: %d) to client: %p\n", dir, client);
   
   tizen_gesture_send_swipe(res, fingers, TIZEN_GESTURE_MODE_DONE, x, y, dir);
   _e_gesture_swipe_cancel();

   gesture->gesture_events.recognized_gesture |= TIZEN_GESTURE_TYPE_SWIPE;
}

static Eina_Bool
_e_gesture_process_device_add(void *event)
{
   Ecore_Event_Device_Info *ev = event;

   if (ev->caps & EVDEV_SEAT_TOUCH)
     {
        gesture->touch_devices = eina_list_append(gesture->touch_devices, ev->identifier);
        GTINF("%s(%s) device is touch device: add list\n", ev->name, ev->identifier);
     }
   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_process_device_del(void *event)
{
   Ecore_Event_Device_Info *ev = event;
   Eina_List *l, *l_next;
   char *data;

   if (ev->caps & EVDEV_SEAT_TOUCH)
     {
        EINA_LIST_FOREACH_SAFE(gesture->touch_devices, l, l_next, data)
          {
             if (!strncmp(data, ev->identifier, strlen(ev->identifier)))
               {
                  GTINF("%s(%s) device is touch device: remove list\n", ev->name, ev->identifier);
                  gesture->touch_devices = eina_list_remove(gesture->touch_devices, data);
                  E_FREE(data);
               }
          }
     }
   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_timer_swipe_start(void *data)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;
   int idx = gesture->gesture_events.num_pressed;
   int i;

   GTDBG("Swipe start timer is expired. Currently alived swipe fingers: 0x%x\n", swipes->enabled_finger);

   for (i = E_GESTURE_FINGER_MAX; i > idx; i--)
     {
        swipes->enabled_finger &= ~(1 << i);
     }
   if ((swipes->direction == E_GESTURE_DIRECTION_DOWN && !swipes->fingers[idx].direction[E_GESTURE_DIRECTION_DOWN].client) ||
       (swipes->direction == E_GESTURE_DIRECTION_LEFT && !swipes->fingers[idx].direction[E_GESTURE_DIRECTION_LEFT].client) ||
       (swipes->direction == E_GESTURE_DIRECTION_UP && !swipes->fingers[idx].direction[E_GESTURE_DIRECTION_UP].client) ||
       (swipes->direction == E_GESTURE_DIRECTION_RIGHT && !swipes->fingers[idx].direction[E_GESTURE_DIRECTION_RIGHT].client))
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

   _e_gesture_swipe_cancel();

   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_swipe_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Swipe *swipes = &gesture->gesture_events.swipes;
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

        if (ev->y < E_GESTURE_SWIPE_START_AREA)
          swipes->direction = E_GESTURE_DIRECTION_DOWN;
        else if (ev->y > e_comp->h - E_GESTURE_SWIPE_START_AREA)
          swipes->direction = E_GESTURE_DIRECTION_UP;
        else if (ev->x < E_GESTURE_SWIPE_START_AREA)
          swipes->direction = E_GESTURE_DIRECTION_RIGHT;
        else if (ev->x > e_comp->w - E_GESTURE_SWIPE_START_AREA)
          swipes->direction = E_GESTURE_DIRECTION_LEFT;

        if (swipes->direction != E_GESTURE_DIRECTION_DOWN &&
       !((swipes->combined_keycode == E_GESTURE_SWIPE_COMBINE_KEY) && swipes->direction == E_GESTURE_DIRECTION_RIGHT))
          {
             _e_gesture_swipe_cancel();
          }
        else
          {
             swipes->fingers[idx].start.x = ev->x;
             swipes->fingers[idx].start.y = ev->y;
             swipes->start_timer = ecore_timer_add(E_GESTURE_SWIPE_START_TIME, _e_gesture_timer_swipe_start, NULL);
             swipes->done_timer = ecore_timer_add(E_GESTURE_SWIPE_DONE_TIME, _e_gesture_timer_swipe_done, NULL);
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
   Coords diff;
   unsigned int idx = ev->multi.device+1;

   if (!(swipes->enabled_finger & (1 << idx)))
     return;

   diff.x = ABS(swipes->fingers[idx].start.x - ev->x);
   diff.y = ABS(swipes->fingers[idx].start.y - ev->y);

   switch(swipes->direction)
     {
        case E_GESTURE_DIRECTION_DOWN:
           if (diff.x > E_GESTURE_SWIPE_DIFF_FAIL)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.y > E_GESTURE_SWIPE_DIFF_SUCCESS)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_DOWN].client, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_DOWN].res);
             }
           break;
        case E_GESTURE_DIRECTION_LEFT:
           if (diff.y > E_GESTURE_SWIPE_DIFF_FAIL)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.x > E_GESTURE_SWIPE_DIFF_SUCCESS)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_LEFT].client, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_LEFT].res);
             }
           break;
        case E_GESTURE_DIRECTION_UP:
           if (diff.x > E_GESTURE_SWIPE_DIFF_FAIL)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.y > E_GESTURE_SWIPE_DIFF_SUCCESS)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_UP].client, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_UP].res);
             }
           break;
        case E_GESTURE_DIRECTION_RIGHT:
           if (diff.y > E_GESTURE_SWIPE_DIFF_FAIL)
             {
                _e_gesture_swipe_cancel();
                break;
             }
           if (diff.x > E_GESTURE_SWIPE_DIFF_SUCCESS)
             {
                _e_gesture_send_swipe(idx, swipes->fingers[idx].start.x, swipes->fingers[idx].start.y, swipes->direction, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_RIGHT].client, swipes->fingers[idx].direction[E_GESTURE_DIRECTION_RIGHT].res);
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
   if (_e_gesture_is_touch_device(ev->dev_name) == EINA_FALSE)
     {
        return EINA_TRUE;
     }
   if (ev->multi.device > E_GESTURE_FINGER_MAX)
     {
        return EINA_TRUE;
     }

   if (gesture->gesture_events.num_pressed == 1)
     {
        gesture->gesture_events.recognized_gesture = 0x0;
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
   if (_e_gesture_is_touch_device(ev->dev_name) == EINA_FALSE)
     {
        return EINA_TRUE;
     }

   if (gesture->gesture_events.recognized_gesture)
     {
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
   if (_e_gesture_is_touch_device(ev->dev_name) == EINA_FALSE)
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

   if (ev->keycode == E_GESTURE_SWIPE_COMBINE_KEY)
     {
        gesture->gesture_events.swipes.combined_keycode = E_GESTURE_SWIPE_COMBINE_KEY;
     }

   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_process_key_up(void *event)
{
   Ecore_Event_Key *ev = event;

   if (ev->keycode == E_GESTURE_SWIPE_COMBINE_KEY)
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
