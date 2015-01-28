//Alec Kosik
//math389
//hw6

//Sandpile Simulation

#include <time.h>
#include "csapp.c"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

//Three structs: the thread info, the barrier, and a row struct.
//I added some stable indicators to ensure that stable rows were
//skipped and to know when the simulation was over.The start and
//end integers are markers for each threads region of the grid.

typedef struct _row_t{
  int* row;
  int stable_indicator;
  pthread_mutex_t mutex;
}row_t;

typedef struct _barrier_t{
  int counter;
  int stable_indicator;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
}barrier_t;

//Grid stable indicator says that from what that thread knows,
//the grid could be stable. 

typedef struct _thread_info_t{
  pthread_t thread;
  barrier_t* barrier;
  int threadnum;
  int numthreads;
  row_t* grid;
  int size;
  int region_stable_indicator;
  int grid_stable_indicator;
  int start;
  int end;
}thread_info_t;

//Sets the barrier counter and the grid's stable indicator
//to the number of threads.Initializes the condition
//variable and the mutex.

void barrier_init(barrier_t* b, int numthreads) {
  b->counter = numthreads;
  b->stable_indicator = numthreads;
  pthread_cond_init(&b->condition,NULL);
  pthread_mutex_init(&b->mutex, NULL);
}

//Barrier_wait acquires the lock for a given thread and decrements the counter
//and possibly the stable indicator.Each thread blocks on a condition variable
//until the counter reaches 0.The last thread wakes up the others, checks
//if the grid is stable(the simulation is complete), and resets counters if it isn't. 

int barrier_wait(barrier_t* barrier,int numthreads, int stable_indicator) {

  pthread_mutex_lock(&barrier->mutex);
  barrier->counter--;
  barrier->stable_indicator -= stable_indicator;

  if(barrier->counter == 0) {
    pthread_cond_broadcast(&barrier->condition);

    if(barrier->stable_indicator == 0) {
      pthread_mutex_unlock(&barrier->mutex);
      return 1;
    }

    barrier->counter = numthreads;
    barrier->stable_indicator = numthreads;
  }

  else{
    pthread_cond_wait(&barrier->condition,&barrier->mutex);

    //Each waking thread checks if the simulation is over
                                                                                
    if(barrier->stable_indicator == 0) {
      pthread_mutex_unlock(&barrier->mutex);
      return 1;
    }

  }
  pthread_mutex_unlock(&barrier->mutex);
  return 0;
}

//Standard nested for-loop for printing the grid out

void output(thread_info_t* tinfo) {
  for(int i = 0; i < tinfo->size; i++) {
    printf("\n");
    for(int j = 0; j < tinfo->size; j++) {
      printf("%d ",tinfo->grid[i].row[j]);
    }
  }
  printf("\n");
}

//For each row a thread looks at on the border of two regions,
//it acquires the necessary locks for itself and the surrounding rows 
//The key is in the order.A beginning row must lock its row first while
//an ending row must lock the next beginning row first.

void acquire_locks(thread_info_t* tinfo, int rownumber) {
  
  if(rownumber == tinfo->start) {
    pthread_mutex_lock(&tinfo->grid[rownumber].mutex);
  }
  if(rownumber - 1 <= tinfo->start && rownumber > 0) {
    pthread_mutex_lock(&tinfo->grid[rownumber-1].mutex);
  }
  if(rownumber + 1 >= tinfo->end - 1 && rownumber < tinfo->size - 1) {
    pthread_mutex_lock(&tinfo->grid[rownumber+1].mutex);
  }
  if(rownumber == tinfo->end - 1) {
    pthread_mutex_lock(&tinfo->grid[rownumber].mutex);
  }
}

//Counterpart to the above

void return_locks(thread_info_t* tinfo, int rownumber) {
  if(rownumber == tinfo->start || rownumber == tinfo->end - 1) {
    pthread_mutex_unlock(&tinfo->grid[rownumber].mutex);
  }
  if(rownumber + 1 >= tinfo->end-1 && rownumber < tinfo->size - 1) {
    pthread_mutex_unlock(&tinfo->grid[rownumber+1].mutex);
  }
  if(rownumber - 1 <= tinfo->start && rownumber > 0) {
    pthread_mutex_unlock(&tinfo->grid[rownumber-1].mutex);
  }
}

//Compute assumes the thread's region is stable and then runs through
//the rows of the region, acquiring locks when needed.Compute fires the
//cells of a given row until it is stable at which time it unlocks
//the mutexes and proceeds to the next row.

void compute(thread_info_t* tinfo) {

  tinfo->region_stable_indicator = 1;

  for(int i = tinfo->start; i < tinfo->end; i++) {

    acquire_locks(tinfo,i);

    while(tinfo->grid[i].stable_indicator == 0) {
      
      //Also assumes each row is stable
      tinfo->grid[i].stable_indicator = 1;

      for(int j = 0; j < tinfo->size; j++) {

	//Cell-firing
	if(tinfo->grid[i].row[j] >= 4) {
	  tinfo->grid[i].row[j] -= 4;

	  //Notes that the region and row may not be stable
	  tinfo->grid[i].stable_indicator = 0;
	  tinfo->region_stable_indicator = 0;

	  //Notes that surrounding regions may not be stable
	  //if a cell is on the edge of the region
	  if(i-1 < tinfo->start || i+1 == tinfo->end) {
	    tinfo->grid_stable_indicator = 0;
	  }

	  //Distributing fired bits, checks if the firing cell
	  //is on the edge of the grid
	  if(i-1 >= 0) {
	    tinfo->grid[i-1].row[j]++;
	    tinfo->grid[i-1].stable_indicator = 0;
	  }
	  if(i+1 < tinfo->size) {
	    tinfo->grid[i+1].row[j]++;
	    tinfo->grid[i+1].stable_indicator = 0;
	  }
	  if(j-1 >= 0) {
	    tinfo->grid[i].row[j-1]++;
	  }
	  if(j+1 < tinfo->size) {
	    tinfo->grid[i].row[j+1]++;
	  }
	}
      }
    }

    return_locks(tinfo,i);
      
  }
}

//Simulate is the main part of each thread.It runs compute and prints 
//out the grid when every thread thinks its region is stable.

void simulate(thread_info_t* tinfo) {
  while(1) {

    compute(tinfo);

    if(tinfo->region_stable_indicator == 1) {
      
      //Checks if the thread's region and surrounding regions are stable
      int stable_indicator = tinfo->grid_stable_indicator & tinfo->region_stable_indicator;

      //Checks if the simulation is over and regroups the threads for printout
      if(barrier_wait(tinfo->barrier,tinfo->numthreads,stable_indicator) == 1) {
	break;

      }
      //Zeroth thread prints out the grid
      else if(tinfo->threadnum == 0) {
	output(tinfo);
      }
      //Reassumes the surrounding regions are stable since each thread
      //is about to recheck its region
      tinfo->grid_stable_indicator = 1;

      //Halts threads while the zeroth thread prints the grid out
      barrier_wait(tinfo->barrier, tinfo->numthreads,stable_indicator);
    }
  }
}

int main(int argc, char** argv) {

  clock_t start = clock();

  //command line arguments
  int numthreads = atoi(argv[1]);
  int gridsize = atoi(argv[2]);

  //declares and makes the grid
  static row_t* grid;
  grid = (row_t*)malloc(gridsize*sizeof(row_t));
  for(int i = 0; i < gridsize; i++) {
    grid[i].stable_indicator = 0;
    grid[i].row = (int*)malloc(gridsize*sizeof(int));
    pthread_mutex_init(&grid[i].mutex,NULL);
    for(int j = 0; j < gridsize; j++) {
      grid[i].row[j] = 0;
    }
  }

  //test
  grid[gridsize/2].row[gridsize/2] = 500;

  //declares a stack allocated array of the thread info structs
  thread_info_t tinfo[numthreads];
  
  //declares and initializes the barrier
  static barrier_t b;
  barrier_init(&b,numthreads);

  //initializes all the remaining threads. start and end are found by dividing
  //the grid in equal portions and multiplying by the thread number.
  for(int i = 0; i < numthreads; i++) {
    
    tinfo[i].numthreads = numthreads;
    tinfo[i].threadnum = i;
    tinfo[i].barrier = &b;
    tinfo[i].grid = grid;
    tinfo[i].start = (gridsize/(numthreads))*(i);
    tinfo[i].end = (gridsize/(numthreads))*(i+1);
    tinfo[i].size = gridsize;
    tinfo[i].region_stable_indicator = 1;
    tinfo[i].grid_stable_indicator = 1;
    if(i != 0) {
      Pthread_create(&tinfo[i].thread, NULL, (void*(*)(void*))simulate, &tinfo[i]);
    }
  }

  simulate(&tinfo[0]);
  output(&tinfo[0]);

  clock_t end = clock();
  double time = ((double)(end - start))/ CLOCKS_PER_SEC;
  printf("Time: %f\n",time);
}
