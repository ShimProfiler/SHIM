JDK=/opt/jdk1.6.0
JIKESRVMJAR=/home/yangxi/code/jike/jikemoma/dist/production_momaIP_x86_64-linux/jksvm.jar
JAVAC=$(JDK)/bin/javac
JAVA=$(JDK)/bin/java
BENCHMARKS=/usr/share/benchmarks
CFLAGS=-O2 -g  -D_GNU_SOURCE -fPIC
ifeq (-m32,$(findstring -m32,$(OPTION)))
M32_FLAG = y
CFLAGS += -m32
endif
