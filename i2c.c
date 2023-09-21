#include "i2c.h"

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

int i2c_write(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char data_buf)
{
    unsigned char msg_buf[2];
    struct i2c_msg messages;
    struct i2c_rdwr_ioctl_data data;

    msg_buf[0] = reg_addr;
    msg_buf[1] = data_buf;

    messages.addr = dev_addr;
    messages.flags = 0;
    messages.len = 2;
    messages.buf = msg_buf;

    data.msgs = &messages;
    data.nmsgs = 1;
    if (ioctl(fd, I2C_RDWR, &data) < 0)
    {
        return ~0;
    }

    usleep(1000);
    return 0;
}

int i2c_read(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char *data_buf)
{
    struct i2c_msg messages[2];
    struct i2c_rdwr_ioctl_data data;

    messages[0].addr = dev_addr;
    messages[0].flags = 0;
    messages[0].len = 1;
    messages[0].buf = &reg_addr;

    messages[1].addr = dev_addr;
    messages[1].flags = I2C_M_RD;
    messages[1].len = 1;
    messages[1].buf = data_buf;

    data.msgs = messages;
    data.nmsgs = 2;
    if (ioctl(fd, I2C_RDWR, &data) < 0)
    {
        return ~0;
    }

    usleep(100);
    return 0;
}
