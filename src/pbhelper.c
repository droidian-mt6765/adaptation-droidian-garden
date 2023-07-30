#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

// Convert seconds to micros
#define SLEEP_PERIOD 0.5*1000000

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
	int backlight_stat_fd;
	int backlight_stat = 0;
	int brightness_control;
	char read_char;
	int last_stat;
	char before_lock[4];


	int bread;
	int bread_before;
	int bwritten;

	backlight_stat_fd = open("/sys/android_brightness/brightness_light",O_RDONLY);
	bread = read(backlight_stat_fd,&read_char,1);
	close(backlight_stat_fd);

	last_stat = read_char - '0';

	for(;;) {
		backlight_stat_fd = open("/sys/android_brightness/brightness_light",O_RDONLY);
		int bread = read(backlight_stat_fd,&read_char,1);
		close(backlight_stat_fd);

		debug(READ_FORMAT,bread,"/sys/android_brightness/brightness_light");
		debug("Read char : %c\n",read_char);

		backlight_stat = read_char - '0';

		debug("last_stat : %d, backlight_stat : %d\n",last_stat,backlight_stat);

		if((backlight_stat == 5 || backlight_stat == 47) && last_stat == 0) {
			// Screen Turned On
			debug("Changing Screen\n");
			debug("Brightness before : %s\n",before_lock);

			// Setting to zero since the values must differ to turn on the screen
			brightness_control = open("/sys/class/leds/lcd-backlight/brightness",O_WRONLY);
			bwritten = write(brightness_control,"0",1);
			close(brightness_control);

			debug(WRITE_FORMAT,bwritten,"/sys/class/leds/lcd-backlight/brightness");
			debug("Setting Brightness to %s\n",before_lock);

			// Setting the Brightness saved before
			brightness_control = open("/sys/class/leds/lcd-backlight/brightness",O_WRONLY);
			bwritten = write(brightness_control,before_lock,bread_before);
			close(brightness_control);

			debug(WRITE_FORMAT,bwritten,"/sys/class/leds/lcd-backlight/brightness");

		} else if((backlight_stat == 5 || backlight_stat == 47) && (last_stat == 5 || last_stat == 47)) {
			// Read the Brightness and save it
			memset(before_lock, 0, 4);
			brightness_control = open("/sys/class/leds/lcd-backlight/brightness",O_RDONLY);
			bread_before = read(brightness_control,before_lock,4);
			close(brightness_control);

			debug(READ_FORMAT,bread,"/sys/class/leds/lcd-backlight/brightness");
			debug("Current Brightness : %s\n",before_lock);
		}
		last_stat = backlight_stat;
		usleep(SLEEP_PERIOD);
	}
}
