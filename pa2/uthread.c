#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include "uthread.h"
#include "list_head.h"
#include "types.h"

/* You can include other header file, if you want. */
#include <string.h>

void *__preemptive_worker(void* args);
void *__non_preemptive_worker(void* args);

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
    enum uthread_state state;
    int tid;

    int lifetime; // This for preemptive scheduling
    int priority; // This for priority scheduling
};

/***************************************************************************************
 * LIST_HEAD(tcbs);
 *
 * DESCRIPTION
 *    Initialize list head of thread control block.
 *
 **************************************************************************************/
LIST_HEAD(tcbs);

int n_tcbs = 0;

struct ucontext_t *t_context;

////
sigset_t set;

enum uthread_sched_policy current_policy;

struct tcb* Main;
struct tcb* prev_tcb;
struct tcb* running_tcb;
struct tcb* daum_tcb;

int terminate_tid = 0;
int join_tid = 0;

bool complete[15];

bool createBehindMain = false;
bool finish = false;
///

void push_tcbs(ucontext_t* new_context, int params[]){
  
  struct tcb* new_tcb = (struct tcb*)malloc(sizeof(struct tcb));

  new_tcb->context = new_context;
  new_tcb->state = READY;
  new_tcb->tid = params[0];
  new_tcb->lifetime = params[1];
  new_tcb->priority = params[2];

  if(current_policy == RR){
    struct list_head* tp;
    struct tcb* current_tcb;
    list_for_each(tp, &tcbs)
      current_tcb = list_entry(tp, struct tcb, list);

    if(current_tcb -> tid == Main->tid)
      createBehindMain = true;
    else
      createBehindMain = false;
  }
    
  list_add_tail(&(new_tcb->list), &tcbs);
}

void pop_tcbs(int del_tid){
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

	list_del(&(del_tcb->list));

  if(del_tcb->tid!=-1)
		free(del_tcb);
}

void finish_check(){ //check if join tcb is terminated
  
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

/***************************************************************************************
 * next_tcb()
 *
 * DESCRIPTION
 *  
 *    Select a tcb with current scheduling policy
 *
 **************************************************************************************/
void next_tcb() {
  //printf("next_tcb start\n");

  struct list_head* tp;
  struct tcb* current_tcb;
  struct tcb* next = (struct tcb*)malloc(sizeof(struct tcb));

  int s=-1;

  if(current_policy == FIFO || current_policy == SJF){

    if(current_policy == FIFO)
      daum_tcb = fifo_scheduling(next);
    else 
      daum_tcb = sjf_scheduling(next);

    if(daum_tcb != NULL){
      terminate_tid = daum_tcb->tid;

      __exit();
      __initialize_exit_context();
      
      if(running_tcb->tid!=-1 || daum_tcb->tid!=-1)
        fprintf(stderr, "SWAP %d -> %d\n", running_tcb->tid, daum_tcb->tid);

      prev_tcb = running_tcb;
      running_tcb = daum_tcb;

      makecontext(running_tcb->context, (void*)&__non_preemptive_worker, 0);
      swapcontext(prev_tcb->context, running_tcb->context);

      return;
    }
    else{
      finish = true;
      if(running_tcb->tid!=-1)
        fprintf(stderr, "SWAP %d -> %d\n", running_tcb->tid, Main->tid);
      running_tcb = Main;
      return;
    }
    
  }
  else if(current_policy == PRIO){
    daum_tcb = prio_scheduling(next);

    if(daum_tcb != NULL){
      if(daum_tcb->lifetime == 0){
        terminate_tid = daum_tcb->tid;
        __exit();
      }

      if(running_tcb->tid != daum_tcb->tid)
        __initialize_exit_context();

      if(running_tcb->tid!=-1 || daum_tcb->tid!=-1)
        fprintf(stderr,"SWAP %d -> %d\n", running_tcb->tid, daum_tcb->tid);

      prev_tcb = running_tcb;
      running_tcb = daum_tcb;

      makecontext(running_tcb->context, (void*)__preemptive_worker, 0);
      running_tcb->context->uc_sigmask = set;

      if(prev_tcb->tid == running_tcb->tid)
         while((sigwait(&set,&s)!=0));
      else{
        while((sigwait(&set,&s)!=0))
          swapcontext(prev_tcb->context, running_tcb->context);
      }

      return;
    }
    else{
      finish=true;
      if(running_tcb->tid!=-1)
        fprintf(stderr,"SWAP %d -> %d\n", running_tcb->tid, Main->tid);
      running_tcb = Main;
      return;
    }
  }
  else if(current_policy == RR){
    daum_tcb = rr_scheduling(next);

    if(running_tcb->tid == -1 && complete[join_tid]){
      struct list_head* tp;

      struct tcb* current_tcb;

      if(daum_tcb != NULL){
        list_for_each(tp, &tcbs) {
          current_tcb = list_entry(tp, struct tcb, list);
          if(current_tcb->tid == daum_tcb->tid){
            (current_tcb->lifetime)++;
            pop_tcbs(current_tcb->tid);
            list_add(&(current_tcb->list), &tcbs);
          }
        }
      }

      finish=true;
      running_tcb = Main;
      return;
    }

    if(daum_tcb != NULL){
      if(daum_tcb->lifetime == 0){
        complete[daum_tcb->tid]=true;
        terminate_tid = daum_tcb->tid;
        __exit();
      }
      __initialize_exit_context();

      if(running_tcb->tid!=-1 || daum_tcb->tid!=-1)
        fprintf(stderr, "SWAP %d -> %d\n",  running_tcb->tid, daum_tcb->tid);

      prev_tcb = running_tcb;
      running_tcb = daum_tcb;

      makecontext(running_tcb->context, (void*)__preemptive_worker, 0);
      while((sigwait(&set,&s)!=0))
          swapcontext(prev_tcb->context, running_tcb->context);
      
      return;
    }
    else{
      finish=true;
      if(running_tcb->tid!=-1)
        fprintf(stderr, "SWAP %d -> %d\n", running_tcb->tid, Main->tid);
      running_tcb = Main;
      return;
    }
  }
}

struct tcb *fifo_scheduling(struct tcb *next) {
  
  struct list_head* tp;

  list_for_each(tp, &tcbs) {
    next = list_entry(tp, struct tcb, list);
    if(next->state == READY)
      return next;
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
  
  struct list_head* tp;

  list_for_each(tp, &tcbs) {
    next = list_entry(tp, struct tcb, list);
    if(next->lifetime > 0 && next->state == READY){
      struct tcb* del_tcb = (struct tcb*)malloc(sizeof(struct tcb));
      t_context = (ucontext_t*)malloc(sizeof(ucontext_t));

      t_context->uc_link = 0;
      t_context->uc_stack.ss_sp=malloc(SIGSTKSZ);
      t_context->uc_stack.ss_size=SIGSTKSZ;
      t_context->uc_stack.ss_flags=0;
      getcontext(t_context);
      
      del_tcb->context = t_context;
      del_tcb->state = next->state;
      del_tcb->tid = next->tid;
      del_tcb->lifetime = next->lifetime;
      del_tcb->priority = next->priority;

      pop_tcbs(next->tid);
      (del_tcb->lifetime)--;
      (next->lifetime)--;
        
      list_add_tail(&(del_tcb->list), &tcbs);

      return next;
    }
  }

  return NULL;
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

  struct list_head* tp;
  struct tcb* prior_tcb = NULL;
  int max = -1;

  list_for_each(tp, &tcbs) {
    next = list_entry(tp, struct tcb, list);
    if(next->priority > max && next->lifetime > 0 && next->state == READY){
      prior_tcb = next;
      max = next->priority;
    }
  }
  if(prior_tcb == NULL)
    return NULL;
  else{
    (prior_tcb->lifetime)--;
    return prior_tcb;
  }
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

  struct list_head* tp;
  struct tcb* shortest_tcb = NULL;
  int min = 100;

  list_for_each(tp, &tcbs) {
    next = list_entry(tp, struct tcb, list);
    if(next->lifetime < min && next->state == READY){
      shortest_tcb = next;
      min = next->lifetime;
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
  
  current_policy = policy;
  memset(complete, false, sizeof(complete));

  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  //sigaddset(&set, SIGINT);

  t_context = (ucontext_t*)malloc(sizeof(ucontext_t));
  Main = (struct tcb*)malloc(sizeof(struct tcb));

  Main->context = t_context;
  Main->state = TERMINATED;
  Main->tid = MAIN_THREAD_TID;
  Main->lifetime = MAIN_THREAD_LIFETIME;
  Main->priority = MAIN_THREAD_PRIORITY;

  getcontext(t_context);

  if(current_policy == RR){
    int params[] = {MAIN_THREAD_TID, MAIN_THREAD_LIFETIME, MAIN_THREAD_PRIORITY};
    t_context = (ucontext_t*)malloc(sizeof(ucontext_t));
    struct tcb* proxy = (struct tcb*)malloc(sizeof(struct tcb));
    getcontext(t_context);

    t_context->uc_link = 0;
    t_context->uc_stack.ss_sp=malloc(SIGSTKSZ);
    t_context->uc_stack.ss_size=SIGSTKSZ;
    t_context->uc_stack.ss_flags=0;

    proxy -> context = t_context;
    proxy -> tid = MAIN_THREAD_TID;
    proxy -> lifetime = MAIN_THREAD_LIFETIME;
    proxy -> priority = MAIN_THREAD_PRIORITY;

    list_add(&(proxy->list),&tcbs);
  }

  n_tcbs++;
  running_tcb = Main;

  /* DO NOT MODIFY THESE TWO LINES */
  __create_run_timer();
  __initialize_exit_context();
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

  int* params = (int*)args;

  t_context = (ucontext_t*)malloc(sizeof(ucontext_t));

  t_context->uc_link=0;
  t_context->uc_stack.ss_sp = malloc(SIGSTKSZ);
  t_context->uc_stack.ss_size = SIGSTKSZ;
  t_context->uc_stack.ss_flags = 0;

  getcontext(t_context);

  n_tcbs++;
  push_tcbs(t_context, params);

  if(createBehindMain && current_policy == RR){
    int params[] = {MAIN_THREAD_TID, MAIN_THREAD_LIFETIME, MAIN_THREAD_PRIORITY};
    pop_tcbs(Main->tid);

    t_context = (ucontext_t*)malloc(sizeof(ucontext_t));    
    struct tcb* proxy = (struct tcb*)malloc(sizeof(struct tcb));

    t_context->uc_link = 0;
    t_context->uc_stack.ss_sp=malloc(SIGSTKSZ);
    t_context->uc_stack.ss_size=SIGSTKSZ;
    t_context->uc_stack.ss_flags=0;

    proxy -> context = t_context;
    proxy -> tid = MAIN_THREAD_TID;
    proxy -> lifetime = MAIN_THREAD_LIFETIME;
    proxy -> priority = MAIN_THREAD_PRIORITY;

    getcontext(t_context);

    list_add_tail(&(proxy->list),&tcbs);
  }

  return params[0];
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

  struct list_head* tp;
  struct tcb* current_tcb;
  join_tid = tid;

  list_for_each(tp, &tcbs) {
    current_tcb = list_entry(tp, struct tcb, list);
    
    if(current_tcb->tid == tid){
      finish_check();

      while(!finish){
        if(current_policy == RR){
          if(complete[tid] && running_tcb->tid==-1)
            break;
        }
      }

      n_tcbs--;
      fprintf(stderr,"JOIN %d\n", tid);

      pop_tcbs(tid);
      finish = false;
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

  if(daum_tcb != NULL)
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
            n_tcbs--;
            temp = list_first_entry(&tcbs, struct tcb, list);
        }
    }
    temp = NULL;
    free(t_context);
}