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
#include <ctype.h>

#include <fcntl.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/sysinfo.h>
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
    struct
    {
        int i2c;
    } dev;
    struct
    {
        int temp;
        long idle, total;
        unsigned int usage;
    } cpu;
    struct
    {
#define MODEL_CFG_SLEEP 1
        unsigned int sleep;
    } cfg;
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
    char const *config;
    uint8_t i2c[3];
    _Bool verbose;
    _Bool set;
    _Bool get;
} model = {
    {0},
    {0, 0, 0, 0},
    {MODEL_CFG_SLEEP},
    {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
     BKDR_DISABLE,
     BKDR_MIDDLE,
     BKDR_GRADED},
    {{42, 60}, 0, MODEL_FAN_SPEED_MAX, BKDR_SINGLE},
    MODEL_CONFIG,
    {0, 0, 0},
    false,
    false,
    false,
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

static void model_load_dev(void)
{
    char buffer[128];
    if (model.verbose)
    {
        puts("[dev]");
    }
    ini_gets("dev", "i2c", MODEL_DEV_I2C, buffer, sizeof(buffer), model.config);
    if (model.verbose)
    {
        printf("i2c=%s\n", buffer);
    }
    model.dev.i2c = open(buffer, O_RDWR);
}

static void model_load_cfg(void)
{
    if (model.verbose)
    {
        puts("[cfg]");
    }
    model.cfg.sleep = (unsigned)ini_getl("cfg", "sleep", MODEL_CFG_SLEEP, model.config);
    if (model.cfg.sleep < MODEL_CFG_SLEEP)
    {
        model.cfg.sleep = MODEL_CFG_SLEEP;
    }
    if (model.verbose)
    {
        printf("sleep=%i\n", model.cfg.sleep);
    }
}

static void model_load_led(void)
{
    char buffer[128];
    if (model.verbose)
    {
        puts("[led]");
    }

    ini_gets("led", "rgb1", "", buffer, sizeof(buffer), model.config);
    byte_parse(model.led.rgb[0], 3, buffer);
    if (model.verbose)
    {
        printf("rgb1=0x%02X,0x%02X,0x%02X\n", model.led.rgb[0][0], model.led.rgb[0][1], model.led.rgb[0][2]);
    }

    ini_gets("led", "rgb2", "", buffer, sizeof(buffer), model.config);
    byte_parse(model.led.rgb[1], 3, buffer);
    if (model.verbose)
    {
        printf("rgb2=0x%02X,0x%02X,0x%02X\n", model.led.rgb[1][0], model.led.rgb[1][1], model.led.rgb[1][2]);
    }

    ini_gets("led", "rgb3", "", buffer, sizeof(buffer), model.config);
    byte_parse(model.led.rgb[2], 3, buffer);
    if (model.verbose)
    {
        printf("rgb3=0x%02X,0x%02X,0x%02X\n", model.led.rgb[2][0], model.led.rgb[2][1], model.led.rgb[2][2]);
    }

    ini_gets("led", "mode", "disable", buffer, sizeof(buffer), model.config);
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

    ini_gets("led", "speed", "middle", buffer, sizeof(buffer), model.config);
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

    ini_gets("led", "color", "green", buffer, sizeof(buffer), model.config);
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
    char buffer[128];
    if (model.verbose)
    {
        puts("[fan]");
    }

    ini_gets("fan", "mode", "single", buffer, sizeof(buffer), model.config);
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
    ini_gets("fan", "bound", "", buffer, sizeof(buffer), model.config);
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

    model.fan.speed = (uint8_t)ini_getl("fan", "speed", 9, model.config);
    if (model.fan.speed > MODEL_FAN_SPEED_MAX)
    {
        model.fan.speed = MODEL_FAN_SPEED_MAX;
    }
    if (model.verbose)
    {
        printf("speed=%u\n", model.fan.speed);
    }
}

void model_load(void)
{
    if (model.verbose)
    {
        printf("Loaded configuration file: %s\n", model.config);
    }
    model_load_dev();
    model_load_cfg();
    model_load_led();
    model_load_fan();
}

static int cpu_get_temp(void)
{
    int temp = 0;
    int fd = open(MODEL_CPU_TEMP, O_RDONLY);
    if (fd > 0)
    {
        char buf[16]; /* -40xxx ~ 85xxx */
        if (read(fd, buf, sizeof(buf)) > 0)
        {
            temp = atoi(buf);
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
    if (model.dev.i2c < 0)
    {
        fprintf(stderr, "Fail to init I2C!\n");
        exit(EXIT_FAILURE);
    }
    if (model.get)
    {
        i2c_read(model.dev.i2c, model.i2c[0], model.i2c[1], model.i2c + 2);
        printf("0x%02X\n", model.i2c[2]);
        exit(EXIT_SUCCESS);
    }
    if (model.set)
    {
        i2c_write(model.dev.i2c, model.i2c[0], model.i2c[1], model.i2c[2]);
        exit(EXIT_SUCCESS);
    }
    for (unsigned int i = 0; i < 3; ++i)
    {
        rgb_set(model.dev.i2c, i, model.led.rgb[i][0], model.led.rgb[i][1], model.led.rgb[i][2]);
    }
    if (model.led.mode == BKDR_DISABLE)
    {
        rgb_off(model.dev.i2c);
    }
    else
    {
        switch (model.led.mode)
        {
        case BKDR_WATER:
            rgb_effect(model.dev.i2c, 0);
            break;
        case BKDR_BREATHING:
            rgb_effect(model.dev.i2c, 1);
            break;
        case BKDR_MARQUEE:
            rgb_effect(model.dev.i2c, 2);
            break;
        case BKDR_RAINBOW:
            rgb_effect(model.dev.i2c, 3);
            break;
        case BKDR_COLORFUL:
            rgb_effect(model.dev.i2c, 4);
            break;
        default:
            break;
        }
        switch (model.led.speed)
        {
        case BKDR_SLOW:
            rgb_speed(model.dev.i2c, 1);
            break;
        default:
        case BKDR_MIDDLE:
            rgb_speed(model.dev.i2c, 2);
            break;
        case BKDR_FAST:
            rgb_speed(model.dev.i2c, 3);
            break;
        }
        switch (model.led.color)
        {
        case BKDR_RED:
            rgb_color(model.dev.i2c, 0);
            break;
        default:
        case BKDR_GREEN:
            rgb_color(model.dev.i2c, 1);
            break;
        case BKDR_BLUE:
            rgb_color(model.dev.i2c, 2);
            break;
        case BKDR_YELLOW:
            rgb_color(model.dev.i2c, 3);
            break;
        case BKDR_PURPLE:
            rgb_color(model.dev.i2c, 4);
            break;
        case BKDR_CYAN:
            rgb_color(model.dev.i2c, 5);
            break;
        case BKDR_WHITE:
            rgb_color(model.dev.i2c, 6);
            break;
        }
    }
    rgb_fan(model.dev.i2c, model.fan.speed);
    model.fan.current_speed = model.fan.speed;
    ssd1306_begin(SSD1306_SWITCHCAPVCC, model.dev.i2c);
    ssd1306_clearDisplay();
    ssd1306_display();
}

static void model_exec(void)
{
    model.cpu.temp = cpu_get_temp();
    unsigned int speed = model.fan.current_speed;
    if (model.fan.mode != BKDR_DIRECT)
    {
        if (model.cpu.temp > 1000 * model.fan.bound.upper)
        {
            speed = MODEL_FAN_SPEED_MAX;
        }
        else if (model.cpu.temp > 1000 * model.fan.bound.lower)
        {
            if (model.fan.mode == BKDR_GRADED)
            {
                int delta = model.cpu.temp - 1000 * model.fan.bound.lower;
                int bound = model.fan.bound.upper - model.fan.bound.lower;
                speed = abs(delta) * MODEL_FAN_SPEED_MAX / bound;
            }
            else
            {
                speed = model.fan.speed;
            }
        }
        else
        {
            speed = 0;
        }
    }
    if (model.fan.current_speed != speed)
    {
        model.fan.current_speed = speed;
        rgb_fan(model.dev.i2c, speed);
    }
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
    sleep(model.cfg.sleep);
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
