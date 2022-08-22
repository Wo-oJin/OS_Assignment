/**********************************************************************
 * Copyright (c) 2020
 *  Jinwoo Jeong <jjw8967@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdint.h>

#include "malloc.h"
#include "types.h"
#include "list_head.h"

#define ALIGNMENT 32
#define HDRSIZE sizeof(header_t)

static LIST_HEAD(free_list); // Don't modify this line
static algo_t g_algo;        // Don't modify this line
static void *bp;             // Don't modify thie line  


/***********************************************************************
 * size_cal()
 *
 * DESCRIPTION
 *   Convert the delivered size to a multiple of 32 to fit the 
 *   system format
 *
 * RETURN VALUE
 *   Return a converted size that fit the system format
 */
int size_cal(int size){
    if(size % 32 == 0)
        return size;
    else
        return ((size/32)+1)*32;
}

/***********************************************************************
 * extend_heap()
 *
 * DESCRIPTION
 *   allocate size of bytes of memory and returns a pointer to the
 *   allocated memory.
 *
 * RETURN VALUE
 *   Return a pointer to the allocated memory.
 */
void *my_malloc(size_t size)
{
  size = size_cal(size);

  struct list_head* tp;

  header_t* block_header;
  header_t* front_header;

  header_t* prev_header;
  header_t* next_header;

  bool Insert_to_mid = false;
  bool last_entry_free = false;
  
  if(g_algo == FIRST_FIT){
    list_for_each(tp, &free_list) {
        block_header = list_entry(tp, header_t, list); 
        if(block_header -> free == 1){ // free인 block을 발견
            if(block_header->size == size){ // 1. size가 같을 때
                block_header->free = 0;
                return (void*)(block_header + 1); // block header가 아닌 실제 memory를 가리키기 위해 +1(=block header의 size = (header_t의 크기))을 해줌
            }
            else if(block_header->size > size){ // 2. free block의 size가 더 클 때
                int origin_size = block_header->size;
                block_header->size = size;
                block_header->free = 0;

                header_t* new_header = block_header + (HDRSIZE + size)/HDRSIZE;
                new_header->size = origin_size - size - HDRSIZE;
                new_header->free = 1;

                __list_add(&(new_header->list),&(block_header->list),(&(block_header->list))->next);
                return (void*)(block_header + 1); // block header가 아닌 실제 memory를 가리키기 위해 +1(=block header의 size = (header_t의 크기))을 해줌
            }
            else{ // 3-1. 새로 block을 할당하기 전 마지막 block이 free인 경우 확인
                if (block_header == list_last_entry(&free_list, header_t, list)){
                    prev_header = front_header;
                    next_header = block_header;
                    Insert_to_mid = true;
                }
            }
        }
        front_header = block_header;
    }
  }
  else if(g_algo == BEST_FIT){
    int shortest_num = -1;
    int shortest_size = 1000000;
    int cnt = 0;

    list_for_each(tp, &free_list) { // size보다 크거나 같은 block 중 가장 작은 block 선택
        block_header = list_entry(tp, header_t, list); 
        if(block_header->free == 1 && (block_header->size <= (size_t)shortest_size) && (block_header->size >= (size_t)size_cal(size))){
            shortest_size = block_header->size;
            shortest_num = cnt;
        }
        cnt++;
    }

    block_header = list_last_entry(&free_list, header_t, list);
    if(block_header->free == 1 && shortest_num == -1) // 마지막 block이 free인지 확인
        last_entry_free = true;

    if(shortest_num!=-1 || block_header->free == 1){ // best-fit으로 block을 찾았거나, 마지막 block이 free인 경우
        cnt = 0;
        list_for_each(tp, &free_list) {
            block_header = list_entry(tp, header_t, list); 
            if(last_entry_free || (cnt == shortest_num)){
                if(!last_entry_free && block_header->size == size){ // 1. size가 같을 때
                    block_header->free = 0;
                    return (void*)(block_header + 1); // block header가 아닌 실제 memory를 가리키기 위해 +1(=block header의 size = (header_t의 크기))을 해줌
                }
                else if(!last_entry_free && block_header->size > size){ // 2. free block의 size가 더 클 때
                    int origin_size = block_header->size;
                    block_header->size = size;
                    block_header->free = 0;

                    header_t* new_header = block_header + (HDRSIZE + size)/HDRSIZE;
                    new_header->size = origin_size - size - HDRSIZE;
                    new_header->free = 1; 

                    __list_add(&(new_header->list),&(block_header->list),(&(block_header->list))->next);
                    return (void*)(block_header + 1); // block header가 아닌 실제 memory를 가리키기 위해 +1(=block header의 size = (header_t의 크기))을 해줌
                }
                else{ // 3-1. 새로 block을 할당하기 전 마지막 block이 free인 경우 확인
                    if (block_header == list_last_entry(&free_list, header_t, list)){
                        prev_header = front_header;
                        next_header = block_header;
                        Insert_to_mid = true;
                    }
                }
            }
        front_header = block_header;
        cnt++;
        }
    }
  }
  
  // 3-2. block을 새로 할당
  header_t* new_header = sbrk(size + HDRSIZE);
  new_header->size = size;
  new_header->free = 0;
    
  if(Insert_to_mid){ // 3-3. block을 새로 할당(마지막 블록 free O)
    __list_add(&(new_header->list),&(prev_header->list),&(next_header->list));
  }
  else{ // 3-3. block을 새로 할당(마지막 블록 free X)
    list_add_tail(&(new_header->list), &free_list);
  }

  return (void*)(new_header + 1); // new_header가 아닌 실제 memory를 가리키기 위해 +1(=block header의 size = (header_t의 크기))을 해줌
}

/***********************************************************************
 * my_realloc()
 *
 * DESCRIPTION
 *   tries to change the size of the allocation pointed to by ptr to
 *   size, and returns ptr. If there is not enough memory block,
 *   my_realloc() creates a new allocation, copies as much of the old
 *   data pointed to by ptr as will fit to the new allocation, frees
 *   the old allocation.
 *
 * RETURN VALUE
 *   Return a pointer to the reallocated memory
 */
void *my_realloc(void *ptr, size_t size)
{
  size = size_cal(size);

  struct list_head* tp;
  header_t* block_header;
  void* now = NULL;

  list_for_each(tp, &free_list) {
    block_header = list_entry(tp, header_t, list);
    now = (void*)block_header;
    if(now == (void*)0xdeadbedf){
        break;
    }
    else{
        if(now + HDRSIZE == ptr){ 
            if(block_header->size < size){ //1. 할당을 원하는 크기가 선택된 블록보다 큰 경우(할당 -> 데이터 이동 -> 해제)
                now = my_malloc(size); // 할당
            }
            else if(block_header->size == size) //2. 할당을 원하는 크기와 딱 맞는 경우
                return ptr; // 전달받은 ptr은 처음부터 payload를 가리키므로 그대로 반환

            my_free(ptr); //해제
            return now; // my_malloc으로 반환된 now는 이미 payload를 가리키고 있음
        }

    }
  }
  return now;
}

/***********************************************************************
 * my_free()
 *
 * DESCRIPTION
 *   deallocates the memory allocation pointed to by ptr.
 */
void my_free(void *ptr)
{
  struct list_head* tp;
  header_t* block_header;
  header_t* front_header;

  void *now;      
 
  list_for_each(tp, &free_list) {
    block_header = list_entry(tp, header_t, list);
    now = (void*)block_header;
    if(now == (void*)0xdeadbedf){
        break;
    }
    else{
        if(now + HDRSIZE == ptr){
            if (block_header != list_last_entry(&free_list, header_t, list)){
                header_t* next_header = list_next_entry(block_header, list);
     
                if(next_header->free == 1){
                    block_header->size += (next_header->size + HDRSIZE);
                    list_del(&(next_header->list));
                    next_header->free = 1;
                }
                
            }
            if(front_header!=NULL && front_header->free == 1){
                front_header->size += (block_header->size + HDRSIZE);
                list_del(&(block_header->list));
            }
            block_header -> free = 1;
            break;
        }
            
        front_header = block_header;
    }
  }
}

/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
/*          ****** EXCEPT TO mem_init() AND mem_deinit(). ******      */
void mem_init(const algo_t algo)
{
  g_algo = algo;
  bp = sbrk(0);
}

void mem_deinit()
{
  header_t *header;
  size_t size = 0;
  list_for_each_entry(header, &free_list, list) {
    size += HDRSIZE + header->size;
  }
  sbrk(-size);

  if (bp != sbrk(0)) {
    fprintf(stderr, "[Error] There is memory leak\n");
  }
}

void print_memory_layout()
{
  header_t *header;
  int cnt = 0;

  printf("===========================\n");
  list_for_each_entry(header, &free_list, list) {
    cnt++;
    printf("%c %ld\n", (header->free) ? 'F' : 'M', header->size);
  }

  printf("Number of block: %d\n", cnt);
  printf("===========================\n");
  return;
}