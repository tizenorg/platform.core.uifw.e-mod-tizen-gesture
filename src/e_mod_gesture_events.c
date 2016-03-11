#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

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
_e_gesture_send_flick(int fingers, int type, int direction, struct wl_client *client, struct wl_resource *res)
{
   enum tizen_gesture_direction dir = 0;
   Ecore_Event_Mouse_Button *ev_cancel;
   switch (direction)
     {
        case E_GESTURE_DIRECT_SOUTHWARD:
           dir = TIZEN_GESTURE_DIRECTION_SOUTHWARD;
           break;
        case E_GESTURE_DIRECT_WESTWARD:
           dir = TIZEN_GESTURE_DIRECTION_WESTWARD;
           break;
        case E_GESTURE_DIRECT_NORTHWARD:
           dir = TIZEN_GESTURE_DIRECTION_NORTHWARD;
           break;
        case E_GESTURE_DIRECT_EASTWARD:
           dir = TIZEN_GESTURE_DIRECTION_EASTWARD;
           break;
     }

   ev_cancel = E_NEW(Ecore_Event_Mouse_Button, 1);
   EINA_SAFETY_ON_NULL_RETURN(ev_cancel);

   ev_cancel->timestamp = (int)(ecore_time_get()*1000);
   ev_cancel->same_screen = 1;

   ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_CANCEL, ev_cancel, NULL, NULL);

   GTINF("Send flick gesture (direction: %d) to client: %p\n", dir, client);
   
   tizen_gesture_send_flick(res, fingers, type, dir);

   gesture->gesture_events.recognized_gesture |= (1 << TIZEN_GESTURE_MODE_FLICK);
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

static void
_e_gesture_flick_cancel(void)
{
   E_Gesture_Event_Flick *flicks = &gesture->gesture_events.flicks;

   if (flicks->start_timer)
     ecore_timer_del(flicks->start_timer);
   flicks->start_timer = NULL;
   if (flicks->done_timer)
     ecore_timer_del(flicks->done_timer);
   flicks->done_timer = NULL;

   flicks->enabled_finger = 0x0;
   flicks->direction = E_GESTURE_DIRECT_NONE;

   gesture->gesture_filter |= (1 << TIZEN_GESTURE_MODE_FLICK);
}

static Eina_Bool
_e_gesture_timer_flick_start(void *data)
{
   E_Gesture_Event_Flick *flicks = &gesture->gesture_events.flicks;
   int i;

   for (i = E_GESTURE_FINGER_MAX; i > gesture->gesture_events.num_pressed; i--)
     {
        flicks->enabled_finger &= ~(1 << i);
     }
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_gesture_timer_flick_done(void *data)
{
   E_Gesture_Event_Flick *flicks = &gesture->gesture_events.flicks;

   GTDBG("Flick done timer is expired. Currently alived flick fingers: 0x%x\n", flicks->enabled_finger);

   _e_gesture_flick_cancel();

   return ECORE_CALLBACK_CANCEL;
}

static void
_e_gesture_process_flick_down(Ecore_Event_Mouse_Button *ev)
{
   E_Gesture_Event_Flick *flicks = &gesture->gesture_events.flicks;
   int i;
   unsigned int idx = ev->multi.device+1;

   if (gesture->gesture_events.num_pressed == 1)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             if (flicks->fingers[i].client)
               {
                  flicks->enabled_finger |= (1 << i);
               }
          }

        if ((ev->x > E_GESTURE_FLICK_START_AREA) &&
            (ev->x < e_comp->w - E_GESTURE_FLICK_START_AREA))
          {
             if (ev->y < E_GESTURE_FLICK_START_AREA)
               flicks->direction = E_GESTURE_DIRECT_SOUTHWARD;
             else if (ev->y > e_comp->h - E_GESTURE_FLICK_START_AREA)
               flicks->direction = E_GESTURE_DIRECT_NORTHWARD;
          }
        else if ((ev->y > E_GESTURE_FLICK_START_AREA) &&
                 (ev->y < e_comp->h - E_GESTURE_FLICK_START_AREA))
          {
             if (ev->x < E_GESTURE_FLICK_START_AREA)
               flicks->direction = E_GESTURE_DIRECT_EASTWARD;
             else if (ev->x > e_comp->w - E_GESTURE_FLICK_START_AREA)
               flicks->direction = E_GESTURE_DIRECT_WESTWARD;
          }
     }
   else
     {
        flicks->enabled_finger &= ~(1 << (gesture->gesture_events.num_pressed - 1));
     }

   if (flicks->direction != E_GESTURE_DIRECT_SOUTHWARD &&
       !((flicks->combined_keycode == E_GESTURE_FLICK_COMBINE_KEY) && flicks->direction == E_GESTURE_DIRECT_EASTWARD))
     {
        _e_gesture_flick_cancel();
     }
   else
     {
        flicks->fingers[idx].start.x = ev->x;
        flicks->fingers[idx].start.y = ev->y;
        flicks->start_timer = ecore_timer_add(E_GESTURE_FLICK_START_TIME, _e_gesture_timer_flick_start, NULL);
        flicks->done_timer = ecore_timer_add(E_GESTURE_FLICK_DONE_TIME, _e_gesture_timer_flick_done, NULL);
     }
}

static void
_e_gesture_process_flick_move(Ecore_Event_Mouse_Move *ev)
{
   E_Gesture_Event_Flick *flicks = &gesture->gesture_events.flicks;
   Coords diff;
   unsigned int idx = ev->multi.device+1;

   if (!(flicks->enabled_finger & (1 << idx)))
     return;

   diff.x = ABS(flicks->fingers[idx].start.x - ev->x);
   diff.y = ABS(flicks->fingers[idx].start.y - ev->y);

   switch(flicks->direction)
     {
        case E_GESTURE_DIRECT_SOUTHWARD:
           if (diff.x > E_GESTURE_FLICK_DIFF_FAIL)
             {
                _e_gesture_flick_cancel();
                break;
             }
           if (diff.y > E_GESTURE_FLICK_DIFF_SUCCESS)
             {
                _e_gesture_send_flick(idx, TIZEN_GESTURE_TYPE_DONE, flicks->direction, flicks->fingers[idx].client, flicks->fingers[idx].res);
             }
           break;
        case E_GESTURE_DIRECT_WESTWARD:
           if (diff.y > E_GESTURE_FLICK_DIFF_FAIL)
             {
                _e_gesture_flick_cancel();
                break;
             }
           if (diff.x > E_GESTURE_FLICK_DIFF_SUCCESS)
             {
                _e_gesture_send_flick(idx, TIZEN_GESTURE_TYPE_DONE, flicks->direction, flicks->fingers[idx].client, flicks->fingers[idx].res);
             }
           break;
        case E_GESTURE_DIRECT_NORTHWARD:
           if (diff.x > E_GESTURE_FLICK_DIFF_FAIL)
             {
                _e_gesture_flick_cancel();
                break;
             }
           if (diff.y > E_GESTURE_FLICK_DIFF_SUCCESS)
             {
                _e_gesture_send_flick(idx, TIZEN_GESTURE_TYPE_DONE, flicks->direction, flicks->fingers[idx].client, flicks->fingers[idx].res);
             }
           break;
        case E_GESTURE_DIRECT_EASTWARD:
           if (diff.y > E_GESTURE_FLICK_DIFF_FAIL)
             {
                _e_gesture_flick_cancel();
                break;
             }
           if (diff.x > E_GESTURE_FLICK_DIFF_SUCCESS)
             {
                _e_gesture_send_flick(idx, TIZEN_GESTURE_TYPE_DONE, flicks->direction, flicks->fingers[idx].client, flicks->fingers[idx].res);
             }
           break;
        default:
           GTWRN("Invalid direction(%d)\n", flicks->direction);
           break;
     }
}

static void
_e_gesture_process_flick_up(Ecore_Event_Mouse_Button *ev)
{
   _e_gesture_flick_cancel();
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
        gesture->gesture_filter = (1 << E_GESTURE_MODE_ALL) & ~gesture->grabbed_gesture;
     }

   if (!(gesture->gesture_filter & (1 << TIZEN_GESTURE_MODE_FLICK)))
     {
        _e_gesture_process_flick_down(ev);
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

   if (!(gesture->gesture_filter & (1 << TIZEN_GESTURE_MODE_FLICK)))
     {
        _e_gesture_process_flick_up(ev);
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

   if (!(gesture->gesture_filter & (1 << TIZEN_GESTURE_MODE_FLICK)))
     {
        _e_gesture_process_flick_move(ev);
     }

   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_process_key_down(void *event)
{
   Ecore_Event_Key *ev = event;

   if (ev->keycode == E_GESTURE_FLICK_COMBINE_KEY)
     {
        gesture->gesture_events.flicks.combined_keycode = E_GESTURE_FLICK_COMBINE_KEY;
     }

   return EINA_TRUE;
}

static Eina_Bool
_e_gesture_process_key_up(void *event)
{
   Ecore_Event_Key *ev = event;

   if (ev->keycode == E_GESTURE_FLICK_COMBINE_KEY)
     {
        gesture->gesture_events.flicks.combined_keycode = 0;
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


struct wl_resource *
e_keyrouter_util_get_surface_from_eclient(E_Client *client)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL
     (client, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL
     (client->comp_data, NULL);

   return client->comp_data->wl_surface;
}
