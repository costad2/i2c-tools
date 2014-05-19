/*
    i2cbusses: Print the installed i2c busses for both 2.4 and 2.6 kernels.
               Part of user-space programs to access for I2C
               devices.
    Copyright (c) 1999-2003  Frodo Looijaard <frodol@dds.nl> and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (C) 2008-2012  Jean Delvare <jdelvare@suse.de>
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

/* For strdup and snprintf */
#define _BSD_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/ioctl.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <i2c/busses.h>

int i2c_open_i2c_dev(int i2cbus, char *filename, size_t size, int quiet) {
	int file = -1;

	snprintf(filename, size,"/dev/i2c-%d", i2cbus);
	file = open(filename, O_RDWR);

	if (file < 0 && !quiet) {
		if (errno == ENOENT) {
			fprintf(stderr, "Error: Could not open file "
					"`/dev/i2c-%d': %s\n", i2cbus, strerror(ENOENT));
		} else {
			fprintf(stderr, "Error: Could not open file "
					"`%s': %s\n", filename, strerror(errno));
			if (errno == EACCES)
				fprintf(stderr, "Run as root?\n");
		}
	}

	return (file);
}

int i2c_get_functionality(int i2cbus, unsigned long *functionality)
{
	int file;
	char filename[20];
	int ret = 0;

	file = i2c_open_i2c_dev(i2cbus, filename, sizeof(filename), 1);
	if (file < 0)
		return -ENODEV;

	if (ioctl(file, I2C_FUNCS, functionality) < 0)
		ret = -ENODEV;

	close(file);

	return ret;
}

int i2c_set_slave_addr(int file, int address, int force)
{
	/* With force, let the user read from/write to the registers
	   even when a driver is also running */
	if (ioctl(file, force ? I2C_SLAVE_FORCE : I2C_SLAVE, address) < 0) {
		fprintf(stderr,
			"Error: Could not set address to 0x%02x: %s\n",
			address, strerror(errno));
		return -errno;
	}
	return 0;
}

/* set timeout in units of 10 ms */
int i2c_set_adapter_timeout(int file, int timeout)
{
	unsigned long timeout_val = 3;
	if (timeout)
		timeout_val = (unsigned long)timeout;

	if (ioctl(file, I2C_TIMEOUT, timeout_val) < 0) {
		fprintf(stderr,"Error: Could not set timeout to %d: %s\n",timeout, strerror(errno));
		return -errno;
	}

	return 0;
}

/* number of times a device address should be polled when not acknowledging */
int i2c_set_adapter_retries(int file, int retries)
{
	unsigned long retries_val = 2;
	if (retries)
		retries_val = (unsigned long)retries;

	if (ioctl(file, I2C_RETRIES, retries_val) < 0) {
		fprintf(stderr,"Error: Could not set retries to %d: %s\n", retries, strerror(errno));
		return -errno;
	}

	return 0;
}
