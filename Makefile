# Target executable
TARGET = datrecord

# Source files
SRCS   = datrecord.c record.c
OBJS   = $(SRCS:.c=.o)

# Compiler & Linker flags for SGI IRIX
CC     = cc
CFLAGS = -O2
LIBS   = -ldataudio

# Default rule
all: $(TARGET)

# Compile target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJS) core

.PHONY: all clean
