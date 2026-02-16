# ==============================================================================
# Makefile with xUniversal Submodule Integration
# ==============================================================================

CC      = gcc

INSTALL_PATH_DIR=$(HOME)/.fus/

# --- Submodule Configuration ---
XUNIV_DIR      = ./xUniversal
XUNIV_BUILD    = $(XUNIV_DIR)/Build
XUNIV_INC_PATH = $(XUNIV_BUILD)/include
XUNIV_LIB_PATH = $(XUNIV_BUILD)/lib

# --- Compiler Flags ---
# -I.              : Include headers from current directory
# -I$(XUNIV_INC_PATH): Include headers from xUniversal Build folder
CFLAGS  = -std=gnu11 -O2 -Wall -Wextra -I. -I$(XUNIV_INC_PATH)

# --- Linker Flags ---
# -L$(XUNIV_LIB_PATH): Tell linker where to find libxuniversal.so
# -Wl,-rpath,...     : Hardcode the library path into the binary so it runs 
#                      without needing LD_LIBRARY_PATH or installing to /usr/lib
LDFLAGS = -L$(XUNIV_LIB_PATH) -Wl,-rpath,$(XUNIV_LIB_PATH) \
          -lxcb -lxcb-xfixes -lz -lxuniversal -lpthread

# --- Project Files ---
SRCS    = $(wildcard *.c)
HEADERS = $(wildcard *.h)
OBJS    = $(SRCS:.c=.o)
BIN     = xClipBoardCapture

# --- Targets ---
.PHONY: all clean xuniversal_build install

# Default target: build submodule first, then build the main app
all: xuniversal_build $(BIN)

# Trigger the Makefile inside xUniversal submodule
xuniversal_build:
	@echo ">>> Checking xUniversal Submodule..."
	@$(MAKE) -C $(XUNIV_DIR)

# Link all compiled object files into the final binary
$(BIN): $(OBJS)
	@echo ">>> Linking $(BIN)..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ">>> Build successful!"

# Pattern rule to compile each .c file into a .o file
%.o: %.c $(HEADERS)
	@echo ">>> Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

restart: all
	@echo ">>> Restarting xClipBoardCapture..."
	@pkill -SIGINT $(BIN) || true
	@sleep 0.5
	@./$(BIN) &

# [NEW]: Install target to deploy the binary to $(HOME)/.fus/
install: all
	@echo ">>> Installing to $(INSTALL_PATH_DIR)..."
	@mkdir -p $(INSTALL_PATH_DIR)
	@cp $(BIN) $(INSTALL_PATH_DIR)
	@echo ">>> Installation complete! You can now run it from $(INSTALL_PATH_DIR)$(BIN)"

clean:
	@echo ">>> Cleaning up ClipboardCapture..."
	rm -f $(BIN) $(OBJS)
	@echo ">>> Cleaning up xUniversal Submodule..."
	@$(MAKE) -C $(XUNIV_DIR) clean

