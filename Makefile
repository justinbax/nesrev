# Example output (WIN32, default mode):
# gcc -Wall -DWIN32 -Iinclude -c -o bin/main.o src/main.c
# gcc -Wall -DWIN32 -o bin/nesrev bin/main.o -Llib -Llib/win32 -lglfw3 -lopengl32 -lgdi32

CC = gcc
CCFLAGS = -Wall

SRCDIR = src
BINDIR = bin
INCLUDEDIR = include
# BASELIBDIR is used to avoid self-references by appending $(LIBDIR)/platform to itself
BASELIBDIR = lib
LIBDIR = $(BASELIBDIR)

# All source files found within src/ with corresponding object files
SRCFILES = $(wildcard $(SRCDIR)/*.c)
OBJFILES = $(SRCFILES:$(SRCDIR)/%.c=$(BINDIR)/%.o)
EXECUTABLE = $(BINDIR)/nesrev

LIBRARIES = glfw3 glew32 opengl32

ifeq ($(OS),Windows_NT)
	CCFLAGS += -DWIN32 -DGLEW_STATIC
	LIBDIR += $(BASELIBDIR)/win32
	LIBRARIES += gdi32
else
	@echo Compiling for platforms other than Windows is not yet supported.
endif

all: $(EXECUTABLE)

# Appends appropriate debug information and optimization flags for debug and release modes
debug: CCFLAGS += -O0 -g
debug: $(EXECUTABLE)

release: CCFLAGS += -O3
release: $(EXECUTABLE)

$(EXECUTABLE): $(OBJFILES)
	$(CC) $(CCFLAGS) -o $(EXECUTABLE) $(OBJFILES) $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(LIBRARIES))

$(BINDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CCFLAGS) -I$(INCLUDEDIR) -c -o $@ $<

.phony: all debug release