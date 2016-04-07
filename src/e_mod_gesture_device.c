#define E_COMP_WL
#include "e_mod_main.h"
#include <string.h>
#include <linux/uinput.h>

static void
_e_gesture_device_keydev_create(void)
{
   int uinp_fd = -1;
   struct uinput_user_dev uinp;
   int ret = 0;

   uinp_fd = open("/dev/uinput", O_WRONLY | O_NDELAY);
   if ( uinp_fd < 0)
     {
        GTWRN("Failed to open /dev/uinput: (%d)\n", uinp_fd);
        return;
     }

   memset(&uinp, 0, sizeof(struct uinput_user_dev));
   strncpy(uinp.name, E_GESTURE_KEYBOARD_NAME, UINPUT_MAX_NAME_SIZE);
   uinp.id.version = 4;
   uinp.id.bustype = BUS_USB;

   ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);
   ioctl(uinp_fd, UI_SET_EVBIT, EV_SYN);
   ioctl(uinp_fd, UI_SET_EVBIT, EV_MSC);

   ioctl(uinp_fd, UI_SET_KEYBIT, KEY_BACK);

   ret = write(uinp_fd, &uinp, sizeof(struct uinput_user_dev));
   if (ret < 0)
     {
        GTWRN("Failed to write UINPUT device\n");
        close(uinp_fd);
        return;
     }
   if (ioctl(uinp_fd, UI_DEV_CREATE))
     {
        GTWRN("Unable to create UINPUT device\n");
        close(uinp_fd);
        return;
     }

   gesture->device.uinp_fd = uinp_fd;
}

void
e_gesture_device_keydev_set(char *option)
{
   if (!option)
     {
        _e_gesture_device_keydev_create();
        gesture->device.kbd_name = strdup(E_GESTURE_KEYBOARD_NAME);
     }
   else if (strncmp(option, "Any", sizeof("Any")))
     {
        gesture->device.kbd_name = strdup(option);
     }
}

Ecore_Device *
_e_gesture_device_ecore_device_get(char *path, unsigned int devclass)
{
   const Eina_List *dev_list = NULL;
   const Eina_List *l;
   Ecore_Device *dev = NULL;
   const char *identifier;

   if (!path) return NULL;

   dev_list = ecore_device_list();
   if (!dev_list) return NULL;
   EINA_LIST_FOREACH(dev_list, l, dev)
     {
        if (!dev) continue;
        GTINF("dev: %s\n", ecore_device_name_get(dev));
        identifier = ecore_device_identifier_get(dev);
        if (!identifier) continue;
        if ((ecore_device_class_get(dev) == devclass) && !(strcmp(identifier, path)))
          return dev;
     }

   return NULL;
}

Eina_Bool
e_gesture_device_add(Ecore_Event_Device_Info *ev)
{
   if (ev->clas == ECORE_DEVICE_CLASS_TOUCH)
     {
        gesture->device.touch_devices = eina_list_append(gesture->device.touch_devices, ev->identifier);
        GTINF("%s(%s) device is touch device: add list\n", ev->name, ev->identifier);
     }
   if ((!gesture->device.kbd_identifier) &&
       (ev->clas == ECORE_DEVICE_CLASS_KEYBOARD))
     {
        if (gesture->device.kbd_name)
          {
             if (!strncmp(ev->name, gesture->device.kbd_name, strlen(gesture->device.kbd_name)))
               {
                  GTINF("%s(%s) device is key generated device in gesture\n", ev->name, ev->identifier);
                  gesture->device.kbd_identifier = strdup(ev->identifier);
                  gesture->device.kbd_device = _e_gesture_device_ecore_device_get(gesture->device.kbd_identifier, ECORE_DEVICE_CLASS_KEYBOARD);
               }
          }
        else
          {
             GTINF("%s(%s) device is key generated device in gesture\n", ev->name, ev->identifier);
             gesture->device.kbd_name = strdup(ev->name);
             gesture->device.kbd_identifier = strdup(ev->identifier);
             gesture->device.kbd_device = _e_gesture_device_ecore_device_get(gesture->device.kbd_identifier, ECORE_DEVICE_CLASS_KEYBOARD);
          }
     }
   return EINA_TRUE;
}

Eina_Bool
e_gesture_device_del(Ecore_Event_Device_Info *ev)
{
   Eina_List *l, *l_next;
   char *data;

   if (ev->clas == ECORE_DEVICE_CLASS_TOUCH)
     {
        EINA_LIST_FOREACH_SAFE(gesture->device.touch_devices, l, l_next, data)
          {
             if (!strncmp(data, ev->identifier, strlen(ev->identifier)))
               {
                  GTINF("%s(%s) device is touch device: remove list\n", ev->name, ev->identifier);
                  gesture->device.touch_devices = eina_list_remove(gesture->device.touch_devices, data);
                  E_FREE(data);
               }
          }
     }
   if ((gesture->device.kbd_identifier) &&
       (ev->clas == ECORE_DEVICE_CLASS_KEYBOARD))
     {
        if (!strncmp(ev->name, gesture->device.kbd_name, strlen(gesture->device.kbd_name)))
          {
             GTWRN("Gesture keyboard device(%s) is disconnected. Gesture cannot create key events\n", gesture->device.kbd_name);
             E_FREE(gesture->device.kbd_identifier);
             E_FREE(gesture->device.kbd_name);
          }
     }
   return EINA_TRUE;
}

Eina_Bool
e_gesture_is_touch_device(const Ecore_Device *dev)
{
   if (ecore_device_class_get(dev) == ECORE_DEVICE_CLASS_TOUCH)
     return EINA_TRUE;

   return EINA_FALSE;
}

void
e_gesture_device_shutdown(void)
{
   E_FREE(gesture->device.kbd_identifier);
   E_FREE(gesture->device.kbd_name);
   gesture->device.touch_devices = eina_list_free(gesture->device.touch_devices);
}
