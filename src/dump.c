#include <stdio.h>
#include <stdarg.h>    

FILE *fpDump = NULL;

void dprintf(const char *fmt, ...)
{
    va_list parms;
    char buf[256];

    // Try to print in the allocated space.
    va_start(parms, fmt);
    vsprintf(buf, fmt, parms);
    va_end(parms);

    // Write the information out to a txt file

    fprintf(fpDump, "%s", buf);
    printf("%s", buf);

}// End dprintf(..)



char g_bigBuf[1024] = {0};

char* IntToBinary(int val, int bits)
{
	int i;
	int c = 0;
    for (i = 0; i < 32; i++) g_bigBuf[i] = '\0';
 
    for (i = bits - 1; i >= 0; i--)
    {
        int on = (val & (1 << i)) ? 1 : 0;
        g_bigBuf[c] = on ? '1' : '0';
        c++;
    }

    return &g_bigBuf[0];
}

/***************************************************************************/


void DumpHufCodes(bitstring* m_blocks, int size)
{
	int i;
    dprintf("HufCodes\n");
    dprintf("Num: %d\n", size);

	for (i = 0; i < size; i++)
    {
        dprintf("%03d\t [%s] [%d]\n", i, IntToBinary(m_blocks[i].value, m_blocks[i].length), m_blocks[i].value);
    }
    dprintf("\n");
    
    fflush(stdout);

}

/***************************************************************************/

void DumpDCTValues(short dct[64])
{

	int i;
    int c = 0;
	dprintf("\n#Extracted DCT values from SOS#\n");
    for ( i = 0; i < 64; i++)
    {
        dprintf("% 4d  ", dct[c++]);

        if ((c > 0) && (c % 8 == 0)) dprintf("\n");
    }
    dprintf("\n");
    
    fflush(stdout);
}

void dumpByte(BYTE *dct, int dx, int dy)
{
	int j;
	int i;
    for (j = 0; j < dy; j++)
    {
        for ( i = 0; i < dx; i++)
        {
            dprintf("%d  ", dct[j * dx + i]);

        }
        dprintf("\n");
    }
    dprintf("\n");
    
    fflush(stdout);

}

void printColor(long *col, int size)
{
	int i;
    for (i = 0; i < size; i++)
        dprintf("%d ", col[i]);

    dprintf("\n");
    dprintf("\n");
    fflush(stdout);

}

void printLong(long *dct, int dx,  int dy)
{
	int i;
	int j;
    for (j = 0; j < dy; j++)
    {
        for (i = 0; i < dx; i++)
        {
            dprintf("%d  ", dct[j * dx + i]);

        }
        dprintf("\n");
    }
    dprintf("\n");
    
    fflush(stdout);

}

void printRangeLimit(BYTE* range_limit, int size)
{
	int i;
    for ( i = 0; i < size; i++)
        dprintf("%d ", range_limit[i]);

    dprintf("\n");
    dprintf("\n");
    fflush(stdout);

}

void dump8x8(long dct[64])
{

    int c = 0;
    int i = 0;
    for (; i < 64; i++)
    {
        dprintf("%4d  ", (int) dct[c++]);

        if ((c > 0) && (c % 8 == 0)) dprintf("\n");
    }
    dprintf("\n");
    fflush(stdout);
}
