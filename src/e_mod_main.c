#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

E_GesturePtr gesture = NULL;
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Gesture Module of Window Manager" };

static void *_e_gesture_init(E_Module *m);
static void _e_gesture_init_handlers(void);


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

/* Function for registering wl_client destroy listener */
int
e_gesture_add_client_destroy_listener(struct wl_client *client, int mode EINA_UNUSED, int num_of_fingers, unsigned int direction)
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
_e_gesture_remove_client_destroy_listener(struct wl_client *client, unsigned int num_of_fingers, unsigned int direction)
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

   e_gesture_add_client_destroy_listener(client, TIZEN_GESTURE_TYPE_SWIPE, num_of_fingers, direction & ~grabbed_direction);
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
        _e_gesture_remove_client_destroy_listener(client, num_of_fingers, direction & ~ungrabbed_direction);
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

static const struct tizen_gesture_interface _e_gesture_implementation = {
   _e_gesture_cb_grab_swipe,
   _e_gesture_cb_ungrab_swipe
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

static void
_e_gesture_init_handlers(void)
{
   gesture->ef_handler = ecore_event_filter_add(NULL, _e_gesture_event_filter, NULL, NULL);
}

static void *
_e_gesture_init(E_Module *m)
{
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

   /* Add filtering mechanism */
   _e_gesture_init_handlers();
   gesture->global = wl_global_create(e_comp_wl->wl.disp, &tizen_gesture_interface, 1, gesture, _e_gesture_cb_bind);
   if (!gesture->global)
     {
        GTERR("Failed to create global !\n");
        goto err;
     }

   gesture->gesture_filter = E_GESTURE_TYPE_MAX;

   if (E_GESTURE_SWIPE_BACK_DEFAULT_ENABLE)
     {
        gesture->grabbed_gesture |= TIZEN_GESTURE_TYPE_SWIPE;
        gesture->gesture_events.swipes.fingers[1].enabled = EINA_TRUE;
        gesture->gesture_events.swipes.fingers[1].direction[E_GESTURE_DIRECTION_DOWN].client = (void *)0x1;
        gesture->gesture_events.swipes.fingers[1].direction[E_GESTURE_DIRECTION_DOWN].res = (void *)0x1;
     }

   e_gesture_device_keydev_set(E_GESTURE_KEYBOARD_DEVICE);

   return m;

err:
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
   e_gesture_device_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m)
{
   /* Save something to be kept */
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
