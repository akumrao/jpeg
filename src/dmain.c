#include <stdio.h>
#include "djpg.h"   
//#include <sys/time.h>
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */

int main(int argc, char *argv[])
{

    clock_t t;

    printf("Usage: %s /var/tmp/test1.jpg /var/tmp/dtest1.bmp uri \n", argv[0]);


    t = clock();

    if (argc == 3)
    {
        ConvertJpgFile(argv[1], argv[2]);
    }

    t = clock() - t;
    printf("It took me %ld clicks (%f seconds).\n", t, ((float) t) / CLOCKS_PER_SEC);


    return 0;	

}

