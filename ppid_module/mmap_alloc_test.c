#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define NPAGES 1

/*
 * Program to test mmap_alloc driver.
 *
 * It reads out values from the buffer allocated by the driver and
 * check the content correctness.
 *
 * You need to manually create a device in dev/. To create it
 *
 * 1. Find the major number assigned to the driver
 *
 *	grep mmap_alloc /proc/devices'
 *
 * 2. Create the special file (assuming major number 254)
 *
 *	mknod /dev/mmap_alloc c 254 0
*/

int main(void)
{
	int fd;
	unsigned int *kadr;

	int len = NPAGES * getpagesize();

	if ((fd=open("/dev/mmap_alloc", O_RDWR|O_SYNC)) < 0) {
		perror("open");
		exit(-1);
	}
	fprintf(stderr, "mmap_alloc: open OK\n");

	kadr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED| MAP_LOCKED,
	    fd, 0);

	if (kadr == MAP_FAILED)	{
		perror("mmap");
		exit(-1);
	}
	fprintf(stderr, "mmap_alloc: mmap OK\n");

	if ((kadr[0]!=0xdead0000) || (kadr[1]!=0xbeef0000)
	    || (kadr[len / sizeof(int) - 2] !=
	        (0xdead0000 + len / sizeof(int) - 2))
	    || (kadr[len / sizeof(int) - 1] !=
	        (0xbeef0000 + len / sizeof(int) - 2))) {
		fprintf(stderr, "mmap_alloc: check ERROR\n");
		fprintf(stderr, "0x%x 0x%x\n", kadr[0], kadr[1]);
		fprintf(stderr, "0x%x 0x%x\n", kadr[len / sizeof(int) - 2],
		    kadr[len / sizeof(int) - 1]);
	} else {
		fprintf(stderr, "mmap_alloc: check OK\n");
	}
	close(fd);
	return(0);
}

