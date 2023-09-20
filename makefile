CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

all: hw2 


hw2: hw2.c
	$(CC) $(CFLAGS) -o hw2 hw2.c
	mkdir log


valgrind: $(hw2)
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all --gen-suppressions=all -s ./hw2

clean:
	rm -f hw2 
	rm -rf log