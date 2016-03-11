#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>

E_GesturePtr gesture = NULL;
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Gesture Module of Window Manager" };

static void *_e_gesture_init(E_Module *m);
static void _e_gesture_init_handlers(void);


static void _e_gesture_wl_client_cb_destroy(struct wl_listener *l, void *data);


/* Function for registering wl_client destroy listener */
int
e_gesture_add_client_destroy_listener(struct wl_client *client)
{
   struct wl_listener *destroy_listener = NULL;
   Eina_List *l;
   struct wl_client *wc_data;

   EINA_LIST_FOREACH(gesture->grab_client_list, l, wc_data)
     {
        if (wc_data)
          {
             if (wc_data == client)
               {
                  return TIZEN_GESTURE_ERROR_NONE;
               }
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
_e_gesture_cb_grab(struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t mode, uint32_t num_of_fingers)
{
   E_Gesture_Event *gev;

   GTINF("client %p is request grab gesture, mode: 0x%x, fingers: %d\n", client, mode, num_of_fingers);
   if (mode >= E_GESTURE_MODE_MAX)
     {
        GTWRN("Invalid gesture mode: %d\n", mode);
        tizen_gesture_send_grab_notify(resource, mode, num_of_fingers, TIZEN_GESTURE_ERROR_INVALID_DATA);
        goto out;
     }
   if (num_of_fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", num_of_fingers, E_GESTURE_FINGER_MAX);
        tizen_gesture_send_grab_notify(resource, mode, num_of_fingers, TIZEN_GESTURE_ERROR_INVALID_DATA);
        goto out;
     }

   gev = &gesture->gesture_events;

   switch(mode)
     {
        case TIZEN_GESTURE_MODE_FLICK:
           if (gev->flicks.fingers[num_of_fingers].client)
             {
                tizen_gesture_send_grab_notify(resource, mode, num_of_fingers, TIZEN_GESTURE_ERROR_GRABBED_ALREADY);
                goto out;
             }
           gev->flicks.fingers[num_of_fingers].client = client;
           gev->flicks.fingers[num_of_fingers].res = resource;
           e_gesture_add_client_destroy_listener(client);
           gesture->grabbed_gesture |= mode;
           tizen_gesture_send_grab_notify(resource, mode, num_of_fingers, TIZEN_GESTURE_ERROR_NONE);
           break;
     }

out:
   return;
}

static void
_e_gesture_cb_ungrab(struct wl_client *client,
                     struct wl_resource *resouce,
                     uint32_t mode, uint32_t num_of_fingers)
{
   int i;
   E_Gesture_Event *gev;

   GTINF("client %p is request ungrab gesture, mode: 0x%x, fingers: %d\n", client, mode, num_of_fingers);
   if (mode >= E_GESTURE_MODE_MAX)
     {
        GTWRN("Invalid gesture mode: %d\n", mode);
        tizen_gesture_send_grab_notify(resouce, mode, num_of_fingers, TIZEN_GESTURE_ERROR_INVALID_DATA);
        goto out;
     }
   if (num_of_fingers > E_GESTURE_FINGER_MAX)
     {
        GTWRN("Do not support %d fingers (max: %d)\n", num_of_fingers, E_GESTURE_FINGER_MAX);
        tizen_gesture_send_grab_notify(resouce, mode, num_of_fingers, TIZEN_GESTURE_ERROR_INVALID_DATA);
        goto out;
     }

   gev = &gesture->gesture_events;

   switch(mode)
     {
        case TIZEN_GESTURE_MODE_FLICK:
           if (gev->flicks.fingers[num_of_fingers].client)
             {
                gev->flicks.fingers[num_of_fingers].client = NULL;
                gev->flicks.fingers[num_of_fingers].res = NULL;
                for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
                  {
                     if (gev->flicks.fingers[i].client) goto out;
                  }
               gesture->grabbed_gesture &= ~mode;
             }
           break;
     }

   tizen_gesture_send_grab_notify(resouce, mode, num_of_fingers, TIZEN_GESTURE_ERROR_NONE);

out:
   return;
}

static const struct tizen_gesture_interface _e_gesture_implementation = {
   _e_gesture_cb_grab,
   _e_gesture_cb_ungrab,
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

   gesture->gesture_filter = E_GESTURE_MODE_MAX;

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
   int i;

   if (gesture->grabbed_gesture & TIZEN_GESTURE_MODE_FLICK)
     {
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             if (gesture->gesture_events.flicks.fingers[i].client == client)
               {
                  gesture->gesture_events.flicks.fingers[i].client = NULL;
                  gesture->gesture_events.flicks.fingers[i].res = NULL;
                  gesture->grab_client_list = eina_list_remove(gesture->grab_client_list, client);
               }
          }
        for (i = 0; i < E_GESTURE_FINGER_MAX+1; i++)
          {
             if (gesture->gesture_events.flicks.fingers[i].client)
               {
                  goto out;
               }
             gesture->grabbed_gesture &= ~TIZEN_GESTURE_MODE_FLICK;
          }
     }

out:
   E_FREE(l);
   l = NULL;
}
