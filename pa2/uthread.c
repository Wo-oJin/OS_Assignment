#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include "uthread.h"
#include "list_head.h"
#include "types.h"

#define MEM 3000

/* You can include other header file, if you want. */


/*******************************************************************
 * struct tcb
 *
 * DESCRIPTION
 *    tcb is a thread control block.
 *    This structure contains some information to maintain thread.
 *
 ******************************************************************/
struct tcb {
    struct list_head list;
    ucontext_t *context;
    enum uthread_state state; //ready, running, terminated
    int tid;

    int lifetime; // This for preemptive scheduling
    int priority; // This for priority scheduling
};

int n_tcbs = 0;
struct ucontext_t *t_context;
LIST_HEAD(tcbs);

struct tcb* Main;

struct tcb* prev_tcb;
struct tcb* running_tcb;
struct tcb* daum_tcb;

int terminate_tid = 0;
int current_tid = 0;
int next_tid = 0;

bool start = false;
bool finish = false;
enum uthread_sched_policy current_policy;

void push_tcbs(ucontext_t* new_context, int params[]) // tcbs에 tcb저장
{
    struct tcb* new_tcb = (struct tcb*)malloc(sizeof(struct tcb));
    
    new_tcb->context = new_context;
    new_tcb->state = READY;
    new_tcb->tid = params[0];
    new_tcb->lifetime = params[1];
    new_tcb->priority = params[2];

    list_add_tail(&(new_tcb->list), &tcbs);
    n_tcbs++;
}

void pop_tcbs(int del_tid)
{
		/* TODO: Implement this function */
		struct list_head* tp;
		struct tcb* del_tcb;
    struct tcb* tp_tcb;

		list_for_each(tp, &tcbs){
      tp_tcb = list_entry(tp, struct tcb, list);
      if(tp_tcb->tid == del_tid){
			  del_tcb = list_entry(tp, struct tcb, list);
        break;
      }
		}

    n_tcbs--;
    if(n_tcbs==0)
      start=false;
		list_del(&(del_tcb->list));
		free(del_tcb);
}

/***************************************************************************************
 * LIST_HEAD(tcbs);
 *
 * DESCRIPTION
 *    Initialize list head of thread control block. 
 *
 **************************************************************************************/

/***************************************************************************************
 * next_tcb()
 *
 * DESCRIPTION
 *  
 *    Select a tcb with current scheduling policy
 *
 **************************************************************************************/
void next_tcb() {

  struct list_head* tp;
  struct tcb* current_tcb;

  /* TODO: You have to implement this function. */
  if(current_policy == FIFO){

    daum_tcb = fifo_scheduling(Main);

    if(daum_tcb != NULL){
      
      terminate_tid = daum_tcb->tid;
      __exit();
      __initialize_exit_context();

     int a = running_tcb->tid, b=daum_tcb->tid;
     //printf("SWAP %d -> %d\n", a, b);
     fprintf(stderr, "SWAP %d -> %d\n", running_tcb->tid, daum_tcb->tid);

      prev_tcb = running_tcb;
      running_tcb = daum_tcb;

      makecontext(running_tcb->context, (void*)&next_tcb, 0);
      swapcontext(prev_tcb->context, running_tcb->context);

      return;
    }
    else{
      finish=true;
      int a = running_tcb->tid;
      //printf("SWAP %d -> %d\n", a, -1);
      fprintf(stderr, "SWAP %d -> %d\n", running_tcb->tid, -1);
      running_tcb = Main;
      return;
    }
    
  }
  else if(current_policy == SJF){
    daum_tcb = sjf_scheduling(Main);

    if(daum_tcb != NULL){
      
      terminate_tid = daum_tcb->tid;
      __exit();
      __initialize_exit_context();

     int a = running_tcb->tid, b=daum_tcb->tid;
     //printf("SWAP %d -> %d\n", a, b);
     fprintf(stderr, "SWAP %d -> %d\n", running_tcb->tid, daum_tcb->tid);

      prev_tcb = running_tcb;
      running_tcb = daum_tcb;

      makecontext(running_tcb->context, (void*)&next_tcb, 0);
      swapcontext(prev_tcb->context, running_tcb->context);

      return;
    }
    else{
      finish=true;
      int a = running_tcb->tid;
      //printf("SWAP %d -> %d\n", a, -1);
      fprintf(stderr, "SWAP %d -> %d\n", running_tcb->tid, -1);
      running_tcb = Main;
      return;
    }
  }
  else if(current_policy == RR){

  }
  else{

  }
}

void finish_check(){
  struct list_head* tp;
  struct tcb* current_tcb;

  list_for_each(tp, &tcbs) {
    current_tcb = list_entry(tp, struct tcb, list);
    if(current_tcb->state != TERMINATED){
      finish = false;
      return;
    }
  }
  finish = true;
  return;
}

struct tcb *fifo_scheduling(struct tcb *next) {
  struct list_head* tp;
  struct tcb* current_tcb;
  bool flag = false;

  list_for_each(tp, &tcbs) {
    current_tcb = list_entry(tp, struct tcb, list);
    if(current_tcb->state == READY)
      return current_tcb;
  }
  return NULL;
}

/***************************************************************************************
 * struct tcb *rr_scheduling(struct tcb *next)
 *
 * DESCRIPTION
 *
 *    This function returns a tcb pointer using round robin policy
 *
 **************************************************************************************/
struct tcb *rr_scheduling(struct tcb *next) {

    /* TODO: You have to implement this function. */

}

/***************************************************************************************
 * struct tcb *prio_scheduling(struct tcb *next)
 *
 * DESCRIPTION
 *
 *    This function returns a tcb pointer using priority policy
 *
 **************************************************************************************/
struct tcb *prio_scheduling(struct tcb *next) {

    /* TODO: You have to implement this function. */

}

/***************************************************************************************
 * struct tcb *sjf_scheduling(struct tcb *next)
 *
 * DESCRIPTION
 *
 *    This function returns a tcb pointer using shortest job first policy
 *
 **************************************************************************************/
struct tcb *sjf_scheduling(struct tcb *next) {

  /* TODO: You have to implement this function. */
  struct list_head* tp;
  struct tcb* current_tcb;
  struct tcb* shortest_tcb = NULL;
  int min = 100;

  list_for_each(tp, &tcbs) {
    current_tcb = list_entry(tp, struct tcb, list);
    if(current_tcb->lifetime < min && current_tcb->state == READY){
      shortest_tcb = current_tcb;
      min = current_tcb->lifetime;
    }
  }
  return shortest_tcb;
}

/***************************************************************************************
 * uthread_init(enum uthread_sched_policy policy)
 *
 * DESCRIPTION

 *    Initialize main thread control block, and do something other to schedule tcbs
 *
 **************************************************************************************/

void uthread_init(enum uthread_sched_policy policy) {
    /* TODO: You have to implement this function. */
    current_policy = policy;

    Main = (struct tcb*)malloc(sizeof(struct tcb));
    ucontext_t* new_context = (ucontext_t*)malloc(sizeof(ucontext_t));
    
    Main->context = new_context;
    Main->state = READY;
    Main->tid = -1;
    Main->lifetime = 0;
    Main->priority = 0;

    getcontext(Main->context);

    running_tcb = Main;
}

/***************************************************************************************
 * uthread_create(void* stub(void *), void* args)
 *
 * DESCRIPTION
 *
 *    Create user level thread. This function returns a tid.
 *
 **************************************************************************************/
int uthread_create(void* stub(void *), void* args) {

  /* TODO: You have to implement this function. */
  int* params = (int*)args;
  ucontext_t* ncontext = (ucontext_t*)malloc(sizeof(ucontext_t));
  getcontext(ncontext);

  char* new_stack = (char*)malloc(sizeof(char)*3000);

  ncontext->uc_link=0;
  ncontext->uc_stack.ss_sp=malloc(MEM);
  ncontext->uc_stack.ss_size=MEM;
  ncontext->uc_stack.ss_flags=0;

  push_tcbs(ncontext, params);
}

/***************************************************************************************
 * uthread_join(int tid)
 *
 * DESCRIPTION
 *
 *    Wait until thread context block is terminated.
 *
 **************************************************************************************/
void uthread_join(int tid) {

    /* TODO: You have to implement this function. */
    struct list_head* tp;
    struct tcb* current_tcb;

    list_for_each(tp, &tcbs) {
      current_tcb = list_entry(tp, struct tcb, list);

      if(current_tcb->tid == tid){
        finish_check();
        while(!finish){
          next_tcb();
        }
        int a = tid;
        //printf("JOIN %d\n", tid);
        fprintf(stderr, "JOIN %d\n", a);
        pop_tcbs(tid);
        break;
      }

    }
}

/***************************************************************************************
 * __exit()
 *
 * DESCRIPTION
 *
 *    When your thread is terminated, the thread have to modify its state in tcb block.
 *
 **************************************************************************************/
void __exit() {
  /* TODO: You have to implement this function. */
  struct list_head* tp;
  struct tcb* current_tcb;

  list_for_each(tp, &tcbs) {
    current_tcb = list_entry(tp, struct tcb, list);

    if(current_tcb->tid == terminate_tid){
      current_tcb->state = TERMINATED;
      break;
    }

  }
}

/***************************************************************************************
 * __initialize_exit_context()
 *
 * DESCRIPTION
 *
 *    This function initializes exit context that your thread context will be linked.
 *
 **************************************************************************************/
void __initialize_exit_context() {

    /* TODO: You have to implement this function. */
  (daum_tcb->context)->uc_link = running_tcb->context;
}

/***************************************************************************************
 *   
 * DO NOT MODIFY UNDER THIS LINE!!!
 *
 **************************************************************************************/

static struct itimerval time_quantum;
static struct sigaction ticker;

void __scheduler() {
    if(n_tcbs > 1)
        next_tcb();
}

void __create_run_timer() {

    time_quantum.it_interval.tv_sec = 0;
    time_quantum.it_interval.tv_usec = SCHEDULER_FREQ;
    time_quantum.it_value = time_quantum.it_interval;
    
    ticker.sa_handler = __scheduler;
    sigemptyset(&ticker.sa_mask);
    sigaction(SIGALRM, &ticker, NULL);
    ticker.sa_flags = 0;
    
    setitimer(ITIMER_REAL, &time_quantum, (struct itimerval*) NULL);
}

void __free_all_tcbs() {
    struct tcb *temp;

    list_for_each_entry(temp, &tcbs, list) {
        if (temp != NULL && temp->tid != -1) {
            list_del(&temp->list);
            free(temp->context);
            free(temp);
        }
    }

    temp = NULL;
    free(t_context);
}