/***************************************************************************/
/*                                                                         */
/*  File: savejpg.h                                                        */
/*  Author: bkenwright@xbdev.net                                           */
/*  URL: www.xbdev.net                                                     */
/*  Date: 19-01-06                                                         */
/*                                                                         */
/***************************************************************************/
/*

Tiny Simplified C source of a JPEG encoder.


The principles of designing my JPEG encoder were mainly clarity and portability.
Kept as simple as possibly, using basic c in most places for portability.

The FDCT routine is taken with minor modifications from Independent JPEG Group's
JPEG software . So don't ask me details about that. 

The program should give the same results on any C compiler which provides at least
256 kb of free memory. I needed that for the precalculated bitcode (3*64k) and
category (64k) arrays , not to mention the memory needed for the truecolor BMP.

Since it's made to encode a truecolor BMP into a JPG file, I think that it
should be no problem for you to figure out how to modify the C source in order
to use it with any RGB image you want to compress (small enough to fit into
your memory) .

A note: The JPG format I used is a personal choice:
	The sampling factors are 1:1 for simplicity reasons, the Huffman tables
 are those given in the standard (so it's not optimized for that particular
 image you want to encode), etc.
I coded only what I needed from a JPEG encoder. Probably it could have 
more advanced features.

There are a lot of other JPG formats (markers in another order, another markers),
so don't ask me why a JPG file not encoded with this encoder has a different
format (different locations , another markers...).

*/

/***************************************************************************/

#ifndef SAVEJPG_H
#define SAVEJPG_H

#ifdef	__cplusplus
extern "C"
{
#endif

void _init_cjpg();
void SaveJpgFile(char *szBmpFileNameIn, char* szJpgFileNameOut, int h, int v);
void ReadBMP24(const char* szBmpFileName, int* Width, int* Height, int* Xdiv8, int* Ydiv8, int hF, int vF,  unsigned char** RGB);
void _exit_cjpg();

#ifdef	__cplusplus
}
#endif


#endif //SAVEJPG_H