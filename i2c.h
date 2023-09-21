#ifndef YAHBOOM_I2C_H
#define YAHBOOM_I2C_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

int i2c_write(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char data_buf);
int i2c_read(int fd, unsigned char dev_addr, unsigned char reg_addr, unsigned char *data_buf);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* i2c.h */
