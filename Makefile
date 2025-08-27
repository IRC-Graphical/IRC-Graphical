# Supports Linux and Windows (MinGW) (Maybe)

# Detect os
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    CC = x86_64-w64-mingw32-gcc
    PKG_CONFIG = x86_64-w64-mingw32-pkg-config
    EXECUTABLE_EXT = .exe
    EXTRA_LIBS = -lws2_32 -liphlpapi
    # Windows-specific paths for GTK and JSON-C
    GTK_CFLAGS = `$(PKG_CONFIG) --cflags gtk+-3.0`
    GTK_LIBS = `$(PKG_CONFIG) --libs gtk+-3.0`
    JSON_CFLAGS = -I/mingw64/include/json-c
    JSON_LIBS = -ljson-c
else
    DETECTED_OS := $(shell uname -s)
    CC = gcc
    PKG_CONFIG = pkg-config
    EXECUTABLE_EXT = 
    EXTRA_LIBS = 
    # Linux/Unix GTK and JSON-C detection
    GTK_CFLAGS = `$(PKG_CONFIG) --cflags gtk+-3.0`
    GTK_LIBS = `$(PKG_CONFIG) --libs gtk+-3.0`
    JSON_CFLAGS = `$(PKG_CONFIG) --cflags json-c`
    JSON_LIBS = `$(PKG_CONFIG) --libs json-c`
endif

# Compiler and linker flags
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS += $(GTK_CFLAGS) $(JSON_CFLAGS)

ifeq ($(DETECTED_OS),Windows)
    CFLAGS += -D_WIN32_WINNT=0x0601 -DWINVER=0x0601
endif

DEBUG_CFLAGS = -g -DDEBUG -O0
RELEASE_CFLAGS = -O2 -DNDEBUG

LIBS = $(GTK_LIBS) $(JSON_LIBS) -lpthread $(EXTRA_LIBS)

# Source files
SOURCES = src/main.c src/network.c src/gui.c src/config.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = bin/irc_client$(EXECUTABLE_EXT)

# Default target
all: release

# Debug build
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(TARGET)

# Release build
release: CFLAGS += $(RELEASE_CFLAGS)
release: $(TARGET)

# Create target directory and build executable
$(TARGET): $(OBJECTS) | bin
	$(CC) $(OBJECTS) -o $@ $(LIBS)

# Compile source files
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create bin directory
bin:
	mkdir -p bin

# Clean build files
clean:
	rm -f src/*.o $(TARGET)
	rm -rf bin

# Install (Linux only)
ifeq ($(DETECTED_OS),Linux)
install: release
	install -m 755 $(TARGET) /usr/local/bin/irc_client
	install -d /usr/local/share/applications
	install -m 644 irc_client.desktop /usr/local/share/applications/
else
install:
	@echo "Install target only available on Linux"
endif

# Uninstall (Linux only)
ifeq ($(DETECTED_OS),Linux)
uninstall:
	rm -f /usr/local/bin/irc_client
	rm -f /usr/local/share/applications/irc_client.desktop
else
uninstall:
	@echo "Uninstall target only available on Linux"
endif

# Windows-specific: Create installer package
ifeq ($(DETECTED_OS),Windows)
package: release
	@echo "Creating Windows package..."
	mkdir -p dist/irc_client
	cp $(TARGET) dist/irc_client/
	cp README.md dist/irc_client/
	cp LICENSE dist/irc_client/
	# Copy required DLLs (you may need to adjust paths)
	# cp /mingw64/bin/libgtk-3-0.dll dist/irc_client/
	# cp /mingw64/bin/libgdk-3-0.dll dist/irc_client/
	# ... (add other required GTK DLLs)
	cd dist && zip -r irc_client_windows.zip irc_client/
else
package:
	@echo "Package target only available on Windows"
endif

# Development helpers
run: debug
	./$(TARGET)

# Check dependencies
check-deps:
	@echo "Checking dependencies for $(DETECTED_OS)..."
ifeq ($(DETECTED_OS),Windows)
	@echo "Required: MinGW-w64, GTK+3 development libraries, json-c"
	@$(PKG_CONFIG) --exists gtk+-3.0 && echo "✓ GTK+3 found" || echo "✗ GTK+3 not found"
	@$(PKG_CONFIG) --exists json-c && echo "✓ json-c found" || echo "✗ json-c not found"
else
	@echo "Required: gcc, GTK+3 development libraries, json-c development libraries"
	@$(PKG_CONFIG) --exists gtk+-3.0 && echo "✓ GTK+3 found" || echo "✗ GTK+3 not found - install libgtk-3-dev"
	@$(PKG_CONFIG) --exists json-c && echo "✓ json-c found" || echo "✗ json-c not found - install libjson-c-dev"
endif

.PHONY: all debug release clean install uninstall package run check-deps
