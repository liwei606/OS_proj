#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>

int main(int argc, char *argv[]) {
	int s, t;
	struct sockaddr_in serv_addr;
	struct hostent *host;
	char str[100];
	
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Socket error\n");
		exit(1);
	}
	
	printf("Trying to connect...\n");
	serv_addr.sin_family = AF_INET;
	host				 = gethostbyname("localhost"); 
	serv_addr.sin_port	 = htons(atoi(argv[1]));
	memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);
	if (connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
		fprintf(stderr, "Connect error\n");
		exit(1);
	}	
	printf("Connection with file system is established\n");	
	while (printf(">"), fgets(str, 100, stdin), !feof(stdin)) {
		if (send(s, str, strlen(str), 0) == -1) {
			fprintf(stderr, "Send error\n");
			exit(1);
		}	
		if ((t = recv(s, str, 100, 0)) > 0) {
			str[t] = '\0';
			printf(str);
		} else {
			if (t < 0) fprintf(stderr, "Receive error\n");
			else fprintf(stderr, "Closed connnection\n");
			exit(1);
			break;
		}
	}	
	close(s);
	return 0;
}
