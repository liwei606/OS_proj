CC= gcc
CFLAGS= -O2 -Wall
OUTPUT=client disk fs

all: $(OUTPUT)
	###### Build successfully #####
	
client: client.c
	$(CC) $(CFLAGS) -o client client.c

disk: disk.c
	$(CC) $(CFLAGS) -o disk disk.c

fs: fs.c
	$(CC) $(CFLAGS) -o fs fs.c -lm

clean:
	rm -f $(OUTPUT)
