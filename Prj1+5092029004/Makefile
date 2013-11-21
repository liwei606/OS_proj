CC= gcc
CFLAGS= -O2 -Wall
OUTPUT= copy shell single multi

all: $(OUTPUT)
	###### Build all successfully #####

copy: copy.c
	$(CC) $(CFLAGS) -o copy copy.c

shell: shell.c
	$(CC) $(CFLAGS) -o shell shell.c

single: single.c
	$(CC) $(CFLAGS) -o single single.c

multi: multi.c
	$(CC) $(CFLAGS) -o multi multi.c -lpthread

test: multi
	##### Benchmark multithreaded matrix multiplication #####
	./multi-benchmark.sh

clean:
	rm -f $(OUTPUT)
	rm -rf multibench

