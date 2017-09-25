#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define MAXMAR 100
#define MAXSTU 100

/*
 * Parameters of the program. The constraints are D < T and
 * S*K <= M*N.
 */
struct demo_parameters {
    int S;   /* Number of students */
    int M;   /* Number of markers */
    int K;   /* Number of markers per demo */
    int N;   /* Number of demos per marker */
    int T;   /* Length of session (minutes) */
    int D;   /* Length of demo (minutes) */
};

/* Global object holding the demo parameters. */
struct demo_parameters parameters;

/* The demo start time, set in the main function. Do not modify this. */
struct timeval starttime;

//Declaration of the global variables of the program

int availMarkers; //total number of available markers
int studentCount; //number of students that terminated the procedure
int markerCount;//number of markers that terminated the procedure

int markerAvail[MAXMAR];//an array containing 1,0,-1 values if the marker is available,busy,terminated respectively
int studentMarker[MAXMAR];//an array containing the studentID in the place of the marker responsible for him
int performedDemo[MAXSTU];// an array containing whether or not the student in the specific position has finished his demo (1) or not(0)
 
//Mutex
static pthread_mutex_t mut;

//Condition Variable
pthread_cond_t cv;


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

//Function that performs pthread_cond_wait.If an error occurs the program aborts.
void wait(){
	int err;
	err=pthread_cond_wait(&cv,&mut);
	if(err){
		printf("Problem caused while waiting for the predicate.Program aborted.");
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

//Function that exits the threads and destroys the mutexes and conditions variables.
void clean_up(){
	int err;
	pthread_mutex_destroy(&mut);
	pthread_cond_destroy(&cv);
	//pthread_exit(NULL);
	
}

/*
 * timenow(): returns current simulated time in "minutes" (cs).
 * Assumes that starttime has been set already.
 * This function is safe to call in the student and marker threads as
 * starttime is set in the run() function.
 */
int timenow() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - starttime.tv_sec) * 100 + (now.tv_usec - starttime.tv_usec) / 10000;
}

/* delay(t): delays for t "minutes" (cs) */
void delay(int t) {
    struct timespec rqtp, rmtp;
    t *= 10;
    rqtp.tv_sec = t / 1000;
    rqtp.tv_nsec = 1000000 * (t % 1000);
    nanosleep(&rqtp, &rmtp);
}

/* panic(): simulates a student's panicking activity */
void panic() {
    delay(random() % (parameters.T - parameters.D));
}

/* demo(): simulates a demo activity */
void demo() {
    delay(parameters.D);
}

// A marker thread.

void *marker(void *arg) {
    int markerID = *(int *)arg;
    int studentID,job;
    
    /* 1. Enter the lab. */
    printf("%d marker %d: enters lab\n", timenow(), markerID);

    //Repeat the procedure N times
	for(job=0;job<parameters.N;job++){
		//Case we have run out of time or students break the loop
		if(timenow()>=parameters.T-parameters.D || studentCount==parameters.S){
			break;	
		}	
		else{
			//Lock the mutex and wait to be grabbed by a student
			lock_mut();
			while(markerAvail[markerID]==1){
				if(timenow()>=parameters.T-parameters.D){
					break;
				}				
				wait();
				
			}

			//Leaving the loop means that this marker has been grabbed
			//Find the studentID that grabbed the marker by looking in the studentMarker[markerID] array

			studentID=studentMarker[markerID];

			//Unlock the mutex
			unlock_mut();
					
			printf("%d marker %d: grabbed by student %d (job %d)\n", timenow(), markerID, studentID, job + 1);
			
			//Lock the mutex and wait for the demo to begin & end
			lock_mut();
			while(performedDemo[studentID]==0){
				wait();
			}
			printf("%d marker %d: finished with student %d (job %d)\n", timenow(), markerID, studentID, job + 1);
			
			//Unlock the mutex
			unlock_mut();
			
			//Make the marker available again, increase the number of available markers 
			lock_mut();
			markerAvail[markerID]=1;
			availMarkers++;
			broadcast();
			unlock_mut();
			
			
		}//else
	}//for
	
	//If we are outside the loop the marker has either terminated the procedure and marked N projects or there has been a timeout
	markerAvail[markerID]=-1; //the marker has terminated
    	availMarkers--;
	
	if(job==parameters.N){
		printf("%d marker %d: exits lab (finished %d jobs)\n", timenow(), markerID, parameters.N);	
	}
	else if(timenow()>=parameters.T-parameters.D){
		printf("%d marker %d: exits lab (timeout)\n", timenow(), markerID);

	}
    
    //Increase the counter that counts the markers that terminated the procedure
    markerCount--;
    return NULL;
}


//Student creation and manipulation

void *student(void *arg) {
    /* The ID of the current student. */
    int studentID = *(int *)arg;
    int j;

    /* 1. Panic! */
    printf("%d student %d: starts panicking\n", timenow(), studentID);
    panic();

    /* 2. Enter the lab. */
    printf("%d student %d: enters lab\n", timenow(), studentID);

    /* 3. Grab K markers. */
    
    //Case the student arrived in the lab late and there is no time to perform the demo.
	if(timenow()>=parameters.T-parameters.D|| markerCount<parameters.K){
		printf("%d student %d: exits lab (timeout)\n", timenow(), studentID);		
	}
    //Case the student is on time and he/she needs to grab K markers
	else{
		lock_mut();
		while(availMarkers<parameters.K){
			wait();
			if(timenow()>=parameters.T-parameters.D){
					break;
				}
		}
		unlock_mut();

		lock_mut();
		int totalMarkers=0;
		for(j=0;j<parameters.M;j++){
			//Use only the idle markers
			if(markerAvail[j]==1){
				studentMarker[j]=studentID;//Store the ID in the studentMarker array
				markerAvail[j]=0; //The marker is no longer available
				availMarkers--;//There are less available markers in total
				totalMarkers++;//The total markers used by the current student augments	
				broadcast();		
			}	
			if(totalMarkers==parameters.K){
	
				break;
			}
		}
		unlock_mut();
		
		//Now that the current student has grabbed K markers the demo can begin
		printf("%d student %d: starts demo\n", timenow(), studentID);
		demo();
		printf("%d student %d: ends demo\n", timenow(), studentID);

		lock_mut();
		//Change the state in the binary values array that stores whether the student perfomed the demo (1) or not (0)		
		performedDemo[studentID]=1;
		broadcast();

		unlock_mut();
		
		printf("%d student %d: exits lab (finished)\n", timenow(), studentID);
				
	       
	}
    //Increase the counter that counts the students that terminated the procedure
	studentCount++;
	return NULL;
}

/* The function that runs the session.
 */
void run() {
    int i,err;
    int markerID[100], studentID[100];
    pthread_t markerT[100], studentT[100];
    
    //Initialize the mutex and the condition variable. Apply error handling in each case.
     err=pthread_mutex_init(&mut,NULL);
     if(err){
	printf("Problem occured during the initialization of the mutex.Program aborted.");
	abort();
     }
     err=pthread_cond_init(&cv,NULL);

     if(err){
	printf("Problem occured during the initialization of the condition variable.Program aborted.");
	abort();
     }

    //Initialize the global variables of the code
	availMarkers=parameters.M;
	studentCount=0;
	markerCount=parameters.M; 
    //Initialize the arrays
	for(i=0;i<parameters.M;i++){
		markerAvail[i]=1;//All markers are available in the beginning of the procedure 
		studentMarker[i]=101;//None of the markers has examined any student yet. Store the largest number that can be stored
	}	

	for(i=0;i<parameters.S;i++){
		performedDemo[i]=0; //None of the students has performed his demo yet
	}

    printf("S=%d M=%d K=%d N=%d T=%d D=%d\n",
        parameters.S,
        parameters.M,
        parameters.K,
        parameters.N,
        parameters.T,
        parameters.D);
    gettimeofday(&starttime, NULL);  /* Save start of simulated time */

    /* Create S student threads */
    for (i = 0; i<parameters.S; i++) {
        studentID[i] = i;
        err=pthread_create(&studentT[i], NULL, student, &studentID[i]);
	if(err){
		printf("Problem occured during the creation of the student pthread.Program aborted.");
		abort();
		}
    }
    /* Create M marker threads */
    for (i = 0; i<parameters.M; i++) {
        markerID[i] = i;
        err=pthread_create(&markerT[i], NULL, marker, &markerID[i]);
	if(err){
		printf("Problem occured during the creation of the marker pthread.Program aborted.");
		abort();}
    }

    /* With the threads now started, the session is in full swing ... */
    delay(parameters.T - parameters.D);

  
    /* Wait for student threads to finish */
    for (i = 0; i<parameters.S; i++) {
        err=pthread_join(studentT[i], NULL);
	if(err){
		printf("Problem caused while joining the student threads.");
		abort();
	}
	if(timenow()>=parameters.T){
		break;
	}
    }
      printf("student joined");
    // Wait for marker threads to finish 
    for (i = 0; i<parameters.M; i++) {
	
        pthread_join(markerT[i], NULL);
	printf("marker joined %d\n",i);
	if(err){
		printf("Problem caused while joining the marker threads.");
		abort();	
	}
	if(timenow()>=parameters.T){
		break;
	}
    }
	
    //CLEAN UP
    clean_up(); 
	  
}




/*
 * main() checks that the parameters are ok. If they are, the interesting bit
 * is in run() so please don't modify main().
 */
int main(int argc, char *argv[]) {
 int i; 
printf("kate");
for(i=0;i<500;i++){
    if (argc < 6) {
        puts("Usage: demo S M K N T D\n");
        exit(1);
    }
    parameters.S = atoi(argv[1]);
    parameters.M = atoi(argv[2]);
    parameters.K = atoi(argv[3]);
    parameters.N = atoi(argv[4]);
    parameters.T = atoi(argv[5]);
    parameters.D = atoi(argv[6]);
    if (parameters.M > 100 || parameters.S > 100) {
        puts("Maximum 100 markers and 100 students allowed.\n");
        exit(1);
    }
    if (parameters.D >= parameters.T) {
        puts("Constraint D < T violated.\n");
        exit(1);
    }
    if (parameters.S*parameters.K > parameters.M*parameters.N) {
        puts("Constraint S*K <= M*N violated.\n");
        exit(1);
    }

    // We're good to go.

    run();
	printf("%d\n",i);
    } printf("kate");
    return 0;
}
