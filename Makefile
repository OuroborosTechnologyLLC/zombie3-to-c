CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./raylib-5.5_linux_amd64/include -g -fPIC
LDFLAGS = -L./raylib-5.5_linux_amd64/lib -Wl,-rpath=./raylib-5.5_linux_amd64/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: main

main: main.c
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS)

# gameLoop.so: ./game/gameLoop.c
# 	$(CC) $(CFLAGS) -o $@.tmp ./game/gameLoop.c $(LDFLAGS) -shared
# 	mv $@.tmp $@

clean:
	rm -f main

