#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define NPAGES 1


int main(void)
{
	int fd;
        int *kadr;

	int len = NPAGES * getpagesize();

	if ((fd=open("/dev/ppid_map", O_RDWR|O_SYNC)) < 0) {
		perror("open");
		exit(-1);
	}
	fprintf(stderr, "/dev/ppid_map: open OK\n");

	kadr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED| MAP_LOCKED,
	    fd, 0);

	if (kadr == MAP_FAILED)	{
		perror("Could not mmap /dev/ppid_map");
		exit(-1);
	}
	fprintf(stderr, "ppid_map: mmap OK\n");

	int cpu = 0;
	while(1){
	  for (cpu=0; cpu<8; cpu++){
	    int *buf = kadr + cpu * (64/sizeof(int));
	    printf("CPU%d: tgid %d pid %d\n", cpu, buf[0], buf[1]);
	  }
	  sleep(1);
	}
	close(fd);
	return(0);
}

