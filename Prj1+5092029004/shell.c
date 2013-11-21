/*****************************************************************
* Problem2 description:
*
* 1.This is a shell-like program that illustrates how Linux spawns
* processes. The shell program acts as a server. When the server
* is running, clients are allowed to connect to the server and
* run the commands. The communition between client and server is
* implemented using Internet-domain socket.
*
* 2.The program can support more than one client.
******************************************************************/

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* maximum number of characters of a command line */
#define MAX_LINE	(200)

/* maximum pending connections */
#define BACKLOG		(200)


void rtrim(char *s) {
	size_t i;
	i = strlen(s);
	while (s[i-1] == '\r' || s[i-1] == '\n' || s[i-1] == ' ') {
		i--;
	}
	s[i] = 0;
}


int main(int argc, char *argv[]) {
    int       list_s;                /*  listening socket          */
    int       conn_s;                /*  connection socket         */
    short int port;                  /*  port number               */
    struct    sockaddr_in servaddr;  /*  socket address structure  */
    char      buffer[MAX_LINE];      /*  character buffer          */
    char      working_dir[MAX_LINE]; /*  current working directory */
    char     *endptr;                /*  for strtol()              */
	pid_t     cpid;

	int ncommand;
	char* args[MAX_LINE][MAX_LINE];
	int infd, outfd;
	int cmdi, argi, bufpos, buflen, endpos, arglen, i;
	
	int seq = 0;

    /*  Get port number from the command line, and
        set to default port if no arguments were supplied  */

    if (argc == 2) {
		port = strtol(argv[1], &endptr, 0);
		if (*endptr) {
			fprintf(stderr, "Invalid port number.\n");
			exit(EXIT_FAILURE);
		}
    } else {
		fprintf(stderr, "Invalid arguments.\n");
		exit(EXIT_FAILURE);
    }

	
    /*  Create the listening socket  */

    if ((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Error creating listening socket.\n");
		exit(EXIT_FAILURE);
    }


    /*  Set all bytes in socket address structure to
        zero, and fill in the relevant data members   */

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);


    /*  Bind our socket addresss to the 
	listening socket, and call listen()  */

    if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "Error calling bind()\n");
		exit(EXIT_FAILURE);
    }

    if (listen(list_s, BACKLOG) < 0) {
		fprintf(stderr, "Error calling listen()\n");
		exit(EXIT_FAILURE);
    }

    
    /*  Enter an infinite loop to respond
        to client requests and echo input  */

    for (;;) {

		/*  Wait for a connection, then accept() it  */

		if ((conn_s = accept(list_s, NULL, NULL)) < 0) {
			fprintf(stderr, "Error calling accept()\n");
			exit(EXIT_FAILURE);
		}
		
		seq++;
		
		if ((cpid = fork()) < 0) {
			fprintf(stderr, "Error calling fork()\n");
			exit(EXIT_FAILURE);
		}
		
		if (cpid == 0) {
			char aname[MAX_LINE], bname[MAX_LINE];
			/*  child process handles client requests  */
			
			printf("New child process (seq=%d)\n", seq);
			
			sprintf(aname, "%da.txt", seq);
			sprintf(bname, "%db.txt", seq);
			
			dup2(conn_s, STDIN_FILENO);
			dup2(conn_s, STDOUT_FILENO);
			dup2(conn_s, STDERR_FILENO);
			
			printf("Welcome to my shell!\r\n"); fflush(stdout);
			
			for (;;) {
				printf("%s$ ", getcwd(working_dir, MAX_LINE)); fflush(stdout);
				if (!fgets(buffer, MAX_LINE, stdin)) break;
				rtrim(buffer);
				if (strcmp(buffer, "exit") == 0) break;
				cmdi = 0;
				argi = 0;
				bufpos = 0;
				buflen = strlen(buffer);
				while (bufpos < buflen) {
					while (buffer[bufpos] == ' ') bufpos++;
					if (buffer[bufpos] == '|') {
						args[cmdi][argi] = NULL;
						argi = 0;
						cmdi++;
						bufpos++;
						continue;
					}
					endpos = bufpos + 1;
					while (endpos < buflen) {
						if (buffer[endpos] == ' ' || buffer[endpos] == '|') break;
						endpos++;
					}
					arglen = endpos - bufpos;
					args[cmdi][argi] = malloc(arglen + 1);
					strncpy(args[cmdi][argi], buffer + bufpos, arglen);
					args[cmdi][argi][arglen] = 0;
					argi++;
					bufpos = endpos;
				}
				args[cmdi][argi] = NULL;
				ncommand = cmdi + 1;
				
				/*fprintf(stderr, "***** DEBUG BEGIN *****\n");
				fprintf(stderr, "ncommand=%d\n", ncommand);
				for (i = 0; i < ncommand; i++) {
					for (argi = 0; args[i][argi] && argi < 10; argi++)
						fprintf(stderr, "\"%s\" ", args[i][argi]);
					fprintf(stderr, "\n");
				}
				fprintf(stderr, "***** DEBUG  END  *****\n");*/
				
				for (i = 0; i < ncommand; i++) {
					int fork_pid;
					if (i == 0) {
						infd = conn_s;
					} else {
						infd = open(i%2?aname:bname, O_RDONLY);
					}
					if (i == ncommand-1) {
						outfd = conn_s;
					} else {
						outfd = open(i%2?bname:aname, O_WRONLY | O_CREAT, 0777);
					}
					fork_pid = fork();
					if (fork_pid < 0) {
						fprintf(stderr, "Error calling fork() when i=%d,ncommand=%d\n", i, ncommand);
						exit(EXIT_FAILURE);
					}
					if (fork_pid == 0) {
						/* child */
						dup2(infd, STDIN_FILENO);
						dup2(outfd, STDOUT_FILENO);
						fprintf(stderr, "Error calling execvp() return value is %d\n", execvp(args[i][0], args[i]));
						exit(EXIT_FAILURE);
					} else {
						/* parent */
						wait(NULL);
						//int status;
						//pid_t child = wait(&status);
						//fprintf(stderr, "Process %d (pid %d) finished with status %d\n", i, child, status);
						if (infd != conn_s) close(infd);
						if (outfd != conn_s) close(outfd);
						continue;
					}
				}
				fflush(stdout);
				fflush(stderr);
				unlink(aname);
				unlink(bname);
			}
			
			printf("exit\r\n"); fflush(stdout);
			
			/*  Close the connected socket  */
			
			if (close(conn_s) < 0) {
				fprintf(stderr, "Error calling close()\n");
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		}
		/*  parent process continues  */
    }
}

