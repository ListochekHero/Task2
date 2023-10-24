#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    int *p = (int *)malloc(sizeof(int));
    printf("address: %p\n", p);
    sleep(15);
    //write operation
    printf("write operation ----->\n");
    int s = 8;
    *p = s;
    sleep(5);
    //read operation
    printf("read operation ----->\n");
    int i = 0;
    i=*p;
    sleep(1);

    return 0;
}
