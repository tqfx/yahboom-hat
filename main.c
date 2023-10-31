/*
 Software for RGB Cooling HAT with adjustable fan and OLED display

 Copyright (C) 2023 tqfx <tqfx@foxmail.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published
 by the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <fcntl.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/sysinfo.h>
#include <linux/limits.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#define LOG_USE_TIME
#define LOG_USE_USEC
#include "log.c"

#include "minIni/minIni.h"
#include "ssd1306_i2c.h"
#include "strpool.h"
#include "main.h"
#include "i2c.h"
#include "rgb.h"

enum
{
    LOG_TRACE,
    LOG_DEBUG,
    LOG_ERROR
};
#define log_trace(...) log_log(LOG_TRACE, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __VA_ARGS__)
static char const *level_strings[] = {"TRACE", "DEBUG", "ERROR"};
static void log_impl_file(struct log_node const *log, char const *fmt, va_list ap)
{
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", log->time)] = '\0';
    fprintf(log->data, "%s.%06lu %-5s %s:%d: ", buf, log->usec, level_strings[log->level], log->file, log->line);
    vfprintf(log->data, fmt, ap);
    fflush(log->data);
}
static char const *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[31m"};
static void log_impl_pipe(struct log_node const *log, char const *fmt, va_list ap)
{
    char buf[16];
    int tty = isatty(fileno(log->data));
    char const *white = tty ? "\x1b[0m" : "";
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", log->time)] = '\0';
    fprintf(log->data, "%s.%06lu %s%-5s%s %s%s:%d:%s ", buf, log->usec,
            tty ? level_colors[log->level] : "", level_strings[log->level],
            white, tty ? "\x1b[90m" : "", log->file, log->line, white);
    vfprintf(log->data, fmt, ap);
}
static int log_is_1(unsigned int level, unsigned int lvl)
{
    return log_isge(level, lvl) && log_islt(level, LOG_ERROR);
}

enum led_mode
{
    LED_MODE_WATER,
    LED_MODE_BREATHING,
    LED_MODE_MARQUEE,
    LED_MODE_RAINBOW,
    LED_MODE_COLORFUL,
    LED_MODE_DISABLE
};
enum led_speed
{
    LED_SPEED_SLOW = 1,
    LED_SPEED_MIDDLE,
    LED_SPEED_FAST
};
enum led_color
{
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
    LED_COLOR_PURPLE,
    LED_COLOR_CYAN,
    LED_COLOR_WHITE
};
enum fan_mode
{
    FAN_MODE_DIRECT,
    FAN_MODE_SIGNLE,
    FAN_MODE_GRADED
};
enum oled_scroll
{
    OLED_SCROLL_STOP,
    OLED_SCROLL_LEFT,
    OLED_SCROLL_RIGHT,
    OLED_SCROLL_DIAGLEFT,
    OLED_SCROLL_DIAGRIGHT
};

#define false 0
#define true !false
static struct model
{
    struct strpool str;
    char const *config;
#define HAT_SLEEP_MIN 1
    unsigned int sleep;
    void (*term)(int);
    int i2cd;
    struct
    {
        long temp;
        long idle, total;
        unsigned int usage;
    } cpu;
    struct
    {
        uint8_t rgb[3][3];
        enum led_mode mode;
        enum led_speed speed;
        enum led_color color;
    } led;
    struct
    {
#define HAT_FAN_BOUND_MAX 65
        struct
        {
            uint8_t lower;
            uint8_t upper;
        } bound;
#define HAT_FAN_SPEED_MAX 9
        uint8_t current_speed;
        uint8_t speed;
        enum fan_mode mode;
    } fan;
    struct
    {
        enum oled_scroll scroll;
        _Bool invert;
        _Bool dimmed;
        _Bool enable;
    } oled;
    struct
    {
        struct log_node out;
        struct log_node err;
        struct log_node log;
    } log;
    uint8_t i2c[3];
    _Bool verbose;
    _Bool get;
    _Bool set;
} hat = {
    .str = STRPOOL_INIT,
    .config = HAT_CONFIG,
    .sleep = HAT_SLEEP_MIN,
    .term = NULL,
    .i2cd = 0,
    .cpu = {.temp = 0, .idle = 0, .total = 0, .usage = 0},
    .led = {
        .rgb = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        .mode = LED_MODE_DISABLE,
        .speed = LED_SPEED_MIDDLE,
        .color = LED_COLOR_GREEN,
    },
    .fan = {
        .bound = {.lower = 42, .upper = 60},
        .current_speed = 0,
        .speed = HAT_FAN_SPEED_MAX,
        .mode = FAN_MODE_SIGNLE,
    },
    .oled = {
        .scroll = OLED_SCROLL_STOP,
        .invert = false,
        .dimmed = false,
        .enable = true,
    },
    .log = {
        .out = LOG_INIT(log_is_1, LOG_DEBUG, log_impl_pipe, NULL),
        .err = LOG_INIT(log_isge, LOG_ERROR, log_impl_pipe, NULL),
        .log = LOG_INIT(log_isge, LOG_TRACE, log_impl_file, NULL),
    },
    .i2c = {0, 0, 0},
    .verbose = false,
    .get = false,
    .set = false,
};

static unsigned int byte_parse(uint8_t *ptr, size_t num, char *text)
{
    unsigned int parsed = 0;
    if (ptr && num && text)
    {
        do
        {
            while (*text && !isdigit(*text))
            {
                ++text;
            }
            if (*text == 0)
            {
                break;
            }
            *ptr++ = (uint8_t)strtol(text, &text, 0);
        } while (++parsed < num && *text);
    }
    return parsed;
}

static uint32_t bkdr(void const *const _str)
{
    uint32_t val = 0;
    if (_str)
    {
        for (uint8_t const *str = (uint8_t const *)_str; *str; ++str)
        {
            val = val * 131 + tolower(*str);
        }
    }
    return val;
}

static void hat_load_led(void)
{
    char const *const section = "led";
    log_debug("  [%s]\n", section);

    char buffer[128];
    ini_gets(section, "rgb1", "", buffer, sizeof(buffer), hat.config);
    byte_parse(hat.led.rgb[0], 3, buffer);
    log_debug("  rgb1=0x%02X,0x%02X,0x%02X\n", hat.led.rgb[0][0], hat.led.rgb[0][1], hat.led.rgb[0][2]);

    ini_gets(section, "rgb2", "", buffer, sizeof(buffer), hat.config);
    byte_parse(hat.led.rgb[1], 3, buffer);
    log_debug("  rgb2=0x%02X,0x%02X,0x%02X\n", hat.led.rgb[1][0], hat.led.rgb[1][1], hat.led.rgb[1][2]);

    ini_gets(section, "rgb3", "", buffer, sizeof(buffer), hat.config);
    byte_parse(hat.led.rgb[2], 3, buffer);
    log_debug("  rgb3=0x%02X,0x%02X,0x%02X\n", hat.led.rgb[2][0], hat.led.rgb[2][1], hat.led.rgb[2][2]);

    char const *mode;
    ini_gets(section, "mode", "disable", buffer, sizeof(buffer), hat.config);
    switch (bkdr(buffer))
    {
    case 0x00000030: // 0
    case 0x35FDBB97: // water
        hat.led.mode = LED_MODE_WATER;
        mode = "water";
        break;
    case 0x00000031: // 1
    case 0x5DC61AC6: // breathing
        hat.led.mode = LED_MODE_BREATHING;
        mode = "breathing";
        break;
    case 0x00000032: // 2
    case 0x1D8B3AA6: // marquee
        hat.led.mode = LED_MODE_MARQUEE;
        mode = "marquee";
        break;
    case 0x00000033: // 3
    case 0x06A0FFBE: // rainbow
        hat.led.mode = LED_MODE_RAINBOW;
        mode = "rainbow";
        break;
    case 0x00000034: // 4
    case 0x4D6E39E2: // colorful
        hat.led.mode = LED_MODE_COLORFUL;
        mode = "rainbow";
        break;
    default:
    case 0xAECA0228: // disable
        hat.led.mode = LED_MODE_DISABLE;
        mode = "disable";
        break;
    }
    log_debug("  mode=%s\n", mode);

    char const *speed;
    ini_gets(section, "speed", "middle", buffer, sizeof(buffer), hat.config);
    switch (bkdr(buffer))
    {
    case 0x00000031: // 1
    case 0x0F855DB1: // slow
        hat.led.speed = LED_SPEED_SLOW;
        speed = "slow";
        break;
    default:
    case 0x57F49EE9: // middle
        hat.led.speed = LED_SPEED_MIDDLE;
        speed = "middle";
        break;
    case 0x00000033: // 3
    case 0x0DC48D78: // fast
        hat.led.speed = LED_SPEED_FAST;
        speed = "fast";
        break;
    }
    log_debug("  speed=%s\n", speed);

    char const *color;
    ini_gets(section, "color", "green", buffer, sizeof(buffer), hat.config);
    switch (bkdr(buffer))
    {
    case 0x00000030: // 0
    case 0x001E0E15: // red
        hat.led.color = LED_COLOR_RED;
        color = "red";
        break;
    default:
    case 0x1F659047: // green
        hat.led.color = LED_COLOR_GREEN;
        color = "green";
        break;
    case 0x00000032: // 2
    case 0x0D3E3966: // blue
        hat.led.color = LED_COLOR_BLUE;
        color = "blue";
        break;
    case 0x00000033: // 3
    case 0xDD05D5C4: // yellow
        hat.led.color = LED_COLOR_YELLOW;
        color = "yellow";
        break;
    case 0x00000034: // 4
    case 0x1F096FF4: // purple
        hat.led.color = LED_COLOR_PURPLE;
        color = "purple";
        break;
    case 0x00000035: // 5
    case 0x0D63E443: // cyan
        hat.led.color = LED_COLOR_CYAN;
        color = "cyan";
        break;
    case 0x00000036: // 6
    case 0x36EB0111: // white
        hat.led.color = LED_COLOR_WHITE;
        color = "white";
        break;
    }
    log_debug("  color=%s\n", color);
}

static void hat_load_fan(void)
{
    char const *const section = "fan";
    log_debug("  [%s]\n", section);

    char buffer[128];
    char const *mode;
    ini_gets(section, "mode", "single", buffer, sizeof(buffer), hat.config);
    switch (bkdr(buffer))
    {
    case 0x00000030: // 0
    case 0x822E74D5: // direct
        hat.fan.mode = FAN_MODE_DIRECT;
        mode = "direct";
        break;
    default:
    case 0x3E6634C4: // single
        hat.fan.mode = FAN_MODE_SIGNLE;
        mode = "single";
        break;
    case 0x00000032: // 2
    case 0x106F56A9: // graded
        hat.fan.mode = FAN_MODE_GRADED;
        mode = "graded";
        break;
    }
    log_debug("  mode=%s\n", mode);

    uint8_t bound;
    char *endptr = buffer;
    ini_gets(section, "bound", "", buffer, sizeof(buffer), hat.config);
    while (*endptr && !isdigit(*endptr))
    {
        ++endptr;
    }
    bound = (uint8_t)strtol(endptr, &endptr, 0);
    if (bound > 0)
    {
        hat.fan.bound.lower = bound < HAT_FAN_BOUND_MAX ? bound : HAT_FAN_BOUND_MAX;
    }
    while (*endptr && !isdigit(*endptr))
    {
        ++endptr;
    }
    bound = (uint8_t)strtol(endptr, &endptr, 0);
    if (bound > 0)
    {
        hat.fan.bound.upper = bound < HAT_FAN_BOUND_MAX ? bound : HAT_FAN_BOUND_MAX;
    }
    if (hat.fan.bound.lower == hat.fan.bound.upper)
    {
        hat.fan.bound.lower = hat.fan.bound.upper - 1;
    }
    else if (hat.fan.bound.lower > hat.fan.bound.upper)
    {
        bound = hat.fan.bound.lower;
        hat.fan.bound.lower = hat.fan.bound.upper;
        hat.fan.bound.upper = bound;
    }
    log_debug("  bound=%u,%u\n", hat.fan.bound.lower, hat.fan.bound.upper);

    hat.fan.speed = (uint8_t)ini_getl(section, "speed", HAT_FAN_SPEED_MAX, hat.config);
    if (hat.fan.speed > HAT_FAN_SPEED_MAX)
    {
        hat.fan.speed = HAT_FAN_SPEED_MAX;
    }
    log_debug("  speed=%u\n", hat.fan.speed);
}

static void hat_load_oled(void)
{
    char const *const section = "oled";
    log_debug("  [%s]\n", section);

    char buffer[128];
    char const *scroll;
    ini_gets(section, "scroll", "stop", buffer, sizeof(buffer), hat.config);
    switch (bkdr(buffer))
    {
    default:
    case 0x0F8775F2: // stop
        hat.oled.scroll = OLED_SCROLL_STOP;
        scroll = "stop";
        break;
    case 0x00000031: // 1
    case 0x0E936497: // left
        hat.oled.scroll = OLED_SCROLL_LEFT;
        scroll = "left";
        break;
    case 0x00000032: // 2
    case 0xDF4832F0: // right
        hat.oled.scroll = OLED_SCROLL_RIGHT;
        scroll = "right";
        break;
    case 0x00000033: // 3
    case 0x2CB9460E: // diagleft
        hat.oled.scroll = OLED_SCROLL_DIAGLEFT;
        scroll = "diagleft";
        break;
    case 0x00000034: // 4
    case 0x4CAA92D5: // diagright
        hat.oled.scroll = OLED_SCROLL_DIAGRIGHT;
        scroll = "diagright";
        break;
    }
    log_debug("  scroll=%s\n", scroll);

    hat.oled.invert = (_Bool)ini_getbool(section, "invert", false, hat.config);
    log_debug("  invert=%u\n", hat.oled.invert);

    hat.oled.dimmed = (_Bool)ini_getbool(section, "dimmed", false, hat.config);
    log_debug("  dimmed=%u\n", hat.oled.dimmed);

    hat.oled.enable = (_Bool)ini_getbool(section, "enable", true, hat.config);
    log_debug("  enable=%u\n", hat.oled.enable);
}

static void hat_load(void)
{
    char self[PATH_MAX];
    int self_n = (int)readlink("/proc/self/exe", self, PATH_MAX);
    char *prefix = getenv("PREFIX");
    prefix = prefix ? prefix : "";

    hat.log.err.data = stderr;
    log_join(&hat.log.err);
    if (hat.verbose)
    {
        hat.log.out.data = stdout;
        log_join(&hat.log.out);
    }
    strpool_init(&hat.str, NULL, 0);
    {
        char *const *log = strpool_putf(&hat.str, "%s%s", prefix, "/var/log/" HAT_LOG);
        hat.log.log.data = fopen(*log, "a");
        if (hat.log.log.data)
        {
            goto hat_log;
        }
        strpool_dump(&hat.str, log);
    }
    {
        char *const *log = strpool_putf(&hat.str, "%.*s.log", self_n, self);
        hat.log.log.data = fopen(*log, "a");
        if (hat.log.log.data)
        {
            goto hat_log;
        }
        strpool_dump(&hat.str, log);
    }
    {
        char *const *log = strpool_putf(&hat.str, "%s%s", prefix, "/tmp/" HAT_LOG);
        hat.log.log.data = fopen(*log, "a");
        if (hat.log.log.data)
        {
            goto hat_log;
        }
        strpool_dump(&hat.str, log);
    }
hat_log:
    if (hat.log.log.data)
    {
        log_join(&hat.log.log);
    }

    int fd = open("/proc/device-tree/model", O_RDONLY);
    if (fd > 0)
    {
        char buffer[64];
        if (read(fd, buffer, sizeof(buffer)) < 0)
        {
            goto close;
        }
        if (strncmp(buffer, "Raspberry Pi 4", sizeof("Raspberry Pi 4") - 1) == 0)
        {
            hat.fan.bound.lower = 48;
            goto close;
        }
        if (strncmp(buffer, "Raspberry Pi 3", sizeof("Raspberry Pi 3") - 1) == 0)
        {
            hat.fan.bound.lower = 42;
            goto close;
        }
        if (strncmp(buffer, "Hobot X3 PI", sizeof("Hobot X3 PI") - 1) == 0)
        {
            hat.fan.bound.lower = 45;
        }
    close:
        log_debug("Model: %s\n", buffer);
        close(fd);
    }

    {
        char *const *config = strpool_putf(&hat.str, "%s%s", prefix, "/etc/" HAT_CONFIG);
        if (access(*config, R_OK) == 0)
        {
            hat.config = *config;
            goto hat_config;
        }
        log_debug("Locate: %s\n", *config);
        strpool_dump(&hat.str, config);
    }
    {
        char *const *config = strpool_putf(&hat.str, "%s/.%s", getenv("HOME"), HAT_CONFIG);
        if (access(*config, R_OK) == 0)
        {
            hat.config = *config;
            goto hat_config;
        }
        log_debug("Locate: %s\n", *config);
        strpool_dump(&hat.str, config);
    }
    {
        char *const *config = strpool_putf(&hat.str, "%.*s.ini", self_n, self);
        if (access(*config, R_OK) == 0)
        {
            hat.config = *config;
            goto hat_config;
        }
        log_debug("Locate: %s\n", *config);
        strpool_dump(&hat.str, config);
    }
hat_config:
    if (access(hat.config, R_OK) == 0)
    {
        log_debug("Config: %s\n", hat.config);
    }
    else
    {
        log_debug("Config:\n");
    }
    log_debug("\n");

    char buffer[128];
    ini_gets("", "i2c", HAT_DEV_I2C, buffer, sizeof(buffer), hat.config);
    if (isdigit(*buffer))
    {
        char *endptr;
        long line = strtol(buffer, &endptr, 0);
        sprintf(buffer, "/dev/i2c-%li", line);
    }
    log_debug("  i2c=%s\n", buffer);
    extern int i2cd;
    i2cd = open(buffer, O_RDWR);
    hat.i2cd = i2cd;

    hat.sleep = (unsigned)ini_getl("", "sleep", HAT_SLEEP_MIN, hat.config);
    if (hat.sleep < HAT_SLEEP_MIN)
    {
        hat.sleep = HAT_SLEEP_MIN;
    }
    log_debug("  sleep=%i\n", hat.sleep);

    hat_load_led();
    hat_load_fan();
    hat_load_oled();
    log_debug("\n");
}

static long cpu_get_temp(void)
{
    long temp = 0;
    int fd = open(HAT_CPU_TEMP, O_RDONLY);
    if (fd > 0)
    {
        char buf[16]; /* -40xxx ~ 85xxx */
        if (read(fd, buf, sizeof(buf)) > 0)
        {
            temp = atol(buf);
        }
        close(fd);
    }
    return temp;
}

static unsigned int cpu_get_usage(void)
{
    unsigned int usage = 0;
    FILE *f = fopen(HAT_CPU_USAGE, "r");
    if (f)
    {
        long user, nice, sys, idle, iowait, irq, softirq;
        if (fscanf(f, " %*s%ld%ld%ld%ld%ld%ld%ld", &user, &nice, &sys, &idle, &iowait, &irq, &softirq) > 0)
        {
            long total = user + nice + sys + idle + iowait + irq + softirq;
            long delta = total - hat.cpu.total;
            usage = (unsigned int)((float)(delta - (idle - hat.cpu.idle)) / delta * 100);
            hat.cpu.total = total;
            hat.cpu.idle = idle;
        }
        fclose(f);
    }
    return usage;
}

static int get_disk(char *buffer)
{
    int ok = 0;
    struct statfs info;
    if (statfs(HAT_DISK_ROOT, &info) == 0)
    {
        unsigned long long free = info.f_bfree * info.f_bsize;
        unsigned long long total = info.f_blocks * info.f_bsize;
        sprintf(buffer, "DISK:%lu/%luMB", (unsigned long)(free >> 20), (unsigned long)(total >> 20));
        ok = 1;
    }
    return ok;
}

static int get_ram(char *buffer)
{
    int ok = 0;
    struct sysinfo info;
    if (sysinfo(&info) == 0)
    {
        sprintf(buffer, "RAM:%lu/%luMB", info.freeram >> 20, info.totalram >> 20);
        ok = 1;
    }
    return ok;
}

static int get_ip(char *buffer)
{
    struct ifaddrs *ifaddrs = NULL;
    int ok = getifaddrs(&ifaddrs) > ~0 ? 1 : 0;
    for (struct ifaddrs *ifaddr = ifaddrs; ifaddr; ifaddr = ifaddr->ifa_next)
    {
        if (ifaddr->ifa_addr->sa_family == AF_INET)
        {
            char address_buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)ifaddr->ifa_addr)->sin_addr, address_buffer, INET_ADDRSTRLEN);
            switch (bkdr(ifaddr->ifa_name))
            {
            case 0x0DA733A3: // eth0
                sprintf(buffer, "%s:%s", ifaddr->ifa_name, address_buffer);
                ok = 1;
                break;
            case 0x37721BEE: // wlan0
                sprintf(buffer, "%s:%s", ifaddr->ifa_name, address_buffer);
                ok = 1;
                break;
            default:
                break;
            }
        }
    }
    freeifaddrs(ifaddrs);
    return ok;
}

static void hat_init(void)
{
    if (hat.i2cd < 0)
    {
        log_error("Failed to initialize I2C!\n");
        exit(EXIT_FAILURE);
    }
    if (hat.get)
    {
        i2c_read(hat.i2cd, hat.i2c[0], hat.i2c[1], hat.i2c + 2);
        printf("0x%02X\n", hat.i2c[2]);
        exit(EXIT_SUCCESS);
    }
    if (hat.set)
    {
        i2c_write(hat.i2cd, hat.i2c[0], hat.i2c[1], hat.i2c[2]);
        exit(EXIT_SUCCESS);
    }
    for (unsigned int i = 0; i < 3; ++i)
    {
        rgb_set(hat.i2cd, i, hat.led.rgb[i][0], hat.led.rgb[i][1], hat.led.rgb[i][2]);
    }
    hat.fan.current_speed = hat.fan.speed;
    rgb_fan(hat.i2cd, hat.fan.speed);
    ssd1306_begin(SSD1306_SWITCHCAPVCC);
    switch (hat.oled.scroll)
    {
    default:
    case OLED_SCROLL_STOP:
        ssd1306_stopscroll();
        break;
    case OLED_SCROLL_LEFT:
        ssd1306_startscrollleft(0x0, 0xF);
        break;
    case OLED_SCROLL_RIGHT:
        ssd1306_startscrollright(0x0, 0xF);
        break;
    case OLED_SCROLL_DIAGLEFT:
        ssd1306_startscrolldiagleft(0x0, 0xF);
        break;
    case OLED_SCROLL_DIAGRIGHT:
        ssd1306_startscrolldiagright(0x0, 0xF);
        break;
    }
    if (hat.oled.invert)
    {
        ssd1306_invertDisplay(hat.oled.invert);
    }
    if (hat.oled.dimmed)
    {
        ssd1306_dim(hat.oled.dimmed);
    }
    ssd1306_clearDisplay();
    ssd1306_display();
}

static void hat_exec(void)
{
    hat.cpu.temp = cpu_get_temp();
    hat.cpu.usage = cpu_get_usage();
    if (hat.led.mode == LED_MODE_DISABLE)
    {
        rgb_off(hat.i2cd);
    }
    else
    {
        rgb_effect(hat.i2cd, hat.led.mode);
        rgb_speed(hat.i2cd, hat.led.speed);
        rgb_color(hat.i2cd, hat.led.color);
    }
    unsigned char speed = hat.fan.current_speed;
    if (hat.fan.mode != FAN_MODE_DIRECT)
    {
        if (hat.cpu.temp > 1000 * hat.fan.bound.upper)
        {
            speed = HAT_FAN_SPEED_MAX;
        }
        else if (hat.cpu.temp > 1000 * hat.fan.bound.lower)
        {
            if (hat.fan.mode == FAN_MODE_GRADED)
            {
                int delta = hat.cpu.temp - 1000 * hat.fan.bound.lower;
                int bound = hat.fan.bound.upper - hat.fan.bound.lower;
                speed = abs(delta) * HAT_FAN_SPEED_MAX / bound;
            }
            else
            {
                speed = hat.fan.speed;
            }
        }
        else
        {
            speed = 0;
        }
    }
    if (speed > 0 || speed != hat.fan.current_speed)
    {
        rgb_fan(hat.i2cd, speed);
    }
    if (hat.oled.enable)
    {
        char buffer[64];
        ssd1306_clearDisplay();
        sprintf(buffer, "CPU:%u%%", hat.cpu.usage);
        ssd1306_drawText(0, 0, buffer);
        sprintf(buffer, "TEMP:%.1fC", hat.cpu.temp / 1000.F);
        ssd1306_drawText(56, 0, buffer);
        get_ram(buffer);
        ssd1306_drawText(0, 8, buffer);
        get_disk(buffer);
        ssd1306_drawText(0, 16, buffer);
        get_ip(buffer);
        ssd1306_drawText(0, 24, buffer);
        ssd1306_display();
    }
}

static void hat_idle(void)
{
    sleep(hat.sleep);
}

static void hat_exit(void)
{
    {
        size_t i = 0;
        log_trace("String: 0x%zX\n", hat.str.num + hat.str.pool.num);
        if (hat.str.num)
        {
            log_trace("\n");
        }
        strpool_pool_foreach(&hat.str, cur)
        {
            log_trace("[%zu]=%s\n", i++, *cur);
        }
        strpool_foreach(&hat.str, cur)
        {
            log_trace("[%zu]=%s\n", i++, *cur);
        }
    }
    strpool_exit(&hat.str);
    if (hat.log.log.data)
    {
        fclose(hat.log.log.data);
    }
}

static void hat_term(int sig)
{
    hat_exit();
    hat.term(sig);
}

int main(int argc, char *argv[])
{
    char const *shortopts = "c:vh";
    struct option const longopts[] = {
        {"get", required_argument, 0, 1},
        {"set", required_argument, 0, 2},
        {"config", required_argument, 0, 'c'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
    };

    for (int ok; (void)(ok = getopt_long(argc, argv, shortopts, longopts, &optind)), ok != -1;)
    {
        switch (ok)
        {
        case 1:
            byte_parse(hat.i2c, sizeof(hat.i2c), optarg);
            hat.get = true;
            break;
        case 2:
            byte_parse(hat.i2c, sizeof(hat.i2c), optarg);
            hat.set = true;
            break;
        case 'c':
            hat.config = optarg;
            break;
        case 'v':
            hat.verbose = true;
            break;
        case '?':
            exit(EXIT_SUCCESS);
        case 'h':
        default:
            printf("Usage: %s [options]\nOptions:\n", argv[0]);
            puts("      --get DEV,REG     Get the value of the register");
            puts("      --set DEV,REG,VAL Set the value of the register");
            puts("  -c, --config FILE     Default configuration file: " HAT_CONFIG);
            puts("  -v, --verbose         Display detailed log information");
            puts("  -h, --help            Display available options");
            exit(EXIT_SUCCESS);
        }
    }

    hat.term = signal(SIGTERM, hat_term);
    atexit(hat_exit);
    hat_load();

    for (hat_init();; hat_idle())
    {
        hat_exec();
    }
}
