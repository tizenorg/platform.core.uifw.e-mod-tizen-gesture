#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include <e.h>
#include <tizen-extension-server-protocol.h>
#include <Ecore_Drm.h>

#define GTERR(msg, ARG...) ERR("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTWRN(msg, ARG...) WRN("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTINF(msg, ARG...) INF("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)
#define GTDBG(msg, ARG...) DBG("[tizen_gesture][%s:%d] "msg, __FUNCTION__, __LINE__, ##ARG)

#define E_GESTURE_FINGER_MAX 3
#define E_GESTURE_TYPE_MAX TIZEN_GESTURE_TYPE_SWIPE+1
#define E_GESTURE_TYPE_ALL TIZEN_GESTURE_TYPE_SWIPE
#define E_GESTURE_KEYBOARD_NAME "Gesture Keyboard"

/* FIX ME: Set values in contiguration file, do not use definition */
#define E_GESTURE_KEYBOARD_DEVICE "Any"

#define E_GESTURE_SWIPE_DONE_TIME 0.5
#define E_GESTURE_SWIPE_START_TIME 0.01
#define E_GESTURE_SWIPE_START_AREA 50
#define E_GESTURE_SWIPE_DIFF_FAIL 100
#define E_GESTURE_SWIPE_DIFF_SUCCESS 300
/* FIX ME: Key code will be get from keymap */
#define E_GESTURE_SWIPE_COMBINE_KEY 124
#define E_GESTURE_SWIPE_BACK_KEY 166
#define E_GESTURE_SWIPE_BACK_DEFAULT_ENABLE EINA_TRUE

#define ABS(x) ((x)>0)?(x):-(x)

typedef struct _E_Gesture E_Gesture;
typedef struct _E_Gesture* E_GesturePtr;
typedef struct _E_Gesture_Event E_Gesture_Event;
typedef struct _E_Gesture_Event_Swipe E_Gesture_Event_Swipe;
typedef struct _E_Gesture_Event_Swipe_Finger E_Gesture_Event_Swipe_Finger;
typedef struct _E_Gesture_Event_Swipe_Finger_Direction E_Gesture_Event_Swipe_Finger_Direction;
typedef struct _E_Gesture_Grabbed_Client E_Gesture_Grabbed_Client;
typedef struct _E_Gesture_Conf_Edd E_Gesture_Conf_Edd;
typedef struct _E_Gesture_Config_Data E_Gesture_Config_Data;

typedef struct _Coords Coords;

typedef enum _E_Gesture_Direction E_Gesture_Direction;

extern E_GesturePtr gesture;

#define E_GESTURE_DIRECTION_MAX 4
enum _E_Gesture_Direction
{
   E_GESTURE_DIRECTION_NONE,
   E_GESTURE_DIRECTION_DOWN, //Start point is North
   E_GESTURE_DIRECTION_LEFT, // Start point is East
   E_GESTURE_DIRECTION_UP, // Start point is South
   E_GESTURE_DIRECTION_RIGHT // Start point is West
};

struct _Coords
{
   int x, y;
};

struct _E_Gesture_Conf_Edd
{
   char *key_device_name;
   struct
   {
      double time_done;
      double time_begin;
      int area_offset;
      int min_length;
      int max_length;
      int compose_key;
      int back_key;
      Eina_Bool default_enable_back;
   } swipe;
};

struct _E_Gesture_Config_Data
{
   E_Module *module;
   E_Config_DD *conf_edd;
   E_Gesture_Conf_Edd *conf;
};

struct _E_Gesture_Event_Swipe_Finger_Direction
{
   struct wl_client *client;
   struct wl_resource *res;
};

struct _E_Gesture_Event_Swipe_Finger
{
   Coords start;
   Eina_Bool enabled;
   E_Gesture_Event_Swipe_Finger_Direction direction[E_GESTURE_DIRECTION_MAX+1];
};

struct _E_Gesture_Grabbed_Client
{
   struct wl_client *client;
   struct wl_listener *destroy_listener;

   E_Gesture_Event_Swipe_Finger swipe_fingers[E_GESTURE_FINGER_MAX+1];
};


struct _E_Gesture_Event_Swipe
{
   E_Gesture_Event_Swipe_Finger fingers[E_GESTURE_FINGER_MAX+1];

   E_Gesture_Direction direction;

   unsigned int combined_keycode;
   unsigned int back_keycode;

   unsigned int enabled_finger;
   Ecore_Timer *start_timer;
   Ecore_Timer *done_timer;
};

struct _E_Gesture_Event
{
   E_Gesture_Event_Swipe swipes;

   int num_pressed;
   Eina_Bool recognized_gesture;
};

struct _E_Gesture
{
   struct wl_global *global;
   E_Gesture_Config_Data *config;
   Eina_Bool enable;

   Ecore_Event_Filter *ef_handler;
   Eina_List *handlers;
   Eina_List *grab_client_list;
   Eina_List *disable_client_list;

   struct
   {
      Eina_List *touch_devices;
      int uinp_fd;
      char *kbd_identifier;
      char *kbd_name;
      Ecore_Device *kbd_device;
   }device;

   unsigned int grabbed_gesture;
   E_Gesture_Event gesture_events;

   unsigned int gesture_filter;
   unsigned int gesture_recognized;
};

/* E Module */
E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

Eina_Bool e_gesture_process_events(void *event, int type);
int e_gesture_type_convert(uint32_t type);

/* Config */
void e_gesture_conf_init(E_Gesture_Config_Data *gconfig);
void e_gesture_conf_deinit(E_Gesture_Config_Data *gconfig);

/* Device control */
void e_gesture_device_shutdown(void);
Eina_Bool e_gesture_device_add(Ecore_Event_Device_Info *ev);
Eina_Bool e_gesture_device_del(Ecore_Event_Device_Info *ev);
Eina_Bool e_gesture_is_touch_device(const Ecore_Device *dev);
void e_gesture_device_keydev_set(char *option);

#endif
