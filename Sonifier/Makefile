LDFLAGS +=  -lpulse-simple -lpulse -lm
EXTRA_CFLAGS += -march=native -O2 -std=c99 -g -Wall -Wextra
EXTRA_CFLAGS += -Wpointer-arith -Werror

EXES = main

all: $(EXES)

main: main.c
	$(CC) -o main main.c `pkg-config --cflags --libs gtk+-3.0` \
	$(EXTRA_CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o $(EXES)
