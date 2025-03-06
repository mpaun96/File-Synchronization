# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Werror -O2 -g

# Libraries
LIBS = -lpthread

# Source files
SRCS = client.c diff.c glthread.c main.c types.c

# Object files (automatically derived from source files)
OBJS = $(SRCS:.c=.o)

# Output executable
TARGET = a.out

# Default target (build the project)
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	@echo "Linking object files..."
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Pattern rule to compile .c files into .o files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean the build directory
clean:
	@echo "Cleaning up..."
	rm -f $(OBJS) $(TARGET)
	@echo "Clean complete."