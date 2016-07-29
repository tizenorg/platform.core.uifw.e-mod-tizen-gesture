#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

E_GesturePtr gesture = NULL;
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Gesture Module of Window Manager" };

static E_Gesture_Config_Data *_e_gesture_init(E_Module *m);
static void _e_gesture_init_handlers(void);

static Eina_Bool _e_gesture_event_filter(void *data, void *loop_data EINA_UNUSED, int type, void *event);
static void _e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data);

static Eina_Bool
_e_gesture_swipe_boundary_check(E_Gesture_Event_Swipe_Finger *fingers, unsigned int direction, unsigned int start_point, unsigned int end_point)
{
   Eina_List *l;
   E_Gesture_Event_Swipe_Finger_Direction *ddata;
   E_Gesture_Conf_Edd *conf = gesture->config->conf;

   if ((conf->swipe.default_enable_back) &&
       (direction == TIZEN_GESTURE_DIRECTION_DOWN ||
       direction == TIZEN_GESTURE_DIRECTION_UP))
     {
        return EINA_FALSE;
     }

   EINA_LIST_FOREACH(fingers->direction[direction], l, ddata)
     {
        if (!(start_point > ddata->end_point || end_point < ddata->start_point))
          return EINA_FALSE;
     }

   return EINA_TRUE;
}

static void
_e_gesture_swipe_grab_add(E_Gesture_Event_Swipe_Finger *fingers, struct wl_client *client, struct wl_resource *res, unsigned int direction, unsigned int start_point, unsigned int end_point)
{
   E_Gesture_Event_Swipe_Finger_Direction *ddata;

   ddata = E_NEW(E_Gesture_Event_Swipe_Finger_Direction, 1);
   EINA_SAFETY_ON_NULL_RETURN(ddata);

   ddata->client = client;
   ddata->res = res;
   ddata->start_point = start_point;
   ddata->end_point = end_point;

   fingers->direction[direction] = eina_list_append(fingers->direction[direction], ddata);
}

static void
_e_gesture_enable(void)
{
   GTINF("enable gesture\n");
   gesture->ef_handler = ecore_event_filter_add(NULL, _e_gesture_event_filter, NULL, NULL);
   gesture->enable = EINA_TRUE;
}

static void
_e_gesture_disable(void)
{
   GTINF("Disable gesture\n");
   ecore_event_filter_del(gesture->ef_handler);
   gesture->ef_handler = NULL;
   gesture->enable = EINA_FALSE;
}

/* Function for registering wl_client destroy listener */
static int
_e_gesture_add_grab_client_destroy_listener(struct wl_client *client)
{
   struct wl_listener *destroy_listener = NULL;
   Eina_List *l;
   struct wl_client *cdata;

   EINA_LIST_FOREACH(gesture->grab_client_list, l, cdata)
     {
        if (cdata == client)
          {
             return TIZEN_GESTURE_ERROR_NONE;
          }
     }

   destroy_listener = E_NEW(struct wl_listener, 1);
   if (!destroy_listener)
     {
        GTERR("Failed to allocate memory for wl_client destroy listener !\n");
        return TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
     }

   destroy_listener->notify = _e_gesture_wl_client_cb_destroy;
   wl_client_add_destroy_listener(client, destroy_listener);

   gesture->grab_client_list = eina_list_append(gesture->grab_client_list, client);

   return TIZEN_KEYROUTER_ERROR_NONE;
}

static void
_e_gesture_remove_grab_client_destroy_listener(struct wl_client *client)
{
   Eina_List *l, *l_next;
   struct wl_client *cdata;
   struct wl_listener *destroy_listener;

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l, l_next, cdata)
     {
        if (cdata == client)
          {
             destroy_listener = wl_client_get_destroy_listener(client, _e_gesture_wl_client_cb_destroy);
             wl_list_remove(&destroy_listener->link);
             E_FREE(destroy_listener);
             gesture->grab_client_list = eina_list_remove_list(gesture->grab_client_list, l);
          }
     }
}

static Eina_Bool
_e_gesture_eclient_list_add(Eina_List **list, E_Client *ec)
{
   Eina_List *l;
   E_Client *data;

   EINA_LIST_FOREACH(*list, l, data)
     {
        if (data == ec) return EINA_FALSE;
     }

   *list = eina_list_append(*list, ec);
   return EINA_TRUE;
}

static void
_e_gesture_cb_grab_swipe(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t num_of_fingers, uint32_t direction,
                   uint32_t start_point, uint32_t end_point)
{
   E_Gesture_Event *gev;
   unsigned int grabbed_direction = 0x0;
   Eina_Bool res = EINA_FALSE;
   unsigned int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("client %p is request grab gesture, fingers: %d, direction: 0x%x (%d ~ %d)\n", client, num_of_fingers, direction, start_point, end_point);
   if (num_of_fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", num_of_fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   res = _e_gesture_swipe_boundary_check(&gev->swipes.fingers[num_of_fingers], direction, start_point, end_point);

   if (res)
     {
        _e_gesture_swipe_grab_add(&gev->swipes.fingers[num_of_fingers], client, resource, direction, start_point, end_point);

        _e_gesture_add_grab_client_destroy_listener(client);
        gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_SWIPE;
        gev->swipes.fingers[num_of_fingers].enabled = EINA_TRUE;

        ret = TIZEN_GESTURE_ERROR_NONE;
     }
   else
     {
        ret = TIZEN_GESTURE_ERROR_GRABBED_ALREADY;
     }

finish:
   tizen_gesture_send_grab_swipe_notify(resource, num_of_fingers, grabbed_direction, ret);
}

static Eina_Bool
_e_gesture_grabbed_client_check(struct wl_client *client)
{
   E_Gesture_Event *gev;
   int i, j;
   Eina_List *l;
   E_Gesture_Event_Swipe_Finger_Direction *ddata;

   gev = &gesture->gesture_events;
   if (gesture->grabbed_gesture & TIZEN_GESTURE_TYPE_SWIPE)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
          {
             if (gev->swipes.fingers[i].enabled == EINA_TRUE)
               {
                  for (j = 0; j < E_GESTURE_DIRECTION_MAX + 1; j++)
                    {
                       EINA_LIST_FOREACH(gev->swipes.fingers[i].direction[j], l, ddata)
                         {
                            if (ddata->client == client) return EINA_TRUE;
                         }
                    }
               }
          }
     }

   return EINA_FALSE;
}

static void
_e_gesture_state_cleanup(void)
{
   E_Gesture_Event *gev;
   int i, j;
   Eina_Bool flag_direction = EINA_FALSE;
   unsigned int cout_enabled = 0;

   gev = &gesture->gesture_events;
   if (gesture->grabbed_gesture & TIZEN_GESTURE_TYPE_SWIPE)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
          {
             flag_direction = EINA_FALSE;
             if (gev->swipes.fingers[i].enabled == EINA_TRUE)
               {
                  for (j = 0; j < E_GESTURE_DIRECTION_MAX + 1; j++)
                    {
                       if (eina_list_count(gev->swipes.fingers[i].direction[j]) != 0)
                         {
                            flag_direction = EINA_TRUE;
                            break;
                         }
                    }
                  if (flag_direction == EINA_FALSE)
                    {
                       gev->swipes.fingers[i].enabled = EINA_FALSE;
                       cout_enabled++;
                    }
               }
             else
               {
                  cout_enabled++;
               }
          }
        if (cout_enabled == E_GESTURE_FINGER_MAX + 1)
          {
             gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_SWIPE;
          }
     }
}

static void
_e_gesture_client_remove(struct wl_client *client)
{
   E_Gesture_Event *gev;
   int i, j;
   Eina_List *l, *l_next;
   E_Gesture_Event_Swipe_Finger_Direction *ddata;
   Eina_Bool flag_direction = EINA_FALSE;
   unsigned int cout_enabled = 0;

   gev = &gesture->gesture_events;
   if (gesture->grabbed_gesture & TIZEN_GESTURE_TYPE_SWIPE)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX + 1; i++)
          {
             flag_direction = EINA_FALSE;
             if (gev->swipes.fingers[i].enabled == EINA_TRUE)
               {
                  for (j = 0; j < E_GESTURE_DIRECTION_MAX + 1; j++)
                    {
                       if (eina_list_count(gev->swipes.fingers[i].direction[j]) != 0)
                         {
                            flag_direction = EINA_TRUE;
                            EINA_LIST_FOREACH_SAFE(gev->swipes.fingers[i].direction[j], l, l_next, ddata)
                              {
                                 if (ddata->client == client)
                                   {
                                      E_FREE(ddata);
                                      gev->swipes.fingers[i].direction[j] = eina_list_remove_list(gev->swipes.fingers[i].direction[j], l);
                                   }
                              }
                         }
                    }
                  if (flag_direction == EINA_FALSE)
                    {
                       gev->swipes.fingers[i].enabled = EINA_FALSE;
                       cout_enabled++;
                    }
               }
             else
               {
                  cout_enabled++;
               }
          }
        if (cout_enabled == E_GESTURE_FINGER_MAX + 1)
          {
             gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_SWIPE;
          }
     }
}


static void
_e_gesture_cb_ungrab_swipe(struct wl_client *client,
                           struct wl_resource *resouce,
                           uint32_t num_of_fingers, uint32_t direction)
{
   E_Gesture_Event *gev;
   int ret = TIZEN_GESTURE_ERROR_NONE;
   Eina_List *l, *l_next;
   E_Gesture_Event_Swipe_Finger_Direction *ddata;
   Eina_Bool flag_removed = EINA_FALSE, res = EINA_FALSE;

   GTINF("client %p is request ungrab swipe gesture, fingers: %d, direction: 0x%x\n", client, num_of_fingers, direction);

   if (num_of_fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", num_of_fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   EINA_LIST_FOREACH_SAFE(gev->swipes.fingers[num_of_fingers].direction[direction], l, l_next, ddata)
     {
        if (ddata->client == client)
          {
             E_FREE(ddata);
             gev->swipes.fingers[num_of_fingers].direction[direction] = eina_list_remove_list(
                gev->swipes.fingers[num_of_fingers].direction[direction], l);
             flag_removed = EINA_TRUE;
          }
     }

   if (flag_removed)
     {
        res = _e_gesture_grabbed_client_check(client);
        if (res == EINA_FALSE)
          {
             _e_gesture_remove_grab_client_destroy_listener(client);
          }
     }

   _e_gesture_state_cleanup();

finish:
   tizen_gesture_send_grab_swipe_notify(resouce, num_of_fingers, direction, ret);
   return;
}

static void
_e_gesture_cb_enable(struct wl_client *client,
                           struct wl_resource *resouce,
                           struct wl_resource *surface,
                           uint32_t enabled)
{
   E_Client *ec;
   Eina_Bool enable_flag, res;

   enable_flag = (Eina_Bool)!!enabled;
   ec = wl_resource_get_user_data(surface);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   if (enable_flag)
     {
        gesture->disable_client_list = eina_list_remove(gesture->disable_client_list, ec);
        ec->gesture_disable = EINA_FALSE;
        if (!gesture->enable && (eina_list_count(gesture->disable_client_list) <= 0))
          {
             _e_gesture_enable();
          }
     }
   else
     {
        res = _e_gesture_eclient_list_add(&gesture->disable_client_list, ec);
        ec->gesture_disable = EINA_TRUE;
        if (res)
          {
             if (gesture->enable)
               {
                  if (e_client_focused_get() == ec)
                    {
                       _e_gesture_disable();
                    }
               }
          }
     }
}


static const struct tizen_gesture_interface _e_gesture_implementation = {
   _e_gesture_cb_grab_swipe,
   _e_gesture_cb_ungrab_swipe,
   _e_gesture_cb_enable
};

/* tizen_gesture global object destroy function */
static void
_e_gesture_cb_destory(struct wl_resource *resource)
{
   /* TODO : destroy resources if exist */
}

/* tizen_keyrouter global object bind function */
static void
_e_gesture_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_GesturePtr gesture_instance = data;
   struct wl_resource *resource;

   resource = wl_resource_create(client, &tizen_gesture_interface, MIN(version, 1), id);

   GTDBG("wl_resource_create(...,tizen_gesture_interface,...)\n");

   if (!resource)
     {
        GTERR("Failed to create resource ! (version :%d, id:%d)\n", version, id);
        wl_client_post_no_memory(client);
	 return;
     }

   wl_resource_set_implementation(resource, &_e_gesture_implementation, gesture_instance, _e_gesture_cb_destory);
}

static Eina_Bool
_e_gesture_event_filter(void *data, void *loop_data EINA_UNUSED, int type, void *event)
{
   (void) data;

   return e_gesture_process_events(event, type);
}

static Eina_Bool
_e_gesture_cb_client_focus_in(void *data, int type, void *event)
{
   E_Client *ec;
   E_Event_Client *ev = (E_Event_Client *)event;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, ECORE_CALLBACK_PASS_ON);
   ec = ev->ec;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec, ECORE_CALLBACK_PASS_ON);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->comp_data, ECORE_CALLBACK_PASS_ON);

   if (ec->gesture_disable && gesture->enable)
     {
        _e_gesture_disable();
     }
   else if (!ec->gesture_disable && !gesture->enable)
     {
        _e_gesture_enable();
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_gesture_cb_client_remove(void *data, int type, void *event)
{
   E_Client *ec, *edata;
   Eina_List *l, *l_next;
   E_Event_Client *ev = (E_Event_Client *)event;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, ECORE_CALLBACK_PASS_ON);
   ec = ev->ec;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec, ECORE_CALLBACK_PASS_ON);

   if (!ec->gesture_disable) return ECORE_CALLBACK_PASS_ON;

   EINA_LIST_FOREACH_SAFE(gesture->disable_client_list, l, l_next, edata)
     {
        if (edata == ec)
          {
             gesture->disable_client_list = eina_list_remove_list(gesture->disable_client_list, l);
          }
     }

   if (!gesture->enable && (eina_list_count(gesture->disable_client_list) <= 0))
     {
        _e_gesture_enable();
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_e_gesture_init_handlers(void)
{
   gesture->ef_handler = ecore_event_filter_add(NULL, _e_gesture_event_filter, NULL, NULL);

   gesture->handlers = eina_list_append(gesture->handlers,
                                        ecore_event_handler_add(E_EVENT_CLIENT_FOCUS_IN,
                                                                _e_gesture_cb_client_focus_in, NULL));
   gesture->handlers = eina_list_append(gesture->handlers,
                                        ecore_event_handler_add(E_EVENT_CLIENT_REMOVE,
                                                                _e_gesture_cb_client_remove, NULL));
}

static E_Gesture_Config_Data *
_e_gesture_init(E_Module *m)
{
   E_Gesture_Config_Data *gconfig = NULL;
   gesture = E_NEW(E_Gesture, 1);

   if (!gesture)
     {
        GTERR("Failed to allocate memory for gesture !\n");
        return NULL;
     }

   if (!e_comp)
     {
        GTERR("Failed to initialize gesture module ! (e_comp == NULL)\n");
        goto err;
     }

   /* Add filtering mechanism 
    * FIXME: Add handlers after first gesture is grabbed
    */
   _e_gesture_init_handlers();

   /* Init config */
   gconfig = E_NEW(E_Gesture_Config_Data, 1);
   EINA_SAFETY_ON_NULL_GOTO(gconfig, err);
   gconfig->module = m;

   e_gesture_conf_init(gconfig);
   EINA_SAFETY_ON_NULL_GOTO(gconfig->conf, err);
   gesture->config = gconfig;

   GTDBG("config value\n");
   GTDBG("keyboard: %s, time_done: %lf, time_begin: %lf\n", gconfig->conf->key_device_name, gconfig->conf->swipe.time_done, gconfig->conf->swipe.time_begin);
   GTDBG("area_offset: %d, min_length: %d, max_length: %d\n", gconfig->conf->swipe.area_offset, gconfig->conf->swipe.min_length, gconfig->conf->swipe.max_length);
   GTDBG("compose key: %d, back: %d, default: %d\n", gconfig->conf->swipe.compose_key, gconfig->conf->swipe.back_key, gconfig->conf->swipe.default_enable_back);

   gesture->global = wl_global_create(e_comp_wl->wl.disp, &tizen_gesture_interface, 1, gesture, _e_gesture_cb_bind);
   if (!gesture->global)
     {
        GTERR("Failed to create global !\n");
        goto err;
     }

   gesture->gesture_filter = E_GESTURE_TYPE_MAX;

   if (gconfig->conf->swipe.default_enable_back)
     {
        gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_SWIPE;
        gesture->gesture_events.swipes.fingers[1].enabled = EINA_TRUE;
     }

   e_gesture_device_keydev_set(gesture->config->conf->key_device_name);

   gesture->enable = EINA_TRUE;

   return gconfig;

err:
   if (gconfig) e_gesture_conf_deinit(gconfig);
   if (gesture && gesture->ef_handler) ecore_event_filter_del(gesture->ef_handler);
   if (gesture) E_FREE(gesture);

   return NULL;
}

E_API void *
e_modapi_init(E_Module *m)
{
   return _e_gesture_init(m);
}

E_API int
e_modapi_shutdown(E_Module *m)
{
   E_Gesture_Config_Data *gconfig = m->data;
   e_gesture_conf_deinit(gconfig);
   e_gesture_device_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m)
{
   /* Save something to be kept */
   E_Gesture_Config_Data *gconfig = m->data;
   e_config_domain_save("module.gesture",
                        gconfig->conf_edd,
                        gconfig->conf);
   return 1;
}

static void
_e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data)
{
   struct wl_client *client = data;

   _e_gesture_client_remove(client);
  _e_gesture_remove_grab_client_destroy_listener(client);
}
