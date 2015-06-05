all:JikesProbes

JikesProbes:
	cd ./JikesRVMProbes; make OPTION=-m32

clean:
	cd ./JikesRVMProbes; make clean
