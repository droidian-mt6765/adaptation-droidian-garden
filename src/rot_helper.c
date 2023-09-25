// https://github.com/thatsitipl/garden_rotation_helper

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <gio/gio.h>
#include <unistd.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>


#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 1600

#define SCREEN_WIDTH_ROT 360
#define SCREEN_HEIGHT_ROT 800

float SCREEN_WIDTH_RATIO = 2.22;
float SCREEN_HEIGHT_RATIO = 2.22;

enum ROTATION_T{
  ROT_NORMAL = 0,
  ROT_RIGHT_UP = 1,
  ROT_BOTTOM_UP = 2,
  ROT_LEFT_UP = 3
};

GError *error = NULL;
GDBusProxy *proxy;
GDBusConnection *connection;
GMainLoop *loop;

uint32_t transform = 0;

bool should_exit = false;

const gchar *bus_name = "org.gnome.Mutter.DisplayConfig";
const gchar *object_path = "/org/gnome/Mutter/DisplayConfig";
const gchar *interface_name = "org.gnome.Mutter.DisplayConfig";
const gchar *method_name = "GetResources";
const gchar *signal_name = "MonitorsChanged";

void update_rotation(){
  GVariant *result;
  GVariant *crtcs;


  result = g_dbus_proxy_call_sync(proxy,
                                  method_name,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);

  if (error != NULL) {
      g_error("D-Bus method call failed: %s\n", error->message);
      g_error_free(error);
      return ;
  }

  /* Crtcs type :a(uxiiiiiuaua{sv})
     u: the ID in the API of this CRTC
     x: he low-level ID of this CRTC (which might be a XID, a KMS handle or something entirely different)
     i (x, y, width, height): the geometry of this CRTC (might be invalid if the CRTC is not in use)
     i: the current mode of the CRTC, or -1 if this CRTC is not used. Note: the size of the mode will always correspond to the width and height of the CRTC
     u: the current transform (espressed according to the wayland protocol)
     au: all possible transforms
     a{sv} properties: other high-level properties that affect this CRTC; they are not necessarily reflected in the hardware. (No property is specified in this version of the API.)
  */
  crtcs = g_variant_get_child_value(result, 1);

  // if switched off size changes
  if(g_variant_n_children(crtcs) != 1){
    return;
  }

  GVariant *array_child = g_variant_get_child_value(crtcs, 0);

  g_variant_get_child(array_child, 7, "u", &transform);

  g_variant_unref(result);
}


static void on_rotation_change(GDBusConnection *connection, const gchar *sender_name, const gchar *object_path, const gchar *interface_name, const gchar *signal_name, GVariant *parameters, gpointer user_data){
  update_rotation();
}

void cleanup(){
  g_error_free(error);
  g_object_unref(connection);
  g_object_unref(proxy);
  g_object_unref(loop);
}

void *touch_transformer(void * event_file){
  struct libevdev *dev = NULL;
  struct libevdev_uinput *uidev;
  struct input_event ev;
  unsigned int new_value;
  unsigned short new_code;


  int fd, uifd;
  int rc = 1;
  int wc = 1;

  // Open event driver
  fd = open((char*) event_file, O_RDONLY);
  rc = libevdev_new_from_fd(fd, &dev);

  if (rc < 0){
    fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
    cleanup();
    return NULL;
  }

  // Grab input
  libevdev_grab(dev, LIBEVDEV_GRAB);

  // Open output file
  uifd = open("/dev/uinput", O_RDWR);

  if (uifd < 0){
    fprintf(stderr, "Failed to open uinput (%s)\n", strerror(-uifd));
    cleanup();
    return NULL;
  }

  // copy event driver
  rc = libevdev_uinput_create_from_device(dev, uifd, &uidev);
  if (rc != 0){
    fprintf(stderr, "Failed to copy device (%s)\n", strerror(-rc));
    cleanup();
    return NULL;
  }

  do {
    // Check if error occured in main thread
    if(should_exit){
      libevdev_grab(dev, LIBEVDEV_UNGRAB);
      libevdev_uinput_destroy(uidev);
      return NULL;
    }

    // Read next event
    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_BLOCKING , &ev);

    new_value = ev.value;
    new_code = ev.code;

    // Check if read was succesful
    if (rc == 0) {
      // Check for Absolute position change
      if(ev.type == EV_ABS){
        // Check if position is x or y
        if(ev.code == ABS_MT_POSITION_X){
          switch(transform){
            case ROT_NORMAL:
              break;
            case ROT_RIGHT_UP:
              new_code = ABS_MT_POSITION_Y;
              new_value = ev.value*(SCREEN_HEIGHT_RATIO);
              break;
            case ROT_BOTTOM_UP:
              new_value = SCREEN_WIDTH - ev.value;
              break;
            case ROT_LEFT_UP:
              new_code = ABS_MT_POSITION_Y;
              new_value = (SCREEN_WIDTH - ev.value)*(SCREEN_HEIGHT_RATIO); //ev.value*(SCREEN_HEIGHT_RATIO+0.19);
              printf("RATIO H: %f\n",SCREEN_HEIGHT_RATIO);
              break;
          }
        } else if(ev.code == ABS_MT_POSITION_Y){
          switch(transform){
            case ROT_NORMAL:
              break;
            case ROT_RIGHT_UP:
              new_code = ABS_MT_POSITION_X;
              new_value = (SCREEN_HEIGHT - ev.value) /(SCREEN_WIDTH_RATIO);
              break;
            case ROT_BOTTOM_UP:
              new_value = SCREEN_HEIGHT - ev.value;
              break;
            case ROT_LEFT_UP:
              new_code = ABS_MT_POSITION_X;
              new_value = ev.value /(SCREEN_WIDTH_RATIO);
              break;
          }
        }
      }

      // Write new/old event
      wc = libevdev_uinput_write_event(uidev, ev.type, new_code, new_value);
      if(wc !=0){
        perror("Error writing event");
      }

    }
  } while (rc == 1 || rc == 0 || rc == -EAGAIN);
  libevdev_grab(dev, LIBEVDEV_UNGRAB);
  libevdev_uinput_destroy(uidev);
}

int main(int argc, char* argv[]){
  pthread_t thread;
  int result_code;


  if(argc != 2){
    printf("Usage %s: <event file>\n",argv[0]);
    cleanup();
    return 1;
  }

  connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

  if (error != NULL) {
      g_error("Failed to connect to D-Bus: %s\n", error->message);
      cleanup();
      return 1;
  }

  guint signal_handler_id = g_dbus_connection_signal_subscribe(
      connection,
      bus_name,
      interface_name,
      signal_name,
      object_path,
      NULL, // No parameter match
      G_DBUS_SIGNAL_FLAGS_NONE,
      on_rotation_change,
      NULL,
      NULL
  );

  if (signal_handler_id == 0) {
      g_error("Failed to add signal handler\n");
      cleanup();
      return 1;
  }

  proxy = g_dbus_proxy_new_sync(connection,
                                G_DBUS_PROXY_FLAGS_NONE,
                                NULL,
                                bus_name,
                                object_path,
                                "org.gnome.Mutter.DisplayConfig",
                                NULL,
                                &error);

  if (error != NULL) {
      g_error("Failed to create D-Bus proxy: %s\n", error->message);
      cleanup();
      return 1;
  }

  loop = g_main_loop_new(NULL, FALSE);

  result_code = pthread_create(&thread, NULL, touch_transformer, argv[1]);
  assert(!result_code);

  g_main_loop_run(loop);

  pthread_join(thread, NULL);

  cleanup();
  return 0;
}
