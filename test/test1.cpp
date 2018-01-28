/**
 *   \file cjpg.h
 *   Copyright (c) 2014 Arvind Umrao< akumrao@yahoo.com>
 *
 *   \brief test case for jpeg parser 
 */

#include <stdio.h>
#include "cjpg.h"
#include "djpg.h"
//#include <sys/time.h>
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#define BYTE unsigned char

int compare_two_binary_files(char const* file1, char const* file2)
{

    FILE *fp1; FILE *fp2;
    int flag = -1;

    fp1 = fopen(file1, "rb");
    if (fp1 == NULL)
    {
	printf("\nError in opening file %s\n", file1);
        return flag;
    }


    fp2 = fopen(file2, "rb");
    if (fp2 == NULL)
    {
	printf("\nError in opening file %s\n", file2);
        return flag;
    }


    int ch1, ch2;
    
    while (((ch1 = fgetc(fp1)) != EOF) && ((ch2 = fgetc(fp2)) != EOF))

    {

	/*
	 * character by character comparision
	 * if equal then continue by comparing till the end of files
	 */
	if (ch1 == ch2)
	{
	    flag = 0;

	    continue;

	}/*
          * If not equal then returns the byte position
          */
	else
	{
	    fseek(fp1, -1, SEEK_CUR);
	    flag = -1;
	    break;
	}
    }

    fclose(fp1);
    fclose(fp2);

    if (flag == -1)
    {
	printf("Two files are not equal %s %s:  byte poistion at which two files differ is %ld\n", file1, file2, ftell(fp1) + 1);
    } else
    {
	printf("Two files are Equal %s %s \n ", file1, file2);
    }

    return flag;

}

int main(int argc, char **argv)
{
    int flag = -1;
    
    char input[256];
    char output[256];;
    char test[256];;
    if (argc > 1)
    {
	sprintf(input, "%s", argv[1]);
	sprintf(output, "%s", "test_gen.jpg");
	sprintf(test, "%s", argv[2]);
	
    } else
    {
	printf("Error: No input file provided.\n");
	return -1;
    }
    
    
    printf("Input File:%s\n", input);
    printf("OutPut file:%s\n", output);
    printf("Target Test File:%s\n", test);
    
    
    _init_cjpg();
    
    SaveJpgFile(input, output, 1, 1);
    flag = compare_two_binary_files(output, test);

    sprintf(input, "%s", argv[1]);
    sprintf(output, "%s", "test2x2_gen.jpg");
    sprintf(test, "%s", "test2x2.jpg");
 
    printf("Input File:%s\n", input);
    printf("OutPut file:%s\n", output);
    printf("Target Test File:%s\n", test);
    
    SaveJpgFile(input, output, 2, 2);
    flag = compare_two_binary_files(output, test);
    
    
    _exit_cjpg();
    return flag;
}


