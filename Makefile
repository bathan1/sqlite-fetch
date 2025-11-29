# ---- Config ----
EXT     := yarts
TARGET  := lib$(EXT).so
SRC     := src/yarts.c src/fetch.c src/clarinet.c
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

# ---- Build Rules ----

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean

