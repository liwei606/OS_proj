/*****************************************************
* Problem1 description:
*
* 1.This is program that forks two processes to copy
* file, one for reading from a source file, and the
* other for writing into a destination file. These two
* processes communicate using pipe system call. The buffer
* is used to copy faster and with the different size of 
* buffersize the program will have different performance.
******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char** argv)
{
  int pipefd[2];
  pid_t cpid;
  char *buffer;
  char *infile;
  char *outfile;
  int bufsize, size;
  
  if (argc < 4)
  {
  	fprintf(stderr, "Usage: %s <infile> <outfile> <bufsize>\n", argv[0]);
  	exit(EXIT_FAILURE);
  }
  
  infile = argv[1];
  outfile = argv[2];
  bufsize = atoi(argv[3]);
  buffer = malloc(bufsize);
  
  fprintf(stderr, "in=%s,out=%s,bufsize=%d\n", infile, outfile, bufsize);
  
  if (pipe(pipefd) == -1)
  {
  	perror("pipe");
  	exit(EXIT_FAILURE);
  }
  
  cpid = fork();
  if (cpid == -1)
  {
  	perror("fork");
  	exit(EXIT_FAILURE);
  }
  
  if (cpid == 0)
  {
  	// child
  	FILE *fout;
  	int total = 0;
  	close(pipefd[1]); // close write end
  	fout = fopen(outfile, "w");
  	while ((size = read(pipefd[0], buffer, bufsize)) > 0)
  	{
  	  //fprintf(stderr, "read\n");
  	  total += size;
  	  fwrite(buffer, size, 1, fout);
  	  //fprintf(stderr, "fwrite\n");
  	}
  	fclose(fout);
  	close(pipefd[0]);
  	fprintf(stderr, "child exits: total=%d\n", total);
  	_exit(EXIT_SUCCESS);
  }
  else
  {
  	// parent
  	FILE *fin;
  	int total = 0;
  	close(pipefd[0]); // close read end
  	fin = fopen(infile, "r");
  	while ((size = fread(buffer, 1, bufsize, fin)) > 0)
  	{
  	  //fprintf(stderr, "fread\n");
  	  total += size;
  	  if (write(pipefd[1], buffer, size) < 0)
  	  {
  	    //fprintf(stderr, "write\n");
  	  	perror("write");
  	  	exit(EXIT_FAILURE);
  	  }
  	}
  	fclose(fin);
  	close(pipefd[1]);
  	wait(NULL); // wait for child
  	fprintf(stderr, "parent exits: total=%d\n", total);
  	exit(EXIT_SUCCESS);
  }
}

