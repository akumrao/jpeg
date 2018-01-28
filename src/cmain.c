#include <stdio.h>
#include "cjpg.h"
//#include <sys/time.h>
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#define BYTE unsigned char

int main(int argc, char *argv[])
{
    /*
    BYTE* RGB = (BYTE*) malloc( 40*3*24);
    for( int x = 0; x < 40*3*24; ++x )
    {
      RGB[x] = x % 255;
    }
    
    WriteBMP24( "/var/tmp/my.bmp", 3, 3, RGB);
            
    BYTE* RGB1 = NULL;// = (BYTE*) malloc( 40*3*24);
   
    int height = 0;
    int width  = 0;
        
    int Xdiv8 = 0;
    int Ydiv8  = 0;
    
    ReadBMP24( "/var/tmp/my.bmp", &width, &height, &Xdiv8, &Ydiv8, &RGB1);
    
    
    
    dumpByte(RGB1, width*3,height);
     * 
     * 
     **/

    clock_t t;

    printf("Usage: %s /var/tmp/oddtest.bmp  /var/tmp/Doddtest2x2.jpg 2x2 \n", argv[0]);


    t = clock();

    _init_cjpg();
    
    if (argc >= 3)
    {

        if (argc == 4)
        {
            int h = 0; /* horizontal sampling factor (1..2) */
            int v = 0;

            sscanf(argv[3], "%dx%d", &h, &v);
            SaveJpgFile(argv[1], argv[2], h, v);

        } else
        {
            SaveJpgFile(argv[1], argv[2], 1, 1);
            SaveJpgFile(argv[1], argv[3], 2, 2);
        }

    } else
    {

        SaveJpgFile("/var/tmp/test2x2.bmp", "/var/tmp/test2x2.jpg", 2, 2);
    }

    _exit_cjpg();
    
    t = clock() - t;
    printf("It took me %ld clicks (%f seconds).\n", t, ((float) t) / CLOCKS_PER_SEC);

    return 0;

}

