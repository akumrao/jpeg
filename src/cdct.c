#include <stdio.h>
//#include "cjpgdef.h"


#define BYTE unsigned char
#define SBYTE signed char
#define SWORD signed short int
#define WORD unsigned short int
#define DWORD unsigned long int
#define SDWORD signed long int



#define FIX_0_382683433  ((long)   98)   /* FIX(0.382683433) */
#define FIX_0_541196100  ((long)  139)   /* FIX(0.541196100) */
#define FIX_0_707106781  ((long)  181)   /* FIX(0.707106781) */
#define FIX_1_306562965  ((long)  334)   /* FIX(1.306562965) */
#define DCTSIZE 8

#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#define DESCALE(x,n)  RIGHT_SHIFT((x) + (1 << ((n)-1)), n)

void jpeg_fdct_8x8(BYTE* data, long* outdata,  int h, int v, unsigned int start_col)
{

#define DESCALE1(x,n)  RIGHT_SHIFT(x, n)
#define MULTIPLY(var,const)  ((long) DESCALE1((var) * (const), 8))


    long tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7; 
    long tmp10, tmp11, tmp12, tmp13;
    long z1, z2, z3, z4, z5, z11, z13;
    long *dataptr;
    BYTE *elemptr;
    int ctr;

    /* Pass 1: process rows. */

#define GETJSAMPLE(value)  ((int) (value))

    dataptr = outdata;
    for (ctr = 0; ctr < 64*h; ctr = ctr + 8*h )
    {
        elemptr = &data[ctr] + start_col ;

        /* Load data into workspace */
        tmp0 = GETJSAMPLE(elemptr[0]) + GETJSAMPLE(elemptr[7]);
        tmp7 = GETJSAMPLE(elemptr[0]) - GETJSAMPLE(elemptr[7]);
        tmp1 = GETJSAMPLE(elemptr[1]) + GETJSAMPLE(elemptr[6]);
        tmp6 = GETJSAMPLE(elemptr[1]) - GETJSAMPLE(elemptr[6]);
        tmp2 = GETJSAMPLE(elemptr[2]) + GETJSAMPLE(elemptr[5]);
        tmp5 = GETJSAMPLE(elemptr[2]) - GETJSAMPLE(elemptr[5]);
        tmp3 = GETJSAMPLE(elemptr[3]) + GETJSAMPLE(elemptr[4]);
        tmp4 = GETJSAMPLE(elemptr[3]) - GETJSAMPLE(elemptr[4]);

        /* Even part */

        tmp10 = tmp0 + tmp3; /* phase 2 */
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

        /* Apply unsigned->signed conversion */
        dataptr[0] = tmp10 + tmp11 - 8 * 128; /* phase 3 */
        dataptr[4] = tmp10 - tmp11;

        z1 = MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
        dataptr[2] = tmp13 + z1; /* phase 5 */
        dataptr[6] = tmp13 - z1;

        /* Odd part */

        tmp10 = tmp4 + tmp5; /* phase 2 */
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        /* The rotator is modified from fig 4-8 to avoid extra negations. */
        z5 = MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
        z2 = MULTIPLY(tmp10, FIX_0_541196100) + z5; /* c2-c6 */
        z4 = MULTIPLY(tmp12, FIX_1_306562965) + z5; /* c2+c6 */
        z3 = MULTIPLY(tmp11, FIX_0_707106781); /* c4 */

        z11 = tmp7 + z3; /* phase 5 */
        z13 = tmp7 - z3;

        dataptr[5] = z13 + z2; /* phase 6 */
        dataptr[3] = z13 - z2;
        dataptr[1] = z11 + z4;
        dataptr[7] = z11 - z4;

        dataptr += DCTSIZE; /* advance pointer to next row */
    }

    /* Pass 2: process columns. */

    dataptr = outdata;
    for (ctr = 7; ctr >= 0; ctr--)
    {
        tmp0 = dataptr[DCTSIZE * 0] + dataptr[DCTSIZE * 7];
        tmp7 = dataptr[DCTSIZE * 0] - dataptr[DCTSIZE * 7];
        tmp1 = dataptr[DCTSIZE * 1] + dataptr[DCTSIZE * 6];
        tmp6 = dataptr[DCTSIZE * 1] - dataptr[DCTSIZE * 6];
        tmp2 = dataptr[DCTSIZE * 2] + dataptr[DCTSIZE * 5];
        tmp5 = dataptr[DCTSIZE * 2] - dataptr[DCTSIZE * 5];
        tmp3 = dataptr[DCTSIZE * 3] + dataptr[DCTSIZE * 4];
        tmp4 = dataptr[DCTSIZE * 3] - dataptr[DCTSIZE * 4];

        /* Even part */

        tmp10 = tmp0 + tmp3; /* phase 2 */
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

        dataptr[DCTSIZE * 0] = tmp10 + tmp11; /* phase 3 */
        dataptr[DCTSIZE * 4] = tmp10 - tmp11;

        z1 = MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
        dataptr[DCTSIZE * 2] = tmp13 + z1; /* phase 5 */
        dataptr[DCTSIZE * 6] = tmp13 - z1;

        /* Odd part */

        tmp10 = tmp4 + tmp5; /* phase 2 */
        tmp11 = tmp5 + tmp6;
        tmp12 = tmp6 + tmp7;

        /* The rotator is modified from fig 4-8 to avoid extra negations. */
        z5 = MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
        z2 = MULTIPLY(tmp10, FIX_0_541196100) + z5; /* c2-c6 */
        z4 = MULTIPLY(tmp12, FIX_1_306562965) + z5; /* c2+c6 */
        z3 = MULTIPLY(tmp11, FIX_0_707106781); /* c4 */

        z11 = tmp7 + z3; /* phase 5 */
        z13 = tmp7 - z3;

        dataptr[DCTSIZE * 5] = z13 + z2; /* phase 6 */
        dataptr[DCTSIZE * 3] = z13 - z2;
        dataptr[DCTSIZE * 1] = z11 + z4;
        dataptr[DCTSIZE * 7] = z11 - z4;

        dataptr++; /* advance pointer to next column */
    }

}





#define FIX_16x16_0_541196100  ((long)  4433)	/* FIX(0.541196100) */

#define MULTIPLY16V16(var1,var2)  ((var1) * (var2))

void jpeg_fdct_16x16 (BYTE *data,  long *outdata, int h, int v, unsigned int start_col)
{
  long tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  long tmp10, tmp11, tmp12, tmp13, tmp14, tmp15, tmp16, tmp17;
  long workspace[64];
  long *dataptr;
  long *wsptr;
  BYTE *elemptr;
  int ctr;

#define CONST_BITS  13
#define PASS1_BITS  2
#define ONE	((long) 1)
#define CONST_SCALE (ONE << CONST_BITS)
#define FIX(x)	((long) ((x) * CONST_SCALE + 0.5)) 
  int ctr1 = 0;
	
  /* Pass 1: process rows.
   * Note results are scaled up by sqrt(8) compared to a true DCT;
   * furthermore, we scale the results by 2**PASS1_BITS.
   * cK represents sqrt(2) * cos(K*pi/32).
   */

  dataptr = outdata;
  ctr = 0;

  for (ctr = 0; ctr < 64*h*v; ctr = ctr + 8*h )
  {
  
    elemptr = &data[ctr] ;

    /* Even part */

    tmp0 = GETJSAMPLE(elemptr[0]) + GETJSAMPLE(elemptr[15]);
    tmp1 = GETJSAMPLE(elemptr[1]) + GETJSAMPLE(elemptr[14]);
    tmp2 = GETJSAMPLE(elemptr[2]) + GETJSAMPLE(elemptr[13]);
    tmp3 = GETJSAMPLE(elemptr[3]) + GETJSAMPLE(elemptr[12]);
    tmp4 = GETJSAMPLE(elemptr[4]) + GETJSAMPLE(elemptr[11]);
    tmp5 = GETJSAMPLE(elemptr[5]) + GETJSAMPLE(elemptr[10]);
    tmp6 = GETJSAMPLE(elemptr[6]) + GETJSAMPLE(elemptr[9]);
    tmp7 = GETJSAMPLE(elemptr[7]) + GETJSAMPLE(elemptr[8]);

    tmp10 = tmp0 + tmp7;
    tmp14 = tmp0 - tmp7;
    tmp11 = tmp1 + tmp6;
    tmp15 = tmp1 - tmp6;
    tmp12 = tmp2 + tmp5;
    tmp16 = tmp2 - tmp5;
    tmp13 = tmp3 + tmp4;
    tmp17 = tmp3 - tmp4;

    tmp0 = GETJSAMPLE(elemptr[0]) - GETJSAMPLE(elemptr[15]);
    tmp1 = GETJSAMPLE(elemptr[1]) - GETJSAMPLE(elemptr[14]);
    tmp2 = GETJSAMPLE(elemptr[2]) - GETJSAMPLE(elemptr[13]);
    tmp3 = GETJSAMPLE(elemptr[3]) - GETJSAMPLE(elemptr[12]);
    tmp4 = GETJSAMPLE(elemptr[4]) - GETJSAMPLE(elemptr[11]);
    tmp5 = GETJSAMPLE(elemptr[5]) - GETJSAMPLE(elemptr[10]);
    tmp6 = GETJSAMPLE(elemptr[6]) - GETJSAMPLE(elemptr[9]);
    tmp7 = GETJSAMPLE(elemptr[7]) - GETJSAMPLE(elemptr[8]);

    /* Apply unsigned->signed conversion */
    dataptr[0] = (long)
      ((tmp10 + tmp11 + tmp12 + tmp13 - 16 * 128) << PASS1_BITS);
    dataptr[4] = (long)
      DESCALE(MULTIPLY16V16(tmp10 - tmp13, FIX(1.306562965)) + /* c4[16] = c2[8] */
	      MULTIPLY16V16(tmp11 - tmp12, FIX_16x16_0_541196100),   /* c12[16] = c6[8] */
	      CONST_BITS-PASS1_BITS);

    tmp10 = MULTIPLY16V16(tmp17 - tmp15, FIX(0.275899379)) +   /* c14[16] = c7[8] */
	    MULTIPLY16V16(tmp14 - tmp16, FIX(1.387039845));    /* c2[16] = c1[8] */

    dataptr[2] = (long)
      DESCALE(tmp10 + MULTIPLY16V16(tmp15, FIX(1.451774982))   /* c6+c14 */
	      + MULTIPLY16V16(tmp16, FIX(2.172734804)),        /* c2+c10 */
	      CONST_BITS-PASS1_BITS);
    dataptr[6] = (long)
      DESCALE(tmp10 - MULTIPLY16V16(tmp14, FIX(0.211164243))   /* c2-c6 */
	      - MULTIPLY16V16(tmp17, FIX(1.061594338)),        /* c10+c14 */
	      CONST_BITS-PASS1_BITS);

    /* Odd part */

    tmp11 = MULTIPLY16V16(tmp0 + tmp1, FIX(1.353318001)) +         /* c3 */
	    MULTIPLY16V16(tmp6 - tmp7, FIX(0.410524528));          /* c13 */
    tmp12 = MULTIPLY16V16(tmp0 + tmp2, FIX(1.247225013)) +         /* c5 */
	    MULTIPLY16V16(tmp5 + tmp7, FIX(0.666655658));          /* c11 */
    tmp13 = MULTIPLY16V16(tmp0 + tmp3, FIX(1.093201867)) +         /* c7 */
	    MULTIPLY16V16(tmp4 - tmp7, FIX(0.897167586));          /* c9 */
    tmp14 = MULTIPLY16V16(tmp1 + tmp2, FIX(0.138617169)) +         /* c15 */
	    MULTIPLY16V16(tmp6 - tmp5, FIX(1.407403738));          /* c1 */
    tmp15 = MULTIPLY16V16(tmp1 + tmp3, - FIX(0.666655658)) +       /* -c11 */
	    MULTIPLY16V16(tmp4 + tmp6, - FIX(1.247225013));        /* -c5 */
    tmp16 = MULTIPLY16V16(tmp2 + tmp3, - FIX(1.353318001)) +       /* -c3 */
	    MULTIPLY16V16(tmp5 - tmp4, FIX(0.410524528));          /* c13 */
    tmp10 = tmp11 + tmp12 + tmp13 -
	    MULTIPLY16V16(tmp0, FIX(2.286341144)) +                /* c7+c5+c3-c1 */
	    MULTIPLY16V16(tmp7, FIX(0.779653625));                 /* c15+c13-c11+c9 */
    tmp11 += tmp14 + tmp15 + MULTIPLY16V16(tmp1, FIX(0.071888074)) /* c9-c3-c15+c11 */
	     - MULTIPLY16V16(tmp6, FIX(1.663905119));              /* c7+c13+c1-c5 */
    tmp12 += tmp14 + tmp16 - MULTIPLY16V16(tmp2, FIX(1.125726048)) /* c7+c5+c15-c3 */
	     + MULTIPLY16V16(tmp5, FIX(1.227391138));              /* c9-c11+c1-c13 */
    tmp13 += tmp15 + tmp16 + MULTIPLY16V16(tmp3, FIX(1.065388962)) /* c15+c3+c11-c7 */
	     + MULTIPLY16V16(tmp4, FIX(2.167985692));              /* c1+c13+c5-c9 */

    dataptr[1] = (long) DESCALE(tmp10, CONST_BITS-PASS1_BITS);
    dataptr[3] = (long) DESCALE(tmp11, CONST_BITS-PASS1_BITS);
    dataptr[5] = (long) DESCALE(tmp12, CONST_BITS-PASS1_BITS);
    dataptr[7] = (long) DESCALE(tmp13, CONST_BITS-PASS1_BITS);

    //ctr++;
    ctr1++;
    
   if (ctr1 == DCTSIZE) 
    {
      // break;
    }
            
    if (ctr != 112) //(8-1)*16)  == 112
    {
      
      dataptr += DCTSIZE;	/* advance pointer to next row */
    } else
      dataptr = workspace;	/* switch pointer to extended workspace */
  }

  /* Pass 2: process columns.
   * We remove the PASS1_BITS scaling, but leave the results scaled up
   * by an overall factor of 8.
   * We must also scale the output by (8/16)**2 = 1/2**2.
   * cK represents sqrt(2) * cos(K*pi/32).
   */

  dataptr = outdata;
  wsptr = workspace;
  for (ctr = DCTSIZE-1; ctr >= 0; ctr--) {
    /* Even part */

    tmp0 = dataptr[DCTSIZE*0] + wsptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] + wsptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] + wsptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] + wsptr[DCTSIZE*4];
    tmp4 = dataptr[DCTSIZE*4] + wsptr[DCTSIZE*3];
    tmp5 = dataptr[DCTSIZE*5] + wsptr[DCTSIZE*2];
    tmp6 = dataptr[DCTSIZE*6] + wsptr[DCTSIZE*1];
    tmp7 = dataptr[DCTSIZE*7] + wsptr[DCTSIZE*0];

    tmp10 = tmp0 + tmp7;
    tmp14 = tmp0 - tmp7;
    tmp11 = tmp1 + tmp6;
    tmp15 = tmp1 - tmp6;
    tmp12 = tmp2 + tmp5;
    tmp16 = tmp2 - tmp5;
    tmp13 = tmp3 + tmp4;
    tmp17 = tmp3 - tmp4;

    tmp0 = dataptr[DCTSIZE*0] - wsptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] - wsptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] - wsptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] - wsptr[DCTSIZE*4];
    tmp4 = dataptr[DCTSIZE*4] - wsptr[DCTSIZE*3];
    tmp5 = dataptr[DCTSIZE*5] - wsptr[DCTSIZE*2];
    tmp6 = dataptr[DCTSIZE*6] - wsptr[DCTSIZE*1];
    tmp7 = dataptr[DCTSIZE*7] - wsptr[DCTSIZE*0];

    dataptr[DCTSIZE*0] = (long)
      DESCALE(tmp10 + tmp11 + tmp12 + tmp13, PASS1_BITS+2);
    dataptr[DCTSIZE*4] = (long)
      DESCALE(MULTIPLY16V16(tmp10 - tmp13, FIX(1.306562965)) + /* c4[16] = c2[8] */
	      MULTIPLY16V16(tmp11 - tmp12, FIX_16x16_0_541196100),   /* c12[16] = c6[8] */
	      CONST_BITS+PASS1_BITS+2);

    tmp10 = MULTIPLY16V16(tmp17 - tmp15, FIX(0.275899379)) +   /* c14[16] = c7[8] */
	    MULTIPLY16V16(tmp14 - tmp16, FIX(1.387039845));    /* c2[16] = c1[8] */

    dataptr[DCTSIZE*2] = (long)
      DESCALE(tmp10 + MULTIPLY16V16(tmp15, FIX(1.451774982))   /* c6+c14 */
	      + MULTIPLY16V16(tmp16, FIX(2.172734804)),        /* c2+10 */
	      CONST_BITS+PASS1_BITS+2);
    dataptr[DCTSIZE*6] = (long)
      DESCALE(tmp10 - MULTIPLY16V16(tmp14, FIX(0.211164243))   /* c2-c6 */
	      - MULTIPLY16V16(tmp17, FIX(1.061594338)),        /* c10+c14 */
	      CONST_BITS+PASS1_BITS+2);

    /* Odd part */

    tmp11 = MULTIPLY16V16(tmp0 + tmp1, FIX(1.353318001)) +         /* c3 */
	    MULTIPLY16V16(tmp6 - tmp7, FIX(0.410524528));          /* c13 */
    tmp12 = MULTIPLY16V16(tmp0 + tmp2, FIX(1.247225013)) +         /* c5 */
	    MULTIPLY16V16(tmp5 + tmp7, FIX(0.666655658));          /* c11 */
    tmp13 = MULTIPLY16V16(tmp0 + tmp3, FIX(1.093201867)) +         /* c7 */
	    MULTIPLY16V16(tmp4 - tmp7, FIX(0.897167586));          /* c9 */
    tmp14 = MULTIPLY16V16(tmp1 + tmp2, FIX(0.138617169)) +         /* c15 */
	    MULTIPLY16V16(tmp6 - tmp5, FIX(1.407403738));          /* c1 */
    tmp15 = MULTIPLY16V16(tmp1 + tmp3, - FIX(0.666655658)) +       /* -c11 */
	    MULTIPLY16V16(tmp4 + tmp6, - FIX(1.247225013));        /* -c5 */
    tmp16 = MULTIPLY16V16(tmp2 + tmp3, - FIX(1.353318001)) +       /* -c3 */
	    MULTIPLY16V16(tmp5 - tmp4, FIX(0.410524528));          /* c13 */
    tmp10 = tmp11 + tmp12 + tmp13 -
	    MULTIPLY16V16(tmp0, FIX(2.286341144)) +                /* c7+c5+c3-c1 */
	    MULTIPLY16V16(tmp7, FIX(0.779653625));                 /* c15+c13-c11+c9 */
    tmp11 += tmp14 + tmp15 + MULTIPLY16V16(tmp1, FIX(0.071888074)) /* c9-c3-c15+c11 */
	     - MULTIPLY16V16(tmp6, FIX(1.663905119));              /* c7+c13+c1-c5 */
    tmp12 += tmp14 + tmp16 - MULTIPLY16V16(tmp2, FIX(1.125726048)) /* c7+c5+c15-c3 */
	     + MULTIPLY16V16(tmp5, FIX(1.227391138));              /* c9-c11+c1-c13 */
    tmp13 += tmp15 + tmp16 + MULTIPLY16V16(tmp3, FIX(1.065388962)) /* c15+c3+c11-c7 */
	     + MULTIPLY16V16(tmp4, FIX(2.167985692));              /* c1+c13+c5-c9 */

    dataptr[DCTSIZE*1] = (long) DESCALE(tmp10, CONST_BITS+PASS1_BITS+2);
    dataptr[DCTSIZE*3] = (long) DESCALE(tmp11, CONST_BITS+PASS1_BITS+2);
    dataptr[DCTSIZE*5] = (long) DESCALE(tmp12, CONST_BITS+PASS1_BITS+2);
    dataptr[DCTSIZE*7] = (long) DESCALE(tmp13, CONST_BITS+PASS1_BITS+2);

    dataptr++;			/* advance pointer to next column */
    wsptr++;			/* advance pointer to next column */
  }
}

