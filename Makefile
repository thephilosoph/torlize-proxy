# torlize – simple SOCKS4/4a client for Tor
# Works on Linux and Windows (MinGW / MSYS2)

# Force gcc (MinGW often has no 'cc' in PATH)
CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -std=c11
LDFLAGS  =
TARGET   = torlize

# Detect Windows / MinGW
ifeq ($(OS),Windows_NT)
    TARGET   = torlize.exe
    LDFLAGS += -lws2_32
    RM       = del /Q
else
    RM       = rm -f
endif

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): torlize.c torlize.h
	$(CC) $(CFLAGS) -o $(TARGET) torlize.c $(LDFLAGS)

debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

clean:
	-$(RM) $(TARGET) torlize.exe *.o 2>nul || true

# Manual override examples:
#   mingw32-make                          (auto-detects Windows)
#   make                                  (Linux)
#   make CC=x86_64-w64-mingw32-gcc        (cross-compile)