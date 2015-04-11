include ./common.mk
objs=shim_core.o shim_example.o
all:shim_example $(objs) JikesProbes
%.o:%.c
	gcc -c $(CFLAGS) $< -o $@ -pthread -lpfm -lrt


shim_example: $(objs)
	gcc $(CFLAGS) $(objs) -o $@ -pthread -lpfm -lrt

JikesProbes:
	cd ./JikesRVMProbes; make OPTION=-m32

clean:
	rm shim_example $(objs)
	cd ./JikesRVMProbes; make clean