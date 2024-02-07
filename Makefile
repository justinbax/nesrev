CC = gcc
CCFLAGS = -Wall

RM = rm
RMFLAGS = 

SRCDIR = src
BINDIR = bin
INCLUDEDIR = include
# BASELIBDIR is used to avoid self-references by appending $(LIBDIR)/{platform} to itself
BASELIBDIR = lib
LIBDIR = $(BASELIBDIR)

# All source and header files found within src/ with corresponding object files for .c files
SRCFILES = $(wildcard $(SRCDIR)/*.c)
OBJFILES = $(SRCFILES:$(SRCDIR)/%.c=$(BINDIR)/%.o)
EXECUTABLE = $(BINDIR)/nesrev
HEADFILES = $(wildcard $(SRCDIR)/*.h)

LIBRARIES = portaudio glfw3 glew32 opengl32

ifeq ($(OS),Windows_NT)
	CCFLAGS += -D_WIN32 -DGLEW_STATIC
	LIBDIR += $(BASELIBDIR)/win32
	LIBRARIES += gdi32 winmm ole32 setupapi
	RM = del
else
	UNAME = $(shell uname -s)
	RMFLAGS = -f
	ifeq ($(UNAME),Linux)
		CCFLAGS += -D_LINUX
		LIBDIR += $(BASELIBDIR)/linux
		LIBRARIES = portaudio glfw GLEW GL
	endif
endif

all: $(EXECUTABLE)

# Appends appropriate debug information and optimization flags for debug and release modes
debug: CCFLAGS += -O0 -g
debug: $(EXECUTABLE)

release: CCFLAGS += -O2
release: $(EXECUTABLE)

clean:
	$(RM) $(RMFLAGS) $(OBJFILES)
	$(RM) $(RMFLAGS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJFILES)
	$(CC) $(CCFLAGS) -o $(EXECUTABLE) $(OBJFILES) $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(LIBRARIES))

# Absolute magic
$(BINDIR)/%.o: $(SRCDIR)/%.c $(HEADFILES)
	$(CC) $(CCFLAGS) -I$(INCLUDEDIR) -c -o $@ $<

.phony: all debug release clean