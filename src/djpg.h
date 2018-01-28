/* 
 * File:   djpg.h
 * Author: root
 *
 * Created on September 4, 2014, 12:15 PM
 */
/***************************************************************************/
/*
    About:
	Simplified jpg/jpeg decoder image loader - so we can take a .jpg file
	either from memory or file, and convert it either to a .bmp or directly
	to its rgb pixel data information.

	Simplified, and only deals with basic jpgs, but it covers all the
	information of how the jpg format works :)
*/
/***************************************************************************/

#ifndef DJPG_H
#define	DJPG_H

#ifdef	__cplusplus
extern "C"
{
#endif

// Takes the rgb pixel values and creates a bitmap file
 void WriteBMP24(const char* szBmpFileName, int Width, int Height,int Xdiv8, int Ydiv8, unsigned char* RGB);

// Pass in the whole jpg file from memory, and it decodes it to RGB pixel data
 int DecodeJpgFileData(const unsigned char* buf, int sizeBuf, unsigned char** rgbpix, unsigned int* width, unsigned int* height, int* Xdiv8, int* Ydiv8);
// Don't forget to delete[] rgbpix if you use DecodeJpgFileData..

// Pass in the filename and it creates a bitmap
int ConvertJpgFile(char* szJpgFileInName, char * szBmpFileOutName);


#ifdef	__cplusplus
}
#endif

#endif	/* DJPG_H */

