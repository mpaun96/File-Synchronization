# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Werror -O2 -g

# Libraries
LIBS = -lpthread

# Source files
SRCS = file_ops.c send_client.c Server.c CRUDshared_array.c types.c diff.c glthread.c main.c

# Object files (manually listing them)
OBJS = $(SRCS:.c=.o)  # Converts .c files to .o files

# Output executable
TARGET = a.out

# Default target (build the project)
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	@echo "Linking object files..."
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Rule indicating that object files depend on source files
$(OBJS): $(SRCS)

# Pattern rule to compile .c files into .o files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean the build directory
clean:
	@echo "Cleaning up..."
	rm -f $(OBJS) $(TARGET)
	@echo "Clean complete."