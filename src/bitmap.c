#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BYTE unsigned char

#pragma pack(1)

struct stBMFH // BitmapFileHeader & BitmapInfoHeader
{
    // BitmapFileHeader
    char bmtype[2]; // 2 bytes - 'B' 'M'
    unsigned int iFileSize; // 4 bytes
    short int reserved1; // 2 bytes
    short int reserved2; // 2 bytes
    unsigned int iOffsetBits; // 4 bytes
    // End of stBMFH structure - size of 14 bytes
    // BitmapInfoHeader
    unsigned int iSizeHeader; // 4 bytes - 40
    unsigned int iWidth; // 4 bytes
    unsigned int iHeight; // 4 bytes
    short int iPlanes; // 2 bytes
    short int iBitCount; // 2 bytes
    unsigned int Compression; // 4 bytes
    unsigned int iSizeImage; // 4 bytes
    unsigned int iXPelsPerMeter; // 4 bytes
    unsigned int iYPelsPerMeter; // 4 bytes
    unsigned int iClrUsed; // 4 bytes
    unsigned int iClrImportant; // 4 bytes
    // End of stBMIF structure - size 40 bytes
    // Total size - 54 bytes
};
#pragma pack()

static void exitmessage(char *error_message)
{
    printf("%s\n", error_message);
    exit(EXIT_FAILURE);
    return;
}

/***************************************************************************/
//
// Read a buffer in 24bits Bitmap (.bmp) format 
//

/***************************************************************************/
void ReadBMP24(const char* szBmpFileName, int* Width, int* Height, int* Xdiv8, int* Ydiv8, int hF, int vF, BYTE** RGB)
{

    BYTE *rgb;
	FILE* fp;
	int y;
    struct stBMFH bh;
	char temp[2048] = {0};
    memset(&bh, 0, sizeof (bh));
    sprintf(temp, "%s", szBmpFileName);
    fp = fopen(temp, "rb");
    if (fp == NULL)
    {
        printf("Could not open File %s \n", szBmpFileName);
        exitmessage("Could not open File");
    }


    fread(&bh, sizeof (bh), 1, fp);

    // Round up the width to the nearest DWORD boundary

    if (bh.bmtype[0] == 'B' && bh.bmtype[1] == 'M' && bh.iBitCount == 24)
    {
		int bufferSize;
         int i;
        int di = 0;

        int bufWidth = bh.iWidth * 3;
        int xfact = 8 * hF;
        int yfact = 8 * vF;

		int iNumPaddedBytes = 4 - bufWidth % 4;
        iNumPaddedBytes = iNumPaddedBytes % 4;


		  // fill infos
        *Height = bh.iHeight;
        *Width = bh.iWidth;

        // of the image
        *Xdiv8 = *Width + (xfact - *Width % xfact) % xfact;
        *Ydiv8 = bh.iHeight + (yfact - bh.iHeight % yfact) % yfact;

        // prepear buffer
        bufferSize = 3 * *Xdiv8 * *Ydiv8;
        rgb = malloc(bufferSize);
        if (rgb == NULL)
        {
            exitmessage("No heap memory left");
        }
        memset(rgb, 0, bufferSize);

 

        for ( y = bh.iHeight - 1; y >= 0; y--)
        {
			int x;
            for ( x = 0; x < bh.iWidth; x++)
            {
                unsigned int rgbpix;
                fread(&rgbpix, 3, 1, fp);

                i = (x + *Xdiv8 * y) * 3;

                rgb[i] = (int) ((rgbpix >> 16) & 0xFF);
                rgb[i + 1] = (int) ((rgbpix >> 8) & 0xFF);
                rgb[i + 2] = (int) (rgbpix & 0xFF);
            }
            di = 0;
            while (*Xdiv8 - bh.iWidth - di)
            {
                memcpy(rgb + i + 3 * (1 + di++), rgb + i, 3);
            }
            if (iNumPaddedBytes > 0)
            {
                BYTE pad = 0;
                fread(&pad, iNumPaddedBytes, 1, fp);
            }

        }

        for ( y = bh.iHeight; y < *Ydiv8; ++y)
        {
            memcpy(rgb + *Xdiv8 * y * 3, rgb + *Xdiv8 * (y - 1)*3, *Xdiv8 * 3);
        }

    }

    //dumpByte(rgb, *Width * 3, *Height);

    *RGB = rgb;

    fclose(fp);

    return;
}
/***************************************************************************/
//
// Save a buffer in 24bits Bitmap (.bmp) format 
//

/***************************************************************************/
void WriteBMP24(const char* szBmpFileName, int Width, int Height, int Xdiv8, int Ydiv8, BYTE* RGB)
{
	int y;
	struct stBMFH bh;
	FILE* fp;
	char temp[2048] = {0};

    // Round up the width to the nearest DWORD boundary
    int iNumPaddedBytes = 4 - (Width * 3) % 4;
    iNumPaddedBytes = iNumPaddedBytes % 4;

    memset(&bh, 0, sizeof (bh));
    bh.bmtype[0] = 'B';
    bh.bmtype[1] = 'M';
    bh.iFileSize = (Width * Height * 3) + (Height * iNumPaddedBytes) + sizeof (bh);
    bh.iOffsetBits = sizeof (struct stBMFH);
    bh.iSizeHeader = 40;
    bh.iPlanes = 1;
    bh.iWidth = Width;
    bh.iHeight = Height;
    bh.iBitCount = 24;

    sprintf(temp, "%s", szBmpFileName);
    fp = fopen(temp, "wb");
    if (fp == NULL)
    {
        printf("Could not create File %s \n", szBmpFileName);
        exitmessage("Could not create File");
    }
    fwrite(&bh, sizeof (bh), 1, fp);
    for (y = Height - 1; y >= 0; y--)
    {	int x;
        for ( x = 0; x < Width; x++)
        {
            int i = (x + (Xdiv8) * y) * 3;
            unsigned int rgbpix = (RGB[i] << 16) | (RGB[i + 1] << 8) | (RGB[i + 2] << 0);
            fwrite(&rgbpix, 3, 1, fp);
        }
        if (iNumPaddedBytes > 0)
        {
            unsigned int pad = 0;
            fwrite(&pad, iNumPaddedBytes, 1, fp);
        }
    }


    //dumpByte(RGB, Width * 3, Height);

    fclose(fp);
}

