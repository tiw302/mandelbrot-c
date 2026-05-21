CC = gcc
CFLAGS = -O3 -Wall -Wextra -I../src/core -I../include -I../third_party/simd-f128
LDFLAGS = -lm

SRC = test_math.c ../src/core/mandelbrot.c ../src/core/julia.c ../src/core/burning_ship.c
OBJ = $(SRC:.c=.o)
TARGET = test_math

# detect avx2 support at compile time
AVX2_SUPPORT := $(shell gcc -march=native -dM -E - < /dev/null | grep -q "__AVX2__" && echo 1 || echo 0)
ifeq ($(AVX2_SUPPORT),1)
	CFLAGS += -mavx2 -mfma
endif

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# build with 128-bit support for testing high-precision kernels
test_f128: test_math.c ../src/core/mandelbrot.c ../src/core/julia.c ../src/core/burning_ship.c
	$(CC) $(CFLAGS) -DUSE_SIMD_F128 -mavx2 -mfma $^ -o test_math_f128 $(LDFLAGS)
	./test_math_f128

clean:
	rm -f $(TARGET) test_math_f128 *.o
