# ---- Config ----
EXT     := yarts
TARGET  := lib$(EXT).so
SRC     := src/yarts.c src/helpers.bassoon.c src/helpers.bhop.c src/helpers.fetch.c src/helpers.prefix.c src/helpers.tcp.c src/helpers.tls.c
OBJ     := $(SRC:.c=.o)

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    TARGET := lib$(EXT).dylib
endif

# Compiler + flags
CC      := gcc
CFLAGS  := -O2 -fPIC -Wall -Wextra -g
LDFLAGS := -shared

# System libraries
LIBS := -lcurl -lyajl -lyyjson -lsqlite3

# Install locations
PREFIX       := /usr/local
LIBDIR       := $(PREFIX)/lib
INCLUDEDIR   := $(PREFIX)/include/$(EXT)

# ---- Build Rules ----

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Install / Uninstall ----
install: $(TARGET)
	@echo "Installing $(TARGET) to $(LIBDIR)"
	mkdir -p $(LIBDIR)
	install -m 755 $(TARGET) $(LIBDIR)

	@echo "Installing public header yarts.h to $(PREFIX)/include"
	install -m 644 src/yarts.h $(PREFIX)/include/yarts.h

ifeq ($(UNAME_S),Linux)
	@echo "Updating ldconfig cache"
	ldconfig
endif

	@echo "Install complete."

uninstall:
	@echo "Removing library from $(LIBDIR)"
	rm -f $(LIBDIR)/$(TARGET)

	@echo "Removing header from $(PREFIX)/include"
	rm -f $(PREFIX)/include/yarts.h

ifeq ($(UNAME_S),Linux)
	@echo "Updating ldconfig cache"
	ldconfig
endif

	@echo "Uninstall complete."

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean install uninstall
