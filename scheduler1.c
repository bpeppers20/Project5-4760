#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h>

#define resSize 20
#define available 10

//Signal handler to catch ctrl-c
static volatile int keepRunning = 1;
static int deadlockDetector(int, int[resSize][2], int[][resSize], int[], FILE *);

void intHandler(int dummy) 
{
    keepRunning = 0;
}

// Important Structures
struct clock
{
	unsigned long nano;
	unsigned int sec;
	int initialResource[resSize];
};

struct mesg_buffer
{
	long mesg_type;
	unsigned long processNum; // Process id Number
	int request[resSize]; // Array for processes that made request
	int granted[resSize]; // Array for processes that have been granted
}message;

// Process Scheduler/ Resource Handler
void scheduler(char* outfile, int total, int verbFlag)
{
	unsigned int quantum = 500000;
	int alive = 0, totalSpawn = 0, msgid, msgid1, msgid2, shmID, timeFlag = 0, i = 0, totalFlag = 0, pid[total], status, request[total][resSize], grantFlag = 0, deadlockFlag = 0, processStuck = 0, lastCheck = 0, fileFlag = 0;
	int resource[resSize][2], resOut[total][resSize]; // To be graphed
	unsigned long increment, timeBetween, cycles = 0;
	char * parameter[32], parameter1[32], parameter2[32], parameter3[32], parameter4[32], parameter5[32];
	//Pointer for the shared memory timer
	struct clock * shmPTR;
	struct clock launchTime;
	
	//Time variables for the time out function
	time_t when, when2;
	//File pointers for input and output, respectively
	FILE * outPut;
	//Key variable for shared memory access.
	unsigned long key, msgKey, msgKey1, msgKey2;
	srand(time(0));
	timeBetween = (rand() % 100000000) + 1000000;
	key = rand();
	
	//Create Message Queues for sync.
	msgKey = ftok("child.c", 65);
	msgKey1 = ftok("child.c", 67);
	msgKey2 = ftok("child.c", 69);
	msgid = msgget(msgKey, 0777 | IPC_CREAT);
	msgid1 = msgget(msgKey1, 0777 | IPC_CREAT);
	msgid2 = msgget(msgKey2, 0777 | IPC_CREAT);
	message.mesg_type = 1;
	

	//Setting initial time for later check.
	time(&when);
	outPut = fopen(outfile, "w");
	//Check for file error.
	if (outPut == NULL) // error check
	{
		perror("Error");
		printf("Output file could not be created.\n");
		exit(EXIT_SUCCESS);
	}
	printf("\n");
	int k, j; // Loop variables
	for(k = 0; k < total; k++)
	{
		for(j = 0; j < resSize; j++)
		{
			resOut[k][j] = 0;
			request[k][j] = 0;
		}
	}
	
	//Storing the clock in shared memory, setting initial timer state to 0.
	shmID = shmget(key, sizeof(struct clock), IPC_CREAT | IPC_EXCL | 0777);
	shmPTR = (struct clock *) shmat(shmID, (void *) 0, 0);
	shmPTR[0].sec = 0;
	shmPTR[0].nano = 0;
	
	//initializing resource arrays
	for(k = 0; k < resSize; k++)
	{
		resource[k][0] = (rand() % available) + 1;
		shmPTR[0].initialResource[k] = resource[k][0];
		if (rand() % 100 < 20)
		{
			resource[k][1] = 1;
		}
		else
		{
			resource[k][1] = 0;
		}
		message.request[k] = 0;
		message.granted[k] = 0;
	}
	launchTime.sec = 0;
	launchTime.nano = timeBetween;
	//Initializing pids to -1 
	for(k = 0; k < total; k++)
	{
		pid[k] = -1;
	}
	
	//Call to signal handler for ctrl-c
	signal(SIGINT, intHandler);
	
	increment = (rand() % 5000000) + 25000000;
	
	//While loop keeps running until all children are spawned, ctrl-c, or time is reached.
	while((i < total) && (keepRunning == 1) && (timeFlag == 0) && fileFlag == 0)
	{
		time(&when2);
		if (when2 - when > 10)
		{
			timeFlag = 1;
		}
		if (ftell(outPut) > 10000000)
		{
			fileFlag = 1;
		}
		//Incrementing the timer.
		shmPTR[0].nano += increment;
		if(shmPTR[0].nano >= 1000000000)
		{
			shmPTR[0].sec += 1;
			shmPTR[0].nano -= 1000000000;
		}
		if (shmPTR[0].sec > lastCheck)
		{
			lastCheck = shmPTR[0].sec;
			printf("Checking for deadlock, %d alive.\n", alive);
			deadlockFlag = deadlockDetector(total, resource, request, pid, outPut); // Check for deadlock
		}
		//If statement to spawn child if timer has passed its birth time.
		if((shmPTR[0].sec > launchTime.sec) || ((shmPTR[0].sec == launchTime.sec) && (shmPTR[0].nano > launchTime.nano)))
		{
			if((pid[i] = fork()) == 0)
			{
				//Converting key, shmID and life to char* for passing to exec.
				sprintf(parameter1, "%li", key);
				sprintf(parameter2, "%li", msgKey);
				sprintf(parameter3, "%li", msgKey1);
				sprintf(parameter4, "%li", msgKey2);
				sprintf(parameter5, "%d", i+1);
				srand(getpid() * (time(0) / 3));
				char * args[] = {parameter1, parameter2, parameter3, parameter4, parameter5, NULL};
				execvp("./child\0", args);
			}
			//Updating launch time for next child.
			launchTime.sec = shmPTR[0].sec;
			launchTime.nano = shmPTR[0].nano;
			launchTime.nano += timeBetween;
			if(launchTime.nano >= 1000000000)
			{
				launchTime.sec += 1;
				launchTime.nano -= 1000000000;
			}
			alive++;
			totalSpawn++;
			i++;
			cycles++;
		}
		//Checking for message from exiting child
		if (msgrcv(msgid2, &message, sizeof(message), 0, IPC_NOWAIT) !=-1)
		{
			int m;
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0;
			}
			fprintf(outPut, "Parent received dead child message from %li, %d.\n", message.processNum, pid[message.processNum-1]);
			for(k = 0; k < resSize; k++)
			{
			//Adding child's resources back to pool if not shareable
				if (resource[k][1] == 0)
				{
					resource[k][0] = resource[k][0] - message.request[k];
				}
				//printf("Parent receiving %d resource %d from dying child %li. %d remain.\n", message.request[k], k, message.processNum, resource[k][0]);
				request[message.processNum-1][k] = 0;
			}
			alive--;
			pid[message.processNum-1] = -1;
			long s;
			for (s = 0; s < totalSpawn; s++)
			{
				for (j = 0; j < resSize; j++){
					if ((request[s][j] > 0) && (request[s][j] < resource[j][0])) // Fulfil request if "OS" has the resource
					{
						grantFlag = 1;
						message.granted[j] = request[k][j];
						if (resource[s][1] == 0)
						{
							resource[j][0] = resource[j][0] - request[s][j];
						}
						resOut[k][j] = message.granted[j]; // Store the fulfilled request
					}
				}
				if(grantFlag)
				{
					grantFlag = 0;
					message.mesg_type = k+1;
					fprintf(outPut, "Parent granting child %li resources from dead child.\n", message.mesg_type);
					msgsnd(msgid1, &message, sizeof(message), 0);
				}
			}
		}
		if (msgrcv(msgid, &message, sizeof(message), 0, IPC_NOWAIT) !=-1)
		{
			int m; // Loop variable
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0; // Empty resourcees graneted array 
			}
			for(k = 0; k < resSize; k++)
			{
				if(resource[k][0] - message.request[k] >= 0) // Grant resource if can
				{
					grantFlag = 1;
					if (resource[k][1] == 0)
					{
						resource[k][0] = resource[k][0] - message.request[k];
					}
					message.granted[k] = message.request[k];
					message.mesg_type = message.processNum;
					request[message.processNum-1][k] = message.granted[k];
				}
				else
				{
					request[message.processNum-1][k] = message.request[k]; // Store and schedule unfulfilled request
				}
			}
			if(grantFlag == 1)
			{
				fprintf(outPut, "Parent granting child %li resources.\n", message.mesg_type);
				msgsnd(msgid1, &message, sizeof(message), 0);
				grantFlag = 0;
			}
		}
	}
	while(alive > 0 && keepRunning == 1 && deadlockFlag == 0 && timeFlag == 0 && fileFlag == 0)
	{
		time(&when2);
		if (when2 - when > 10)
		{
			timeFlag = 1;
		}
		if (ftell(outPut) > 10000000)
		{
			fileFlag = 1;
		}
		shmPTR[0].nano += increment;
		if(shmPTR[0].nano >= 1000000000)
		{
			shmPTR[0].sec += 1;
			shmPTR[0].nano -= 1000000000;
		}
		if (shmPTR[0].sec > lastCheck)
		{
			lastCheck = shmPTR[0].sec;
			printf("Checking for deadlock, %d alive.\n", alive);
			deadlockFlag = deadlockDetector(total, resource, request, pid, outPut);
		}
		if (msgrcv(msgid2, &message, sizeof(message), 0, IPC_NOWAIT) !=-1)
		{
			int m; // Loop Variable
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0;
			}
			fprintf(outPut, "Parent received dead child message from %li, %d.\n", message.processNum, pid[message.processNum-1]);
			for(k = 0; k < resSize; k++)
			{
				if (resource[k][1] == 0)
				{
					resource[k][0] = resource[k][0] - message.request[k];
				}
				//printf("Parent receiving %d %d resource from dying child %li. %d remain.\n", message.request[k], k, message.processNum, resource[k][0]);
				request[message.processNum-1][k] = 0;
			}
			alive--;
			pid[message.processNum-1] = -1;
			long s;
			for (s = 0; s < totalSpawn; s++)
			{
				for (j = 0; j < resSize; j++)
				{
					if ((request[s][j] > 0) && (request[s][j] < resource[j][0]))
					{
						grantFlag = 1;
						message.mesg_type = s+1;
						message.granted[j] = request[s][j];
						if (resource[s][1] == 0){
							resource[j][0] = resource[j][0] - request[s][j];
						}
						resOut[s][j] = message.granted[j];
					}
				}
				if(grantFlag)
				{
					grantFlag = 0;
					fprintf(outPut, "Parent granting child %li resources from dead child.\n", message.mesg_type);
					msgsnd(msgid1, &message, sizeof(message), 0);
				}
			}
		}
		if (msgrcv(msgid, &message, sizeof(message), 0, IPC_NOWAIT) !=-1)
		{
			int m; // Loop Variable
			for(m = 0; m < resSize; m++)
			{
				message.granted[m] = 0;
			}
			for(k = 0; k < resSize; k++)
			{
				if(resource[k][0] - message.request[k] >= 0)
				{
					grantFlag = 1;
					if (resource[k][1] == 0)
					{
						resource[k][0] = resource[k][0] - message.request[k];
					}
					message.granted[k] = message.request[k];
					message.mesg_type = message.processNum;
					resOut[message.processNum][k] = message.granted[k];
				}
				else
				{
					request[message.processNum-1][k] = message.request[k];
				}
			}
			if (grantFlag == 1)
			{
				fprintf(outPut, "Parent granting child %li resources.\n", message.mesg_type);
				msgsnd(msgid1, &message, sizeof(message), 0);
				grantFlag = 0;
			}
		}
	}
	if(timeFlag == 1)
	{
		printf("Program has reached its allotted time, exiting.\n");
		//fprintf(outPut, "Scheduler terminated at %li nanoseconds due to time limit.\n",  shmPTR[0].timeClock);
	}
	if(totalFlag == 1)
	{
		printf("Program has reached its allotted children, exiting.\n");
		//fprintf(outPut, "Scheduler terminated at %li nanoseconds due to process limit.\n",  shmPTR[0].timeClock);
	}
	if(keepRunning == 0)
	{
		printf("Terminated due to ctrl-c signal.\n");
		//fprintf(outPut, "Scheduler terminated at %li nanoseconds due to ctrl-c signal.\n",  shmPTR[0].timeClock);
	}
	if (fileFlag == 1)
	{
		printf("Terminated due to log file length.\n");
	}
	for (i = 0; i < total; i++)
	{
		if (pid[i] > -1)
		{
			fprintf(outPut, "Killing process %d, %d\n", i+1, pid[i]);
			if (kill(pid[i], SIGINT) == 0)
			{
				usleep(100000);
				kill(pid[i], SIGKILL);
			}
		}
	}
	shmdt(shmPTR);
	shmctl(shmID, IPC_RMID, NULL);
	msgctl(msgid, IPC_RMID, NULL);
	msgctl(msgid1, IPC_RMID, NULL);
	msgctl(msgid2, IPC_RMID, NULL);
	wait(NULL);
	fclose(outPut);
}

static int deadlockDetector(int total, int resources[resSize][2], int requests[total][resSize], int pid[total], FILE * outPut) // To detect and resolve deadlock
{
	int procReq[resSize], deadlocked[total];
	int sysLock = 0, status = 1, processStuck = 0;
	int i, j; // Loop Variables
	for (i = 0; i < total; i++)
	{
		if (pid[i] > 0)
		{
			fprintf(outPut, "Process %d, %d unfulfilled requests, available resources: ", i+1, pid[i]);
			
			for (j = 0; j < resSize; j++)
			{
				procReq[j] = requests[i][j];
				fprintf(outPut, "%d, %d ||", procReq[j], resources[j][0]);
			}
			fprintf(outPut, "\n");
		}
		for (j = 0; j < resSize; j++)
		{
			if ((procReq[j] <= resources[j][0]) && (pid[i] > 0))
			{
				deadlocked[i] = 0;
			}
			else
			{
				deadlocked[i] = 1;
				break;
			}
		}
	}
	for (i = 0; i < total; i++)
	{
		if ((deadlocked[i] == 1) && (pid[i] > 0))
		{
			sysLock = 1;
		}
		else if (deadlocked[i] == 0)
		{
			sysLock = 0;
			fprintf(outPut, "No deadlock, process %d, %d can complete.\n", i+1, pid[i]);
			break;
		}
	}
	if (sysLock)
	{
		fprintf(outPut, "System deadlocked\n");
		printf("System deadlocked\n");
		for (i = 0; i < total; i++)
		{
			if (pid[i] > -1)
			{
				fprintf(outPut, "Killing process %d, %d\n", i+1, pid[i]);
				printf("Killing process %d, %d\n", i+1, pid[i]);
				if (kill(pid[i], SIGINT) == 0)
				{
					usleep(100000);
					kill(pid[i], SIGKILL);
				}
			}
		}
	}
	return sysLock;
}

