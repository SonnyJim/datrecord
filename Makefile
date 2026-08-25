# Target executables
TARGET1 = datrecord
TARGET2 = verifydat

# Source files
SRCS1   = datrecord.c record.c
OBJS1   = $(SRCS1:.c=.o)

SRCS2   = verifydat.c
OBJS2   = $(SRCS2:.c=.o)

ALL_OBJS = $(OBJS1) $(OBJS2)

# Compiler & Linker flags for SGI IRIX
CC      = cc
CFLAGS  = -O2
LIBS1    = -ldataudio
LIBS2    = -ldataudio -lmediad

# Default rule: build both targets
all: $(TARGET1) $(TARGET2)

# Compile target 1 (datrecord)
$(TARGET1): $(OBJS1)
	$(CC) $(CFLAGS) -o $@ $(OBJS1) $(LIBS1)

# Compile target 2 (verifydat)
$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(LIBS2)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(TARGET1) $(TARGET2) $(ALL_OBJS) core

.PHONY: all clean
