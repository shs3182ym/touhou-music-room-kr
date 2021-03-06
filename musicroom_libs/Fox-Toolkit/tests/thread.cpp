/********************************************************************************
*                                                                               *
*                                Thread Pool Test                               *
*                                                                               *
*********************************************************************************
* Copyright (C) 1999,2009 by Jeroen van der Zijp.   All Rights Reserved.        *
********************************************************************************/
#include "fx.h"
#include "FXThreadPool.h"


/*
  Notes:

  - Thread pool test.

*/

/*******************************************************************************/



// Job runner
class Runner : public FXRunnable {
protected:
  FXdouble value;
  FXint    number;
  FXint    count;
public:
  Runner(FXint n,FXint c):value(1.0),number(n),count(c){}
  virtual FXint run();
  };


// Job producer
class Producer : public FXThread {
protected:
  FXThreadPool *pool;
  FXint         count;
  FXint         groups;
public:
  Producer(FXThreadPool *p,FXint c,FXint g):pool(p),count(c),groups(g){}
  virtual FXint run();
  };


// Run jobs
FXint Runner::run(){
  printf("runner %d start\n",number);
  value=1.0;
  for(FXint i=0; i<count; i++){
    value=cos(value);
    }
  printf("runner %d done\n",number);
  delete this;
  return 1;
  }


// Generate jobs
FXint Producer::run(){
  FXint job=0;
  printf("producer start\n");
  FXuint seed=1013904223u;
  for(FXint g=0; g<groups; ++g){
    for(FXint c=0; c<count; c++){
      //FXThread::sleep(50000000);
      if(!pool->execute(new Runner(job,fxrandom(seed)/1000))) goto x;
      printf("producer job %d\n",job);
      job++;
      }
    printf("producer waiting\n");
    pool->wait();
    printf("producer resumed\n");
    }
x:printf("producer done\n");
  return 1;
  }



// Start
int main(int,char**){
  int cpus=FXThread::processors();
  int started;

  // Trace
  fxTraceLevel=151;

  // Make thread pool
  FXThreadPool pool(10);

  // Make producer thread
  Producer producer(&pool,100,10);
  
  printf("Found %d processors\n",cpus);

  printf("starting pool\n");
  started=pool.start(1,8,1);
  getchar();
  printf("started pool %d\n",started);
  getchar();

  printf("starting jobs\n");
  producer.start();
  printf("running jobs\n");

  getchar();
  printf("stopping\n");
  pool.stop();
  printf("stopped\n");

  getchar();
  producer.join();
  printf("bye\n");
  return 1;
  }
