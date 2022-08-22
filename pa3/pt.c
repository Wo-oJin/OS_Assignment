#include <stdio.h>
#include <unistd.h>

int main(void)
{
    void *curr_brk, *tmp_brk = NULL;
    int a;
    /* sbrk(0) gives current program break location */
    a = sbrk(0);
    printf("prog break1 : %d\n", a);
}