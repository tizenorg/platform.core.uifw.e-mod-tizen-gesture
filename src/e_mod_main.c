#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

E_GesturePtr gesture = NULL;
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Gesture Module of Window Manager" };

static E_Gesture_Config_Data *_e_gesture_init(E_Module *m);
static void _e_gesture_init_handlers(void);

static Eina_Bool _e_gesture_event_filter(void *data, void *loop_data EINA_UNUSED, int type, void *event);
static void _e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data);

static void
_e_gesture_swipe_set_client_to_list(struct wl_client *client, E_Gesture_Event_Swipe_Finger *fingers, unsigned int direction)
{
   if (direction & TIZEN_GESTURE_DIRECTION_DOWN)
     fingers->direction[E_GESTURE_DIRECTION_DOWN].client = client;
   if (direction & TIZEN_GESTURE_DIRECTION_LEFT)
     fingers->direction[E_GESTURE_DIRECTION_LEFT].client = client;
   if (direction & TIZEN_GESTURE_DIRECTION_UP)
     fingers->direction[E_GESTURE_DIRECTION_UP].client = client;
   if (direction & TIZEN_GESTURE_DIRECTION_RIGHT)
     fingers->direction[E_GESTURE_DIRECTION_RIGHT].client = client;
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
_e_gesture_add_grab_client_destroy_listener(struct wl_client *client, int mode EINA_UNUSED, int num_of_fingers, unsigned int direction)
{
   struct wl_listener *destroy_listener = NULL;
   Eina_List *l;
   E_Gesture_Grabbed_Client *grabbed_client, *data;

   EINA_LIST_FOREACH(gesture->grab_client_list, l, data)
     {
        if (data->client == client)
          {
             _e_gesture_swipe_set_client_to_list(client, &data->swipe_fingers[num_of_fingers], direction);

             return TIZEN_GESTURE_ERROR_NONE;
          }
     }

   destroy_listener = E_NEW(struct wl_listener, 1);
   if (!destroy_listener)
     {
        GTERR("Failed to allocate memory for wl_client destroy listener !\n");
        return TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
     }

   grabbed_client = E_NEW(E_Gesture_Grabbed_Client, 1);
   if (!grabbed_client)
     {
        GTERR("Failed to allocate memory to save client information !\n");
        return TIZEN_GESTURE_ERROR_NO_SYSTEM_RESOURCES;
     }

   destroy_listener->notify = _e_gesture_wl_client_cb_destroy;
   wl_client_add_destroy_listener(client, destroy_listener);
   grabbed_client->client = client;
   grabbed_client->destroy_listener = destroy_listener;
   _e_gesture_swipe_set_client_to_list(client, &grabbed_client->swipe_fingers[num_of_fingers], direction);

   gesture->grab_client_list = eina_list_append(gesture->grab_client_list, grabbed_client);

   return TIZEN_KEYROUTER_ERROR_NONE;
}

static void
_e_gesture_remove_grab_client_destroy_listener(struct wl_client *client, unsigned int num_of_fingers, unsigned int direction)
{
   Eina_List *l, *l_next;
   E_Gesture_Grabbed_Client *data;
   int i;

   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l, l_next, data)
     {
        if (data->client == client)
          {
             _e_gesture_swipe_set_client_to_list(NULL, &data->swipe_fingers[num_of_fingers], direction);

             for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
               {
                  if (data->swipe_fingers[i].direction[E_GESTURE_DIRECTION_DOWN].client ||
                      data->swipe_fingers[i].direction[E_GESTURE_DIRECTION_LEFT].client ||
                      data->swipe_fingers[i].direction[E_GESTURE_DIRECTION_UP].client ||
                      data->swipe_fingers[i].direction[E_GESTURE_DIRECTION_RIGHT].client)
                    {
                       return;
                    }
               }
             wl_list_remove(&data->destroy_listener->link);
             E_FREE(data->destroy_listener);
             gesture->grab_client_list = eina_list_remove(gesture->grab_client_list, data);
             E_FREE(data);
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
                   uint32_t num_of_fingers, uint32_t direction)
{
   E_Gesture_Event *gev;
   unsigned int grabbed_direction = 0x0;

   GTINF("client %p is request grab gesture, fingers: %d, direction: 0x%x\n", client, num_of_fingers, direction);
   if (num_of_fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", num_of_fingers, E_GESTURE_FINGER_MAX);
        tizen_gesture_send_grab_swipe_notify(resource, num_of_fingers, direction, TIZEN_GESTURE_ERROR_INVALID_DATA);
        goto out;
     }

   gev = &gesture->gesture_events;

   if (direction & TIZEN_GESTURE_DIRECTION_DOWN)
     {
        if (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_DOWN].client)
           {
              grabbed_direction |= TIZEN_GESTURE_DIRECTION_DOWN;
           }
        else
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_DOWN].client = client;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_DOWN].res = resource;
           }
     }
   if (direction & TIZEN_GESTURE_DIRECTION_LEFT)
     {
        if (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_LEFT].client)
           {
              grabbed_direction |= TIZEN_GESTURE_DIRECTION_LEFT;
           }
        else
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_LEFT].client = client;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_LEFT].res = resource;
           }
     }
   if (direction & TIZEN_GESTURE_DIRECTION_UP)
     {
        if (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_UP].client)
           {
              grabbed_direction |= TIZEN_GESTURE_DIRECTION_UP;
           }
        else
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_UP].client = client;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_UP].res = resource;
           }
     }
   if (direction & TIZEN_GESTURE_DIRECTION_RIGHT)
     {
        if (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_RIGHT].client)
           {
              grabbed_direction |= TIZEN_GESTURE_DIRECTION_RIGHT;
           }
        else
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_RIGHT].client = client;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_RIGHT].res = resource;
           }
     }

   if (grabbed_direction)
     tizen_gesture_send_grab_swipe_notify(resource, num_of_fingers, grabbed_direction, TIZEN_GESTURE_ERROR_GRABBED_ALREADY);

   _e_gesture_add_grab_client_destroy_listener(client, TIZEN_GESTURE_TYPE_SWIPE, num_of_fingers, direction & ~grabbed_direction);
   gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_SWIPE;
   gev->swipes.fingers[num_of_fingers].enabled = EINA_TRUE;

   if (!grabbed_direction)
     tizen_gesture_send_grab_swipe_notify(resource, num_of_fingers, direction, TIZEN_GESTURE_ERROR_NONE);

out:
   return;
}

static void
_e_gesture_cb_ungrab_swipe(struct wl_client *client,
                           struct wl_resource *resouce,
                           uint32_t num_of_fingers, uint32_t direction)
{
   int i, j;
   E_Gesture_Event *gev;
   unsigned int ungrabbed_direction = 0x0;
   int ret = TIZEN_GESTURE_ERROR_NONE;

   GTINF("client %p is request ungrab swipe gesture, fingers: %d, direction: 0x%x, client: %p\n", client, num_of_fingers, direction, gesture->gesture_events.swipes.fingers[0].direction[3].client);

   if (num_of_fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", num_of_fingers, E_GESTURE_FINGER_MAX);
        ret = TIZEN_GESTURE_ERROR_INVALID_DATA;
        goto finish;
     }

   gev = &gesture->gesture_events;

   if (direction & TIZEN_GESTURE_DIRECTION_DOWN)
     {
        if ((gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_DOWN].client) &&
            (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_DOWN].client == client))
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_DOWN].client = NULL;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_DOWN].res = NULL;
           }
        else
           {
              ungrabbed_direction |= TIZEN_GESTURE_DIRECTION_DOWN;
           }
     }
   if (direction & TIZEN_GESTURE_DIRECTION_LEFT)
     {
        if ((gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_LEFT].client) &&
            (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_LEFT].client == client))
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_LEFT].client = NULL;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_LEFT].res = NULL;
           }
        else
           {
              ungrabbed_direction |= TIZEN_GESTURE_DIRECTION_LEFT;
           }
     }
   if (direction & TIZEN_GESTURE_DIRECTION_UP)
     {
        if ((gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_UP].client) &&
            (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_UP].client == client))
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_UP].client = NULL;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_UP].res = NULL;
           }
        else
           {
              ungrabbed_direction |= TIZEN_GESTURE_DIRECTION_UP;
           }
     }
   if (direction & TIZEN_GESTURE_DIRECTION_RIGHT)
     {
        if ((gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_RIGHT].client) &&
            (gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_RIGHT].client == client))
           {
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_RIGHT].client = NULL;
              gev->swipes.fingers[num_of_fingers].direction[E_GESTURE_DIRECTION_RIGHT].res = NULL;
           }
        else
           {
              ungrabbed_direction |= TIZEN_GESTURE_DIRECTION_RIGHT;
           }
     }

   if (direction & ~ungrabbed_direction)
     {
        _e_gesture_remove_grab_client_destroy_listener(client, num_of_fingers, direction & ~ungrabbed_direction);
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             for (j = 0; j < E_GESTURE_DIRECTION_MAX+1; j++)
               {
                  if (gev->swipes.fingers[i].direction[j].client)
                    {
                       goto finish;
                    }
               }
             gev->swipes.fingers[i].enabled = EINA_FALSE;
          }
        gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_SWIPE;
     }

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
        gesture->gesture_events.swipes.fingers[1].direction[E_GESTURE_DIRECTION_DOWN].client = (void *)0x1;
        gesture->gesture_events.swipes.fingers[1].direction[E_GESTURE_DIRECTION_DOWN].res = (void *)0x1;
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
   int i, j;
   Eina_List *l_list, *l_next;
   E_Gesture_Grabbed_Client *client_data;

   if (gesture->grabbed_gesture & TIZEN_GESTURE_TYPE_SWIPE)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             for (j = 0; j < E_GESTURE_DIRECTION_MAX+1; j++)
               {
                  if (gesture->gesture_events.swipes.fingers[i].direction[j].client == client)
                    {
                       gesture->gesture_events.swipes.fingers[i].direction[j].client = NULL;
                       gesture->gesture_events.swipes.fingers[i].direction[j].res = NULL;
                    }
               }
          }

        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             for (j = 0; j < E_GESTURE_DIRECTION_MAX+1; j++)
               {
                  if (gesture->gesture_events.swipes.fingers[i].direction[j].client)
                    {
                       goto out;
                    }
               }
             gesture->gesture_events.swipes.fingers[i].enabled = EINA_FALSE;
          }
        gesture->grabbed_gesture &= ~TIZEN_GESTURE_TYPE_SWIPE;
     }

out:
   E_FREE(l);
   l = NULL;
   EINA_LIST_FOREACH_SAFE(gesture->grab_client_list, l_list, l_next, client_data)
     {
        if (client_data->client == client)
          {
             gesture->grab_client_list = eina_list_remove(gesture->grab_client_list, client_data);
             E_FREE(client_data);
          }
     }
}
