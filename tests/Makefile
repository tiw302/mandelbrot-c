CC = gcc
CFLAGS = -O3 -Wall -Wextra -I../src/core -I../include
LDFLAGS = -lm

SRC = test_math.c ../src/core/mandelbrot.c ../src/core/julia.c
OBJ = $(SRC:.c=.o)
TARGET = test_math

# detect AVX2 support
AVX2_SUPPORT := $(shell gcc -march=native -dM -E - < /dev/null | grep -q "__AVX2__" && echo 1 || echo 0)
ifeq ($(AVX2_SUPPORT),1)
	CFLAGS += -mavx2 -mfma
endif

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o
