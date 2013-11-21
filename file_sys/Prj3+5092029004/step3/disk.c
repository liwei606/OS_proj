#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>

#define sector_size 256
#define buffersize 4096

typedef struct {
	char *a;
} DiskMap;

typedef struct {
	int num_cylinder;
	int num_sector;
} DiskGeometry;

typedef struct {
	DiskMap *map;
	DiskGeometry geometry;
	double track_time;
} Disk;

DiskMap* disk_map_open(char* disk_storage, int *fd, int length) {
	DiskMap *map;
	map = malloc(sizeof(DiskMap));
	int res;
	off_t result;

	*fd = open(disk_storage, O_RDWR | O_SYNC | O_CREAT, S_IWRITE | S_IREAD);
	if (*fd < 0) {
		fprintf(stderr, "Dist_storage open error!\n");
		exit(-1);
	}
	result = lseek(*fd, length - 1, SEEK_SET);
	if (result < 0) {
		fprintf(stderr, "File strench the length with lseek error!\n");
		close(*fd);
		exit(-1);
	}
	map->a = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE, *fd, 0);
	if ((res = write(*fd, "", 1)) != 1) {
		fprintf(stderr, "Error writing last byte!\n");
		close(*fd);
		exit(-1);
	}	
	return map;
}

void disk_map_read(DiskMap *map, int c, int s, int offset, char data[256]) {
	int size;
	for (size = 0; size < sector_size; ++size) {
		data[size] = map->a[c * s * sector_size + offset + size];
	}
}

void disk_map_write(DiskMap *map, int c, int s, int offset, char data[256]) {
	strcpy(map->a + c * s * sector_size + offset, data);
}

void disk_map_close(DiskMap *map, int *fd, int length) {
	munmap(map->a, length);
	close(*fd);
}

Disk* disk_open(char* disk_storage, int cylinder, int sector, double tracktime, int *fd, int length) {
	Disk *disk;
	disk = malloc(sizeof(Disk));
	disk->map = disk_map_open(disk_storage, fd, length);
	disk->geometry.num_cylinder = cylinder;
	disk->geometry.num_sector = sector;
	disk->track_time = tracktime;
	return disk;
}

DiskGeometry disk_information(Disk *disk) {
	return disk->geometry;
}

int disk_read(Disk *disk, int c, int s, int offset, char data[256]) {
	if (c >= disk->geometry.num_cylinder || s >= disk->geometry.num_sector) return 0;
	else {
		disk_map_read(disk->map, c, s, offset, data);
		return 1;
	}
}

int disk_write(Disk *disk, int c, int s, int offset, char data[256]) {
	if (c >= disk->geometry.num_cylinder || s >= disk->geometry.num_sector || sizeof(data) > sector_size || strcmp(data, "") == 0) return 0;
	else {
		disk_map_write(disk->map, c, s, offset, data);
		return 1;
	}
}

void disk_close(Disk *disk, int *fd, int length) {
	disk_map_close(disk->map, fd, length);
}


int main(int argc, char *argv[]) {
	Disk *disk;
	int length, *fd; fd = malloc(sizeof(int));
	length = atoi(argv[1]) * atoi(argv[2]) * sector_size;
	if (argc != 6) { 
		fprintf(stderr, "Parameters error!\n");
		exit(EXIT_FAILURE);
	}
	int cylinder_num, sector_num;
	cylinder_num = atoi(argv[1]); sector_num = atoi(argv[2]);
	disk = disk_open(argv[4], cylinder_num, sector_num, atoi(argv[3]), fd, length);
	char instr[100], ins[100];
	char *data;	// To store the read or write data
	data = malloc(sizeof(char) * sector_size);
	FILE *outfile;
	int cylinder, sector, last_cylinder = 0, page_num, offset;
	double tracktime;
	int sd, file_sock;
	struct sockaddr_in name;
	
	// disk server socket
	sd = socket(AF_INET, SOCK_STREAM, 0);
    name.sin_family		 = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    name.sin_port		 = htons(atoi(argv[5]));
    
    if (bind(sd, (struct sockaddr *) &name, sizeof(name)) == -1) {
    	fprintf(stderr, "Bind error\n");
    	exit(1);
    }
    if (listen(sd, 1) == -1) {
    	fprintf(stderr, "Listen error\n");
    	exit(1);
    }
   
    if ((file_sock = accept(sd, 0, 0)) == -1) {
    	fprintf(stderr, "Accept error\n");
    	exit(1);
    }
    printf("Connection with file system is established!\n");
    outfile = fdopen(file_sock, "w");
	
	for (; ;) {
		recv(file_sock, ins, 100, 0);
		if (sscanf(ins, "%s", instr) != 1) fprintf(stderr, "Instruction error!\n");
		// For instruction I
		if (strcmp(instr, "I") == 0) {
			fprintf(outfile, "%d %d\n", disk->geometry.num_cylinder, disk->geometry.num_sector);
		}
		// For instruction R c s
		else if (strcmp(instr, "R") == 0) {
			if (sscanf(ins, "%*s%d%d", &page_num, &offset) != 2) fprintf(stderr, "Instruction error!\n");
			cylinder = page_num / sector_num;
			sector = page_num % sector_num;
			if (disk_read(disk, cylinder, sector, offset, data) == 0) fprintf(outfile, "No\n");
			else {
				fprintf(outfile, "Yes %s\n", data);
				if (send(file_sock, data, strlen(data), 0) == -1) {
					fprintf(stderr, "Send error\n");
					exit(1);
				}
				tracktime = abs(cylinder - last_cylinder) * disk->track_time;
				fprintf(outfile, "The track to track delay is: %f us\n", tracktime);			// To output the track to track delay
				last_cylinder = cylinder;
			}
		}
		// For instruction W c s d
		else if (strcmp(instr, "W") == 0) {
			strcpy(data, "");
			if (sscanf(ins, "%*s%d%d%[^\n]", &page_num, &offset, data) != 3) fprintf(stderr, "Instruction error!\n");
			cylinder = page_num / sector_num;
			sector = page_num % sector_num;
			if (disk_write(disk, cylinder, sector, offset, data) == 0) fprintf(outfile, "No\n");
			else {
				fprintf(outfile, "Yes\n");
				tracktime = abs(cylinder - last_cylinder) * disk->track_time;
				fprintf(outfile, "The track to track delay is: %f us\n", tracktime);			// To output the track to track delay
				last_cylinder = cylinder;
			}
		}
		// For instruction E
		else if (strcmp(instr, "E") == 0) {
			fprintf(outfile, "Goodbye!\n");
			fclose(outfile);
			break;
		}
	}
	close(file_sock);
	close(sd);
	disk_close(disk, fd, length);
	exit(EXIT_SUCCESS);
}
