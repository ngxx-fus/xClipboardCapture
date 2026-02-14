# ==========================================
# Modernized Makefile for ClipboardCapture
# ==========================================

CC = gcc

# Compiler flags: Include current dir (-I.), enable warnings, optimize (-O2)
CFLAGS = -std=gnu11 -O2 -Wall -I. $(shell sdl2-config --cflags)

# Linker flags: Cleaned up duplicates, added -lpthread for future multi-threading
LDFLAGS = $(shell sdl2-config --libs) -lxcb -lxcb-xfixes -lz -lxuniversal -lpthread

# Source files
TEST_SRC = *.c 

# Header files (Crucial for tracking changes!)
HEADERS = CBC_Setup.h CBC_SysFile.h ClipboardCapture.h

# Output binary
TEST_BIN = TestProg

.PHONY: all clean

all: $(TEST_BIN)

# The binary depends on BOTH source files AND header files.
# If ANY of these files change, it will recompile.
$(TEST_BIN): $(TEST_SRC) $(HEADERS)
	@echo ">>> Compiling $(TEST_BIN)..."
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_BIN) $(LDFLAGS)
	@echo ">>> Build successful!"

clean:
	@echo ">>> Cleaning up..."
	rm -f $(TEST_BIN)
