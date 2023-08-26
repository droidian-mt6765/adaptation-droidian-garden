#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <batman/wlrdisplay.h>

// Convert seconds to micros
#define SLEEP_PERIOD 0.2*1000000

// For debug uncomment
//#define DEBUG
#define READ_FORMAT "Read : %d bytes from %s\n"
#define WRITE_FORMAT "Wrote : %d bytes to %s\n"

void debug(const char* format, ...) {
    #ifdef DEBUG
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    #endif
}

int main(int argc,char* argv[]){
    setenv("XDG_RUNTIME_DIR", "/run/user/32011", 1);

    int brightness_control;
    int last_stat = 1;
    char before_lock[4];
    int bwritten;
    int bread_before;
    bool first_run = true;

    for(;;) {
        int backlight_stat = wlrdisplay(argc, argv);
        debug("wlrdisplay result: %d\n", backlight_stat);

        if(backlight_stat == 0 && last_stat == 1) {
            // Screen Turned On
            debug("Changing Screen\n");

            if(!first_run) {
                debug("Brightness before : %s\n", before_lock);

                // Setting to zero since the values must differ to turn on the screen
                brightness_control = open("/sys/class/leds/lcd-backlight/brightness",O_WRONLY);
                bwritten = write(brightness_control, "0", 1);
                close(brightness_control);

                debug(WRITE_FORMAT, bwritten, "/sys/class/leds/lcd-backlight/brightness");
                debug("Setting Brightness to %s\n", before_lock);

                // Setting the Brightness saved before
                brightness_control = open("/sys/class/leds/lcd-backlight/brightness",O_WRONLY);
                bwritten = write(brightness_control, before_lock, bread_before);
                close(brightness_control);

                debug(WRITE_FORMAT, bwritten, "/sys/class/leds/lcd-backlight/brightness");
            }

        } else if(backlight_stat == 1 && last_stat == 0) {
            // Read the Brightness and save it
            memset(before_lock, 0, 4);
            brightness_control = open("/sys/class/leds/lcd-backlight/brightness", O_RDONLY);
            bread_before = read(brightness_control, before_lock, 4);
            close(brightness_control);

            debug(READ_FORMAT, bread_before, "/sys/class/leds/lcd-backlight/brightness");
            debug("Current Brightness : %s\n", before_lock);

            first_run = false;
        }

        last_stat = backlight_stat;
        usleep(SLEEP_PERIOD);
    }
}
