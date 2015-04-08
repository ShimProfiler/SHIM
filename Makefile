objs=shim_core.o shim_example.o shim_counter.o
all:shim_example $(objs) JikesProbes
%.o:%.c
	gcc -c -std=c99 $< -o $@ -pthread -lpfm -lrt


shim_example: $(objs)
	gcc $(objs) -o $@ -pthread -lpfm -lrt

JikesProbes:
	cd ./JikesRVMProbes; make OPTION=-m32

clean:
	rm shim_example $(objs)
	cd ./JikesRVMProbes; make clean