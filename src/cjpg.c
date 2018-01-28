/***************************************************************************/
/*                                                                         */
/*  File: savejpg.h                                                        */
/*  Author: Arvind Umrao<akumrao@yahoo.com>                                         */

/*                                                                         */
/***************************************************************************/
/*
        Tiny Simplified C source of a JPEG encoder.
        A BMP truecolor to JPEG encoder

 *.bmp -> *.jpg
 */
/***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctables.h"
#include "cjpgdef.h"
#include "cjpg.h"
/*
#define DEBUGPRIVATE
#define DEBUGPUBLIC
*/

#ifdef DEBUGPUBLIC

#include "dump.c"
#endif 



/***************************************************************************/


// The Huffman tables we'll use:
static bitstring YDC_HT[12];
static bitstring CbDC_HT[12];
static bitstring YAC_HT[256];
static bitstring CbAC_HT[256];

static BYTE *category_alloc;
static BYTE *category; //Here we'll keep the category of the numbers in range: -32767..32767
static bitstring *bitcode_alloc;
static bitstring *bitcode; // their bitcoded representation

//Precalculated tables for a faster YCbCr->RGB transformation
// We use a SDWORD table because we'll scale values by 2^16 and work with integers
static SDWORD YRtab[256], YGtab[256], YBtab[256];
static SDWORD CbRtab[256], CbGtab[256], CbBtab[256];
static SDWORD CrRtab[256], CrGtab[256], CrBtab[256];
static long fdtbl_Y[64];
static long fdtbl_Cb_fast[64]; //the same with the fdtbl_Cr[64]
static long fdtbl_Cb_slow[64]; //the same with the fdtbl_Cr[64]



/***************************************************************************/
//////////////////////////////////////////

void init_JpegOut(struct StJpegOut *jpOut)
{
    jpOut->free_in_buffer = OUTPUT_BUF_SIZE;
    jpOut->fp_jpeg_stream = fopen(jpOut->JPG_filename, "w+b");
    if( jpOut->fp_jpeg_stream == NULL)
    {
        #ifdef DEBUGPUBLIC
		dprintf( "File does not exist %s", jpOut->JPG_filename );
		#endif
		exitmessage("File does not exist.");
    }
    jpOut->next_output_byte = (unsigned char*) (malloc(OUTPUT_BUF_SIZE));
    if( jpOut->next_output_byte == NULL)
    {
        exitmessage("Not enough memory.");
    }

}

void write_JpegOut( struct StJpegOut *jpOut)
{

    jpOut->next_output_byte = jpOut->next_output_byte - (OUTPUT_BUF_SIZE - jpOut->free_in_buffer);
    fwrite(jpOut->next_output_byte, 1, (OUTPUT_BUF_SIZE - jpOut->free_in_buffer), jpOut->fp_jpeg_stream);

#ifdef _DEBUG
    fflush(jpOut->fp_jpeg_stream);
#endif

    jpOut->free_in_buffer = OUTPUT_BUF_SIZE;
}

void close_JpegOut(struct StJpegOut *jpOut)
{
    write_JpegOut(jpOut);
    fclose(jpOut->fp_jpeg_stream);
    free(jpOut->next_output_byte);

}

void emit_byte(struct StJpegOut *jpOut, unsigned char val)
/* Emit a byte */
{


    *jpOut->next_output_byte++ = val;


    if (--jpOut->free_in_buffer == 0)
    {

        write_JpegOut(jpOut);
    }
}
//////////////////////////////////////////

void write_APP0info(struct StJpegOut *jpOut )
//Nothing to overwrite for APP0info
{
    writeword(APP0info.marker);
    writeword(APP0info.length);
    writebyte('J');
    writebyte('F');
    writebyte('I');
    writebyte('F');
    writebyte(0);
    writebyte(APP0info.versionhi);
    writebyte(APP0info.versionlo);
    writebyte(APP0info.xyunits);
    writeword(APP0info.xdensity);
    writeword(APP0info.ydensity);
    writebyte(APP0info.thumbnwidth);
    writebyte(APP0info.thumbnheight);
}

void write_SOF0info(struct StJpegIn *stJpegIn, struct StJpegOut *jpOut)
// We should overwrite width and height
{
    writeword(SOF0info.marker);
    writeword(SOF0info.length);
    writebyte(SOF0info.precision);
    writeword(SOF0info.height);
    writeword(SOF0info.width);
    writebyte(SOF0info.nrofcomponents);
    writebyte(SOF0info.IdY);

    SOF0info.HVY = (stJpegIn->h_samp_factor << 4) + stJpegIn->v_samp_factor;

    writebyte(SOF0info.HVY);
    writebyte(SOF0info.QTY);
    writebyte(SOF0info.IdCb);
    writebyte(SOF0info.HVCb);
    writebyte(SOF0info.QTCb);
    writebyte(SOF0info.IdCr);
    writebyte(SOF0info.HVCr);
    writebyte(SOF0info.QTCr);
}

void write_DQTinfo( struct StJpegOut *jpOut)
{
    BYTE i;
    writeword(DQTinfo.marker);
    writeword(DQTinfo.length);
    writebyte(DQTinfo.QTYinfo);
    for (i = 0; i < 64; i++) writebyte(DQTinfo.Ytable[zigzag[i]]);
    writeword(DQTinfo.marker);
    writeword(DQTinfo.length);
    writebyte(DQTinfo.QTCbinfo);
    for (i = 0; i < 64; i++) writebyte(DQTinfo.Cbtable[zigzag[i]]);
}

void set_quant_table(BYTE *basic_table, BYTE scale_factor, BYTE *newtable)
// Set quantization table and zigzag reorder it
{
    BYTE i;
    long temp;
    for (i = 0; i < 64; i++)
    {
        temp = ((long) basic_table[i] * scale_factor + 50L) / 100L;
        //limit the values to the valid range
        if (temp <= 0L) temp = 1L;
        if (temp > 255L) temp = 255L; //limit to baseline range if requested

        //newtable[zigzag[i]] = (BYTE) temp;
        newtable[i] = (BYTE) temp;

    }
}

void set_DQTinfo()
{
    BYTE scalefactor = 50; // scalefactor controls the visual quality of the image
    // the smaller is, the better image we'll get, and the smaller
    // compression we'll achieve
    DQTinfo.marker = 0xFFDB;
    DQTinfo.length = 67;
    DQTinfo.QTYinfo = 0;
    DQTinfo.QTCbinfo = 1;
    set_quant_table(std_luminance_qt, scalefactor, DQTinfo.Ytable);
    set_quant_table(std_chrominance_qt, scalefactor, DQTinfo.Cbtable);
}

void write_DHTinfo(struct StJpegOut *jpOut )
{
    BYTE i;
    writeword(DHTinfo.marker);
    writeword((12 + 2 + 1 + 16)); // writeword(DHTinfo.length);
    writebyte(DHTinfo.HTYDCinfo);
    for (i = 0; i < 16; i++) writebyte(DHTinfo.YDC_nrcodes[i]);
    for (i = 0; i <= 11; i++) writebyte(DHTinfo.YDC_values[i]);
    writeword(DHTinfo.marker);
    writeword((162 + 2 + 1 + 16));
    writebyte(DHTinfo.HTYACinfo);
    for (i = 0; i < 16; i++) writebyte(DHTinfo.YAC_nrcodes[i]);
    for (i = 0; i <= 161; i++) writebyte(DHTinfo.YAC_values[i]);
    writeword(DHTinfo.marker);
    writeword((12 + 2 + 1 + 16));
    writebyte(DHTinfo.HTCbDCinfo);
    for (i = 0; i < 16; i++) writebyte(DHTinfo.CbDC_nrcodes[i]);
    for (i = 0; i <= 11; i++) writebyte(DHTinfo.CbDC_values[i]);
    writeword(DHTinfo.marker);
    writeword((162 + 2 + 1 + 16));
    writebyte(DHTinfo.HTCbACinfo);
    for (i = 0; i < 16; i++) writebyte(DHTinfo.CbAC_nrcodes[i]);
    for (i = 0; i <= 161; i++) writebyte(DHTinfo.CbAC_values[i]);
}

void set_DHTinfo()
{
    BYTE i;
    DHTinfo.marker = 0xFFC4;
    DHTinfo.length = 0x01A2;
    DHTinfo.HTYDCinfo = 0;
    for (i = 0; i < 16; i++) DHTinfo.YDC_nrcodes[i] = std_dc_luminance_nrcodes[i + 1];
    for (i = 0; i <= 11; i++) DHTinfo.YDC_values[i] = std_dc_luminance_values[i];
    DHTinfo.HTYACinfo = 0x10;
    for (i = 0; i < 16; i++) DHTinfo.YAC_nrcodes[i] = std_ac_luminance_nrcodes[i + 1];
    for (i = 0; i <= 161; i++) DHTinfo.YAC_values[i] = std_ac_luminance_values[i];
    DHTinfo.HTCbDCinfo = 1;
    for (i = 0; i < 16; i++) DHTinfo.CbDC_nrcodes[i] = std_dc_chrominance_nrcodes[i + 1];
    for (i = 0; i <= 11; i++) DHTinfo.CbDC_values[i] = std_dc_chrominance_values[i];
    DHTinfo.HTCbACinfo = 0x11;
    for (i = 0; i < 16; i++) DHTinfo.CbAC_nrcodes[i] = std_ac_chrominance_nrcodes[i + 1];
    for (i = 0; i <= 161; i++) DHTinfo.CbAC_values[i] = std_ac_chrominance_values[i];
}

void write_SOSinfo(struct StJpegOut *jpOut)
//Nothing to overwrite for SOSinfo
{
    writeword(SOSinfo.marker);
    writeword(SOSinfo.length);
    writebyte(SOSinfo.nrofcomponents);
    writebyte(SOSinfo.IdY);
    writebyte(SOSinfo.HTY);
    writebyte(SOSinfo.IdCb);
    writebyte(SOSinfo.HTCb);
    writebyte(SOSinfo.IdCr);
    writebyte(SOSinfo.HTCr);
    writebyte(SOSinfo.Ss);
    writebyte(SOSinfo.Se);
    writebyte(SOSinfo.Bf);
}

void write_comment(struct StJpegOut *jpOut, BYTE *comment)
{
    WORD i, length;
    writeword(0xFFFE); //The COM marker
    length = (WORD) strlen((const char *) comment);
    writeword((length + 2));
    for (i = 0; i < length; i++) writebyte(comment[i]);
}

void writebits(struct StJpegOut *jpOut, bitstring bs)
// A portable version; it should be done in assembler
{

    long put_buffer;
    int put_bits;

    /* if size is 0, caller used an invalid Huffman table entry */
    if (bs.length == 0)
    {
        // ERREXIT(entropy->cinfo, JERR_HUFF_MISSING_CODE);
        printf("JERR_HUFF_MISSING_CODE\n");
    }

    put_buffer = bs.value;

    put_bits = bs.length + jpOut->cur.put_bits;

    put_buffer <<= 24 - put_bits; /* align incoming bits */

    /* and merge with old buffer contents */
    put_buffer |= jpOut->cur.put_buffer;

    while (put_bits >= 8)
    {
        int c = (int) ((put_buffer >> 16) & 0xFF);

        writebyte(c);
        if (c == 0xFF)
        { /* need to stuff a zero byte? */
            writebyte(0);
        }
        put_buffer <<= 8;
        put_bits -= 8;
    }

    jpOut->cur.put_buffer = put_buffer; /* update variables */
    jpOut->cur.put_bits = put_bits;

}

void compute_Huffman_table(BYTE *nrcodes, BYTE *std_table, bitstring *HT)
{
    BYTE k, j;
    BYTE pos_in_table;
    WORD codevalue;
    codevalue = 0;
    pos_in_table = 0;
    for (k = 1; k <= 16; k++)
    {
        for (j = 1; j <= nrcodes[k]; j++)
        {
            HT[std_table[pos_in_table]].value = codevalue;
            HT[std_table[pos_in_table]].length = k;
            pos_in_table++;
            codevalue++;
        }
        codevalue *= 2;
    }

    //  int x = 1;
}

void init_Huffman_tables()
{
    compute_Huffman_table(std_dc_luminance_nrcodes, std_dc_luminance_values, YDC_HT);
    compute_Huffman_table(std_dc_chrominance_nrcodes, std_dc_chrominance_values, CbDC_HT);
    compute_Huffman_table(std_ac_luminance_nrcodes, std_ac_luminance_values, YAC_HT);
    compute_Huffman_table(std_ac_chrominance_nrcodes, std_ac_chrominance_values, CbAC_HT);

#ifdef DEBUGPRIVATE

    DumpHufCodes(YDC_HT, 12);
    DumpHufCodes(YAC_HT, 256);

    DumpHufCodes(CbDC_HT, 12);
    DumpHufCodes(CbAC_HT, 256);

#endif //DEBUGPRIVATE

}

void set_numbers_category_and_bitcode()
{
    SDWORD nr;
    SDWORD nrlower, nrupper;
    BYTE cat;

    category_alloc = (BYTE *) malloc(65535 * sizeof (BYTE));
    if (category_alloc == NULL) exitmessage("Not enough memory.");
    category = category_alloc + 32767; //allow negative subscripts
    bitcode_alloc = (bitstring *) malloc(65535 * sizeof (bitstring));
    if (bitcode_alloc == NULL) exitmessage("Not enough memory.");
    bitcode = bitcode_alloc + 32767;
    nrlower = 1;
    nrupper = 2;
    for (cat = 1; cat <= 15; cat++)
    {
        //Positive numbers
        for (nr = nrlower; nr < nrupper; nr++)
        {
            category[nr] = cat;
            bitcode[nr].length = cat;
            bitcode[nr].value = (WORD) nr;
        }
        //Negative numbers
        for (nr = -(nrupper - 1); nr <= -nrlower; nr++)
        {
            category[nr] = cat;
            bitcode[nr].length = cat;
            bitcode[nr].value = (WORD) (nrupper - 1 + nr);
        }
        nrlower <<= 1;
        nrupper <<= 1;
    }
}

void precalculate_YCbCr_tables()
{
#define ONE_HALF	((long) 1 << (16-1))
#define CBCR_OFFSET	((long) 128 << 16)


    WORD R, G, B;
    for (R = 0; R <= 255; R++)
    {
        YRtab[R] = (SDWORD) (65536 * 0.299 + 0.5) * R;
        CbRtab[R] = (SDWORD) (-(65536 * 0.168735892 + 0.5)) * R;
        CrRtab[R] = (SDWORD) ((65536 * 0.5 + 0.5)) * R + CBCR_OFFSET + ONE_HALF - 1;
    }
    for (G = 0; G <= 255; G++)
    {
        YGtab[G] = (SDWORD) (65536 * 0.587 + 0.5) * G;
        CbGtab[G] = (SDWORD) (-(65536 * 0.331264108 + 0.5)) * G;
        CrGtab[G] = (SDWORD) (-(65536 * 0.418687589 + 0.5)) * G;
    }
    for (B = 0; B <= 255; B++)
    {
        YBtab[B] = (SDWORD) ((65536 * 0.114 + 0.5) * B + ONE_HALF);
        CbBtab[B] = (SDWORD) (((65536 * 0.5 + 0.5))) * B + CBCR_OFFSET + ONE_HALF - 1;
        CrBtab[B] = (SDWORD) (-(65536 * 0.081312411 + 0.5)) * B;
    }
#ifdef DEBUGPRIVATE
    dprintf("Color tables \n");
    printColor(CrRtab, 256);
    printColor(CrGtab, 256);
    printColor(CrBtab, 256);

#endif

}

// Using a bit modified form of the FDCT routine from IJG's C source:
// Forward DCT routine idea taken from Independent JPEG Group's C source for
// JPEG encoders/decoders

// For float AA&N IDCT method, divisors are equal to quantization
//   coefficients scaled by scalefactor[row]*scalefactor[col], where
//   scalefactor[0] = 1
//   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
//   We apply a further scale factor of 8.
//   What's actually stored is 1/divisor so that the inner loop can
//   use a multiplication rather than a division.

void prepare_quant_tables()
{
    //    double aanscalefactor[8] = {1.0, 1.387039845, 1.306562965, 1.175875602,
    //      1.0, 0.785694958, 0.541196100, 0.275899379};

    short aanscales[64] = {
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




    int i = 0;
    for (i = 0; i < 64; i++)
    {

#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#define DESCALE(x,n)  RIGHT_SHIFT((x) + (1 << ((n)-1)), n)

        fdtbl_Y[i] = (long) DESCALE((DQTinfo.Ytable[i] * aanscales[i]), 11);

        fdtbl_Cb_fast[i] = (long) DESCALE((DQTinfo.Cbtable[i] * aanscales[i]), 11); // fast DCT

        fdtbl_Cb_slow[i] = ((long) DQTinfo.Cbtable[i]) << 3; // for slow DCT



    }

#ifdef DEBUGPRIVATE
    //dprintf("Build Qantization table \n");
    //dump8x8(fdtbl_Y);
    //dump8x8(fdtbl_Cb);
#endif // DEBUGPRIVATE
}





void jpeg_fdct_16x16(BYTE *data, long *outdata, int h, int v, unsigned int start_col);
void jpeg_fdct_8x8(BYTE* data, long* outdata,  int h, int v, unsigned int start_col);

void quantization(long *fdtbl, long *outdata)
{
    int i;
    long temp;
	long temp2;
    // dump(outdata);
    // Quantize/descale the coefficients, and store into output array
    for (i = 0; i < 64; i++)
    {
        temp = outdata[i];

        temp2 = outdata[i] * fdtbl[i];
        temp2 = (SWORD) ((SWORD) (temp2 + 16384.5) - 16384);


        if (temp < 0)
        {
            temp = -temp;
            temp += fdtbl[i] >> 1; /* for rounding */
            DIVIDE_BY(temp, fdtbl[i]);
            temp = -temp;
        } else
        {
            temp += fdtbl[i] >> 1; /* for rounding */
            DIVIDE_BY(temp, fdtbl[i]);
        }

        outdata[i] = temp;
    }
}

/*
 * Perform the forward DCT on a 16x16 sample block.
 */



void process_DU(BYTE *ComponentDU, long *fdtbl, SWORD *DC,
        bitstring *HTDC, bitstring *HTAC, Jpeg_fdct jpeg_fdct,struct StJpegOut *jpOut, unsigned int start_col)
{
    bitstring EOB = HTAC[0x00]; //endof block
    bitstring M16zeroes = HTAC[0xF0];
    int r, k;

    register int temp, temp2;

    SWORD Diff;

    //dump1(ComponentDU);
    #ifdef DEBUGPUBLIC
    dprintf("YUV values \n");
    dumpByte(ComponentDU, 8 * jpOut->h_samp_factor, 8 * jpOut->v_samp_factor);        
    #endif

    jpeg_fdct(ComponentDU, jpOut->DU_DCT, jpOut->h_samp_factor, jpOut->v_samp_factor, start_col);

#ifdef DEBUGPUBLIC
    dprintf("Before  quantization \n");
    dump8x8(jpOut->DU_DCT);
#endif

    quantization(fdtbl, jpOut->DU_DCT);

#ifdef DEBUGPUBLIC
    dprintf("After quantization \n");
    dump8x8(jpOut->DU_DCT);
#endif
    //zigzag reorder
    //for (i = 0; i <= 63; i++) DU[zigzag[i]] = DU_DCT[i];
    Diff = jpOut->DU_DCT[0]-*DC; //
    *DC = jpOut->DU_DCT[0];
    //Encode DC
    if (Diff == 0) writebits(jpOut, HTDC[0]); //Diff might be 0
    else
    {
        writebits(jpOut, HTDC[category[Diff]]);
        writebits(jpOut, bitcode[Diff]);
    }
    //Encode ACs


    r = 0; /* r = run length of zeros */

    for (k = 1; k <= 63; k++)
    {
        if ((temp2 = jpOut->DU_DCT[zigzag[k]]) == 0)
        {
            r++;
        } else
        {
            /* if run length > 15, must emit special run-length-16 codes (0xF0) */
            while (r > 15)
            {
                writebits(jpOut, M16zeroes);
                r -= 16;
            }
            //( r *16)
            temp = (r << 4) + category[temp2];

            writebits(jpOut, HTAC[temp]);
            writebits(jpOut, bitcode[temp2]);

            /* Emit that number of bits of the value, if positive, */

            r = 0;
        }
    }

    if (r > 0)
        writebits(jpOut,EOB);
}

void process_YDU(BYTE *ComponentDU, long *fdtbl, SWORD *DC,
        bitstring *HTDC, bitstring *HTAC, Jpeg_fdct jpeg_fdct,struct StJpegOut *jpOut, unsigned int start_col)
{
	int y,x;

    for ( y = 0; y < jpOut->v_samp_factor; ++y)
    {
        for ( x = 0; x < jpOut->h_samp_factor; ++x)
        {
            process_DU(ComponentDU + 64 * y*jpOut->h_samp_factor, fdtbl_Y, DC, HTDC, HTAC, jpeg_fdct,jpOut, 8 * x);
        }
    }
}

void load_data_units_from_RGB_buffer( struct StJpegIn *jpIn, WORD xpos, WORD ypos)
{
    BYTE x, y;
    BYTE pos = 0;
    DWORD location;
    BYTE R, G, B;
    location = (ypos * jpIn->Xdiv8 + xpos)*3;

    //for (ypos = 0; ypos < Ydiv8; ypos += )
    //for (xpos = 0; xpos < Xdiv8; xpos += )


    for (y = 0; y < 8 * jpIn->v_samp_factor; y++)
    {
        for (x = 0; x < 8 * jpIn->h_samp_factor; x++)
        {
            R = jpIn->RGB_buffer[location++];
            G = jpIn->RGB_buffer[location++];
            B = jpIn->RGB_buffer[location++];
            jpIn->YDU[pos] = Y(R, G, B);
            jpIn->CbDU[pos] = Cb(R, G, B);
            jpIn->CrDU[pos] = Cr(R, G, B);
            //location++;
            pos++;
        }



        location += (jpIn->Xdiv8 - 8 * jpIn->v_samp_factor)*3;
    }

#ifdef DEBUGPUBLIC
//    dprintf("YUV values \n");
  //  dumpByte(jpIn->YDU, 8 * jpIn->h_samp_factor, 8 * jpIn->v_samp_factor);
  //  dumpByte(jpIn->CbDU, 8 * jpIn->h_samp_factor, 8 * jpIn->v_samp_factor);
//    dumpByte(jpIn->CrDU, 8 * jpIn->h_samp_factor, 8 * jpIn->v_samp_factor);
#endif

}

void main_encoder(struct StJpegIn *jpIn, struct StJpegOut *jpOut)
{
    SWORD DCY = 0, DCCb = 0, DCCr = 0; //DC coefficients used for differential encoding
    WORD xpos, ypos;

    for (ypos = 0; ypos < jpIn->Ydiv8; ypos += 8 * jpIn->v_samp_factor)
        for (xpos = 0; xpos < jpIn->Xdiv8; xpos += 8 * jpIn->h_samp_factor)
        {
            load_data_units_from_RGB_buffer(jpIn, xpos, ypos);

            jpOut->process_du_Y(jpIn->YDU, fdtbl_Y, &DCY, YDC_HT, YAC_HT, jpOut->jpeg_fdctY,jpOut, 0);
            //return;
            process_DU(jpIn->CbDU, jpIn->fdtbl_Cb, &DCCb, CbDC_HT, CbAC_HT, jpOut->jpeg_fdctCbCr,jpOut,0);
            process_DU(jpIn->CrDU, jpIn->fdtbl_Cb, &DCCr, CbDC_HT, CbAC_HT, jpOut->jpeg_fdctCbCr,jpOut,0);
        }
}

void _init_cjpg()
{
    #ifdef DEBUGPUBLIC
    fpDump = fopen("/var/tmp/output.txt", "a+");
    #endif 

    set_DQTinfo();
    set_DHTinfo();
    init_Huffman_tables();
    set_numbers_category_and_bitcode();
    precalculate_YCbCr_tables();
    prepare_quant_tables();
   
}


void _exit_cjpg()
{
    
    free(category_alloc);
    free(bitcode_alloc);
    
    #ifdef DEBUGPUBLIC
    fclose(fpDump);
    #endif  
}

void SaveJpgFile(char *szBmpFileNameIn, char* szJpgFileNameOut, int h, int v)
{

    char BMP_filename[64];
    
    struct StJpegIn jpIn;
    struct StJpegOut jpegOut;
    int w_original, h_original; //the original image dimensions,
    struct StJpegOut *jpOut = &jpegOut;
    
    jpOut->cur.put_bits = 0;
    jpOut->cur.put_buffer = 0;
    
    jpIn.h_samp_factor = h; /* horizontal sampling factor (1..2) */
    jpIn.v_samp_factor = v;
    jpegOut.h_samp_factor = h; /* horizontal sampling factor (1..2) */
    jpegOut.v_samp_factor = v;
    
    

#ifdef DEBUGPUBLIC
    dprintf("Compress %s to %s, with sampling %dx%d \n",szBmpFileNameIn, szJpgFileNameOut, h,v );
#endif

    

    strcpy(BMP_filename, szBmpFileNameIn);
    strcpy(jpegOut.JPG_filename, szJpgFileNameOut);

 

    
    if (h == 1 && v == 1)
    {
        jpegOut.jpeg_fdctY = &jpeg_fdct_8x8;
        jpegOut.jpeg_fdctCbCr = &jpeg_fdct_8x8;
        jpegOut.process_du_Y = &process_DU;
        jpIn.fdtbl_Cb = fdtbl_Cb_fast;
    } else
    {
        jpegOut.jpeg_fdctY = &jpeg_fdct_8x8;
        jpegOut.jpeg_fdctCbCr = &jpeg_fdct_16x16;
        jpegOut.process_du_Y = &process_YDU;
        jpIn.fdtbl_Cb = fdtbl_Cb_slow;
    }
    
    jpIn.YDU = (BYTE*) malloc(64 * h * v);
    jpIn.CbDU = (BYTE*) malloc(64 * h * v);
    jpIn.CrDU = (BYTE*) malloc(64 * h * v);
    
    if( jpIn.YDU == NULL || jpIn.CbDU == NULL || jpIn.CrDU == NULL )
    {
        exitmessage("Not enough memory.");
    }

    init_JpegOut(jpOut);
    
    ReadBMP24(BMP_filename, &w_original, &h_original, &jpIn.Xdiv8, &jpIn.Ydiv8, h, v, &jpIn.RGB_buffer);

    
    
    SOF0info.width = w_original;
    SOF0info.height = h_original;
    
   
    writeword(0xFFD8); //SOI
    
    write_APP0info(jpOut);

    write_DQTinfo(jpOut);
    write_SOF0info( &jpIn, jpOut);
    write_DHTinfo(jpOut);
    write_SOSinfo(jpOut);

    //bytenew=0;bytepos=7;
    main_encoder(&jpIn, jpOut);
    //Do the bit alignment of the EOI marker


    if (jpegOut.cur.put_bits > 0) // Flush,  fill any partial byte with ones 
    {
        bitstring fillbits;
        fillbits.length = 7;
        fillbits.value = 0x7F;
        writebits(jpOut,fillbits);
        //jpegOut.cur.put_buffer = 0 ;
        //jpegOut.cur.put_bits = 0 ;
    }

    writeword(0xFFD9); //EOI
  
    close_JpegOut(jpOut);
    free(jpIn.RGB_buffer);
}

