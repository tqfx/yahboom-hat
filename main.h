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
#ifndef YAHBOOM_MAIN_H
#define YAHBOOM_MAIN_H

#define HAT_I2C_ADDR 0x0D
#define HAT_CONFIG "yahboom-hat.ini"
#define HAT_DEV_I2C "/dev/i2c-0"
#define HAT_CPU_TEMP "/sys/class/thermal/thermal_zone0/temp"
#define HAT_CPU_USAGE "/proc/stat"
#define HAT_DISK_ROOT "/"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* main.h */
