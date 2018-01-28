/* 
 * File:   djpegdef.h
 * Author: root
 *
 * Created on September 4, 2014, 12:13 PM
 */

#ifndef DJPEGDEF_H
#define	DJPEGDEF_H


#define HUFFMAN_TABLES	4
#define COMPONENTS	4

#define cY  1
#define cCb 2
#define cCr 3

#define HUFF_LOOKAHEAD 8
#define DCTSIZE 8

#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#define DESCALE1(x,n)  RIGHT_SHIFT((x) + (1 << ((n)-1)), n)
#define DESCALE(x,n)  RIGHT_SHIFT(x, n)
#define SCALEBITS	16	/* speediest right-shift on some machines */
#define ONE_HALF	((long) 1 << (SCALEBITS-1))
#define FIX(x)		((long) ((x) * (1L<<SCALEBITS) + 0.5))


#define BYTE unsigned char
#define SBYTE signed char
#define SWORD signed short int
#define WORD unsigned short int
#define DWORD unsigned long int
#define SDWORD signed long int

#define MAXJSAMPLE 255
#define CENTERJSAMPLE 128

#define MEMZERO(target,size)	memset((void *)(target), 0, (size_t)(size))
#define MEMCOPY(dest,src,size)	memcpy((void *)(dest), (const void *)(src), (size_t)(size))

typedef struct { BYTE length;
		 WORD value;} bitstring;

/***************************************************************************/
struct stHuffmanTable
{
    BYTE bits[17]; // 17 values from jpg file, 
    // k =1-16 ; L[k] indicates the number of Huffman codes of length k
    BYTE hufval[256]; // 256 codes read in from the jpeg file

    int m_numBlocks;
    bitstring m_blocks[256];

    /* Basic tables: (element [0] of each array is unused) */
    long maxcode[18]; /* largest code of length k (-1 if none) */
    /* (maxcode[17] is a sentinel to ensure jpeg_huff_decode terminates) */
    long valoffset[17]; /* huffval[] offset for codes of length k */
    /* valoffset[k] = huffval[] index of 1st symbol of code length k, less
     * the smallest code of length k; so given a code of length k, the
     * corresponding symbol is huffval[code + valoffset[k]]
     */
    /* Lookahead tables: indexed by the next HUFF_LOOKAHEAD bits of
     * the input data stream.  If the next Huffman code is no more
     * than HUFF_LOOKAHEAD bits long, we can obtain its length and
     * the corresponding symbol directly from these tables.
     */
    int look_nbits[1 << HUFF_LOOKAHEAD]; /* # bits, or 0 if too long */
    BYTE look_sym[1 << HUFF_LOOKAHEAD]; /* symbol, or unused */
};

struct stComponent
{
    unsigned int m_hFactor;
    unsigned int m_vFactor;
    long * m_qTable; // Pointer to the quantisation table to use
    struct stHuffmanTable* m_acTable;
    struct stHuffmanTable* m_dcTable;
    short int m_DCT[65]; // DCT coef
    int m_previousDC;
};


 struct bitread_working_state
{ /* Bitreading working state within an MCU */
    /* Current data source location */
    /* We need a copy, rather than munging the original, in case of suspension */
    const BYTE * next_input_byte; /* => next byte to read from source */
    size_t bytes_in_buffer; /* # of bytes remaining in source buffer */
    /* Bit input buffer --- note these values are kept in register variables,
     * not in this struct, inside the inner loops.
     */
    long get_buffer; /* current bit-extraction buffer */
    int bits_left; /* # of unused bits in it */
    /* Pointer needed by jpeg_fill_bit_buffer. */
};


struct stJpegData
{
    BYTE* m_rgb; // Final Red Green Blue pixel data
    unsigned int m_width; // Width of image
    unsigned int m_height; // Height of image

    unsigned int XDiv8; // Width of image
    unsigned int YDiv8; // Height of image
    
    const BYTE* m_stream; // Pointer to the current stream
    int m_restart_interval;

    struct stComponent m_component_info[COMPONENTS];

    long m_Q_tables[COMPONENTS][64]; // quantization tables
    struct stHuffmanTable m_HTDC[HUFFMAN_TABLES]; // DC huffman tables  
    struct stHuffmanTable m_HTAC[HUFFMAN_TABLES]; // AC huffman tables

    // Temp space used after the IDCT to store each components
    BYTE *m_Y;
    BYTE *m_Cr;
    BYTE *m_Cb;

    // Internal Pointer use for colorspace conversion, do not modify it !!!
    BYTE * m_colourspace;
    
    struct bitread_working_state br_state;  //local state
    struct bitread_working_state gl_state;  //global state
    
    int unread_marker;
  //  int x = ) * sizeof(BYTE);
    BYTE *range_limit;
   // struct My_color my_color;
    int* Cr_r_tab;		/* => table for Cr to R conversion */
    int* Cb_b_tab;		/* => table for Cb to B conversion */
    long* Cr_g_tab;		/* => table for Cr to G conversion */
    long* Cb_g_tab;		/* => table for Cb to G conversion */
    };




#endif	/* DJPEGDEF_H */

