/* Server program for key-value store. */

#include "kv.h"
#include "parser.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <semaphore.h>

#define NTHREADS 4
#define BACKLOG 10


int connectionEstabl=0;
static pthread_mutex_t mut;
static pthread_mutex_t dataMut;
pthread_cond_t cv;
int accepted[100];
int threadin=0;
int full=0;
int queue_counter=0;
int locationAccepted=0;
int shut_down=0;
int byMain=0;
/*FUNCTIONS*/

//Function that locks the mutex.If an error occurs the program is aborted.
void lock_mut(){
	int err;
	err=pthread_mutex_lock(&mut);
	if(err){
		printf("Problem caused while locking the mutex.Program aborted.");
		abort();
	}
}

//Function that unclocks the mutex.If an error occurs the program aborts.
void unlock_mut(){
	int err;
	err=pthread_mutex_unlock(&mut);
	if(err){
		printf("Problem caused while unlocking the mutex.Program aborted.");
		abort();
	}
}
//FUNCTIONS FOR LOCKING AND UNLOCKING THE SECOND MUTEX.
void data_lock_mut(){
	int err;
	err=pthread_mutex_lock(&dataMut);
	if(err){
		printf("Problem caused while locking the data mutex.Program aborted.");
		abort();
	}
}

//Function that unclocks the mutex.If an error occurs the program aborts.
void data_unlock_mut(){
	int err;
	err=pthread_mutex_unlock(&dataMut);
	if(err){
		printf("Problem caused while unlocking the data mutex.Program aborted.");
		abort();
	}
}

//Function that performs pthread_cond_wait.If an error occurs the program aborts.
void wait(){
	int err;
	err=pthread_cond_wait(&cv,&mut);
	if(err){
		printf("Problem caused while waiting for the predicate.Program aborted.");
		abort();
	}
}

//Function that performs pthread_cond_signal.If an error occurs the program aborts.
void signal(){
	int err;
	err=pthread_cond_signal(&cv);;
	if(err){
		printf("Problem caused while signaling.Program aborted.");
		abort();
	}
}

//Function that performs pthread_cond_broadcast.If an error occurs the program aborts.
void broadcast(){
	int err;
	err=pthread_cond_broadcast(&cv);
	if(err){
		printf("Problem caused while broadcasting.Program aborted.");
		abort();
	}
}

//Destroy the mutexes and condtion variables
void cleanUp(){
	int err;
	err=pthread_mutex_destroy(&mut);
	err=pthread_mutex_destroy(&dataMut);
	err=pthread_cond_destroy(&cv);
	if(err)
	{
		printf("Problem caused while destroying the condition variables and mutexes.Program aborted.");
		abort();	
	}
	
}


/* A worker thread. You should write the code of this function. */
void* worker(void* p) 
{

	ssize_t typedData;
	char bufferWorker[LINE]; 
	char* key;
	char* text;
	enum DATA_CMD cmd;
	int run=1;
	int fd;
	char *value;
	/*MAKE THE WORKERS WAIT UNTIL THEY GET CONNECTED*/
	printf("The threads are here.\n");
	while(1)
	{
		//Make the workers wait until they get accepted
		
		lock_mut();
		while(connectionEstabl==0 && shut_down==0)
		{
			wait();
		}

		if (shut_down)
		{
			unlock_mut();
			break;
		}
		connectionEstabl=0;
		fd=accepted[locationAccepted];
		locationAccepted++;

		//This shows the locations that have been accepted so far from accepted array
		unlock_mut();

		//Print the welcome message to the clients
		strcpy(bufferWorker,"Welcome to KV store! \n");
		write(fd, bufferWorker, strlen(bufferWorker));

		while(run)
		{
			typedData=read(fd, bufferWorker, LINE);

				if(typedData>0)
				{
					/*Write the message to the administrator's terminal*/
					parse_d(bufferWorker,&cmd,&key,&text);
					data_lock_mut();

					switch (cmd)
					{
						case D_PUT:
							value=malloc(strlen(text+1));
							strcpy(value,text);
							if(createItem(key,value)==0)
							{
								strcpy(bufferWorker,"Data successfully stored! \n");
								write(fd, bufferWorker, strlen(bufferWorker));
							}
							else
							{
								if(itemExists(key))
								{
									if(!updateItem(key,value))
									{
										strcpy(bufferWorker,"Data successfully updated! \n");
										write(fd, bufferWorker, strlen(bufferWorker));
									}	
								}
								else
								{
									strcpy(bufferWorker,"Error occured while trying to input data! \n");
									write(fd, bufferWorker, strlen(bufferWorker));
								}
							}
							break;
						case D_GET:
								if(findValue(key)!=NULL)
								{
									strcpy(bufferWorker,findValue(key));
									strcat(bufferWorker,"\n");
									write(fd, bufferWorker, strlen(bufferWorker));
								}
								else
								{
									strcpy(bufferWorker,"Problem occured while getting the data! \n");
									write(fd, bufferWorker, strlen(bufferWorker));
								}
							break;
						case D_COUNT:
								sprintf(bufferWorker,"%d\n", countItems());
								write(fd, bufferWorker, strlen(bufferWorker));
							break;
						case D_DELETE:
								if(deleteItem(key,0)==0)
								{
									strcpy(bufferWorker,"Data successfully deleted! \n");
									write(fd, bufferWorker, strlen(bufferWorker));
								}
								else
								{
									strcpy(bufferWorker,"An error occured!The key value is either null or doesn't exist! \n");
									write(fd, bufferWorker, strlen(bufferWorker));	
								}
							break;
						case D_EXISTS:
								if(itemExists(key))
								{
									strcpy(bufferWorker,"The data exists in the database! \n");
									write(fd, bufferWorker, strlen(bufferWorker));
								}
								else
								{
									strcpy(bufferWorker,"The key doesn't exist in the database! \n");
									write(fd, bufferWorker, strlen(bufferWorker));
								}
							break;
						case D_END:
								
								strcpy(bufferWorker,"Goodbye! \n");
								write(fd, bufferWorker, strlen(bufferWorker));
								close(fd);
							    data_unlock_mut();
								run=0;
							break;
						case D_ERR_OL:
							strcpy(bufferWorker,"Error:The line is too long! \n");
							write(fd, bufferWorker, strlen(bufferWorker));
							break;
						case D_ERR_INVALID:
							strcpy(bufferWorker,"Error:Invalid command! \n");
							write(fd, bufferWorker, strlen(bufferWorker));
							break;
						case D_ERR_SHORT:
							strcpy(bufferWorker,"Error:Too few parameters! \n");
							write(fd, bufferWorker, strlen(bufferWorker));
							break;
						case D_ERR_LONG:
							strcpy(bufferWorker,"Error:Too many parameters! \n");
							write(fd, bufferWorker, strlen(bufferWorker));
							break;

					}//end of switch

					
				data_unlock_mut();	

				}//end of if

				
				else if(!typedData)
				{
					printf("Connection in data closed.Problem caused while reading1\n");
					close(fd);
					break;
				
				}//end of else if
				
				else 
				{	
					printf("Problem while reading from workers.The connection closed!\n");
					close(fd);
				}

				
			

		}//while in the datastore
		close(fd);
		run=1;
		lock_mut();
		queue_counter--;
		full=0;
		if (byMain>locationAccepted)
		{	
			connectionEstabl=1;
		}
		unlock_mut();

	}//start the thread

pthread_exit(NULL);
return NULL;
}//end of workers

/* You may add code to the main() function. */
int main(int argc, char** argv) {
    int cport, dport; /* control and data ports. */
    int err,i;
    int run=1;
    int dataAcceptance;
    int controlAcceptance;
    char buffer[LINE];
   	ssize_t typed;
   	enum CONTROL_CMD parsed; 
   	struct sockaddr_in controlAddr;
	struct sockaddr_in dataAddr;
	struct pollfd connections[2];
	

	int controlFD;
	int dataFD;
	int polling;
	int workerID[4];

	

    /*Create the threads*/
	pthread_t workers[4];

	printf("Server started.\n");

	for (i = 0; i<4; i++) 
	{
        workerID[i] = i;
        err=pthread_create(&workers[i], NULL, worker, &workerID[i]);
		if(err)
		{
			printf("Problem occured during the creation of the worker pthread.The server terminated.\n");
			abort();
		}
		
    }
	   
    /*Store the input/ports in variables*/

	if (argc < 3) {
        printf("Usage: %s data-port control-port\n", argv[0]);
        exit(1);
	} else {
        cport = atoi(argv[2]);
        dport = atoi(argv[1]);
	}


	/*Socket creation*/

	controlFD= socket(AF_INET, SOCK_STREAM,0);
	dataFD=socket(AF_INET, SOCK_STREAM,0);

	if (controlFD<0 || dataFD<0)
	{
		printf("Problem caused while opening the sockets.The server shuts down.");
		abort();
	}


	/*controlAddr initialisation and bind*/

	socklen_t len1=sizeof(controlAddr); 
	memset(&controlAddr, 0, len1);
	controlAddr.sin_family= AF_INET;
	controlAddr.sin_addr.s_addr= htonl(INADDR_ANY);
	controlAddr.sin_port=htons(cport);


	if(bind(controlFD,(struct sockaddr *)&controlAddr,sizeof(struct sockaddr_in))<0)
	{
		perror("Problem caused while binding the control socket.The server shuts down.");
		close(controlFD);
		abort();	
	}

	/*dataAddr initialisation and bind*/

	socklen_t len2=sizeof(dataAddr);
	memset(&dataAddr,0,len2);
	dataAddr.sin_family=AF_INET;
	dataAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	dataAddr.sin_port=htons(dport);

	if(bind(dataFD,(struct sockaddr *)&dataAddr,sizeof(struct sockaddr_in))<0)
	{
		perror("Problem caused while binding the data socket.The server shuts down.");
		close(dataFD);
		abort();	
	}

	/*Listen for both controlFD and dataFD*/

	err=listen( controlFD, BACKLOG);
	if(err==-1)
	{
		printf("Problem caused while listening for controlFD.The server shuts down.");
		close(controlFD);
		abort();
	}

	err=listen( dataFD, BACKLOG);
	if(err==-1)
	{
		printf("Problem caused while listening for dataFD.The server shuts down.");
		close(dataFD);
		abort();
	}

	/*Initialise the poll structure*/

	/*Connections made in the control port*/
 	connections[0].fd=controlFD;
 	connections[0].events=POLLIN;

 	/*Connections made in the data port*/
 	connections[1].fd=dataFD;
 	connections[1].events=POLLIN;
	
	
	while(run)
	{	
		/*Use poll function to find which socket is trying to connect*/
		polling=poll(connections, 2, -1);
		
		if(polling==-1)
		{
			perror("Error occured while trying to poll.");
			abort();
		}
		else if(polling==0)
		{
			printf("Timeout occured! \n");
			abort();
		}
		else
		{
			/*Check for events in the controller port*/
			if (connections[0].revents & POLLIN)
			{	
				controlAcceptance=accept(controlFD, (struct sockaddr *)&controlAddr,&len1);
				printf("Connection made in the controller socket.\n");
				strcpy(buffer,"Welcome administrator!\n");
				write(controlAcceptance, buffer, strlen(buffer));
				
				/*Read the message typed by the administrator*/
				typed=read(controlAcceptance, buffer, LINE); 

				if(typed>0)
				{
					/*Write the message to the administrator's terminal*/
					parsed=parse_c(buffer);

					//Check what was typed inside the parser
					if(parsed==C_COUNT)
					{
						sprintf(buffer,"%i\n",countItems());
						write(controlAcceptance, buffer, strlen(buffer));
						close(controlAcceptance);
					}
					else if(parsed==C_SHUTDOWN)
					{
						strcpy(buffer," Goodbye! \n");
						write(controlAcceptance, buffer, strlen(buffer));
						close(controlAcceptance);
						lock_mut();
						shut_down=1;
						broadcast();
						unlock_mut();
						break;
					}
					else
					{
						strcpy(buffer," Goodbye! \n");
						write(controlAcceptance, buffer, strlen(buffer));
						close(controlAcceptance);
					}


				}

				
				else if(typed==0)
				{
					printf("Connection in controller closed.\n");
					close(controlAcceptance);
					break;
				}
				else
				{
					printf("Problem while reading from controller.\n");
					close(controlAcceptance);
					abort();
				}
			}

			/*Check for events in the data port*/
			if (connections[1].revents & POLLIN)
			{	

				dataAcceptance=accept(dataFD, (struct sockaddr *)&dataAddr,&len2);
				
				lock_mut();
				accepted[byMain]=dataAcceptance;
				byMain++;
				printf("Connection made in the data socket.\n");
				if(!full)
				{
					connectionEstabl=1;
					queue_counter++;
					
					if (queue_counter==	NTHREADS)
					{
						full=1;
					}
					signal();
					printf("Connections in datastore now %d\n", queue_counter);
				}
				unlock_mut();

			}

		}

	}
    
   for (i = 0; i<4; i++) {
	
     	pthread_join(workers[i], NULL);
		if(err){
			printf("Problem caused while joining the worker threads.");
			abort();	
			}
	}
	printf("Worker threads joined\n");
    cleanUp();
    close(controlFD);
    close(dataFD);

    return 0;
}

