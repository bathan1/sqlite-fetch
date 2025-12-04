# Disable *all* implicit rules and implicit variables
MAKEFLAGS += -rR
.SUFFIXES:

# ---- Names ----
API_EXT     := yapi
SQLITE_EXT  := yarts

API_TARGET     := lib$(API_EXT).so
SQLITE_TARGET  := lib$(SQLITE_EXT).so

# ---- Source Files ----
SRC_COMMON := \
    src/yapi.c \
    src/lib/deque.c src/lib/bhop.c src/lib/fetch.c \
    src/lib/cfns.c src/lib/tcp.c src/lib/sql.c

SRC_SQLITE := \
    src/yarts.c

OBJ_COMMON  := $(SRC_COMMON:.c=.o)
OBJ_SQLITE  := $(SRC_SQLITE:.c=.o)

# ---- Tools ----
CC      := gcc
CFLAGS  := -O2 -fPIC -Wall -Wextra -g
LDFLAGS := -shared
LIBS    := -lcurl -lyajl -lyyjson -lsqlite3

# ---- Install Locations ----
PREFIX     := /usr/local
LIBDIR     := $(PREFIX)/lib
INCLUDEDIR := $(PREFIX)/include

# ---- Default: Only build SQLite extension ----
default: $(SQLITE_TARGET)

# ---- Build both ----
all: $(API_TARGET) $(SQLITE_TARGET)

# ---- Build Rules ----
$(API_TARGET): $(OBJ_COMMON)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SQLITE_TARGET): $(OBJ_SQLITE) $(OBJ_COMMON)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Install Public API (NOT the SQLite extension) ----
install: $(API_TARGET)
	@echo "Installing $(API_TARGET) to $(LIBDIR)"
	mkdir -p $(LIBDIR)
	install -m 755 $(API_TARGET) $(LIBDIR)

	@echo "Installing public header yarts.h to $(INCLUDEDIR)"
	install -m 644 src/yapi.h $(INCLUDEDIR)/yapi.h

	@echo "Updating ldconfig cache (Linux only)"
	@if [ "$(shell uname -s)" = "Linux" ]; then ldconfig; fi

	@echo "Install complete."

# ---- Uninstall ----
uninstall:
	@echo "Removing $(API_TARGET) from $(LIBDIR)"
	rm -f $(LIBDIR)/$(API_TARGET)

	@echo "Removing yarts.h from $(INCLUDEDIR)"
	rm -f $(INCLUDEDIR)/yarts.h

	@echo "Updating ldconfig cache (Linux only)"
	@if [ "$(shell uname -s)" = "Linux" ]; then ldconfig; fi

	@echo "Uninstall complete."

# ---- Clean ----
clean:
	rm -f $(OBJ_COMMON) $(OBJ_SQLITE) $(API_TARGET) $(SQLITE_TARGET)

.PHONY: default all install uninstall clean
