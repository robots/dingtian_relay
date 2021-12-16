#include "platform.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "console.h"

#include "rtc.h"
#include "syscall.h"

//#define DEBUG 1
#undef DEBUG

#define FDDEVSHIFT 8
#define FDMASK     0xff

/* sbrk stuff*/
extern unsigned int _end_ram; // end of ram
extern unsigned int _end_sram; // end of bss

static caddr_t heap = NULL;


static struct devoptab_t con_devoptab;

const struct devoptab_t *syscall_devs[] = {
	&con_devoptab,
#if HAVE_SPIFFS
	&spifs_devoptab,
#endif
#if HAVE_RAMDISK
	&ramdisk_devoptab,
#endif
	NULL
};

void syscall_init(void)
{
	uint32_t i = 0;

	while (syscall_devs[i]) {
#ifdef DEBUG
		int ret = syscall_devs[i]->init();
		console_printf(CON_ERR, "SYS: Initing '%s' ret %d\n", syscall_devs[i]->name, ret);
#else
		syscall_devs[i]->init();
#endif
		i++;
	}

	//console_printf(CON_ERR, "SyscalL: Init done\n");
}

// low level bulk memory allocator - used by malloc
caddr_t _sbrk ( int increment )
{
	caddr_t prevHeap;
	caddr_t nextHeap;

	if (heap == NULL) {
		// first allocation
		heap = (caddr_t)&_end_sram;
	}

	prevHeap = heap;

	// Always return data aligned on a 8 byte boundary
	nextHeap = (caddr_t)(((unsigned int)(heap + increment) + 7) & ~7);

	// get current stack pointer
	//register caddr_t stackPtr asm ("sp");

	// Check enough space and there is no collision with stack coming the other way
	// if stack is above start of heap
	if (nextHeap >= (caddr_t)&_end_ram) {
		console_printf(CON_ERR, "SYS: Out of memory\n");
		return NULL; // error - no more memory
	} else {
		heap = nextHeap;
		return (caddr_t) prevHeap;
	}
}

int _gettimeofday (struct timeval * tp, void * tzvp)
{
	struct timezone *tzp = tzvp;
	if (tp) {
		/* Ask the host for the seconds since the Unix epoch.  */
		tp->tv_sec = rtc_get_time();
		tp->tv_usec = 0;
	}

	/* Return fixed data for the timezone.  */
	if (tzp) {
		tzp->tz_minuteswest = 0;
		tzp->tz_dsttime = 0;
	}

	return 0;
}

static int path2dev(const char *path)
{
	char devname[SYSCALL_DEVNAME];
	int which_devoptab = 0;
	char *file;

	file = strstr(path, "/");
	if ((!file) || ((file - path) >= SYSCALL_DEVNAME)) {
		errno = ENODEV;
		return -1;
	}

	memset(devname, 0, SYSCALL_DEVNAME);
	memcpy(devname, path, file - path);

	do {
		if (strcmp(syscall_devs[which_devoptab]->name, devname) == 0) {
			break;
		}
		which_devoptab++;
	} while (syscall_devs[which_devoptab]);

	if (!syscall_devs[which_devoptab]) {
		errno = ENODEV;
		return -1;
	}

	return which_devoptab;
}

int _open_r(struct _reent *ptr, const char *path, int flags, int mode )
{
	int which_devoptab = 0;
	int fd = -1;
	char *file;

#ifdef DEBUG
	console_printf(CON_ERR, "SYS: Open path '%s'\n", path);
#endif

	file = strstr(path, "/");
	if ((!file) || ((file - path) >= SYSCALL_DEVNAME)) {
		errno = ENODEV;
		return -1;
	}
	file++;

	which_devoptab = path2dev(path);
	if (which_devoptab < 0) {
		errno = ENODEV;
		return -1;
	}

	fd = syscall_devs[which_devoptab]->open_r( ptr, file, flags, mode );
#ifdef DEBUG
	console_printf(CON_ERR, "SYS: devopt->open_r(%s) = %d \n", file, fd);
#endif

	if (fd == -1) {
		return -1;
	}

	fd |= (which_devoptab << FDDEVSHIFT);
#ifdef DEBUG
	console_printf(CON_ERR, "SYS: open_r = %x \n", fd);
#endif

	return fd;
}

long _close_r(struct _reent *ptr, int fd)
{
	int which;

	which = fd >> FDDEVSHIFT;
	fd = fd & FDMASK;

#ifdef DEBUG
	console_printf(CON_ERR, "SYS: close_r = dev: %d fd:%d \n", which, fd);
#endif

	return syscall_devs[which]->close_r(ptr, fd);
}

long _write_r(struct _reent *ptr, int fd, const void *buf, size_t cnt )
{
	int which;

	which = fd >> FDDEVSHIFT;
	fd = fd & FDMASK;

#ifdef DEBUG
	console_printf(CON_ERR, "SYS: write_r = dev: %d fd:%d \n", which, fd);
#endif

	return syscall_devs[which]->write_r(ptr, fd, buf, cnt);
}

long _read_r(struct _reent *ptr, int fd, void *buf, size_t cnt )
{
	int which;

	which = fd >> FDDEVSHIFT;
	fd = fd & FDMASK;

#ifdef DEBUG
	console_printf(CON_ERR, "SYS: read_r = dev: %d fd:%d \n", which, fd);
#endif

	return syscall_devs[which]->read_r(ptr, fd, buf, cnt);
}

int _lseek_r(struct _reent *r, int fd, int ptr, int dir)
{
	int which;

	which = fd >> FDDEVSHIFT;
	fd = fd & FDMASK;

#ifdef DEBUG
	console_printf(CON_ERR, "SYS: lseek_r = dev: %d fd:%d \n", which, fd);
#endif

	return syscall_devs[which]->lseek_r(r, fd, ptr, dir);
}

int _unlink_r(struct _reent *ptr, const char *path)
{
	int which = path2dev(path);
	char *file;

	if (which < 0) {
		return -1;
	}

	file = strstr(path, "/");
	if ((!file) || ((file - path) >= SYSCALL_DEVNAME)) {
		errno = ENODEV;
		return -1;
	}
	file++;

#ifdef DEBUG
	console_printf(CON_ERR, "SYS: unlink_r = dev: %d file:%s \n", which, file);
#endif

	return syscall_devs[which]->unlink_r(ptr, file);
}

int _isatty (int fd)
{
	int which;

	which = fd >> FDDEVSHIFT;

	if (which == 0) {
		return 1;
	}

	return 0;
}

int _fstat (int fd, struct stat *st)
{
	(void)fd;
	(void)st;
/*
	st->st_mode = S_IFCHR;
	st->st_blksize = 256;
	return 0;
*/
	return -1;
}

void *mem_malloc(size_t size);
void  mem_free(void *mem);
/*
void *__wrap__malloc_r(struct _reent *r, size_t size)
{
	(void)r;
	return mem_malloc(size);
}

void __wrap__free_r(struct _reent *r, void *ptr)
{
	(void)r;
	mem_free(ptr);
}
*/
static int con_init()
{
	return 0;
}

static int con_open(struct _reent *r, const char *path, int flags, int mode)
{
	(void)r;
	(void)path;
	(void)flags;
	(void)mode;

	return -1;
}

static int con_close(struct _reent *r, int fd)
{
	(void)r;
	(void)fd;

	return 0;
}

static long con_write(struct _reent *r, int fd, const char *ptr, int len)
{
	(void)r;

	switch (fd) {
		case STDOUT_FILENO:
			console_write(CON_INFO, ptr, len);
			break;
		case STDERR_FILENO:
			console_write(CON_ERR, ptr, len);
			break;
		default:
			return -1;
	}

	return len;
}

static long con_read(struct _reent *r, int fd, char *ptr, int len)
{
	(void)r;
	(void)fd;
	(void)ptr;
	(void)len;

	return -1;
}

static int con_lseek(struct _reent *r, int fd, int ptr, int dir)
{
	(void)r;
	(void)fd;
	(void)ptr;
	(void)dir;

	return -1;
}

static int con_unlink(struct _reent *r, const char *path)
{
	(void)r;
	(void)path;

	return -1;
}


static struct devoptab_t con_devoptab = {
	"console",
	con_init,
	con_open,
	con_close,
	con_write,
	con_read,
	con_lseek,
	con_unlink,
};
