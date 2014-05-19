/*
    busses.h - SMBus helper functions

    Copyright (C) 2014 Danielle Costantino <danielle.costantino@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/

#ifndef LIB_I2C_BUSSES_H
#define LIB_I2C_BUSSES_H

#include <linux/types.h>
#include <linux/i2c.h>
#include <unistd.h>

extern int i2c_open_i2c_dev(int i2cbus, char *filename, size_t size, int quiet);
extern int i2c_get_functionality(int i2cbus, unsigned long *functionality);
extern int i2c_set_slave_addr(int file, int address, int force);
extern int i2c_set_adapter_timeout(int file, int timeout);
extern int i2c_set_adapter_retries(int file, int retries);

#endif /* LIB_I2C_BUSSES_H */
