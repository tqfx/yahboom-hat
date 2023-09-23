/*
 Raspberry Pi RGB Cooling HAT with adjustable fan and OLED display for 4B/3B+/3B

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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <fcntl.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/sysinfo.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <getopt.h>

#include "minIni/minIni.h"
#include "ssd1306_i2c.h"
#include "main.h"
#include "i2c.h"
#include "rgb.h"

#define false 0
#define true !false
static struct model
{
    char const *config;
#define MODEL_SLEEP_MIN 1
    unsigned int sleep;
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
#define BKDR_DISABLE 0xAECA0228
#define BKDR_WATER 0x35FDBB97
#define BKDR_BREATHING 0x5DC61AC6
#define BKDR_MARQUEE 0x1D8B3AA6
#define BKDR_RAINBOW 0x06A0FFBE
#define BKDR_COLORFUL 0x4D6E39E2
        uint32_t mode;
#define BKDR_SLOW 0x0F855DB1
#define BKDR_MIDDLE 0x57F49EE9
#define BKDR_FAST 0x0DC48D78
        uint32_t speed;
#define BKDR_RED 0x001E0E15
#define BKDR_GREEN 0x1F659047
#define BKDR_BLUE 0x0D3E3966
#define BKDR_YELLOW 0xDD05D5C4
#define BKDR_PURPLE 0x1F096FF4
#define BKDR_CYAN 0x0D63E443
#define BKDR_WHITE 0x36EB0111
        uint32_t color;
    } led;
    struct
    {
        struct
        {
            uint8_t lower;
            uint8_t upper;
#define MODEL_FAN_BOUND_MAX 65
        } bound;
#define MODEL_FAN_SPEED_MAX 9
        uint8_t current_speed;
        uint8_t speed;
#define BKDR_DIRECT 0x822E74D5
#define BKDR_SINGLE 0x3E6634C4
#define BKDR_GRADED 0x106F56A9
        uint32_t mode;
    } fan;
    struct
    {
#define BKDR_STOP 0x0F8775F2
#define BKDR_LEFT 0x0E936497
#define BKDR_RIGHT 0xDF4832F0
#define BKDR_DIAGLEFT 0x2CB9460E
#define BKDR_DIAGRIGHT 0x4CAA92D5
        uint32_t scroll;
        _Bool invert;
        _Bool dimmed;
    } oled;
    uint8_t i2c[3];
    _Bool verbose;
    _Bool get;
    _Bool set;
} model = {
    .config = MODEL_CONFIG,
    .sleep = MODEL_SLEEP_MIN,
    .i2cd = 0,
    .cpu = {.temp = 0, .idle = 0, .total = 0, .usage = 0},
    .led = {
        .rgb = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        .mode = BKDR_DISABLE,
        .speed = BKDR_MIDDLE,
        .color = BKDR_GREEN,
    },
    .fan = {
        .bound = {.lower = 42, .upper = 60},
        .current_speed = 0,
        .speed = MODEL_FAN_SPEED_MAX,
        .mode = BKDR_SINGLE,
    },
    .oled = {
        .scroll = BKDR_STOP,
        .invert = false,
        .dimmed = false,
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

static void model_load_led(void)
{
    char const *const section = "led";
    if (model.verbose)
    {
        printf("[%s]\n", section);
    }

    char buffer[128];
    ini_gets(section, "rgb1", "", buffer, sizeof(buffer), model.config);
    byte_parse(model.led.rgb[0], 3, buffer);
    if (model.verbose)
    {
        printf("rgb1=0x%02X,0x%02X,0x%02X\n", model.led.rgb[0][0], model.led.rgb[0][1], model.led.rgb[0][2]);
    }

    ini_gets(section, "rgb2", "", buffer, sizeof(buffer), model.config);
    byte_parse(model.led.rgb[1], 3, buffer);
    if (model.verbose)
    {
        printf("rgb2=0x%02X,0x%02X,0x%02X\n", model.led.rgb[1][0], model.led.rgb[1][1], model.led.rgb[1][2]);
    }

    ini_gets(section, "rgb3", "", buffer, sizeof(buffer), model.config);
    byte_parse(model.led.rgb[2], 3, buffer);
    if (model.verbose)
    {
        printf("rgb3=0x%02X,0x%02X,0x%02X\n", model.led.rgb[2][0], model.led.rgb[2][1], model.led.rgb[2][2]);
    }

    ini_gets(section, "mode", "disable", buffer, sizeof(buffer), model.config);
    model.led.mode = bkdr(buffer);
    if (model.verbose)
    {
        char const *mode;
        switch (model.led.mode)
        {
        default:
        case BKDR_DISABLE:
            mode = "disable";
            break;
        case BKDR_WATER:
            mode = "water";
            break;
        case BKDR_BREATHING:
            mode = "breathing";
            break;
        case BKDR_MARQUEE:
            mode = "marquee";
            break;
        case BKDR_RAINBOW:
            mode = "rainbow";
            break;
        case BKDR_COLORFUL:
            mode = "colorful";
            break;
        }
        printf("mode=%s\n", mode);
    }

    ini_gets(section, "speed", "middle", buffer, sizeof(buffer), model.config);
    model.led.speed = bkdr(buffer);
    if (model.verbose)
    {
        char const *speed;
        switch (model.led.speed)
        {
        case BKDR_SLOW:
            speed = "slow";
            break;
        default:
        case BKDR_MIDDLE:
            speed = "middle";
            break;
        case BKDR_FAST:
            speed = "fast";
            break;
        }
        printf("speed=%s\n", speed);
    }

    ini_gets(section, "color", "green", buffer, sizeof(buffer), model.config);
    model.led.color = bkdr(buffer);
    if (model.verbose)
    {
        char const *color;
        switch (model.led.color)
        {
        case BKDR_RED:
            color = "red";
            break;
        default:
        case BKDR_GREEN:
            color = "green";
            break;
        case BKDR_BLUE:
            color = "blue";
            break;
        case BKDR_YELLOW:
            color = "yellow";
            break;
        case BKDR_PURPLE:
            color = "purple";
            break;
        case BKDR_CYAN:
            color = "cyan";
            break;
        case BKDR_WHITE:
            color = "white";
            break;
        }
        printf("color=%s\n", color);
    }
}

static void model_load_fan(void)
{
    char const *const section = "fan";
    if (model.verbose)
    {
        printf("[%s]\n", section);
    }

    char buffer[128];
    ini_gets(section, "mode", "single", buffer, sizeof(buffer), model.config);
    model.fan.mode = bkdr(buffer);
    if (model.verbose)
    {
        char const *mode;
        switch (model.fan.mode)
        {
        case BKDR_DIRECT:
            mode = "direct";
            break;
        default:
        case BKDR_SINGLE:
            mode = "single";
            break;
        case BKDR_GRADED:
            mode = "graded";
            break;
        }
        printf("mode=%s\n", mode);
    }

    uint8_t bound;
    char *endptr = buffer;
    ini_gets(section, "bound", "", buffer, sizeof(buffer), model.config);
    while (*endptr && !isdigit(*endptr))
    {
        ++endptr;
    }
    bound = (uint8_t)strtol(endptr, &endptr, 0);
    if (bound > 0)
    {
        model.fan.bound.lower = bound < MODEL_FAN_BOUND_MAX ? bound : MODEL_FAN_BOUND_MAX;
    }
    while (*endptr && !isdigit(*endptr))
    {
        ++endptr;
    }
    bound = (uint8_t)strtol(endptr, &endptr, 0);
    if (bound > 0)
    {
        model.fan.bound.upper = bound < MODEL_FAN_BOUND_MAX ? bound : MODEL_FAN_BOUND_MAX;
    }
    if (model.fan.bound.lower == model.fan.bound.upper)
    {
        model.fan.bound.lower = model.fan.bound.upper - 1;
    }
    else if (model.fan.bound.lower > model.fan.bound.upper)
    {
        bound = model.fan.bound.lower;
        model.fan.bound.lower = model.fan.bound.upper;
        model.fan.bound.upper = bound;
    }
    if (model.verbose)
    {
        printf("bound=%u,%u\n", model.fan.bound.lower, model.fan.bound.upper);
    }

    model.fan.speed = (uint8_t)ini_getl(section, "speed", 9, model.config);
    if (model.fan.speed > MODEL_FAN_SPEED_MAX)
    {
        model.fan.speed = MODEL_FAN_SPEED_MAX;
    }
    if (model.verbose)
    {
        printf("speed=%u\n", model.fan.speed);
    }
}

static void model_load_oled(void)
{
    char const *const section = "oled";
    if (model.verbose)
    {
        printf("[%s]\n", section);
    }

    char buffer[128];
    ini_gets(section, "scroll", "stop", buffer, sizeof(buffer), model.config);
    model.oled.scroll = bkdr(buffer);
    if (model.verbose)
    {
        char const *scroll;
        switch (model.oled.scroll)
        {
        default:
        case BKDR_STOP:
            scroll = "stop";
            break;
        case BKDR_LEFT:
            scroll = "left";
            break;
        case BKDR_RIGHT:
            scroll = "right";
            break;
        case BKDR_DIAGLEFT:
            scroll = "diagleft";
            break;
        case BKDR_DIAGRIGHT:
            scroll = "diagright";
            break;
        }
        printf("scroll=%s\n", scroll);
    }

    model.oled.invert = (_Bool)ini_getbool(section, "invert", false, model.config);
    if (model.verbose)
    {
        printf("invert=%u\n", model.oled.invert);
    }

    model.oled.dimmed = (_Bool)ini_getbool(section, "dimmed", false, model.config);
    if (model.verbose)
    {
        printf("dimmed=%u\n", model.oled.dimmed);
    }
}

void model_load(void)
{
    int fd = open("/proc/device-tree/model", O_RDONLY);
    if (fd > 0)
    {
        char buffer[32];
        if (read(fd, buffer, sizeof(buffer)) < 0)
        {
            goto close;
        }
        if (strncmp(buffer, "Raspberry Pi 4", sizeof("Raspberry Pi 4") - 1) == 0)
        {
            model.fan.bound.lower = 48;
            goto close;
        }
        if (strncmp(buffer, "Raspberry Pi 3", sizeof("Raspberry Pi 3") - 1) == 0)
        {
            model.fan.bound.lower = 42;
            goto close;
        }
        if (strncmp(buffer, "Hobot X3 PI", sizeof("Hobot X3 PI") - 1) == 0)
        {
            model.fan.bound.lower = 45;
        }
    close:
        if (model.verbose)
        {
            printf("Model: %s\n", buffer);
        }
        close(fd);
    }
    if (model.verbose)
    {
        printf("Loaded configuration file: %s\n", model.config);
    }

    char buffer[128];
    ini_gets("", "i2c", MODEL_DEV_I2C, buffer, sizeof(buffer), model.config);
    if (model.verbose)
    {
        printf("i2c=%s\n", buffer);
    }
    model.i2cd = open(buffer, O_RDWR);

    model.sleep = (unsigned)ini_getl("", "sleep", MODEL_SLEEP_MIN, model.config);
    if (model.sleep < MODEL_SLEEP_MIN)
    {
        model.sleep = MODEL_SLEEP_MIN;
    }
    if (model.verbose)
    {
        printf("sleep=%i\n", model.sleep);
    }

    model_load_led();
    model_load_fan();
    model_load_oled();
}

static long cpu_get_temp(void)
{
    long temp = 0;
    int fd = open(MODEL_CPU_TEMP, O_RDONLY);
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
    FILE *f = fopen(MODEL_CPU_USAGE, "r");
    if (f)
    {
        long user, nice, sys, idle, iowait, irq, softirq;
        if (fscanf(f, " %*s%ld%ld%ld%ld%ld%ld%ld", &user, &nice, &sys, &idle, &iowait, &irq, &softirq))
        {
            long total = user + nice + sys + idle + iowait + irq + softirq;
            long delta = total - model.cpu.total;
            model.cpu.usage = (unsigned int)((float)(delta - (idle - model.cpu.idle)) / delta * 100);
            model.cpu.total = total;
            model.cpu.idle = idle;
        }
        fclose(f);
    }
    return model.cpu.usage;
}

static int get_disk(char *buffer)
{
    int ok = 0;
    struct statfs info;
    if (statfs(MODEL_DISK_ROOT, &info) == 0)
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
    int ok = 0;
    struct ifaddrs *ifaddrs;
    getifaddrs(&ifaddrs);
    while (ifaddrs)
    {
        if (ifaddrs->ifa_addr->sa_family == AF_INET)
        {
            char address_buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)ifaddrs->ifa_addr)->sin_addr, address_buffer, INET_ADDRSTRLEN);
            switch (bkdr(ifaddrs->ifa_name))
            {
            case 0x0DA733A3: // eth0
                sprintf(buffer, "eth0:%s", address_buffer);
                ok = 1;
                break;
            case 0x37721BEE: // wlan0
                sprintf(buffer, "wlan0:%s", address_buffer);
                ok = 1;
                break;
            default:
                break;
            }
        }
        ifaddrs = ifaddrs->ifa_next;
    }
    return ok;
}

static void model_init(void)
{
    if (model.i2cd < 0)
    {
        fprintf(stderr, "Fail to init I2C!\n");
        exit(EXIT_FAILURE);
    }
    ioctl(model.i2cd, I2C_RETRIES, 5);
    if (model.get)
    {
        i2c_read(model.i2cd, model.i2c[0], model.i2c[1], model.i2c + 2);
        printf("0x%02X\n", model.i2c[2]);
        exit(EXIT_SUCCESS);
    }
    if (model.set)
    {
        i2c_write(model.i2cd, model.i2c[0], model.i2c[1], model.i2c[2]);
        exit(EXIT_SUCCESS);
    }
    for (unsigned int i = 0; i < 3; ++i)
    {
        rgb_set(model.i2cd, i, model.led.rgb[i][0], model.led.rgb[i][1], model.led.rgb[i][2]);
    }
    if (model.led.mode != BKDR_DISABLE)
    {
        switch (model.led.mode)
        {
        case BKDR_WATER:
            rgb_effect(model.i2cd, 0);
            break;
        case BKDR_BREATHING:
            rgb_effect(model.i2cd, 1);
            break;
        case BKDR_MARQUEE:
            rgb_effect(model.i2cd, 2);
            break;
        case BKDR_RAINBOW:
            rgb_effect(model.i2cd, 3);
            break;
        case BKDR_COLORFUL:
            rgb_effect(model.i2cd, 4);
            break;
        default:
            break;
        }
        switch (model.led.speed)
        {
        case BKDR_SLOW:
            rgb_speed(model.i2cd, 1);
            break;
        default:
        case BKDR_MIDDLE:
            rgb_speed(model.i2cd, 2);
            break;
        case BKDR_FAST:
            rgb_speed(model.i2cd, 3);
            break;
        }
        switch (model.led.color)
        {
        case BKDR_RED:
            rgb_color(model.i2cd, 0);
            break;
        default:
        case BKDR_GREEN:
            rgb_color(model.i2cd, 1);
            break;
        case BKDR_BLUE:
            rgb_color(model.i2cd, 2);
            break;
        case BKDR_YELLOW:
            rgb_color(model.i2cd, 3);
            break;
        case BKDR_PURPLE:
            rgb_color(model.i2cd, 4);
            break;
        case BKDR_CYAN:
            rgb_color(model.i2cd, 5);
            break;
        case BKDR_WHITE:
            rgb_color(model.i2cd, 6);
            break;
        }
    }
    rgb_fan(model.i2cd, model.fan.speed);
    model.fan.current_speed = model.fan.speed;
    ssd1306_begin(SSD1306_SWITCHCAPVCC, model.i2cd);
    switch (model.oled.scroll)
    {
    default:
    case BKDR_STOP:
        ssd1306_stopscroll();
        break;
    case BKDR_LEFT:
        ssd1306_startscrollleft(0x0, 0xF);
        break;
    case BKDR_RIGHT:
        ssd1306_startscrollright(0x0, 0xF);
        break;
    case BKDR_DIAGLEFT:
        ssd1306_startscrolldiagleft(0x0, 0xF);
        break;
    case BKDR_DIAGRIGHT:
        ssd1306_startscrolldiagright(0x0, 0xF);
        break;
    }
    if (model.oled.invert)
    {
        ssd1306_invertDisplay(model.oled.invert);
    }
    if (model.oled.dimmed)
    {
        ssd1306_dim(model.oled.dimmed);
    }
    ssd1306_clearDisplay();
    ssd1306_display();
}

static void model_exec(void)
{
    model.cpu.temp = cpu_get_temp();
    if (model.led.mode == BKDR_DISABLE)
    {
        rgb_off(model.i2cd);
    }
    if (model.fan.mode != BKDR_DIRECT)
    {
        if (model.cpu.temp > 1000 * model.fan.bound.upper)
        {
            model.fan.current_speed = MODEL_FAN_SPEED_MAX;
        }
        else if (model.cpu.temp > 1000 * model.fan.bound.lower)
        {
            if (model.fan.mode == BKDR_GRADED)
            {
                int delta = model.cpu.temp - 1000 * model.fan.bound.lower;
                int bound = model.fan.bound.upper - model.fan.bound.lower;
                model.fan.current_speed = abs(delta) * MODEL_FAN_SPEED_MAX / bound;
            }
            else
            {
                model.fan.current_speed = model.fan.speed;
            }
        }
        else
        {
            model.fan.current_speed = 0;
        }
    }
    rgb_fan(model.i2cd, model.fan.current_speed);
    {
        char buffer[64];
        ssd1306_clearDisplay();
        sprintf(buffer, "CPU:%u%%", cpu_get_usage());
        ssd1306_drawText(0, 0, buffer);
        sprintf(buffer, "TEMP:%.1fC", model.cpu.temp / 1000.F);
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

static void model_idle(void)
{
    sleep(model.sleep);
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
            byte_parse(model.i2c, sizeof(model.i2c), optarg);
            model.get = true;
            break;
        case 2:
            byte_parse(model.i2c, sizeof(model.i2c), optarg);
            model.set = true;
            break;
        case 'c':
            model.config = optarg;
            break;
        case 'v':
            model.verbose = true;
            break;
        case '?':
            exit(EXIT_SUCCESS);
        case 'h':
        default:
            printf("Usage: %s [options]\nOptions:\n", argv[0]);
            puts("      --get DEV,REG     Get the value of the register");
            puts("      --set DEV,REG,VAL Set the value of the register");
            puts("  -c, --config FILE     Default configuration file: " MODEL_CONFIG);
            puts("  -v, --verbose         Display detailed log information");
            puts("  -h, --help            Display available options");
            exit(EXIT_SUCCESS);
        }
    }

    model_load();

    for (model_init();; model_idle())
    {
        model_exec();
    }
}
