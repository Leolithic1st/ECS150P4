# Target library
lib := libfs.a

SRC := $(wildcard *.c)
SRC_OBJ := $(patsubst %.c,%.o,$(SRC))

CC := gcc
CFLAGS := -Wall -Werror -pthread

all: $(lib)

$(lib): $(SRC_OBJ)
	ar rcs $(lib) $(SRC_OBJ)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(SRC_OBJ)
	rm -f $(lib)