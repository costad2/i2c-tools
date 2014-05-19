/*
    i2cbusses: Print the installed i2c busses for both 2.4 and 2.6 kernels.
               Part of user-space programs to access for I2C
               devices.
    Copyright (c) 1999-2003  Frodo Looijaard <frodol@dds.nl> and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (C) 2008-2012  Jean Delvare <jdelvare@suse.de>

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
#include <sys/param.h>	/* for NAME_MAX */
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>	/* for strcasecmp() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <i2c/busses.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "list.h"

#ifndef SYSFS_MAGIC
#define SYSFS_MAGIC 0x62656572
#endif // SYSFS_MAGIC

#ifndef ATTR_MAX
#define ATTR_MAX    128
#endif // ATTR_MAX

int foundsysfs = 0;
char sysfs_mount[NAME_MAX];

struct i2c_adap {
	int nr;
	char *name;
	const char *funcs;
	const char *algo;
};

/*
 * An i2c_dev represents an i2c_adapter ... an I2C or SMBus master, not a
 * slave (i2c_client) with which messages will be exchanged.  It's coupled
 * with a character special file which is accessed by user mode drivers.
 *
 * The list of i2c_dev structures is parallel to the i2c_adapter lists
 * maintained by the driver model, and is updated using bus notifications.
 */
struct i2c_dev {
	struct list_head list;
	struct i2c_adap *adap;
	dev_t *dev;
};


/* returns !0 if sysfs filesystem was found, 0 otherwise */
static int init_sysfs(void) {
	struct statfs statfsbuf;

	snprintf(sysfs_mount, NAME_MAX, "%s", "/sys");
	if (statfs(sysfs_mount, &statfsbuf) < 0 || statfsbuf.f_type != SYSFS_MAGIC)
		return (0);

	return (1);
}

/*
 * Read an attribute from sysfs
 * Returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
static char *sysfs_read_attr(const char *device, const char *attr) {
	char path[NAME_MAX];
	char buf[ATTR_MAX], *p;
	FILE *f;

	snprintf(path, NAME_MAX, "%s/%s", device, attr);

	if (!(f = fopen(path, "r")))
		return NULL;
	p = fgets(buf, ATTR_MAX, f);
	fclose(f);
	if (!p)
		return NULL;

	/* Last byte is a '\n'; chop that off */
	p = strndup(buf, strlen(buf) - 1);
	if (!p)
		perror("Out of memory");
	return (p);
}

/*
 * Call an arbitrary function for each device of the given bus type
 * Returns 0 on success (all calls returned 0), a positive errno for
 * local errors, or a negative error value if any call fails.
 */
static int sysfs_foreach_busdev(const char *bus_type,
		int (*func)(const char *, const char *)) {
	char path[NAME_MAX];
	int path_off, ret;
	DIR *dir;
	struct dirent *ent;

	path_off = snprintf(path, NAME_MAX, "%s/bus/%s/devices", sysfs_mount,
			bus_type);
	if (!(dir = opendir(path)))
		return errno;

	ret = 0;
	while (!ret && (ent = readdir(dir))) {
		if (ent->d_name[0] == '.') /* skip hidden entries */
			continue;

		snprintf(path + path_off, NAME_MAX - path_off, "/%s", ent->d_name);
		ret = func(path, ent->d_name);
	}

	closedir(dir);
	return (ret);
}

/*
 * Call an arbitrary function for each class device of the given class
 * Returns 0 on success (all calls returned 0), a positive errno for
 * local errors, or a negative error value if any call fails.
 */
static int sysfs_foreach_classdev(const char *class_name,
		int (*func)(const char *, const char *)) {
	char path[NAME_MAX];
	int path_off, ret;
	DIR *dir;
	struct dirent *ent;

	path_off = snprintf(path, NAME_MAX, "%s/class/%s", sysfs_mount, class_name);
	if (!(dir = opendir(path)))
		return errno;

	ret = 0;
	while (!ret && (ent = readdir(dir))) {
		if (ent->d_name[0] == '.') /* skip hidden entries */
			continue;

		snprintf(path + path_off, NAME_MAX - path_off, "/%s", ent->d_name);
		ret = func(path, ent->d_name);
	}

	closedir(dir);
	return (ret);
}

static struct i2c_adap *gather_i2c_busses(void);

#define MISSING_FUNC_FMT	"Error: Adapter does not have %s capability\n"

enum adt { adt_dummy, adt_isa, adt_i2c, adt_smbus, adt_unknown };

struct adap_type {
	const char *funcs;
	const char* algo;
};

static struct adap_type adap_types[5] = {
	{ .funcs	= "dummy",
	  .algo		= "Dummy bus", },
	{ .funcs	= "isa",
	  .algo		= "ISA bus", },
	{ .funcs	= "i2c",
	  .algo		= "I2C adapter", },
	{ .funcs	= "smbus",
	  .algo		= "SMBus adapter", },
	{ .funcs	= "unknown",
	  .algo		= "N/A", },
};

static enum adt i2c_get_funcs(int i2cbus)
{
	unsigned long funcs;
	int file;
	char filename[20];
	enum adt ret;

	file = i2c_open_i2c_dev(i2cbus, filename, sizeof(filename), 1);
	if (file < 0)
		return adt_unknown;

	if (ioctl(file, I2C_FUNCS, &funcs) < 0)
		ret = adt_unknown;
	else if (funcs & I2C_FUNC_I2C)
		ret = adt_i2c;
	else if (funcs & (I2C_FUNC_SMBUS_BYTE |
			  I2C_FUNC_SMBUS_BYTE_DATA |
			  I2C_FUNC_SMBUS_WORD_DATA))
		ret = adt_smbus;
	else
		ret = adt_dummy;

	close(file);
	return ret;
}

static void free_adapters(struct i2c_adap *adapters)
{
	int i;

	for (i = 0; adapters[i].name; i++)
		free(adapters[i].name);
	free(adapters);
}

/* We allocate space for the adapters in bunches. The last item is a
   terminator, so here we start with room for 7 adapters, which should
   be enough in most cases. If not, we allocate more later as needed. */
#define BUNCH	8

/* n must match the size of adapters at calling time */
static struct i2c_adap *more_adapters(struct i2c_adap *adapters, int n)
{
	struct i2c_adap *new_adapters;

	new_adapters = realloc(adapters, (n + BUNCH) * sizeof(struct i2c_adap));
	if (!new_adapters) {
		free_adapters(adapters);
		return NULL;
	}
	memset(new_adapters + n, 0, BUNCH * sizeof(struct i2c_adap));

	return new_adapters;
}

static struct i2c_adap *gather_i2c_busses(void)
{
	char s[120];
	struct dirent *de, *dde;
	DIR *dir, *ddir;
	FILE *f;
	char sysfs[NAME_MAX], n[NAME_MAX];
	int count=0;
	struct i2c_adap *adapters;

	adapters = calloc(BUNCH, sizeof(struct i2c_adap));
	if (!adapters)
		return NULL;

	if (foundsysfs == 0)
		foundsysfs = init_sysfs();

	if (foundsysfs)
		sprintf(sysfs, "%s", sysfs_mount);
	else
		return NULL;

	/* Bus numbers in i2c-adapter don't necessarily match those in
	   i2c-dev and what we really care about are the i2c-dev numbers.
	   Unfortunately the names are harder to get in i2c-dev */
	strcat(sysfs, "/class/i2c-dev");
	if(!(dir = opendir(sysfs)))
		goto done;
	/* go through the busses */
	while ((de = readdir(dir)) != NULL) {
		if (de->d_name[0] == '.') /* skip hidden entries */
			continue;

		/* this should work for kernels 2.6.5 or higher and */
		/* is preferred because is unambiguous */
		sprintf(n, "%s/%s/name", sysfs, de->d_name);

		f = fopen(n, "r");
		/* this seems to work for ISA */
		if(f == NULL) {
			sprintf(n, "%s/%s/device/name", sysfs, de->d_name);
			f = fopen(n, "r");
		}
		/* non-ISA is much harder */
		/* and this won't find the correct bus name if a driver
		   has more than one bus */
		if(f == NULL) {
			sprintf(n, "%s/%s/device", sysfs, de->d_name);
			if(!(ddir = opendir(n)))
				continue;
			while ((dde = readdir(ddir)) != NULL) {
				if (dde->d_name[0] == '.') /* skip hidden entries */
					continue;
				if ((!strncmp(dde->d_name, "i2c-", 4))) {
					sprintf(n, "%s/%s/device/%s/name",
						sysfs, de->d_name, dde->d_name);
					if((f = fopen(n, "r")))
						goto found;
				}
			}
		}

found:
		if (f != NULL) {
			int i2cbus;
			enum adt type;
			char *px;

			px = fgets(s, 120, f);
			fclose(f);
			if (!px) {
				fprintf(stderr, "%s: read error\n", n);
				continue;
			}
			if ((px = strchr(s, '\n')) != NULL)
				*px = 0;
			if (!sscanf(de->d_name, "i2c-%d", &i2cbus))
				continue;
			if (!strncmp(s, "ISA ", 4)) {
				type = adt_isa;
			} else {
				/* Attempt to probe for adapter capabilities */
				type = i2c_get_funcs(i2cbus);
			}

			if ((count + 1) % BUNCH == 0) {
				/* We need more space */
				adapters = more_adapters(adapters, count + 1);
				if (!adapters)
					return NULL;
			}

			adapters[count].nr = i2cbus;
			adapters[count].name = strdup(s);
			if (adapters[count].name == NULL) {
				free_adapters(adapters);
				return NULL;
			}
			adapters[count].funcs = adap_types[type].funcs;
			adapters[count].algo = adap_types[type].algo;
			count++;
		}
	}
	closedir(dir);

done:
	return adapters;
}

static int lookup_i2c_bus_by_name(const char *bus_name)
{
	struct i2c_adap *adapters;
	int i, i2cbus = -1;

	adapters = gather_i2c_busses();
	if (adapters == NULL) {
		fprintf(stderr, "Error: Out of memory!\n");
		return -3;
	}

	/* Walk the list of i2c busses, looking for the one with the
	   right name */
	for (i = 0; adapters[i].name; i++) {
		if (strcmp(adapters[i].name, bus_name) == 0) {
			if (i2cbus >= 0) {
				fprintf(stderr,
					"Error: I2C bus name is not unique!\n");
				i2cbus = -4;
				goto done;
			}
			i2cbus = adapters[i].nr;
		}
	}

	if (i2cbus == -1)
		fprintf(stderr, "Error: I2C bus name doesn't match any "
			"bus present!\n");

done:
	free_adapters(adapters);
	return i2cbus;
}

/*
 * Parse an I2CBUS command line argument and return the corresponding
 * bus number, or a negative value if the bus is invalid.
 */
int i2c_lookup_i2c_bus(const char *i2cbus_arg)
{
	unsigned long i2cbus;
	char *end;

	i2cbus = strtoul(i2cbus_arg, &end, 0);
	if (*end || !*i2cbus_arg) {
		/* Not a number, maybe a name? */
		return lookup_i2c_bus_by_name(i2cbus_arg);
	}
	if (i2cbus > 0xFFFFF) {
		fprintf(stderr, "Error: I2C bus out of range!\n");
		return -2;
	}

	return i2cbus;
}

/*
 * Parse a CHIP-ADDRESS command line argument and return the corresponding
 * chip address, or a negative value if the address is invalid.
 */
int i2c_parse_i2c_address(const char *address_arg)
{
	long address;
	char *end;

	address = strtol(address_arg, &end, 0);
	if (*end || !*address_arg) {
		fprintf(stderr, "Error: Chip address is not a number!\n");
		return -1;
	}
	if (address < 0x03 || address > 0x77) {
		fprintf(stderr, "Error: Chip address out of range "
			"(0x03-0x77)!\n");
		return -2;
	}

	return address;
}


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
