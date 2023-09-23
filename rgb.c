#include "rgb.h"
#include "i2c.h"
#include "main.h"

void rgb_set(int fd_i2c, unsigned char num, unsigned char R, unsigned char G, unsigned char B)
{
    if (num >= 3)
    {
        num = 0xFF;
    }
    i2c_write(fd_i2c, HAT_I2C_ADDR, 0x00, num);
    i2c_write(fd_i2c, HAT_I2C_ADDR, 0x01, R);
    i2c_write(fd_i2c, HAT_I2C_ADDR, 0x02, G);
    i2c_write(fd_i2c, HAT_I2C_ADDR, 0x03, B);
}

void rgb_effect(int fd_i2c, unsigned char effect)
{
    if (effect <= 4)
    {
        i2c_write(fd_i2c, HAT_I2C_ADDR, 0x04, effect);
    }
}

void rgb_speed(int fd_i2c, unsigned char speed)
{
    if (speed >= 1 && speed <= 3)
    {
        i2c_write(fd_i2c, HAT_I2C_ADDR, 0x05, speed);
    }
}

void rgb_color(int fd_i2c, unsigned char color)
{
    if (color <= 6)
    {
        i2c_write(fd_i2c, HAT_I2C_ADDR, 0x06, color);
    }
}

void rgb_off(int fd_i2c)
{
    unsigned char data_buf = 0x00;
    i2c_write(fd_i2c, HAT_I2C_ADDR, 0x07, data_buf);
}

void rgb_fan(int fd_i2c, unsigned char speed)
{
    unsigned char data_buf = 0x00;
    if (speed > 8)
    {
        data_buf = 1;
    }
    else if (speed > 0)
    {
        data_buf = speed + 1;
    }
    i2c_write(fd_i2c, HAT_I2C_ADDR, 0x08, data_buf);
}
