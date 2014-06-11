uname_S=$(shell uname -s)
ifeq (Darwin, $(uname_S))
  CFLAGS=-Ilibuv/include -g -I/usr/include/luajit-2.0 -DLUV_STACK_CHECK -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Werror -fPIC
  LIBS=-lm -lluajit-5.1 -framework CoreServices -framework Cocoa -L/usr/local/lib/
  SHARED_LIB_FLAGS=-bundle -o luv.so temp/luv.o libuv/libuv.a temp/common.o temp/buffer.o
else
  CFLAGS=-Ilibuv/include -ggdb -O0 -I/usr/include/luajit-2.0 -DLUV_STACK_CHECK -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Werror -fPIC
  LIBS=-lm -lrt
  SHARED_LIB_FLAGS=-shared -fPIC -ggdb -O0 -o luv.so temp/luv.o libuv/.libs/libuv.so temp/common.o temp/buffer.o
endif

all: luv.so

#libuv/libuv.a:
#	CPPFLAGS=-fPIC $(MAKE) -C libuv

temp:
	mkdir temp

temp/%.o: src/%.c
	$(CC) -c $< -o $@ ${CFLAGS}

temp/buffer.o: src/buffer.c src/buffer.h
temp/common.o: src/common.c src/common.h 
temp/luv.o: src/luv.c src/luv.h src/luv_functions.c 

luv.so: temp/luv.o libuv/.libs/libuv.so temp/common.o temp/buffer.o
	$(CC) ${SHARED_LIB_FLAGS} ${LIBS}

clean:
	make -C libuv clean
	rm -rf temp
	rm -f luv.so
