/***************************************************************************/
/*                                                                         */
/*  File: djpg.                                                            */
/*  Author: Arvind Umrao <akumrao@yahoo.com>                               */
/*  Released on: 04-09-2014                                                */
/*                                                                         */
/*                                                                         */
/*                                                                         */
/***************************************************************************/
/*
    About:
        Simplified jpg/jpeg decoder image loader - so we can take a .jpg file
        either from memory or file, and convert it either to a .bmp or directly
        to its rgb pixel data information.

        Simplified, and only deals with basic jpgs, but it covers all the
        information of how the jpg format works :)

        Can be used to convert a jpg in memory to rgb pixels in memory.

        Or you can pass it a jpg file name and an output bmp filename, and it
        loads and writes out a bmp file.f

        i.e.
        ConvertJpgFile("cross.jpg", "cross.bmp")
 */
/***************************************************************************/
#include <stdio.h>
#include <string.h>		// memset(..)
#include <stdlib.h>		// sqrt(..), cos(..)


#include "djpg.h"
#include "djpgdef.h"

/*
#define DEBUGPRIVATE 1
#define DEBUGPUBLIC 1

*/

#ifdef DEBUGPUBLIC
#include "dump.c"
#endif 

/***************************************************************************/

#define DQT 	 0xDB	// Define Quantization Table
#define SOF 	 0xC0	// Start of Frame (size information)
#define DHT 	 0xC4	// Huffman Table
#define SOI 	 0xD8	// Start of Image
#define SOS 	 0xDA	// Start of Scan
#define EOI 	 0xD9	// End of Image, or End of File
#define APP0	 0xE0

#define BYTE_TO_WORD(x) (((x)[0]<<8)|(x)[1])

/***************************************************************************/
static const BYTE zigzag[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
};

static void exitmessage(char *error_message)
{
    printf("%s\n", error_message);
    exit(EXIT_FAILURE);
    return;
}

static void exitmessage1(char *error_message, int d)
{
    printf("%s %d\n", error_message, d);
    exit(EXIT_FAILURE);
    return;
}



 BYTE gtable[1408] = {0}; // (5 * (MAXJSAMPLE + 1) + CENTERJSAMPLE) * sizeof (BYTE));
void prepare_range_limit_table(struct stJpegData *jdata)
/* Allocate and fill in the sample_range_limit table */
{
    BYTE * table = gtable;
    int i;
     table += (MAXJSAMPLE + 1); /* allow negative subscripts of simple table */
    jdata->range_limit = table;
    /* First segment of "simple" table: limit[x] = 0 for x < 0 */
    MEMZERO(table - (MAXJSAMPLE + 1), (MAXJSAMPLE + 1) * sizeof (BYTE));
    /* Main part of "simple" table: limit[x] = x */
    for (i = 0; i <= MAXJSAMPLE; i++)
        table[i] = (BYTE) i;
    table += CENTERJSAMPLE; /* Point to where post-IDCT table starts */
    /* End of simple table, rest of first half of post-IDCT table */
    for (i = CENTERJSAMPLE; i < 2 * (MAXJSAMPLE + 1); i++)
        table[i] = MAXJSAMPLE;
    /* Second half of post-IDCT table */
    MEMZERO(table + (2 * (MAXJSAMPLE + 1)),
            (2 * (MAXJSAMPLE + 1) - CENTERJSAMPLE) * sizeof (BYTE));
    MEMCOPY(table + (4 * (MAXJSAMPLE + 1) - CENTERJSAMPLE),
            jdata->range_limit, CENTERJSAMPLE * sizeof (BYTE));
}

void build_ycc_rgb_table(struct stJpegData *jdata)
/* Normal case, sYCC */
{

    int i;
    long x;

    //memory leaks // fix it
    jdata->Cr_r_tab = (int *) malloc((MAXJSAMPLE + 1) * sizeof (int));
    jdata->Cb_b_tab = (int *) malloc((MAXJSAMPLE + 1) * sizeof (int));
    jdata->Cr_g_tab = (long *) malloc((MAXJSAMPLE + 1) * sizeof (long));
    jdata->Cb_g_tab = (long *) malloc((MAXJSAMPLE + 1) * sizeof (long));

    if (jdata->Cr_r_tab == NULL || jdata->Cb_b_tab == NULL || jdata->Cr_g_tab == NULL || jdata->Cb_g_tab == NULL)
        exitmessage("Not enough heap memory");

    for (i = 0, x = -CENTERJSAMPLE; i <= MAXJSAMPLE; i++, x++)
    {
        /* i is the actual input pixel value, in the range 0..MAXJSAMPLE */
        /* The Cb or Cr value we are thinking of is x = i - CENTERJSAMPLE */
        /* Cr=>R value is nearest int to 1.402 * x */
        jdata->Cr_r_tab[i] = (int)
                RIGHT_SHIFT(FIX(1.402) * x + ONE_HALF, 16);
        /* Cb=>B value is nearest int to 1.772 * x */
        jdata->Cb_b_tab[i] = (int)
                RIGHT_SHIFT(FIX(1.772) * x + ONE_HALF, 16);
        /* Cr=>G value is scaled-up -0.714136286 * x */
        jdata->Cr_g_tab[i] = (-FIX(0.714136286)) * x;
        /* Cb=>G value is scaled-up -0.344136286 * x */
        /* We also add in ONE_HALF so that need not do it in inner loop */
        jdata->Cb_g_tab[i] = (-FIX(0.344136286)) * x + ONE_HALF;
    }
}

/***************************************************************************/
// 
//  Returns the size of the file in bytes
//

/***************************************************************************/
int FileSize(FILE *fp)
{
    long pos;
    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return pos;
}


/***************************************************************************/

// Takes two array of bits, and build the huffman table for size, and code

/***************************************************************************/
void BuildHuffmanTable(const BYTE *nrcodes, const BYTE *std_table, struct stHuffmanTable *HT, int isDC)
{

    BYTE k, j;
    BYTE pos_in_table;
    WORD codevalue;
    
    int l;
    char huffsize[257];
	int numsymbols;
    unsigned int huffcode[257];
	unsigned int code;
    int si ;


	int i = 0;
    int p = 0;

	 int lookbits = 0;
	
	codevalue = 0;
    pos_in_table = 0;


    for (k = 1; k <= 16; k++)
    {
        for (j = 1; j <= nrcodes[k]; j++)
        {
            HT->m_blocks[std_table[pos_in_table]].value = codevalue;
            HT->m_blocks[std_table[pos_in_table]].length = k;
            pos_in_table++;
            codevalue++;
        }
        codevalue *= 2;
    }


    // int x = 1 << 8;
    //  x = 0;



    /* Figure F.15: generate decoding tables for bit-sequential decoding */
 
    for (l = 1; l <= 16; l++)
    {
        i = (int) nrcodes[l];

        while (i--)
            huffsize[p++] = (char) l;
    }
    huffsize[p] = 0;
    numsymbols = p;

    /* Figure C.2: generate the codes themselves */
    /* We also validate that the counts represent a legal Huffman code tree. */

    code = 0;
    si = huffsize[0];
    p = 0;
    while (huffsize[p])
    {
        while (((int) huffsize[p]) == si)
        {
            huffcode[p++] = code;
            code++;
        }
        /* code is now 1 more than the last code used for codelength si; but
         * it must still fit in si bits, since no code is allowed to be all ones.
         */
        code <<= 1;
        si++;
    }


    p = 0;
    for ( l = 1; l <= 16; l++)
    {
        if (nrcodes[l])
        {
            /* valoffset[l] = huffval[] index of 1st symbol of code length l,
             * minus the minimum code of length l
             */
            HT->valoffset[l] = (long) p - (long) huffcode[p];
            p += nrcodes[l];
            HT->maxcode[l] = huffcode[p - 1]; /* maximum code of length l */
        } else
        {
            HT->maxcode[l] = -1; /* -1 if no codes of this length */
        }
    }
    HT->maxcode[17] = 0xFFFFFL; /* ensures jpeg_huff_decode terminates */

    /* Compute lookahead tables to speed up decoding.
     * First we set all the table entries to 0, indicating "too long";
     * then we iterate through the Huffman codes that are short enough and
     * fill in all the entries that correspond to bit sequences starting
     * with that code.
     */

    memset(HT->look_nbits, 0, sizeof ( HT->look_nbits));
   
    p = 0;
    for ( l = 1; l <= HUFF_LOOKAHEAD; l++)
    {
		int i; 
        for ( i = 1; i <= (int) nrcodes[l]; i++, p++)
        {	
			int ctr;
            /* l = current code's length, p = its index in huffcode[] & huffval[]. */
            /* Generate left-justified code followed by all possible bit sequences */
            lookbits = huffcode[p] << (HUFF_LOOKAHEAD - l);
            for ( ctr = 1 << (HUFF_LOOKAHEAD - l); ctr > 0; ctr--)
            {
                HT->look_nbits[lookbits] = l;
                HT->look_sym[lookbits] = std_table[p];
                lookbits++;
            }
        }
    }

    /* Validate symbols as being reasonable.
     * For AC tables, we make no check, but accept all byte values 0..255.
     * For DC tables, we require the symbols to be in range 0..15.
     * (Tighter bounds could be applied depending on the data depth and mode,
     * but this is sufficient to ensure safe decoding.)
     */


    if (isDC)
    {
        HT->m_numBlocks = 16;

        for (i = 0; i < numsymbols; i++)
        {
            int sym = std_table[i];
            if (sym < 0 || sym > 15)
            {
                exitmessage("JERR_BAD_HUFF_TABLE");
                return;
            }
        }
    } else
        HT->m_numBlocks = 256;



    //GenHuffCodes(HT->m_numBlocks, HT->m_blocks, HT->hufval);
}

/***************************************************************************/

void PrintSOF(const BYTE *stream)
{
    int width;
    int height;
    int nr_components;
    int precision;

#ifdef DEBUGPUBLIC
    const char *nr_components_to_string[] = {"????",
        "Grayscale",
        "????",
        "YCbCr",
        "CYMK"};
#endif

    precision = stream[2];
    height = BYTE_TO_WORD(stream + 3);
    width = BYTE_TO_WORD(stream + 5);
    nr_components = stream[7];

#ifdef DEBUGPUBLIC
    dprintf("> SOF marker\n");
    dprintf("Size:%dx%d nr_components:%d (%s)  precision:%d\n",

            width, height,
            nr_components,
            nr_components_to_string[nr_components],
            precision);
#endif
}

/***************************************************************************/

int ParseSOF(struct stJpegData *jdata, const BYTE *stream)
{
    /*
    SOF		16		0xffc0		Start Of Frame
    Lf		16		3Nf+8		Frame header length
    P		8		8			Sample precision
    Y		16		0-65535		Number of lines
    X		16		1-65535		Samples per line
    Nf		8		1-255		Number of image components (e.g. Y, U and V).

    ---------Repeats for the number of components (e.g. Nf)-----------------
    Ci		8		0-255		Component identifier
    Hi		4		1-4			Horizontal Sampling Factor
    Vi		4		1-4			Vertical Sampling Factor
    Tqi		8		0-3			Quantization Table Selector.
     */
	int i;
    

    int height = BYTE_TO_WORD(stream + 3);
    int width = BYTE_TO_WORD(stream + 5);
    int nr_components = stream[7];
	PrintSOF(stream);
    stream += 8;
    for ( i = 0; i < nr_components; i++)
    {
        int cid = *stream++;
        int sampling_factor = *stream++;
        int Q_table = *stream++;

        struct stComponent *c = &jdata->m_component_info[cid];
        c->m_vFactor = sampling_factor & 0xf;
        c->m_hFactor = sampling_factor >> 4;
        c->m_qTable = jdata->m_Q_tables[Q_table];

#ifdef DEBUGPUBLIC
        dprintf("Component:%d  factor:%dx%d  Quantization table:%d\n",
                cid,
                c->m_hFactor,
                c->m_vFactor,
                Q_table);
#endif
    }
    jdata->m_width = width;
    jdata->m_height = height;

    return 0;
}


typedef void (*Jpeg_ifdct)(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col);
void jpeg_idct_ifast(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col);
void jpeg_idct_16x16(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col);
void jpeg_idct_16x8(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col);
void jpeg_idct_8x16(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col);
/***************************************************************************/
Jpeg_ifdct jpeg_ifdct;

void BuildQuantizationTable(struct stJpegData *jdata)
{
	long * table;
	int i;
    static const short aanscales[64] = {
        /* precomputed values scaled up by 14 bits */
        16384, 22725, 21407, 19266, 16384, 12873, 8867, 4520,
        22725, 31521, 29692, 26722, 22725, 17855, 12299, 6270,
        21407, 29692, 27969, 25172, 21407, 16819, 11585, 5906,
        19266, 26722, 25172, 22654, 19266, 15137, 10426, 5315,
        16384, 22725, 21407, 19266, 16384, 12873, 8867, 4520,
        12873, 17855, 16819, 15137, 12873, 10114, 6967, 3552,
        8867, 12299, 11585, 10426, 8867, 6967, 4799, 2446,
        4520, 6270, 5906, 5315, 4520, 3552, 2446, 1247
    };



    int hFactor = jdata->m_component_info[cY].m_hFactor;
    int vFactor = jdata->m_component_info[cY].m_vFactor;



    if (hFactor == 1 && vFactor == 1)
    {
        jpeg_ifdct = &jpeg_idct_ifast;
    } else if (hFactor == 1 && vFactor == 2)
    {
        jpeg_ifdct = &jpeg_idct_8x16;
    } else if (hFactor == 2 && vFactor == 1)
    {
        jpeg_ifdct = &jpeg_idct_16x8;
    } else if (hFactor == 2 && vFactor == 2)
    {
        jpeg_ifdct = &jpeg_idct_16x16;
    } else
    {
        char tmp[256];
        sprintf(tmp, "We do not support file format hFactorxvFactor %dx%d", hFactor, vFactor);
        exitmessage(tmp);
    }

    jdata->m_Y = (BYTE*) malloc(64 * hFactor * vFactor);
    jdata->m_Cb = (BYTE*) malloc(64 * hFactor * vFactor);
    jdata->m_Cr = (BYTE*) malloc(64 * hFactor * vFactor);

    if( jdata->m_Y == NULL || jdata->m_Cb == NULL || jdata->m_Cr == NULL )
    {
        exitmessage("Not enough memory.");
    }
    
    
    for ( i = 0; i < 64; i++)
    {
        table = jdata->m_Q_tables[0];
        table[i] = (int) DESCALE1((table[i] * aanscales[i]), 12);


        table = jdata->m_Q_tables[1];
        if (hFactor == 1 && vFactor == 1)
            table[i] = (int) DESCALE1((table[i] * aanscales[i]), 12);
        //else
        //ismtbl[i] = (ISLOW_MULT_TYPE) qtbl->quantval[i];
    }

#ifdef DEBUGPRIVATE    
    dprintf("Build Quantization Table\n");
    dump8x8(jdata->m_Q_tables[0]);
    dump8x8(jdata->m_Q_tables[1]);
#endif

}

void InitQuantizationTable(long *qtable, const BYTE *ref_table)
{
	int i;
	int c = 0;

    for (i = 0; i < 8; i++)
    {	int j;
        for (j = 0; j < 8; j++)
        {
            BYTE val = ref_table[c];
            qtable[zigzag[c]] = val;
            c++;
        }
    }

#ifdef DEBUGPUBLIC
    dump8x8(qtable);
#endif 

}

/***************************************************************************/

int ParseDQT(struct stJpegData *jdata, const BYTE *stream)
{
    int length, qi;
    long *table;

#ifdef DEBUGPUBLIC
    dprintf("> DQT marker\n");
#endif
    length = BYTE_TO_WORD(stream) - 2;
    stream += 2; // Skip length

    while (length > 0)
    {
		int qprecision;
		int qindex;

        qi = *stream++;

        qprecision = qi >> 4; // upper 4 bits specify the precision
        qindex = qi & 0xf; // index is lower 4 bits

        if (qprecision)
        {
            // precision in this case is either 0 or 1 and indicates the precision 
            // of the quantized values;
            // 8-bit (baseline) for 0 and  up to 16-bit for 1 

            exitmessage("Error - 16 bits quantization table is not supported\n");
            //DBG_HALT;
        }

        if (qindex >= 4)
        {
            exitmessage1("Error - No more 4 quantization table is supported (got %d)\n", qi);
            //DBG_HALT;
        }

        // The quantization table is the next 64 bytes
        table = jdata->m_Q_tables[qindex];

        // the quantization tables are stored in zigzag format, so we
        // use this functino to read them all in and de-zig zag them
        InitQuantizationTable(table, stream);
        stream += 64;
        length -= 65;
    }

    return 0;
}

/***************************************************************************/

int ParseSOS(struct stJpegData *jdata, const BYTE *stream)
{
    /*
    SOS		16		0xffd8			Start Of Scan
    Ls		16		2Ns + 6			Scan header length
    Ns		8		1-4				Number of image components
    Csj		8		0-255			Scan Component Selector
    Tdj		4		0-1				DC Coding Table Selector
    Taj		4		0-1				AC Coding Table Selector
    Ss		8		0				Start of spectral selection
    Se		8		63				End of spectral selection
    Ah		4		0				Successive Approximation Bit High
    Ai		4		0				Successive Approximation Bit Low
     */
	unsigned int i;
    unsigned int nr_components = stream[2];

#ifdef DEBUGPUBLIC
    dprintf("> SOS marker\n");
#endif
    if (nr_components != 3)
    {
        exitmessage("Error - We only support YCbCr image\n");
        //DBG_HALT;
    }


    stream += 3;
    for (i = 0; i < nr_components; i++)
    {
        unsigned int cid = *stream++;
        unsigned int table = *stream++;

        if ((table & 0xf) >= 4)
        {
            exitmessage("Error - We do not support more than 2 AC Huffman table\n");
            //DBG_HALT;
        }
        if ((table >> 4) >= 4)
        {
            exitmessage("Error - We do not support more than 2 DC Huffman table\n");
            //DBG_HALT;
        }
#ifdef DEBUGPUBLIC
        dprintf("ComponentId:%d  tableAC:%d tableDC:%d\n", cid, table & 0xf, table >> 4);
#endif

        jdata->m_component_info[cid].m_acTable = &jdata->m_HTAC[table & 0xf];
        jdata->m_component_info[cid].m_dcTable = &jdata->m_HTDC[table >> 4];
    }
    jdata->m_stream = stream + 3;
    return 0;
}

/***************************************************************************/
int ParseDHT(struct stJpegData *jdata, const BYTE *stream)
{
    /*
    u8 0xff 
    u8 0xc4 (type of segment) 
    u16 be length of segment 
    4-bits class (0 is DC, 1 is AC, more on this later) 
    4-bits table id 
    array of 16 u8 number of elements for each of 16 depths 
    array of u8 elements, in order of depth 
     */

    unsigned int count, i;
    BYTE *huff_bits;
    int length, index;

    length = BYTE_TO_WORD(stream) - 2;
    stream += 2; // Skip length

#ifdef DEBUGPUBLIC
    dprintf("> DHT marker (length=%d)\n", length);
#endif

    while (length > 0)
    {
        index = *stream++;

        // We need to calculate the number of bytes 'vals' will takes

        count = 0;
        if (index & 0xf0)
        {
            huff_bits = jdata->m_HTAC[index & 0xf].bits;
            for (i = 1; i < 17; i++)
            {
                huff_bits[i] = *stream++;
                count += huff_bits[i];
            }

            // AC
        } else
        {
            huff_bits = jdata->m_HTDC[index & 0xf].bits;
            for (i = 1; i < 17; i++)
            {
                huff_bits[i] = *stream++;
                count += huff_bits[i];
            }
            // dc
        }

        huff_bits[0] = 0;



        if (count > 256)
        {
            exitmessage("Error - No more than 1024 bytes is allowed to describe a huffman table");
            //DBG_HALT;
        }
        if ((index & 0xf) >= HUFFMAN_TABLES)
        {
            exitmessage1("Error - No mode than %d Huffman tables is supported\n", HUFFMAN_TABLES);
            //DBG_HALT;
        }
#ifdef DEBUGPUBLIC
        dprintf("Huffman table %s n%d\n", (index & 0xf0) ? "AC" : "DC", index & 0xf);
        dprintf("Length of the table: %d\n", count);
#endif

        if (index & 0xf0)
        {
            BYTE* huffval = jdata->m_HTAC[index & 0xf].hufval;
            for (i = 0; i < count; i++)
                huffval[i] = *stream++;

            BuildHuffmanTable(huff_bits, huffval, &jdata->m_HTAC[index & 0xf], 0); // AC
        } else
        {
            BYTE* huffval = jdata->m_HTDC[index & 0xf].hufval;
            for (i = 0; i < count; i++)
                huffval[i] = *stream++;

            BuildHuffmanTable(huff_bits, huffval, &jdata->m_HTDC[index & 0xf], 1); // DC
        }

        length -= 1;
        length -= 16;
        length -= count;
    }
#ifdef DEBUGPUBLIC
    dprintf("< DHT marker\n");
#endif
    return 0;
}

/***************************************************************************/

int ParseJFIF(struct stJpegData *jdata, const BYTE *stream)
{
    int chuck_len;
    int marker;
    int sos_marker_found = 0;
    int dht_marker_found = 0;

    prepare_range_limit_table(jdata);
    build_ycc_rgb_table(jdata);


#ifdef DEBUGPRIVATE
    dprintf("range_limit_table \n");
    printRangeLimit(jdata->range_limit, (5 * (MAXJSAMPLE + 1) + CENTERJSAMPLE));

    dprintf("Color tables \n");


    printColor((long*) jdata->Cr_r_tab, MAXJSAMPLE + 1);
    printColor((long*) jdata->Cb_b_tab, MAXJSAMPLE + 1);
    printColor(jdata->Cr_g_tab, MAXJSAMPLE + 1);
    printColor(jdata->Cb_g_tab, MAXJSAMPLE + 1);



    dprintf("\n");
    dprintf("\n");
#endif  
    // Parse marker
    while (!sos_marker_found)
    {
        if (*stream++ != 0xff)
        {
            goto bogus_jpeg_format;
        }

        // Skip any padding ff byte (this is normal)
        while (*stream == 0xff)
        {
            stream++;
        }

        marker = *stream++;
        chuck_len = BYTE_TO_WORD(stream);

        switch (marker)
        {
            case SOF:
            {

                if (ParseSOF(jdata, stream) < 0)
                    return -1;
                BuildQuantizationTable(jdata);
            }
                break;

            case DQT:
            {
#ifdef DEBUGPUBLIC
                dprintf("DQT Tables \n");
#endif
                if (ParseDQT(jdata, stream) < 0)
                    return -1;
            }
                break;

            case SOS:
            {

                if (ParseSOS(jdata, stream) < 0)
                    return -1;
                sos_marker_found = 1;
            }
                break;

            case DHT:
            {
                if (ParseDHT(jdata, stream) < 0)
                    return -1;
                dht_marker_found = 1;
            }
                break;

                // The reason I added these additional skips here, is because for
                // certain jpg compressions, like swf, it splits the encoding 
                // and image data with SOI & EOI extra tags, so we need to skip
                // over them here and decode the whole image
            case SOI:
            case EOI:
            {
                chuck_len = 0;
                break;
            }
                break;

            case 0xDD: //DRI: Restart_markers=1;
            {
                jdata->m_restart_interval = BYTE_TO_WORD(stream);
#ifdef DEBUGPUBLIC
                dprintf("DRI - Restart_marker\n");
#endif
            }
                break;

            case APP0:
            {
#ifdef DEBUGPUBLIC
                dprintf("APP0 Chunk ('txt' information) skipping\n");
#endif
            }
                break;

            default:
            {
#ifdef DEBUGPUBLIC
                dprintf("ERROR> Unknown marker %2.2x\n", marker);
#endif
            }
                break;
        }

        stream += chuck_len;
    }

    if (!dht_marker_found)
    {
        exitmessage("ERROR> No Huffman table loaded\n");
        //DBG_HALT;
    }

    return 0;

bogus_jpeg_format:
    exitmessage("ERROR> Bogus jpeg format\n");
    //DBG_HALT;
    return -1;
}

/***************************************************************************/

int JpegParseHeader(struct stJpegData *jdata, const BYTE *buf, unsigned int size)
{
	const BYTE* startStream;
    // Identify the file
    if ((buf[0] != 0xFF) || (buf[1] != SOI))
    {
        exitmessage("Not a JPG file ?\n");
        //DBG_HALT;
        return -1;
    }

    startStream = buf + 2;


#ifdef DEBUGPUBLIC
	{
    const int fileSize = size - 2;
    dprintf("-|- File thinks its size is: %d bytes\n", fileSize);
	}
#endif

   
    return ParseJFIF(jdata, startStream);
}

/***************************************************************************/

void JpegGetImageSize(struct stJpegData *jdata, unsigned int *width, unsigned int *height)
{

    *width = jdata->m_width;
    *height = jdata->m_height;
}


/***************************************************************************/
/*
 * Code for extracting next Huffman-coded symbol from input bit stream.
 * Again, this is time-critical and we make the main paths be macros.
 *
 * We use a lookahead table to process codes of up to HUFF_LOOKAHEAD bits
 * without looping.  Usually, more than 95% of the Huffman codes will be 8
 * or fewer bits long.  The few overlength codes are handled with a loop,
 * which need not be inline code.
 *
 * Notes about the HUFF_DECODE macro:
 * 1. Near the end of the data segment, we may fail to get enough bits
 *    for a lookahead.  In that case, we do it the hard way.
 * 2. If the lookahead table contains no entry, the next code must be
 *    more than HUFF_LOOKAHEAD bits long.
 * 3. jpeg_huff_decode returns -1 if forced to suspend.
 */

#define HUFF_DECODE(result,state,htbl,failaction,slowlabel) \
{ register int nb, look; \
  if (bits_left < HUFF_LOOKAHEAD) { \
    if (! jpeg_fill_bit_buffer(jdata,&state,get_buffer,bits_left, 0)) {failaction;} \
    get_buffer = state.get_buffer; bits_left = state.bits_left; \
    if (bits_left < HUFF_LOOKAHEAD) { \
      nb = 1; goto slowlabel; \
    } \
  } \
  look = PEEK_BITS(HUFF_LOOKAHEAD); \
  if ((nb = htbl->look_nbits[look]) != 0) { \
    DROP_BITS(nb); \
    result = htbl->look_sym[look]; \
  } else { \
    nb = HUFF_LOOKAHEAD+1; \
slowlabel: \
    if ((result=jpeg_huff_decode(jdata, &state,get_buffer,bits_left,htbl,nb)) < 0) \
	{ failaction; } \
    get_buffer = state.get_buffer; bits_left = state.bits_left; \
  } \
}

/*
 * Out-of-line code for bit fetching.
 * Note: current values of get_buffer and bits_left are passed as parameters,
 * but are returned in the corresponding fields of the state struct.
 *
 * On most machines MIN_GET_BITS should be 25 to allow the full 32-bit width
 * of get_buffer to be used.  (On machines with wider words, an even larger
 * buffer could be used.)  However, on some machines 32-bit shifts are
 * quite slow and take time proportional to the number of places shifted.
 * (This is true with most PC compilers, for instance.)  In this case it may
 * be a win to set MIN_GET_BITS to the minimum value of 15.  This reduces the
 * average shift distance at the cost of more calls to jpeg_fill_bit_buffer.
 */


#define BIT_BUF_SIZE 32
#define MIN_GET_BITS  (BIT_BUF_SIZE-7)

int jpeg_fill_bit_buffer(struct stJpegData *jdata, struct bitread_working_state *state,
        long get_buffer, int bits_left,
        int nbits)
/* Load up the bit buffer to a depth of at least nbits */
{
    /* Copy heavily used state fields into locals (hopefully registers) */
    const BYTE* next_input_byte = state->next_input_byte;
    //register size_t bytes_in_buffer = state->bytes_in_buffer;


    /* Attempt to load at least MIN_GET_BITS bits into get_buffer. */
    /* (It is assumed that no request will be for more than that many bits.) */
    /* We fail to do so only if we hit a marker or are forced to suspend. */

    if (jdata->unread_marker == 0)
    { /* cannot advance past a marker */
        while (bits_left < MIN_GET_BITS)
        {
            register int c;


            c = (*next_input_byte++);

            /* If it's 0xFF, check and discard stuffed zero byte */
            if (c == 0xFF)
            {
                /* Loop here to discard any padding FF's on terminating marker,
                 * so that we can save a valid unread_marker value.  NOTE: we will
                 * accept multiple FF's followed by a 0 as meaning a single FF data
                 * byte.  This data pattern is not valid according to the standard.
                 */
                do
                {
                    c = (*next_input_byte++);
                } while (c == 0xFF);

                if (c == 0)
                {
                    /* Found FF/00, which represents an FF data byte */
                    c = 0xFF;
                } else
                {
                    /* Oops, it's actually a marker indicating end of compressed data.
                     * Save the marker code for later use.
                     * Fine point: it might appear that we should save the marker into
                     * bitread working state, not straight into permanent state.  But
                     * once we have hit a marker, we cannot need to suspend within the
                     * current MCU, because we will read no more bytes from the data
                     * source.  So it is OK to update permanent state right away.
                     */
                    jdata->unread_marker = c;
                    /* See if we need to insert some fake zero bits. */
                    goto no_more_bytes;
                }
            }

            /* OK, load c into get_buffer */
            get_buffer = (get_buffer << 8) | c;
            bits_left += 8;
        } /* end while */
    } else
    {
no_more_bytes:
        /* We get here if we've read the marker that terminates the compressed
         * data segment.  There should be enough bits in the buffer register
         * to satisfy the request; if so, no problem.
         */
        if (nbits > bits_left)
        {

            /* Fill the buffer with zero bits */
            get_buffer <<= MIN_GET_BITS - bits_left;
            bits_left = MIN_GET_BITS;
        }
    }

    /* Unload the local registers */
    state->next_input_byte = next_input_byte;
    //state->bytes_in_buffer = bytes_in_buffer;
    state->get_buffer = get_buffer;
    state->bits_left = bits_left;

    return 1;
}


#define BIT_MASK(nbits)   bmask[nbits]
#define HUFF_EXTEND(x,s)  ((x) <= bmask[(s) - 1] ? (x) - bmask[s] : (x))

static const int bmask[16] = /* bmask[n] is mask for n rightmost bits */ {0, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF,
    0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF};


/*
 * These macros provide the in-line portion of bit fetching.
 * Use CHECK_BIT_BUFFER to ensure there are N bits in get_buffer
 * before using GET_BITS, PEEK_BITS, or DROP_BITS.
 * The variables get_buffer and bits_left are assumed to be locals,
 * but the state struct might not be (jpeg_huff_decode needs this).
 *	CHECK_BIT_BUFFER(state,n,action);
 *		Ensure there are N bits in get_buffer; if suspend, take action.
 *      val = GET_BITS(n);
 *		Fetch next N bits.
 *      val = PEEK_BITS(n);
 *		Fetch next N bits without removing them from the buffer.
 *	DROP_BITS(n);
 *		Discard next N bits.
 * The value N should be a simple variable, not an expression, because it
 * is evaluated multiple times.
 */

#define CHECK_BIT_BUFFER(state,nbits,action) \
	{ if (bits_left < (nbits)) {  \
	    if (! jpeg_fill_bit_buffer(jdata, &(state),get_buffer,bits_left,nbits))  \
	      { action; }  \
	    get_buffer = (state).get_buffer; bits_left = (state).bits_left; } }

#define GET_BITS(nbits) \
	(((int) (get_buffer >> (bits_left -= (nbits)))) & BIT_MASK(nbits))

#define PEEK_BITS(nbits) \
	(((int) (get_buffer >> (bits_left -  (nbits)))) & BIT_MASK(nbits))

#define DROP_BITS(nbits) \
	(bits_left -= (nbits))

/*
 * Out-of-line code for Huffman code decoding.
 */




int jpeg_huff_decode(struct stJpegData *jdata, struct bitread_working_state * state,
        long get_buffer, int bits_left,
        struct stHuffmanTable* htbl, int min_bits)
{
    register int l = min_bits;
    register long code;

    /* HUFF_DECODE has determined that the code is at least min_bits */
    /* bits long, so fetch that many bits in one swoop. */

    CHECK_BIT_BUFFER(*state, l, return -1);
    code = GET_BITS(l);

    /* Collect the rest of the Huffman code one bit at a time. */
    /* This is per Figure F.16 in the JPEG spec. */

    while (code > htbl->maxcode[l])
    {
        code <<= 1;
        CHECK_BIT_BUFFER(*state, 1, return -1);
        code |= GET_BITS(1);
        l++;
    }

    /* Unload the local registers */
    state->get_buffer = get_buffer;
    state->bits_left = bits_left;

    /* With garbage input we may reach the sentinel value l = 17. */

    if (l > 16)
    {
        exitmessage("JWRN_HUFF_BAD_CODE");
        return 0; /* fake a zero as the safest result */
    }

    return htbl->hufval[ (int) (code + htbl->valoffset[l]) ];
}

int ProcessHuffmanDataUnit(struct stJpegData *jdata, int indx)
{
    //DBG_ASSERT(indx>=0 && indx<4);
	int coef_limit;
    struct stComponent *c = &jdata->m_component_info[indx];
    long get_buffer = jdata->gl_state.bytes_in_buffer;
    int bits_left   =  jdata->gl_state.bits_left;

    struct stHuffmanTable *htbl = c->m_dcTable;

    int s = 0;
    int k = 0;
    int r = 0;

	memset(c->m_DCT, 0, sizeof (c->m_DCT)); //Initialize DCT_tcoeff

    HUFF_DECODE(s, jdata->br_state, htbl, return 0, label1);

    htbl = c->m_acTable;

    k = 1;
    coef_limit = 64;
   
        /* Convert DC difference to actual value, update last_dc_val */
        if (s)
        {
            CHECK_BIT_BUFFER(jdata->br_state, s, return 0);
            r = GET_BITS(s);
            s = HUFF_EXTEND(r, s);
        }

        s += c->m_previousDC;
        c->m_previousDC = s;
        /* Output the DC coefficient */
        c->m_DCT[0] = (short) s;

        /* Section F.2.2.2: decode the AC coefficients */
        /* Since zeroes are skipped, output area must be cleared beforehand */
        for (; k < coef_limit; k++)
        {

            HUFF_DECODE(s, jdata->br_state, htbl, return 0, label2);


            r = s >> 4;
            s &= 15;

            if (s)
            {
                k += r;
                CHECK_BIT_BUFFER(jdata->br_state, s, return 0);
                r = GET_BITS(s);
                s = HUFF_EXTEND(r, s);
                /* Output coefficient in natural (dezigzagged) order.
                 * Note: the extra entries in jpeg_natural_order[] will save us
                 * if k >= DCTSIZE2, which could happen if the data is corrupted.
                 */
                c->m_DCT[zigzag[k]] = (short) s;
            } else
            {
                if (r != 15)
                    goto EndOfBlock;
                k += 15;
            }
        }
   

    /* Section F.2.2.2: decode the AC coefficients */
    /* In this path we just discard the values */
    for (; k < 64; k++)
    {
        HUFF_DECODE(s, jdata->br_state, htbl, return 0, label3);

        r = s >> 4;
        s &= 15;

        if (s)
        {
            k += r;
            CHECK_BIT_BUFFER(jdata->br_state, s, return 0);
            DROP_BITS(s);
        } else
        {
            if (r != 15)
                break;
            k += 15;
        }
    }

EndOfBlock:
    ;



    jdata->gl_state.bytes_in_buffer = get_buffer;
    jdata->gl_state.bits_left = bits_left;
    
#ifdef DEBUGPRIVATE
    DumpDCTValues(c->m_DCT);
#endif

    return 1;
}

/***************************************************************************/

void YCrCB_to_RGB24_Block8x8(struct stJpegData *jdata, int hFactor, int vFactor, int imgx, int imgy, int imgw, int imgh)
{
    const BYTE *Y, *Cb, *Cr;
    BYTE *pix;
	int y;

    int * Crrtab = jdata->Cr_r_tab;
    int * Cbbtab = jdata->Cb_b_tab;
    long * Crgtab = jdata->Cr_g_tab;
    long * Cbgtab = jdata->Cb_g_tab;

    BYTE *range_limit = jdata->range_limit;

	Y = jdata->m_Y;
    Cb = jdata->m_Cb;
    Cr = jdata->m_Cr;


    //	dprintf("***pix***\n\n");
    for (y = 0; y < (8 * vFactor); y++)
    {	int x;
        for (x = 0; x < (8 * hFactor); x++)
        {

            // if (x + imgx >= imgw) continue;
            // if (y + imgy >= imgh) continue;

            int poff = x * 3 + imgw * 3 * y;
            int coff = (int) (x + y * 8 * hFactor);

            int y = Y[coff];
            int cb = Cb[coff];
            int cr = Cr[coff];

			pix = &(jdata->m_colourspace[poff]);

            pix[0] = range_limit[y + Crrtab[cr]];
            pix[1] = range_limit[y +
                    ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
                    SCALEBITS))];
            pix[2] = range_limit[y + Cbbtab[cb]];

        }
    }

}

/***************************************************************************/
void DecodeMCU(struct stJpegData *jdata, int hor, int ver)
{
    int y,x;

    for (y = 0; y < ver; y++)
    {
        for (x = 0; x < hor; x++)
        {
            ProcessHuffmanDataUnit(jdata, cY);
            jpeg_idct_ifast(jdata, &jdata->m_component_info[cY], &jdata->m_Y[64 * y * hor], 8 * x);
        }
    }

    // Cb
    ProcessHuffmanDataUnit(jdata, cCb);
    jpeg_ifdct(jdata, &jdata->m_component_info[cCb], jdata->m_Cb, 0);

    // Cr
    ProcessHuffmanDataUnit(jdata, cCr);
    jpeg_ifdct(jdata, &jdata->m_component_info[cCr], jdata->m_Cr, 0);


#ifdef DEBUGPUBLIC
    dprintf("After Dequatization and IDCT\n");
    dumpByte(jdata->m_Y, hor * 8, ver * 8);
    dumpByte(jdata->m_Cb, hor * 8, ver * 8);
    dumpByte(jdata->m_Cr, hor * 8, ver * 8);

#endif


}

/***************************************************************************/

int JpegDecode(struct stJpegData *jdata)
{
	int y;
    int hFactor = jdata->m_component_info[cY].m_hFactor;
    int vFactor = jdata->m_component_info[cY].m_vFactor;

	int xstride_by_mcu = 8 * hFactor;
    int ystride_by_mcu = 8 * vFactor;

    // RGB24:
    if (jdata->m_rgb == NULL)
    {

        int xfact = 8 * hFactor;
        int yfact = 8 * vFactor;
        // of the image
        jdata->XDiv8 = jdata->m_width + (xfact - jdata->m_width % xfact) % xfact;
        jdata->YDiv8 = jdata->m_height + (yfact - jdata->m_height % yfact) % yfact;

        jdata->m_rgb = (BYTE*) malloc(jdata->XDiv8 * 3 * jdata->YDiv8); // 100 is a safetly

#ifndef JPG_SPEEDUP
        // memset(jdata->m_rgb, 0, width * height);
#endif
    }

    jdata->br_state.next_input_byte = jdata->m_stream;

    jdata->m_component_info[0].m_previousDC = 0;
    jdata->m_component_info[1].m_previousDC = 0;
    jdata->m_component_info[2].m_previousDC = 0;
    jdata->m_component_info[3].m_previousDC = 0;



    // Don't forget to that block can be either 8 or 16 lines
    // unsigned int bytes_per_blocklines = jdata->XDiv8 * 3 * ystride_by_mcu;

    //unsigned int bytes_per_mcu = 3 * xstride_by_mcu;

    // Just the decode the image by 'macroblock' (size is 8x8, 8x16, or 16x16)
    for ( y = 0; y < (int) jdata->YDiv8; y += ystride_by_mcu)
    {	int x;
        for ( x = 0; x < (int) jdata->XDiv8; x += xstride_by_mcu)
        {
            jdata->m_colourspace = jdata->m_rgb + x * 3 + (y * jdata->XDiv8 * 3);

            // Decode MCU Plane
            DecodeMCU(jdata, hFactor, vFactor);

            //	if (x>20) continue;
            //	if (y>8) continue;

            YCrCB_to_RGB24_Block8x8(jdata, hFactor, vFactor, x, y, jdata->XDiv8, jdata->YDiv8);
        }
    }


    return 0;
}

/***************************************************************************/
//
// Take Jpg data, i.e. jpg file read into memory, and decompress it to an
// array of rgb pixel values.
//
// Note - Memory is allocated for this function, so delete it when finished
//

/***************************************************************************/
int DecodeJpgFileData(const BYTE* buf, // Jpg file in memory
        const int sizeBuf, // Size jpg in bytes in memory
        BYTE** rgbpix, // Output rgb pixels
        unsigned int* width, // Output image width
        unsigned int* height,
        int* Xdiv8, int* Ydiv8) // Output image height
{
    // Allocate memory for our decoded jpg structure, all our data will be
    // decompressed and stored in here for the various stages of our jpeg decoding
    struct stJpegData* jdec = (struct stJpegData*) malloc(sizeof (struct stJpegData));
    memset(jdec, 0, sizeof (struct stJpegData));

    if (jdec == NULL)
    {
        exitmessage("Not enough memory to alloc the structure need for decompressing\n");
        //DBG_HALT;
        return 0;
    }

    // Start Parsing.....reading & storing data
    if (JpegParseHeader(jdec, buf, sizeBuf) < 0)
    {
        exitmessage("ERROR > parsing jpg header\n");
        return -1;
    }

    // We've read it all in, now start using it, to decompress and create rgb values
#ifdef DEBUGPUBLIC
    dprintf("Decoding JPEG image...\n");
#endif
    
    jdec->gl_state.bytes_in_buffer = 0;
    jdec->gl_state.bits_left = 0;
    
    JpegDecode(jdec);

    // Get the size of the image
    JpegGetImageSize(jdec, width, height);

    *rgbpix = jdec->m_rgb;

    *Xdiv8 = jdec->XDiv8;
    *Ydiv8 = jdec->YDiv8;

    free(jdec->Cr_r_tab);
    free(jdec->Cb_b_tab);
    free(jdec->Cr_g_tab);
    free(jdec->Cb_g_tab);

    free(jdec->m_Y);
    free(jdec->m_Cb);
    free(jdec->m_Cr);

    // free(jdec->m_rgb);
    //  free(jdec->range_limit);

    // Release the memory for our jpeg decoder structure jdec
    free(jdec);

    return 1;
}

/***************************************************************************/
//
// Load one jpeg image, and decompress it, and save the result.
//

/***************************************************************************/
int ConvertJpgFile(char* szJpgFileInName, char * szBmpFileOutName)
{
    FILE *fp;
    unsigned int lengthOfFile;
    BYTE *buf;

	BYTE* rgbpix = NULL;
    unsigned int width = 0;
    unsigned int height = 0;

    int XDiv8 = 0;
    int YDiv8 = 0;


    #ifdef DEBUGPUBLIC
    fpDump = fopen("/var/tmp/output.txt", "a+");
    #endif

    // Load the Jpeg into memory
    fp = fopen(szJpgFileInName, "rb");
    if (fp == NULL)
    {
        printf("Cannot open jpg file: %s\n", szJpgFileInName);
        //DBG_HALT;
        return 0;
    }

    lengthOfFile = FileSize(fp);
    buf = malloc(lengthOfFile + 4); // +4 is safety padding
    if (buf == NULL)
    {
        exitmessage("Not enough memory for loading file\n");
        //DBG_HALT;
        return 0;
    }
    fread(buf, lengthOfFile, 1, fp);
    fclose(fp);

    DecodeJpgFileData(buf, lengthOfFile, &rgbpix, &width, &height, &XDiv8, &YDiv8);

    if (rgbpix == NULL)
    {
        exitmessage("Failed to decode jpg\n");
        //DBG_HALT;
        return 0;
    }

    // Delete our data we read in from the file
    free(buf);

    // Save it
    WriteBMP24(szBmpFileOutName, width, height, XDiv8, YDiv8, rgbpix);

    // Since we don't need the pixel information anymore, we must
    // release this as well
    free(rgbpix);

    
    #ifdef DEBUGPUBLIC
    fclose(fpDump);
    #endif  

    return 1;
}

/***************************************************************************/

