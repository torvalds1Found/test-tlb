# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2 -std=c11
TARGET = test-tlb

# Targets and rules
all: $(TARGET)

$(TARGET): test-tlb.o
	$(CC) $(CFLAGS) -o $(TARGET) test-tlb.o

test-tlb.o: test-tlb.c
	$(CC) $(CFLAGS) -c test-tlb.c

run: $(TARGET)
	@echo "Running tests with various sizes and strides..."
	@for size in 1M 2M 4M 8M 16M 32M 64M 128M 256M; do \
	    for stride in 4k 8k 16k 32k 64k 128k 256k 512k 1M; do \
	        echo "Running with size $$size and stride $$stride:"; \
	        ./$(TARGET) $$size $$stride; \
	    done; \
	done

clean:
	rm -f $(TARGET) test-tlb.o
