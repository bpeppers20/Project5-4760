//Name: Bryant Peppers
//Class: CS4760
//Project5: Resource Manager

#include<stdio.h>
#include<fcntl.h>
#include<getopt.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<string.h>
#include<stdlib.h>
#include "scheduler.h" // Header file call to seperate file since this code is a lot to write

// Function Protoype
void help();

int main(int argc, char * argv[])
{      
	int opt, x = 0, total = 18, hFlag = 0, verbFlag = 0;
	char * outfile = "output.txt";
	//Assigning default case name.
	if((opt = getopt(argc, argv, "-ho:n:v")) != -1)
	{
		do{
			x++;
			switch(opt){
			case 'h':
				help();
				hFlag = 1;
				break;
			case 'o':
				outfile = argv[x+1];
				break;
			case 'n':
				total = atoi(argv[x+1]);
				if (total > 18)
					total = 18;
				break;
			case 'v':
				verbFlag = 1; // TO enable verbose option
				break;
			//Case handles last run through of getopt.
			case 1:
				break;
			}
		} while((opt = getopt(argc, argv, "-ho:n:v")) != -1);
	}
	if (hFlag == 0)
		scheduler(outfile, total, verbFlag);
	return 0; 
}
void help()
{
	printf("This program will mimic the OS's resource manager and output the results in a file\n");
	printf("Options:\n");
	printf("-h: Prints this help message.\n");
	printf("-n: Number of children to be created.  The maximum is 18.\n");
	printf("-o: Name of the output file.The default is output.txt.\n");
}

