# Mandelbrot Set Renderer - Cross-Platform Makefile
# Works on Linux, macOS, and Windows (with MSYS2/MinGW)

CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native -Iinclude
LDFLAGS = -lm -lz

# Detect operating system
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

# Windows detection
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    TARGET := mandelbrot.exe
else
    DETECTED_OS := $(UNAME_S)
    TARGET := mandelbrot
endif

# Platform-specific configuration
ifeq ($(DETECTED_OS),Linux)
    CFLAGS  += $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null || echo "-I/usr/include/SDL2")
    LDFLAGS += $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null || echo "-lSDL2 -lSDL2_ttf")
    LDFLAGS += -lpthread
endif

ifeq ($(DETECTED_OS),Darwin)
    SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null)
    SDL_LIBS   := $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null)

    ifeq ($(SDL_CFLAGS),)
        HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
        CFLAGS  += -I$(HOMEBREW_PREFIX)/include -I$(HOMEBREW_PREFIX)/include/SDL2
        LDFLAGS += -L$(HOMEBREW_PREFIX)/lib
        SDL_LIBS := -lSDL2 -lSDL2_ttf
    endif

    CFLAGS  += $(SDL_CFLAGS)
    LDFLAGS += $(SDL_LIBS) -lpthread
endif

ifeq ($(DETECTED_OS),Windows)
    CFLAGS  += $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null || echo "-I/mingw64/include/SDL2")
    LDFLAGS += $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null || echo "-lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf")
    LDFLAGS += -lpthread -mwindows
endif

# Source and object files
SRCDIR  = src
OBJDIR  = obj
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/mandelbrot.c \
          $(SRCDIR)/renderer.c \
          $(SRCDIR)/julia.c \
          $(SRCDIR)/screenshot.c
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

# Default target
.PHONY: all
all: banner $(OBJDIR) $(TARGET)
	@echo ""
	@echo "✓ Build complete!"
	@echo ""
	@echo "Run with: ./$(TARGET)"
	@echo ""

# Create object directory
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Build executable
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	@$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Banner
.PHONY: banner
banner:
	@echo "════════════════════════════════════════════════════════"
	@echo "  Mandelbrot Set Renderer"
	@echo "  Platform: $(DETECTED_OS)"
	@echo "════════════════════════════════════════════════════════"
	@echo ""

# Clean
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OBJDIR) $(TARGET)
	@echo "✓ Clean complete!"

# Install (Linux/macOS only)
.PHONY: install
install: $(TARGET)
ifeq ($(DETECTED_OS),Windows)
	@echo "Install target not supported on Windows"
else
	@echo "Installing to /usr/local/bin..."
	@install -m 755 $(TARGET) /usr/local/bin/
	@echo "✓ Installation complete!"
endif

# Uninstall
.PHONY: uninstall
uninstall:
ifeq ($(DETECTED_OS),Windows)
	@echo "Uninstall target not supported on Windows"
else
	@echo "Removing from /usr/local/bin..."
	@rm -f /usr/local/bin/$(TARGET)
	@echo "✓ Uninstall complete!"
endif

# Debug build
.PHONY: debug
debug: CFLAGS = -Wall -Wextra -g -Iinclude
debug: CFLAGS += $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null || echo "")
debug: clean $(TARGET)
	@echo "✓ Debug build complete!"

# Run
.PHONY: run
run: $(TARGET)
	@./$(TARGET)

# Help
.PHONY: help
help:
	@echo "Mandelbrot Set Renderer - Makefile Targets"
	@echo ""
	@echo "  make          - Build the program (optimized)"
	@echo "  make run      - Build and run"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make install  - Install to /usr/local/bin (requires sudo)"
	@echo "  make uninstall- Uninstall from /usr/local/bin"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Current configuration:"
	@echo "  OS:       $(DETECTED_OS)"
	@echo "  Compiler: $(CC)"
	@echo "  Target:   $(TARGET)"
	@echo "  Obj Dir:  $(OBJDIR)"

# Dependencies
$(OBJDIR)/main.o:       include/config.h include/mandelbrot.h include/julia.h \
                        include/renderer.h include/screenshot.h
$(OBJDIR)/mandelbrot.o: include/mandelbrot.h include/config.h
$(OBJDIR)/julia.o:      include/julia.h include/mandelbrot.h include/config.h
$(OBJDIR)/renderer.o:   include/renderer.h include/mandelbrot.h include/julia.h \
                        include/config.h
$(OBJDIR)/screenshot.o: include/screenshot.h
