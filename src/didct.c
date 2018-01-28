#include <stdio.h>
#include <stdarg.h>     // So we can use ... (in dprintf)
#include "djpgdef.h"


/*
 * Each IDCT routine is responsible for range-limiting its results and
 * converting them to unsigned form (0..MAXJSAMPLE).  The raw outputs could
 * be quite far out of range if the input data is corrupt, so a bulletproof
 * range-limiting step is required.  We use a mask-and-table-lookup method
 * to do the combined operations quickly.  See the comments with
 * prepare_range_limit_table (in jdmaster.c) for more info.
 */

#define FIX_iFast_1_082392200  ((long)  277)		/* FIX(1.082392200) */
#define FIX_iFast_1_414213562  ((long)  362)		/* FIX(1.414213562) */
#define FIX_iFast_1_847759065  ((long)  473)		/* FIX(1.847759065) */
#define  FIX_iFast_2_613125930  ((long)  669)		/* FIX(2.613125930) */

#define RANGE_MASK  (MAXJSAMPLE * 4 + 3) /* 2 bits wider than legal samples */

#define IRIGHT_SHIFT(x,shft)	((x) >> (shft))
#define IDESCALE(x,n)  ((int) IRIGHT_SHIFT(x, n))



/* Multiply a DCTELEM variable by an long constant, and immediately
 * descale to yield a DCTELEM result.
 */

#define MULTIPLY(var,const)  ((long) DESCALE((var) * (const), 8))

#define DEQUANTIZE(coef,quantval)  (((int) (coef)) * (quantval))

#define BYTE unsigned char
#define PASS1_BITS 2


//Dequantization and IDCT;

void jpeg_idct_ifast(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col)
{
    long tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    long tmp10, tmp11, tmp12, tmp13;
    long z5, z10, z11, z12, z13;
    short* inptr;
    long * quantptr;
    int * wsptr;
    BYTE* outptr;
    BYTE *range_limit = jdata->range_limit + CENTERJSAMPLE;
    
    int hFactor = comp->m_hFactor;
    
    int ctr;
    int workspace[64]; /* buffers data between passes */


    /* Pass 1: process columns from input, store into work array. */

    inptr = comp->m_DCT;
    quantptr = (long *) comp->m_qTable;
    wsptr = workspace;
    for (ctr = DCTSIZE; ctr > 0; ctr--)
    {
        /* Due to quantization, we will usually find that many of the input
         * coefficients are zero, especially the AC terms.  We can exploit this
         * by short-circuiting the IDCT calculation for any column in which all
         * the AC terms are zero.  In that case each output is equal to the
         * DC coefficient (with scale factor as needed).
         * With typical images and quantization tables, half or more of the
         * column DCT calculations can be simplified this way.
         */

        if (inptr[DCTSIZE * 1] == 0 && inptr[DCTSIZE * 2] == 0 &&
                inptr[DCTSIZE * 3] == 0 && inptr[DCTSIZE * 4] == 0 &&
                inptr[DCTSIZE * 5] == 0 && inptr[DCTSIZE * 6] == 0 &&
                inptr[DCTSIZE * 7] == 0)
        {
            /* AC terms all zero */
            int dcval = (int) DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);

            wsptr[DCTSIZE * 0] = dcval;
            wsptr[DCTSIZE * 1] = dcval;
            wsptr[DCTSIZE * 2] = dcval;
            wsptr[DCTSIZE * 3] = dcval;
            wsptr[DCTSIZE * 4] = dcval;
            wsptr[DCTSIZE * 5] = dcval;
            wsptr[DCTSIZE * 6] = dcval;
            wsptr[DCTSIZE * 7] = dcval;

            inptr++; /* advance pointers to next column */
            quantptr++;
            wsptr++;
            continue;
        }

        /* Even part */

        tmp0 = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);
        tmp1 = DEQUANTIZE(inptr[DCTSIZE * 2], quantptr[DCTSIZE * 2]);
        tmp2 = DEQUANTIZE(inptr[DCTSIZE * 4], quantptr[DCTSIZE * 4]);
        tmp3 = DEQUANTIZE(inptr[DCTSIZE * 6], quantptr[DCTSIZE * 6]);

        tmp10 = tmp0 + tmp2; /* phase 3 */
        tmp11 = tmp0 - tmp2;

        tmp13 = tmp1 + tmp3; /* phases 5-3 */
        tmp12 = MULTIPLY(tmp1 - tmp3, FIX_iFast_1_414213562) - tmp13; /* 2*c4 */

        tmp0 = tmp10 + tmp13; /* phase 2 */
        tmp3 = tmp10 - tmp13;
        tmp1 = tmp11 + tmp12;
        tmp2 = tmp11 - tmp12;

        /* Odd part */

        tmp4 = DEQUANTIZE(inptr[DCTSIZE * 1], quantptr[DCTSIZE * 1]);
        tmp5 = DEQUANTIZE(inptr[DCTSIZE * 3], quantptr[DCTSIZE * 3]);
        tmp6 = DEQUANTIZE(inptr[DCTSIZE * 5], quantptr[DCTSIZE * 5]);
        tmp7 = DEQUANTIZE(inptr[DCTSIZE * 7], quantptr[DCTSIZE * 7]);

        z13 = tmp6 + tmp5; /* phase 6 */
        z10 = tmp6 - tmp5;
        z11 = tmp4 + tmp7;
        z12 = tmp4 - tmp7;

        tmp7 = z11 + z13; /* phase 5 */
        tmp11 = MULTIPLY(z11 - z13, FIX_iFast_1_414213562); /* 2*c4 */

        z5 = MULTIPLY(z10 + z12, FIX_iFast_1_847759065); /* 2*c2 */
        tmp10 = MULTIPLY(z12, FIX_iFast_1_082392200) - z5; /* 2*(c2-c6) */
        tmp12 = MULTIPLY(z10, -FIX_iFast_2_613125930) + z5; /* -2*(c2+c6) */

        tmp6 = tmp12 - tmp7; /* phase 2 */
        tmp5 = tmp11 - tmp6;
        tmp4 = tmp10 + tmp5;

        wsptr[DCTSIZE * 0] = (int) (tmp0 + tmp7);
        wsptr[DCTSIZE * 7] = (int) (tmp0 - tmp7);
        wsptr[DCTSIZE * 1] = (int) (tmp1 + tmp6);
        wsptr[DCTSIZE * 6] = (int) (tmp1 - tmp6);
        wsptr[DCTSIZE * 2] = (int) (tmp2 + tmp5);
        wsptr[DCTSIZE * 5] = (int) (tmp2 - tmp5);
        wsptr[DCTSIZE * 4] = (int) (tmp3 + tmp4);
        wsptr[DCTSIZE * 3] = (int) (tmp3 - tmp4);

        inptr++; /* advance pointers to next column */
        quantptr++;
        wsptr++;
    }

    /* Pass 2: process rows from work array, store into output array. */
    /* Note that we must descale the results by a factor of 8 == 2**3, */
    /* and also undo the PASS1_BITS scaling. */

    wsptr = workspace;
    for (ctr = 0; ctr < DCTSIZE; ctr++)
    {
        outptr = output_buf + 8 * hFactor * ctr + output_col;
        /* Rows of zeroes can be exploited in the same way as we did with columns.
         * However, the column calculation has created many nonzero AC terms, so
         * the simplification applies less often (typically 5% to 10% of the time).
         * On machines with very fast multiplication, it's possible that the
         * test takes more time than it's worth.  In that case this section
         * may be commented out.
         */

#ifndef NO_ZERO_ROW_TEST
        if (wsptr[1] == 0 && wsptr[2] == 0 && wsptr[3] == 0 && wsptr[4] == 0 &&
                wsptr[5] == 0 && wsptr[6] == 0 && wsptr[7] == 0)
        {
            /* AC terms all zero */
            BYTE dcval = range_limit[IDESCALE(wsptr[0], PASS1_BITS + 3)
                    & RANGE_MASK];

            outptr[0] = dcval;
            outptr[1] = dcval;
            outptr[2] = dcval;
            outptr[3] = dcval;
            outptr[4] = dcval;
            outptr[5] = dcval;
            outptr[6] = dcval;
            outptr[7] = dcval;

            wsptr += DCTSIZE; /* advance pointer to next row */
            continue;
        }
#endif

        /* Even part */

        tmp10 = ((long) wsptr[0] + (long) wsptr[4]);
        tmp11 = ((long) wsptr[0] - (long) wsptr[4]);

        tmp13 = ((long) wsptr[2] + (long) wsptr[6]);
        tmp12 = MULTIPLY((long) wsptr[2] - (long) wsptr[6], FIX_iFast_1_414213562)
                - tmp13;

        tmp0 = tmp10 + tmp13;
        tmp3 = tmp10 - tmp13;
        tmp1 = tmp11 + tmp12;
        tmp2 = tmp11 - tmp12;

        /* Odd part */

        z13 = (long) wsptr[5] + (long) wsptr[3];
        z10 = (long) wsptr[5] - (long) wsptr[3];
        z11 = (long) wsptr[1] + (long) wsptr[7];
        z12 = (long) wsptr[1] - (long) wsptr[7];

        tmp7 = z11 + z13; /* phase 5 */
        tmp11 = MULTIPLY(z11 - z13, FIX_iFast_1_414213562); /* 2*c4 */

        z5 = MULTIPLY(z10 + z12, FIX_iFast_1_847759065); /* 2*c2 */
        tmp10 = MULTIPLY(z12, FIX_iFast_1_082392200) - z5; /* 2*(c2-c6) */
        tmp12 = MULTIPLY(z10, -FIX_iFast_2_613125930) + z5; /* -2*(c2+c6) */

        tmp6 = tmp12 - tmp7; /* phase 2 */
        tmp5 = tmp11 - tmp6;
        tmp4 = tmp10 + tmp5;

        /* Final output stage: scale down by a factor of 8 and range-limit */

        outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS + 3)
                & RANGE_MASK];

        wsptr += DCTSIZE; /* advance pointer to next row */
    }
}



#define CONST_BITS  13

#define FIX_SLOW_0_298631336  ((long)  2446)	/* FIX_SLOW(0.298631336) */
#define FIX_SLOW_0_390180644  ((long)  3196)	/* FIX_SLOW(0.390180644) */
#define FIX_SLOW_0_541196100  ((long)  4433)	/* FIX_SLOW(0.541196100) */
#define FIX_SLOW_0_765366865  ((long)  6270)	/* FIX_SLOW(0.765366865) */
#define FIX_SLOW_0_899976223  ((long)  7373)	/* FIX_SLOW(0.899976223) */
#define FIX_SLOW_1_175875602  ((long)  9633)	/* FIX_SLOW(1.175875602) */
#define FIX_SLOW_1_501321110  ((long)  12299)	/* FIX_SLOW(1.501321110) */
#define FIX_SLOW_1_847759065  ((long)  15137)	/* FIX_SLOW(1.847759065) */
#define FIX_SLOW_1_961570560  ((long)  16069)	/* FIX_SLOW(1.961570560) */
#define FIX_SLOW_2_053119869  ((long)  16819)	/* FIX_SLOW(2.053119869) */
#define FIX_SLOW_2_562915447  ((long)  20995)	/* FIX_SLOW(2.562915447) */
#define FIX_SLOW_3_072711026  ((long)  25172)	/* FIX_SLOW(3.072711026) */

#define MULTIPLY16C16(var,const)  ((var) * (const))
#define CONST_SCALE (1 << CONST_BITS)
#define FIX_SLOW(x)	((long) ((x) * CONST_SCALE + 0.5))
void jpeg_idct_16x16(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col)
{
    long tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13;
    long tmp20, tmp21, tmp22, tmp23, tmp24, tmp25, tmp26, tmp27;
    long z1, z2, z3, z4;
    short* inptr;
    long * quantptr;
    int * wsptr;
    BYTE* outptr;
    BYTE* range_limit = jdata->range_limit + CENTERJSAMPLE;
    int ctr;
    int workspace[8 * 16]; /* buffers data between passes */

    /* Pass 1: process columns from input, store into work array. */

    inptr = comp->m_DCT;
    quantptr = (long *) comp->m_qTable;
    wsptr = workspace;
    for (ctr = 0; ctr < 8; ctr++, inptr++, quantptr++, wsptr++)
    {
        /* Even part */

        tmp0 = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);
        tmp0 <<= CONST_BITS;
        /* Add fudge factor here for final descale. */
        tmp0 += 1 << (CONST_BITS - PASS1_BITS - 1);

        z1 = DEQUANTIZE(inptr[DCTSIZE * 4], quantptr[DCTSIZE * 4]);
        tmp1 = MULTIPLY16C16(z1, FIX_SLOW(1.306562965)); /* c4[16] = c2[8] */
        tmp2 = MULTIPLY16C16(z1, FIX_SLOW_0_541196100); /* c12[16] = c6[8] */

        tmp10 = tmp0 + tmp1;
        tmp11 = tmp0 - tmp1;
        tmp12 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;

        z1 = DEQUANTIZE(inptr[DCTSIZE * 2], quantptr[DCTSIZE * 2]);
        z2 = DEQUANTIZE(inptr[DCTSIZE * 6], quantptr[DCTSIZE * 6]);
        z3 = z1 - z2;
        z4 = MULTIPLY16C16(z3, FIX_SLOW(0.275899379)); /* c14[16] = c7[8] */
        z3 = MULTIPLY16C16(z3, FIX_SLOW(1.387039845)); /* c2[16] = c1[8] */

        tmp0 = z3 + MULTIPLY16C16(z2, FIX_SLOW_2_562915447); /* (c6+c2)[16] = (c3+c1)[8] */
        tmp1 = z4 + MULTIPLY16C16(z1, FIX_SLOW_0_899976223); /* (c6-c14)[16] = (c3-c7)[8] */
        tmp2 = z3 - MULTIPLY16C16(z1, FIX_SLOW(0.601344887)); /* (c2-c10)[16] = (c1-c5)[8] */
        tmp3 = z4 - MULTIPLY16C16(z2, FIX_SLOW(0.509795579)); /* (c10-c14)[16] = (c5-c7)[8] */

        tmp20 = tmp10 + tmp0;
        tmp27 = tmp10 - tmp0;
        tmp21 = tmp12 + tmp1;
        tmp26 = tmp12 - tmp1;
        tmp22 = tmp13 + tmp2;
        tmp25 = tmp13 - tmp2;
        tmp23 = tmp11 + tmp3;
        tmp24 = tmp11 - tmp3;

        /* Odd part */

        z1 = DEQUANTIZE(inptr[DCTSIZE * 1], quantptr[DCTSIZE * 1]);
        z2 = DEQUANTIZE(inptr[DCTSIZE * 3], quantptr[DCTSIZE * 3]);
        z3 = DEQUANTIZE(inptr[DCTSIZE * 5], quantptr[DCTSIZE * 5]);
        z4 = DEQUANTIZE(inptr[DCTSIZE * 7], quantptr[DCTSIZE * 7]);

        tmp11 = z1 + z3;

        tmp1 = MULTIPLY16C16(z1 + z2, FIX_SLOW(1.353318001)); /* c3 */
        tmp2 = MULTIPLY16C16(tmp11, FIX_SLOW(1.247225013)); /* c5 */
        tmp3 = MULTIPLY16C16(z1 + z4, FIX_SLOW(1.093201867)); /* c7 */
        tmp10 = MULTIPLY16C16(z1 - z4, FIX_SLOW(0.897167586)); /* c9 */
        tmp11 = MULTIPLY16C16(tmp11, FIX_SLOW(0.666655658)); /* c11 */
        tmp12 = MULTIPLY16C16(z1 - z2, FIX_SLOW(0.410524528)); /* c13 */
        tmp0 = tmp1 + tmp2 + tmp3 -
                MULTIPLY16C16(z1, FIX_SLOW(2.286341144)); /* c7+c5+c3-c1 */
        tmp13 = tmp10 + tmp11 + tmp12 -
                MULTIPLY16C16(z1, FIX_SLOW(1.835730603)); /* c9+c11+c13-c15 */
        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW(0.138617169)); /* c15 */
        tmp1 += z1 + MULTIPLY16C16(z2, FIX_SLOW(0.071888074)); /* c9+c11-c3-c15 */
        tmp2 += z1 - MULTIPLY16C16(z3, FIX_SLOW(1.125726048)); /* c5+c7+c15-c3 */
        z1 = MULTIPLY16C16(z3 - z2, FIX_SLOW(1.407403738)); /* c1 */
        tmp11 += z1 - MULTIPLY16C16(z3, FIX_SLOW(0.766367282)); /* c1+c11-c9-c13 */
        tmp12 += z1 + MULTIPLY16C16(z2, FIX_SLOW(1.971951411)); /* c1+c5+c13-c7 */
        z2 += z4;
        z1 = MULTIPLY16C16(z2, -FIX_SLOW(0.666655658)); /* -c11 */
        tmp1 += z1;
        tmp3 += z1 + MULTIPLY16C16(z4, FIX_SLOW(1.065388962)); /* c3+c11+c15-c7 */
        z2 = MULTIPLY16C16(z2, -FIX_SLOW(1.247225013)); /* -c5 */
        tmp10 += z2 + MULTIPLY16C16(z4, FIX_SLOW(3.141271809)); /* c1+c5+c9-c13 */
        tmp12 += z2;
        z2 = MULTIPLY16C16(z3 + z4, -FIX_SLOW(1.353318001)); /* -c3 */
        tmp2 += z2;
        tmp3 += z2;
        z2 = MULTIPLY16C16(z4 - z3, FIX_SLOW(0.410524528)); /* c13 */
        tmp10 += z2;
        tmp11 += z2;

        /* Final output stage */

        wsptr[8 * 0] = (int) RIGHT_SHIFT(tmp20 + tmp0, CONST_BITS - PASS1_BITS);
        wsptr[8 * 15] = (int) RIGHT_SHIFT(tmp20 - tmp0, CONST_BITS - PASS1_BITS);
        wsptr[8 * 1] = (int) RIGHT_SHIFT(tmp21 + tmp1, CONST_BITS - PASS1_BITS);
        wsptr[8 * 14] = (int) RIGHT_SHIFT(tmp21 - tmp1, CONST_BITS - PASS1_BITS);
        wsptr[8 * 2] = (int) RIGHT_SHIFT(tmp22 + tmp2, CONST_BITS - PASS1_BITS);
        wsptr[8 * 13] = (int) RIGHT_SHIFT(tmp22 - tmp2, CONST_BITS - PASS1_BITS);
        wsptr[8 * 3] = (int) RIGHT_SHIFT(tmp23 + tmp3, CONST_BITS - PASS1_BITS);
        wsptr[8 * 12] = (int) RIGHT_SHIFT(tmp23 - tmp3, CONST_BITS - PASS1_BITS);
        wsptr[8 * 4] = (int) RIGHT_SHIFT(tmp24 + tmp10, CONST_BITS - PASS1_BITS);
        wsptr[8 * 11] = (int) RIGHT_SHIFT(tmp24 - tmp10, CONST_BITS - PASS1_BITS);
        wsptr[8 * 5] = (int) RIGHT_SHIFT(tmp25 + tmp11, CONST_BITS - PASS1_BITS);
        wsptr[8 * 10] = (int) RIGHT_SHIFT(tmp25 - tmp11, CONST_BITS - PASS1_BITS);
        wsptr[8 * 6] = (int) RIGHT_SHIFT(tmp26 + tmp12, CONST_BITS - PASS1_BITS);
        wsptr[8 * 9] = (int) RIGHT_SHIFT(tmp26 - tmp12, CONST_BITS - PASS1_BITS);
        wsptr[8 * 7] = (int) RIGHT_SHIFT(tmp27 + tmp13, CONST_BITS - PASS1_BITS);
        wsptr[8 * 8] = (int) RIGHT_SHIFT(tmp27 - tmp13, CONST_BITS - PASS1_BITS);
    }

    /* Pass 2: process 16 rows from work array, store into output array. */

    wsptr = workspace;
    for (ctr = 0; ctr < 16; ctr++)
    {
        outptr = output_buf+ 16*ctr + output_col;

        /* Even part */

        /* Add fudge factor here for final descale. */
        tmp0 = (long) wsptr[0] + (1 << (PASS1_BITS + 2));
        tmp0 <<= CONST_BITS;

        z1 = (long) wsptr[4];
        tmp1 = MULTIPLY16C16(z1, FIX_SLOW(1.306562965)); /* c4[16] = c2[8] */
        tmp2 = MULTIPLY16C16(z1, FIX_SLOW_0_541196100); /* c12[16] = c6[8] */

        tmp10 = tmp0 + tmp1;
        tmp11 = tmp0 - tmp1;
        tmp12 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;

        z1 = (long) wsptr[2];
        z2 = (long) wsptr[6];
        z3 = z1 - z2;
        z4 = MULTIPLY16C16(z3, FIX_SLOW(0.275899379)); /* c14[16] = c7[8] */
        z3 = MULTIPLY16C16(z3, FIX_SLOW(1.387039845)); /* c2[16] = c1[8] */

        tmp0 = z3 + MULTIPLY16C16(z2, FIX_SLOW_2_562915447); /* (c6+c2)[16] = (c3+c1)[8] */
        tmp1 = z4 + MULTIPLY16C16(z1, FIX_SLOW_0_899976223); /* (c6-c14)[16] = (c3-c7)[8] */
        tmp2 = z3 - MULTIPLY16C16(z1, FIX_SLOW(0.601344887)); /* (c2-c10)[16] = (c1-c5)[8] */
        tmp3 = z4 - MULTIPLY16C16(z2, FIX_SLOW(0.509795579)); /* (c10-c14)[16] = (c5-c7)[8] */

        tmp20 = tmp10 + tmp0;
        tmp27 = tmp10 - tmp0;
        tmp21 = tmp12 + tmp1;
        tmp26 = tmp12 - tmp1;
        tmp22 = tmp13 + tmp2;
        tmp25 = tmp13 - tmp2;
        tmp23 = tmp11 + tmp3;
        tmp24 = tmp11 - tmp3;

        /* Odd part */

        z1 = (long) wsptr[1];
        z2 = (long) wsptr[3];
        z3 = (long) wsptr[5];
        z4 = (long) wsptr[7];

        tmp11 = z1 + z3;

        tmp1 = MULTIPLY16C16(z1 + z2, FIX_SLOW(1.353318001)); /* c3 */
        tmp2 = MULTIPLY16C16(tmp11, FIX_SLOW(1.247225013)); /* c5 */
        tmp3 = MULTIPLY16C16(z1 + z4, FIX_SLOW(1.093201867)); /* c7 */
        tmp10 = MULTIPLY16C16(z1 - z4, FIX_SLOW(0.897167586)); /* c9 */
        tmp11 = MULTIPLY16C16(tmp11, FIX_SLOW(0.666655658)); /* c11 */
        tmp12 = MULTIPLY16C16(z1 - z2, FIX_SLOW(0.410524528)); /* c13 */
        tmp0 = tmp1 + tmp2 + tmp3 -
                MULTIPLY16C16(z1, FIX_SLOW(2.286341144)); /* c7+c5+c3-c1 */
        tmp13 = tmp10 + tmp11 + tmp12 -
                MULTIPLY16C16(z1, FIX_SLOW(1.835730603)); /* c9+c11+c13-c15 */
        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW(0.138617169)); /* c15 */
        tmp1 += z1 + MULTIPLY16C16(z2, FIX_SLOW(0.071888074)); /* c9+c11-c3-c15 */
        tmp2 += z1 - MULTIPLY16C16(z3, FIX_SLOW(1.125726048)); /* c5+c7+c15-c3 */
        z1 = MULTIPLY16C16(z3 - z2, FIX_SLOW(1.407403738)); /* c1 */
        tmp11 += z1 - MULTIPLY16C16(z3, FIX_SLOW(0.766367282)); /* c1+c11-c9-c13 */
        tmp12 += z1 + MULTIPLY16C16(z2, FIX_SLOW(1.971951411)); /* c1+c5+c13-c7 */
        z2 += z4;
        z1 = MULTIPLY16C16(z2, -FIX_SLOW(0.666655658)); /* -c11 */
        tmp1 += z1;
        tmp3 += z1 + MULTIPLY16C16(z4, FIX_SLOW(1.065388962)); /* c3+c11+c15-c7 */
        z2 = MULTIPLY16C16(z2, -FIX_SLOW(1.247225013)); /* -c5 */
        tmp10 += z2 + MULTIPLY16C16(z4, FIX_SLOW(3.141271809)); /* c1+c5+c9-c13 */
        tmp12 += z2;
        z2 = MULTIPLY16C16(z3 + z4, -FIX_SLOW(1.353318001)); /* -c3 */
        tmp2 += z2;
        tmp3 += z2;
        z2 = MULTIPLY16C16(z4 - z3, FIX_SLOW(0.410524528)); /* c13 */
        tmp10 += z2;
        tmp11 += z2;

        /* Final output stage */

        outptr[0] = range_limit[(int) RIGHT_SHIFT(tmp20 + tmp0,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[15] = range_limit[(int) RIGHT_SHIFT(tmp20 - tmp0,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[1] = range_limit[(int) RIGHT_SHIFT(tmp21 + tmp1,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[14] = range_limit[(int) RIGHT_SHIFT(tmp21 - tmp1,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[2] = range_limit[(int) RIGHT_SHIFT(tmp22 + tmp2,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[13] = range_limit[(int) RIGHT_SHIFT(tmp22 - tmp2,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[3] = range_limit[(int) RIGHT_SHIFT(tmp23 + tmp3,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[12] = range_limit[(int) RIGHT_SHIFT(tmp23 - tmp3,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[4] = range_limit[(int) RIGHT_SHIFT(tmp24 + tmp10,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[11] = range_limit[(int) RIGHT_SHIFT(tmp24 - tmp10,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[5] = range_limit[(int) RIGHT_SHIFT(tmp25 + tmp11,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[10] = range_limit[(int) RIGHT_SHIFT(tmp25 - tmp11,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[6] = range_limit[(int) RIGHT_SHIFT(tmp26 + tmp12,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[9] = range_limit[(int) RIGHT_SHIFT(tmp26 - tmp12,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[7] = range_limit[(int) RIGHT_SHIFT(tmp27 + tmp13,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[8] = range_limit[(int) RIGHT_SHIFT(tmp27 - tmp13,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];

        wsptr += 8; /* advance pointer to next row */
    }
}

/*
 * Perform dequantization and inverse DCT on one block of coefficients,
 * producing a 16x8 output block.
 *
 * 8-point IDCT in pass 1 (columns), 16-point in pass 2 (rows).
 */

void jpeg_idct_16x8(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col)
{
    long tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13;
    long tmp20, tmp21, tmp22, tmp23, tmp24, tmp25, tmp26, tmp27;
    long z1, z2, z3, z4;
    short* inptr;
    long* quantptr;
    int * wsptr;
    BYTE* outptr;
    BYTE* range_limit = jdata->range_limit + CENTERJSAMPLE;
    int ctr;
    int workspace[8 * 8]; /* buffers data between passes */

    /* Pass 1: process columns from input, store into work array.
     * Note results are scaled up by sqrt(8) compared to a true IDCT;
     * furthermore, we scale the results by 2**PASS1_BITS.
     * 8-point IDCT kernel, cK represents sqrt(2) * cos(K*pi/16).
     */

    inptr = comp->m_DCT;
    quantptr = (long *) comp->m_qTable;
    wsptr = workspace;
    for (ctr = DCTSIZE; ctr > 0; ctr--)
    {
        /* Due to quantization, we will usually find that many of the input
         * coefficients are zero, especially the AC terms.  We can exploit this
         * by short-circuiting the IDCT calculation for any column in which all
         * the AC terms are zero.  In that case each output is equal to the
         * DC coefficient (with scale factor as needed).
         * With typical images and quantization tables, half or more of the
         * column DCT calculations can be simplified this way.
         */

        if (inptr[DCTSIZE * 1] == 0 && inptr[DCTSIZE * 2] == 0 &&
                inptr[DCTSIZE * 3] == 0 && inptr[DCTSIZE * 4] == 0 &&
                inptr[DCTSIZE * 5] == 0 && inptr[DCTSIZE * 6] == 0 &&
                inptr[DCTSIZE * 7] == 0)
        {
            /* AC terms all zero */
            int dcval = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]) << PASS1_BITS;

            wsptr[DCTSIZE * 0] = dcval;
            wsptr[DCTSIZE * 1] = dcval;
            wsptr[DCTSIZE * 2] = dcval;
            wsptr[DCTSIZE * 3] = dcval;
            wsptr[DCTSIZE * 4] = dcval;
            wsptr[DCTSIZE * 5] = dcval;
            wsptr[DCTSIZE * 6] = dcval;
            wsptr[DCTSIZE * 7] = dcval;

            inptr++; /* advance pointers to next column */
            quantptr++;
            wsptr++;
            continue;
        }

        /* Even part: reverse the even part of the forward DCT.
         * The rotator is c(-6).
         */

        z2 = DEQUANTIZE(inptr[DCTSIZE * 2], quantptr[DCTSIZE * 2]);
        z3 = DEQUANTIZE(inptr[DCTSIZE * 6], quantptr[DCTSIZE * 6]);

        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW_0_541196100); /* c6 */
        tmp2 = z1 + MULTIPLY16C16(z2, FIX_SLOW_0_765366865); /* c2-c6 */
        tmp3 = z1 - MULTIPLY16C16(z3, FIX_SLOW_1_847759065); /* c2+c6 */

        z2 = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);
        z3 = DEQUANTIZE(inptr[DCTSIZE * 4], quantptr[DCTSIZE * 4]);
        z2 <<= CONST_BITS;
        z3 <<= CONST_BITS;
        /* Add fudge factor here for final descale. */
        z2 += 1 << (CONST_BITS - PASS1_BITS - 1);

        tmp0 = z2 + z3;
        tmp1 = z2 - z3;

        tmp10 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;
        tmp11 = tmp1 + tmp3;
        tmp12 = tmp1 - tmp3;

        /* Odd part per figure 8; the matrix is unitary and hence its
         * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
         */

        tmp0 = DEQUANTIZE(inptr[DCTSIZE * 7], quantptr[DCTSIZE * 7]);
        tmp1 = DEQUANTIZE(inptr[DCTSIZE * 5], quantptr[DCTSIZE * 5]);
        tmp2 = DEQUANTIZE(inptr[DCTSIZE * 3], quantptr[DCTSIZE * 3]);
        tmp3 = DEQUANTIZE(inptr[DCTSIZE * 1], quantptr[DCTSIZE * 1]);

        z2 = tmp0 + tmp2;
        z3 = tmp1 + tmp3;

        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW_1_175875602); /*  c3 */
        z2 = MULTIPLY16C16(z2, -FIX_SLOW_1_961570560); /* -c3-c5 */
        z3 = MULTIPLY16C16(z3, -FIX_SLOW_0_390180644); /* -c3+c5 */
        z2 += z1;
        z3 += z1;

        z1 = MULTIPLY16C16(tmp0 + tmp3, -FIX_SLOW_0_899976223); /* -c3+c7 */
        tmp0 = MULTIPLY16C16(tmp0, FIX_SLOW_0_298631336); /* -c1+c3+c5-c7 */
        tmp3 = MULTIPLY16C16(tmp3, FIX_SLOW_1_501321110); /*  c1+c3-c5-c7 */
        tmp0 += z1 + z2;
        tmp3 += z1 + z3;

        z1 = MULTIPLY16C16(tmp1 + tmp2, -FIX_SLOW_2_562915447); /* -c1-c3 */
        tmp1 = MULTIPLY16C16(tmp1, FIX_SLOW_2_053119869); /*  c1+c3-c5+c7 */
        tmp2 = MULTIPLY16C16(tmp2, FIX_SLOW_3_072711026); /*  c1+c3+c5-c7 */
        tmp1 += z1 + z3;
        tmp2 += z1 + z2;

        /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

        wsptr[DCTSIZE * 0] = (int) RIGHT_SHIFT(tmp10 + tmp3, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE * 7] = (int) RIGHT_SHIFT(tmp10 - tmp3, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE * 1] = (int) RIGHT_SHIFT(tmp11 + tmp2, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE * 6] = (int) RIGHT_SHIFT(tmp11 - tmp2, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE * 2] = (int) RIGHT_SHIFT(tmp12 + tmp1, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE * 5] = (int) RIGHT_SHIFT(tmp12 - tmp1, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE * 3] = (int) RIGHT_SHIFT(tmp13 + tmp0, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE * 4] = (int) RIGHT_SHIFT(tmp13 - tmp0, CONST_BITS - PASS1_BITS);

        inptr++; /* advance pointers to next column */
        quantptr++;
        wsptr++;
    }

    /* Pass 2: process 8 rows from work array, store into output array.
     * 16-point IDCT kernel, cK represents sqrt(2) * cos(K*pi/32).
     */

    wsptr = workspace;
    for (ctr = 0; ctr < 8; ctr++)
    {
        outptr = output_buf + ctr*16 + output_col;

        /* Even part */

        /* Add fudge factor here for final descale. */
        tmp0 = (long) wsptr[0] + (1 << (PASS1_BITS + 2));
        tmp0 <<= CONST_BITS;

        z1 = (long) wsptr[4];
        tmp1 = MULTIPLY16C16(z1, FIX_SLOW(1.306562965)); /* c4[16] = c2[8] */
        tmp2 = MULTIPLY16C16(z1, FIX_SLOW_0_541196100); /* c12[16] = c6[8] */

        tmp10 = tmp0 + tmp1;
        tmp11 = tmp0 - tmp1;
        tmp12 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;

        z1 = (long) wsptr[2];
        z2 = (long) wsptr[6];
        z3 = z1 - z2;
        z4 = MULTIPLY16C16(z3, FIX_SLOW(0.275899379)); /* c14[16] = c7[8] */
        z3 = MULTIPLY16C16(z3, FIX_SLOW(1.387039845)); /* c2[16] = c1[8] */

        tmp0 = z3 + MULTIPLY16C16(z2, FIX_SLOW_2_562915447); /* (c6+c2)[16] = (c3+c1)[8] */
        tmp1 = z4 + MULTIPLY16C16(z1, FIX_SLOW_0_899976223); /* (c6-c14)[16] = (c3-c7)[8] */
        tmp2 = z3 - MULTIPLY16C16(z1, FIX_SLOW(0.601344887)); /* (c2-c10)[16] = (c1-c5)[8] */
        tmp3 = z4 - MULTIPLY16C16(z2, FIX_SLOW(0.509795579)); /* (c10-c14)[16] = (c5-c7)[8] */

        tmp20 = tmp10 + tmp0;
        tmp27 = tmp10 - tmp0;
        tmp21 = tmp12 + tmp1;
        tmp26 = tmp12 - tmp1;
        tmp22 = tmp13 + tmp2;
        tmp25 = tmp13 - tmp2;
        tmp23 = tmp11 + tmp3;
        tmp24 = tmp11 - tmp3;

        /* Odd part */

        z1 = (long) wsptr[1];
        z2 = (long) wsptr[3];
        z3 = (long) wsptr[5];
        z4 = (long) wsptr[7];

        tmp11 = z1 + z3;

        tmp1 = MULTIPLY16C16(z1 + z2, FIX_SLOW(1.353318001)); /* c3 */
        tmp2 = MULTIPLY16C16(tmp11, FIX_SLOW(1.247225013)); /* c5 */
        tmp3 = MULTIPLY16C16(z1 + z4, FIX_SLOW(1.093201867)); /* c7 */
        tmp10 = MULTIPLY16C16(z1 - z4, FIX_SLOW(0.897167586)); /* c9 */
        tmp11 = MULTIPLY16C16(tmp11, FIX_SLOW(0.666655658)); /* c11 */
        tmp12 = MULTIPLY16C16(z1 - z2, FIX_SLOW(0.410524528)); /* c13 */
        tmp0 = tmp1 + tmp2 + tmp3 -
                MULTIPLY16C16(z1, FIX_SLOW(2.286341144)); /* c7+c5+c3-c1 */
        tmp13 = tmp10 + tmp11 + tmp12 -
                MULTIPLY16C16(z1, FIX_SLOW(1.835730603)); /* c9+c11+c13-c15 */
        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW(0.138617169)); /* c15 */
        tmp1 += z1 + MULTIPLY16C16(z2, FIX_SLOW(0.071888074)); /* c9+c11-c3-c15 */
        tmp2 += z1 - MULTIPLY16C16(z3, FIX_SLOW(1.125726048)); /* c5+c7+c15-c3 */
        z1 = MULTIPLY16C16(z3 - z2, FIX_SLOW(1.407403738)); /* c1 */
        tmp11 += z1 - MULTIPLY16C16(z3, FIX_SLOW(0.766367282)); /* c1+c11-c9-c13 */
        tmp12 += z1 + MULTIPLY16C16(z2, FIX_SLOW(1.971951411)); /* c1+c5+c13-c7 */
        z2 += z4;
        z1 = MULTIPLY16C16(z2, -FIX_SLOW(0.666655658)); /* -c11 */
        tmp1 += z1;
        tmp3 += z1 + MULTIPLY16C16(z4, FIX_SLOW(1.065388962)); /* c3+c11+c15-c7 */
        z2 = MULTIPLY16C16(z2, -FIX_SLOW(1.247225013)); /* -c5 */
        tmp10 += z2 + MULTIPLY16C16(z4, FIX_SLOW(3.141271809)); /* c1+c5+c9-c13 */
        tmp12 += z2;
        z2 = MULTIPLY16C16(z3 + z4, -FIX_SLOW(1.353318001)); /* -c3 */
        tmp2 += z2;
        tmp3 += z2;
        z2 = MULTIPLY16C16(z4 - z3, FIX_SLOW(0.410524528)); /* c13 */
        tmp10 += z2;
        tmp11 += z2;

        /* Final output stage */

        outptr[0] = range_limit[(int) RIGHT_SHIFT(tmp20 + tmp0,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[15] = range_limit[(int) RIGHT_SHIFT(tmp20 - tmp0,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[1] = range_limit[(int) RIGHT_SHIFT(tmp21 + tmp1,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[14] = range_limit[(int) RIGHT_SHIFT(tmp21 - tmp1,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[2] = range_limit[(int) RIGHT_SHIFT(tmp22 + tmp2,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[13] = range_limit[(int) RIGHT_SHIFT(tmp22 - tmp2,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[3] = range_limit[(int) RIGHT_SHIFT(tmp23 + tmp3,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[12] = range_limit[(int) RIGHT_SHIFT(tmp23 - tmp3,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[4] = range_limit[(int) RIGHT_SHIFT(tmp24 + tmp10,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[11] = range_limit[(int) RIGHT_SHIFT(tmp24 - tmp10,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[5] = range_limit[(int) RIGHT_SHIFT(tmp25 + tmp11,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[10] = range_limit[(int) RIGHT_SHIFT(tmp25 - tmp11,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[6] = range_limit[(int) RIGHT_SHIFT(tmp26 + tmp12,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[9] = range_limit[(int) RIGHT_SHIFT(tmp26 - tmp12,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[7] = range_limit[(int) RIGHT_SHIFT(tmp27 + tmp13,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[8] = range_limit[(int) RIGHT_SHIFT(tmp27 - tmp13,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];

        wsptr += 8; /* advance pointer to next row */
    }
}

/*
 * Perform dequantization and inverse DCT on one block of coefficients,
 * producing a 8x16 output block.
 *
 * 16-point IDCT in pass 1 (columns), 8-point in pass 2 (rows).
 */


void jpeg_idct_8x16(struct stJpegData *jdata, struct stComponent *comp, BYTE* output_buf, unsigned int output_col)
{
    long tmp0, tmp1, tmp2, tmp3, tmp10, tmp11, tmp12, tmp13;
    long tmp20, tmp21, tmp22, tmp23, tmp24, tmp25, tmp26, tmp27;
    long z1, z2, z3, z4;
    short* inptr;
    long * quantptr;
    int * wsptr;
    BYTE* outptr;
    BYTE* range_limit = jdata->range_limit + CENTERJSAMPLE;
    int ctr;
    int workspace[8 * 16]; /* buffers data between passes */

    /* Pass 1: process columns from input, store into work array.
     * 16-point IDCT kernel, cK represents sqrt(2) * cos(K*pi/32).
     */

    inptr = comp->m_DCT;
    quantptr = (long *) comp->m_qTable;
    wsptr = workspace;
    for (ctr = 0; ctr < 8; ctr++, inptr++, quantptr++, wsptr++)
    {
        /* Even part */

        tmp0 = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);
        tmp0 <<= CONST_BITS;
        /* Add fudge factor here for final descale. */
        tmp0 += 1 << (CONST_BITS - PASS1_BITS - 1);

        z1 = DEQUANTIZE(inptr[DCTSIZE * 4], quantptr[DCTSIZE * 4]);
        tmp1 = MULTIPLY16C16(z1, FIX_SLOW(1.306562965)); /* c4[16] = c2[8] */
        tmp2 = MULTIPLY16C16(z1, FIX_SLOW_0_541196100); /* c12[16] = c6[8] */

        tmp10 = tmp0 + tmp1;
        tmp11 = tmp0 - tmp1;
        tmp12 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;

        z1 = DEQUANTIZE(inptr[DCTSIZE * 2], quantptr[DCTSIZE * 2]);
        z2 = DEQUANTIZE(inptr[DCTSIZE * 6], quantptr[DCTSIZE * 6]);
        z3 = z1 - z2;
        z4 = MULTIPLY16C16(z3, FIX_SLOW(0.275899379)); /* c14[16] = c7[8] */
        z3 = MULTIPLY16C16(z3, FIX_SLOW(1.387039845)); /* c2[16] = c1[8] */

        tmp0 = z3 + MULTIPLY16C16(z2, FIX_SLOW_2_562915447); /* (c6+c2)[16] = (c3+c1)[8] */
        tmp1 = z4 + MULTIPLY16C16(z1, FIX_SLOW_0_899976223); /* (c6-c14)[16] = (c3-c7)[8] */
        tmp2 = z3 - MULTIPLY16C16(z1, FIX_SLOW(0.601344887)); /* (c2-c10)[16] = (c1-c5)[8] */
        tmp3 = z4 - MULTIPLY16C16(z2, FIX_SLOW(0.509795579)); /* (c10-c14)[16] = (c5-c7)[8] */

        tmp20 = tmp10 + tmp0;
        tmp27 = tmp10 - tmp0;
        tmp21 = tmp12 + tmp1;
        tmp26 = tmp12 - tmp1;
        tmp22 = tmp13 + tmp2;
        tmp25 = tmp13 - tmp2;
        tmp23 = tmp11 + tmp3;
        tmp24 = tmp11 - tmp3;

        /* Odd part */

        z1 = DEQUANTIZE(inptr[DCTSIZE * 1], quantptr[DCTSIZE * 1]);
        z2 = DEQUANTIZE(inptr[DCTSIZE * 3], quantptr[DCTSIZE * 3]);
        z3 = DEQUANTIZE(inptr[DCTSIZE * 5], quantptr[DCTSIZE * 5]);
        z4 = DEQUANTIZE(inptr[DCTSIZE * 7], quantptr[DCTSIZE * 7]);

        tmp11 = z1 + z3;

        tmp1 = MULTIPLY16C16(z1 + z2, FIX_SLOW(1.353318001)); /* c3 */
        tmp2 = MULTIPLY16C16(tmp11, FIX_SLOW(1.247225013)); /* c5 */
        tmp3 = MULTIPLY16C16(z1 + z4, FIX_SLOW(1.093201867)); /* c7 */
        tmp10 = MULTIPLY16C16(z1 - z4, FIX_SLOW(0.897167586)); /* c9 */
        tmp11 = MULTIPLY16C16(tmp11, FIX_SLOW(0.666655658)); /* c11 */
        tmp12 = MULTIPLY16C16(z1 - z2, FIX_SLOW(0.410524528)); /* c13 */
        tmp0 = tmp1 + tmp2 + tmp3 -
                MULTIPLY16C16(z1, FIX_SLOW(2.286341144)); /* c7+c5+c3-c1 */
        tmp13 = tmp10 + tmp11 + tmp12 -
                MULTIPLY16C16(z1, FIX_SLOW(1.835730603)); /* c9+c11+c13-c15 */
        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW(0.138617169)); /* c15 */
        tmp1 += z1 + MULTIPLY16C16(z2, FIX_SLOW(0.071888074)); /* c9+c11-c3-c15 */
        tmp2 += z1 - MULTIPLY16C16(z3, FIX_SLOW(1.125726048)); /* c5+c7+c15-c3 */
        z1 = MULTIPLY16C16(z3 - z2, FIX_SLOW(1.407403738)); /* c1 */
        tmp11 += z1 - MULTIPLY16C16(z3, FIX_SLOW(0.766367282)); /* c1+c11-c9-c13 */
        tmp12 += z1 + MULTIPLY16C16(z2, FIX_SLOW(1.971951411)); /* c1+c5+c13-c7 */
        z2 += z4;
        z1 = MULTIPLY16C16(z2, -FIX_SLOW(0.666655658)); /* -c11 */
        tmp1 += z1;
        tmp3 += z1 + MULTIPLY16C16(z4, FIX_SLOW(1.065388962)); /* c3+c11+c15-c7 */
        z2 = MULTIPLY16C16(z2, -FIX_SLOW(1.247225013)); /* -c5 */
        tmp10 += z2 + MULTIPLY16C16(z4, FIX_SLOW(3.141271809)); /* c1+c5+c9-c13 */
        tmp12 += z2;
        z2 = MULTIPLY16C16(z3 + z4, -FIX_SLOW(1.353318001)); /* -c3 */
        tmp2 += z2;
        tmp3 += z2;
        z2 = MULTIPLY16C16(z4 - z3, FIX_SLOW(0.410524528)); /* c13 */
        tmp10 += z2;
        tmp11 += z2;

        /* Final output stage */

        wsptr[8 * 0] = (int) RIGHT_SHIFT(tmp20 + tmp0, CONST_BITS - PASS1_BITS);
        wsptr[8 * 15] = (int) RIGHT_SHIFT(tmp20 - tmp0, CONST_BITS - PASS1_BITS);
        wsptr[8 * 1] = (int) RIGHT_SHIFT(tmp21 + tmp1, CONST_BITS - PASS1_BITS);
        wsptr[8 * 14] = (int) RIGHT_SHIFT(tmp21 - tmp1, CONST_BITS - PASS1_BITS);
        wsptr[8 * 2] = (int) RIGHT_SHIFT(tmp22 + tmp2, CONST_BITS - PASS1_BITS);
        wsptr[8 * 13] = (int) RIGHT_SHIFT(tmp22 - tmp2, CONST_BITS - PASS1_BITS);
        wsptr[8 * 3] = (int) RIGHT_SHIFT(tmp23 + tmp3, CONST_BITS - PASS1_BITS);
        wsptr[8 * 12] = (int) RIGHT_SHIFT(tmp23 - tmp3, CONST_BITS - PASS1_BITS);
        wsptr[8 * 4] = (int) RIGHT_SHIFT(tmp24 + tmp10, CONST_BITS - PASS1_BITS);
        wsptr[8 * 11] = (int) RIGHT_SHIFT(tmp24 - tmp10, CONST_BITS - PASS1_BITS);
        wsptr[8 * 5] = (int) RIGHT_SHIFT(tmp25 + tmp11, CONST_BITS - PASS1_BITS);
        wsptr[8 * 10] = (int) RIGHT_SHIFT(tmp25 - tmp11, CONST_BITS - PASS1_BITS);
        wsptr[8 * 6] = (int) RIGHT_SHIFT(tmp26 + tmp12, CONST_BITS - PASS1_BITS);
        wsptr[8 * 9] = (int) RIGHT_SHIFT(tmp26 - tmp12, CONST_BITS - PASS1_BITS);
        wsptr[8 * 7] = (int) RIGHT_SHIFT(tmp27 + tmp13, CONST_BITS - PASS1_BITS);
        wsptr[8 * 8] = (int) RIGHT_SHIFT(tmp27 - tmp13, CONST_BITS - PASS1_BITS);
    }

    /* Pass 2: process rows from work array, store into output array.
     * Note that we must descale the results by a factor of 8 == 2**3,
     * and also undo the PASS1_BITS scaling.
     * 8-point IDCT kernel, cK represents sqrt(2) * cos(K*pi/16).
     */

    wsptr = workspace;
    for (ctr = 0; ctr < 16; ctr++)
    {
        outptr = output_buf + ctr*8 + output_col;

        /* Even part: reverse the even part of the forward DCT.
         * The rotator is c(-6).
         */

        z2 = (long) wsptr[2];
        z3 = (long) wsptr[6];

        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW_0_541196100); /* c6 */
        tmp2 = z1 + MULTIPLY16C16(z2, FIX_SLOW_0_765366865); /* c2-c6 */
        tmp3 = z1 - MULTIPLY16C16(z3, FIX_SLOW_1_847759065); /* c2+c6 */

        /* Add fudge factor here for final descale. */
        z2 = (long) wsptr[0] + (1 << (PASS1_BITS + 2));
        z3 = (long) wsptr[4];

        tmp0 = (z2 + z3) << CONST_BITS;
        tmp1 = (z2 - z3) << CONST_BITS;

        tmp10 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;
        tmp11 = tmp1 + tmp3;
        tmp12 = tmp1 - tmp3;

        /* Odd part per figure 8; the matrix is unitary and hence its
         * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
         */

        tmp0 = (long) wsptr[7];
        tmp1 = (long) wsptr[5];
        tmp2 = (long) wsptr[3];
        tmp3 = (long) wsptr[1];

        z2 = tmp0 + tmp2;
        z3 = tmp1 + tmp3;

        z1 = MULTIPLY16C16(z2 + z3, FIX_SLOW_1_175875602); /*  c3 */
        z2 = MULTIPLY16C16(z2, -FIX_SLOW_1_961570560); /* -c3-c5 */
        z3 = MULTIPLY16C16(z3, -FIX_SLOW_0_390180644); /* -c3+c5 */
        z2 += z1;
        z3 += z1;

        z1 = MULTIPLY16C16(tmp0 + tmp3, -FIX_SLOW_0_899976223); /* -c3+c7 */
        tmp0 = MULTIPLY16C16(tmp0, FIX_SLOW_0_298631336); /* -c1+c3+c5-c7 */
        tmp3 = MULTIPLY16C16(tmp3, FIX_SLOW_1_501321110); /*  c1+c3-c5-c7 */
        tmp0 += z1 + z2;
        tmp3 += z1 + z3;

        z1 = MULTIPLY16C16(tmp1 + tmp2, -FIX_SLOW_2_562915447); /* -c1-c3 */
        tmp1 = MULTIPLY16C16(tmp1, FIX_SLOW_2_053119869); /*  c1+c3-c5+c7 */
        tmp2 = MULTIPLY16C16(tmp2, FIX_SLOW_3_072711026); /*  c1+c3+c5-c7 */
        tmp1 += z1 + z3;
        tmp2 += z1 + z2;

        /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

        outptr[0] = range_limit[(int) RIGHT_SHIFT(tmp10 + tmp3,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[7] = range_limit[(int) RIGHT_SHIFT(tmp10 - tmp3,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[1] = range_limit[(int) RIGHT_SHIFT(tmp11 + tmp2,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[6] = range_limit[(int) RIGHT_SHIFT(tmp11 - tmp2,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[2] = range_limit[(int) RIGHT_SHIFT(tmp12 + tmp1,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[5] = range_limit[(int) RIGHT_SHIFT(tmp12 - tmp1,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[3] = range_limit[(int) RIGHT_SHIFT(tmp13 + tmp0,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];
        outptr[4] = range_limit[(int) RIGHT_SHIFT(tmp13 - tmp0,
                CONST_BITS + PASS1_BITS + 3)
                & RANGE_MASK];

        wsptr += DCTSIZE; /* advance pointer to next row */
    }
}


