#ifndef YAHBOOM_RGB_H
#define YAHBOOM_RGB_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

void rgb_set(int fd_i2c, unsigned char num, unsigned char R, unsigned char G, unsigned char B);
void rgb_effect(int fd_i2c, unsigned char effect);
void rgb_speed(int fd_i2c, unsigned char speed);
void rgb_color(int fd_i2c, unsigned char color);
void rgb_fan(int fd_i2c, unsigned char speed);
void rgb_off(int fd_i2c);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* rgb.h */
