### @file Makefile
### @brief Build script for XUniversal Shared Library with install and test options

CC          = gcc
CFLAGS      = -Wall -Wextra -fPIC -O2
LDFLAGS     = -shared
TARGET      = libxuniversal.so
SRC         = xUniversal.c
OBJ         = $(SRC:.c=.o)

# Installation paths
INSTALL_DIR = ./Build
INCLUDE_DIR = $(INSTALL_DIR)/include
LIB_DIR     = $(INSTALL_DIR)/lib

# Test configuration
TEST_SRC    = TestLib.c
TEST_BIN    = TestLib

.PHONY: all clean install test

### @brief Default target: build the shared library
all: $(TARGET) install

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

### @brief Install headers and .so, then cleanup local build artifacts
install: $(TARGET)
	@echo "Installing to $(INSTALL_DIR)..."
	mkdir -p $(INCLUDE_DIR) $(LIB_DIR)
	cp *.h $(INCLUDE_DIR)/
	cp $(TARGET) $(LIB_DIR)/
	@echo "Cleaning up local build files..."
	rm -f $(TARGET) $(OBJ)
	@echo "Done! Library is ready at $(INSTALL_DIR)."

### @brief Compile and run the test application using the installed library
### @details Uses -Wl,-rpath to link against the library in the Build folder
test: install
	@echo "Building test application..."
	$(CC) $(TEST_SRC) -o $(TEST_BIN) \
		-I$(INCLUDE_DIR) \
		-L$(LIB_DIR) \
		-lxuniversal \
		-Wl,-rpath,$(LIB_DIR)
	@echo "Running test execution..."
	@./$(TEST_BIN)
	@echo "Test completed."

### @brief Remove local objects and the test binary
clean:
	rm -vrf $(OBJ) $(TARGET) $(TEST_BIN)
	rm -vrf $(INSTALL_DIR)
