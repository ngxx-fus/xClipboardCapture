# clipboardManager/Makefile (test harness)
# Build test binary .test.cbm.c and link with prebuilt objects, zlib and SDL2

CC = gcc
CFLAGS = -std=gnu11 -O2 -Wall -I.  $(shell sdl2-config --cflags)
LDFLAGS = -lxcb -lz $(shell sdl2-config --libs) -lxuniversal -lxcb -lxcb-xfixes -lz -lSDL2

TEST_SRC = TestProg.c 
TEST_BIN = TestProg

# Link with prebuilt objects in ../build/*
OBJS = \

.PHONY: all clean

all: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(OBJS)
	$(CC) $(CFLAGS) $(TEST_SRC) $(OBJS) -o $(TEST_BIN) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN)
