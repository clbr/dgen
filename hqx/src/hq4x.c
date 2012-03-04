/*
 * Copyright (C) 2003 Maxim Stepin ( maxst@hiend3d.com )
 *
 * Copyright (C) 2010 Cameron Zemek ( grom@zeminvaders.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdint.h>
#include "hqx.h"
#include "common.h"

#define PIXEL00_0     *dp = w[5];
#define PIXEL00_11    *dp = Interp1(w[5], w[4]);
#define PIXEL00_12    *dp = Interp1(w[5], w[2]);
#define PIXEL00_20    *dp = Interp2(w[5], w[2], w[4]);
#define PIXEL00_50    *dp = Interp5(w[2], w[4]);
#define PIXEL00_80    *dp = Interp8(w[5], w[1]);
#define PIXEL00_81    *dp = Interp8(w[5], w[4]);
#define PIXEL00_82    *dp = Interp8(w[5], w[2]);
#define PIXEL01_0     *(dp+1) = w[5];
#define PIXEL01_10    *(dp+1) = Interp1(w[5], w[1]);
#define PIXEL01_12    *(dp+1) = Interp1(w[5], w[2]);
#define PIXEL01_14    *(dp+1) = Interp1(w[2], w[5]);
#define PIXEL01_21    *(dp+1) = Interp2(w[2], w[5], w[4]);
#define PIXEL01_31    *(dp+1) = Interp3(w[5], w[4]);
#define PIXEL01_50    *(dp+1) = Interp5(w[2], w[5]);
#define PIXEL01_60    *(dp+1) = Interp6(w[5], w[2], w[4]);
#define PIXEL01_61    *(dp+1) = Interp6(w[5], w[2], w[1]);
#define PIXEL01_82    *(dp+1) = Interp8(w[5], w[2]);
#define PIXEL01_83    *(dp+1) = Interp8(w[2], w[4]);
#define PIXEL02_0     *(dp+2) = w[5];
#define PIXEL02_10    *(dp+2) = Interp1(w[5], w[3]);
#define PIXEL02_11    *(dp+2) = Interp1(w[5], w[2]);
#define PIXEL02_13    *(dp+2) = Interp1(w[2], w[5]);
#define PIXEL02_21    *(dp+2) = Interp2(w[2], w[5], w[6]);
#define PIXEL02_32    *(dp+2) = Interp3(w[5], w[6]);
#define PIXEL02_50    *(dp+2) = Interp5(w[2], w[5]);
#define PIXEL02_60    *(dp+2) = Interp6(w[5], w[2], w[6]);
#define PIXEL02_61    *(dp+2) = Interp6(w[5], w[2], w[3]);
#define PIXEL02_81    *(dp+2) = Interp8(w[5], w[2]);
#define PIXEL02_83    *(dp+2) = Interp8(w[2], w[6]);
#define PIXEL03_0     *(dp+3) = w[5];
#define PIXEL03_11    *(dp+3) = Interp1(w[5], w[2]);
#define PIXEL03_12    *(dp+3) = Interp1(w[5], w[6]);
#define PIXEL03_20    *(dp+3) = Interp2(w[5], w[2], w[6]);
#define PIXEL03_50    *(dp+3) = Interp5(w[2], w[6]);
#define PIXEL03_80    *(dp+3) = Interp8(w[5], w[3]);
#define PIXEL03_81    *(dp+3) = Interp8(w[5], w[2]);
#define PIXEL03_82    *(dp+3) = Interp8(w[5], w[6]);
#define PIXEL10_0     *(dp+dpL) = w[5];
#define PIXEL10_10    *(dp+dpL) = Interp1(w[5], w[1]);
#define PIXEL10_11    *(dp+dpL) = Interp1(w[5], w[4]);
#define PIXEL10_13    *(dp+dpL) = Interp1(w[4], w[5]);
#define PIXEL10_21    *(dp+dpL) = Interp2(w[4], w[5], w[2]);
#define PIXEL10_32    *(dp+dpL) = Interp3(w[5], w[2]);
#define PIXEL10_50    *(dp+dpL) = Interp5(w[4], w[5]);
#define PIXEL10_60    *(dp+dpL) = Interp6(w[5], w[4], w[2]);
#define PIXEL10_61    *(dp+dpL) = Interp6(w[5], w[4], w[1]);
#define PIXEL10_81    *(dp+dpL) = Interp8(w[5], w[4]);
#define PIXEL10_83    *(dp+dpL) = Interp8(w[4], w[2]);
#define PIXEL11_0     *(dp+dpL+1) = w[5];
#define PIXEL11_30    *(dp+dpL+1) = Interp3(w[5], w[1]);
#define PIXEL11_31    *(dp+dpL+1) = Interp3(w[5], w[4]);
#define PIXEL11_32    *(dp+dpL+1) = Interp3(w[5], w[2]);
#define PIXEL11_70    *(dp+dpL+1) = Interp7(w[5], w[4], w[2]);
#define PIXEL12_0     *(dp+dpL+2) = w[5];
#define PIXEL12_30    *(dp+dpL+2) = Interp3(w[5], w[3]);
#define PIXEL12_31    *(dp+dpL+2) = Interp3(w[5], w[2]);
#define PIXEL12_32    *(dp+dpL+2) = Interp3(w[5], w[6]);
#define PIXEL12_70    *(dp+dpL+2) = Interp7(w[5], w[6], w[2]);
#define PIXEL13_0     *(dp+dpL+3) = w[5];
#define PIXEL13_10    *(dp+dpL+3) = Interp1(w[5], w[3]);
#define PIXEL13_12    *(dp+dpL+3) = Interp1(w[5], w[6]);
#define PIXEL13_14    *(dp+dpL+3) = Interp1(w[6], w[5]);
#define PIXEL13_21    *(dp+dpL+3) = Interp2(w[6], w[5], w[2]);
#define PIXEL13_31    *(dp+dpL+3) = Interp3(w[5], w[2]);
#define PIXEL13_50    *(dp+dpL+3) = Interp5(w[6], w[5]);
#define PIXEL13_60    *(dp+dpL+3) = Interp6(w[5], w[6], w[2]);
#define PIXEL13_61    *(dp+dpL+3) = Interp6(w[5], w[6], w[3]);
#define PIXEL13_82    *(dp+dpL+3) = Interp8(w[5], w[6]);
#define PIXEL13_83    *(dp+dpL+3) = Interp8(w[6], w[2]);
#define PIXEL20_0     *(dp+dpL+dpL) = w[5];
#define PIXEL20_10    *(dp+dpL+dpL) = Interp1(w[5], w[7]);
#define PIXEL20_12    *(dp+dpL+dpL) = Interp1(w[5], w[4]);
#define PIXEL20_14    *(dp+dpL+dpL) = Interp1(w[4], w[5]);
#define PIXEL20_21    *(dp+dpL+dpL) = Interp2(w[4], w[5], w[8]);
#define PIXEL20_31    *(dp+dpL+dpL) = Interp3(w[5], w[8]);
#define PIXEL20_50    *(dp+dpL+dpL) = Interp5(w[4], w[5]);
#define PIXEL20_60    *(dp+dpL+dpL) = Interp6(w[5], w[4], w[8]);
#define PIXEL20_61    *(dp+dpL+dpL) = Interp6(w[5], w[4], w[7]);
#define PIXEL20_82    *(dp+dpL+dpL) = Interp8(w[5], w[4]);
#define PIXEL20_83    *(dp+dpL+dpL) = Interp8(w[4], w[8]);
#define PIXEL21_0     *(dp+dpL+dpL+1) = w[5];
#define PIXEL21_30    *(dp+dpL+dpL+1) = Interp3(w[5], w[7]);
#define PIXEL21_31    *(dp+dpL+dpL+1) = Interp3(w[5], w[8]);
#define PIXEL21_32    *(dp+dpL+dpL+1) = Interp3(w[5], w[4]);
#define PIXEL21_70    *(dp+dpL+dpL+1) = Interp7(w[5], w[4], w[8]);
#define PIXEL22_0     *(dp+dpL+dpL+2) = w[5];
#define PIXEL22_30    *(dp+dpL+dpL+2) = Interp3(w[5], w[9]);
#define PIXEL22_31    *(dp+dpL+dpL+2) = Interp3(w[5], w[6]);
#define PIXEL22_32    *(dp+dpL+dpL+2) = Interp3(w[5], w[8]);
#define PIXEL22_70    *(dp+dpL+dpL+2) = Interp7(w[5], w[6], w[8]);
#define PIXEL23_0     *(dp+dpL+dpL+3) = w[5];
#define PIXEL23_10    *(dp+dpL+dpL+3) = Interp1(w[5], w[9]);
#define PIXEL23_11    *(dp+dpL+dpL+3) = Interp1(w[5], w[6]);
#define PIXEL23_13    *(dp+dpL+dpL+3) = Interp1(w[6], w[5]);
#define PIXEL23_21    *(dp+dpL+dpL+3) = Interp2(w[6], w[5], w[8]);
#define PIXEL23_32    *(dp+dpL+dpL+3) = Interp3(w[5], w[8]);
#define PIXEL23_50    *(dp+dpL+dpL+3) = Interp5(w[6], w[5]);
#define PIXEL23_60    *(dp+dpL+dpL+3) = Interp6(w[5], w[6], w[8]);
#define PIXEL23_61    *(dp+dpL+dpL+3) = Interp6(w[5], w[6], w[9]);
#define PIXEL23_81    *(dp+dpL+dpL+3) = Interp8(w[5], w[6]);
#define PIXEL23_83    *(dp+dpL+dpL+3) = Interp8(w[6], w[8]);
#define PIXEL30_0     *(dp+dpL+dpL+dpL) = w[5];
#define PIXEL30_11    *(dp+dpL+dpL+dpL) = Interp1(w[5], w[8]);
#define PIXEL30_12    *(dp+dpL+dpL+dpL) = Interp1(w[5], w[4]);
#define PIXEL30_20    *(dp+dpL+dpL+dpL) = Interp2(w[5], w[8], w[4]);
#define PIXEL30_50    *(dp+dpL+dpL+dpL) = Interp5(w[8], w[4]);
#define PIXEL30_80    *(dp+dpL+dpL+dpL) = Interp8(w[5], w[7]);
#define PIXEL30_81    *(dp+dpL+dpL+dpL) = Interp8(w[5], w[8]);
#define PIXEL30_82    *(dp+dpL+dpL+dpL) = Interp8(w[5], w[4]);
#define PIXEL31_0     *(dp+dpL+dpL+dpL+1) = w[5];
#define PIXEL31_10    *(dp+dpL+dpL+dpL+1) = Interp1(w[5], w[7]);
#define PIXEL31_11    *(dp+dpL+dpL+dpL+1) = Interp1(w[5], w[8]);
#define PIXEL31_13    *(dp+dpL+dpL+dpL+1) = Interp1(w[8], w[5]);
#define PIXEL31_21    *(dp+dpL+dpL+dpL+1) = Interp2(w[8], w[5], w[4]);
#define PIXEL31_32    *(dp+dpL+dpL+dpL+1) = Interp3(w[5], w[4]);
#define PIXEL31_50    *(dp+dpL+dpL+dpL+1) = Interp5(w[8], w[5]);
#define PIXEL31_60    *(dp+dpL+dpL+dpL+1) = Interp6(w[5], w[8], w[4]);
#define PIXEL31_61    *(dp+dpL+dpL+dpL+1) = Interp6(w[5], w[8], w[7]);
#define PIXEL31_81    *(dp+dpL+dpL+dpL+1) = Interp8(w[5], w[8]);
#define PIXEL31_83    *(dp+dpL+dpL+dpL+1) = Interp8(w[8], w[4]);
#define PIXEL32_0     *(dp+dpL+dpL+dpL+2) = w[5];
#define PIXEL32_10    *(dp+dpL+dpL+dpL+2) = Interp1(w[5], w[9]);
#define PIXEL32_12    *(dp+dpL+dpL+dpL+2) = Interp1(w[5], w[8]);
#define PIXEL32_14    *(dp+dpL+dpL+dpL+2) = Interp1(w[8], w[5]);
#define PIXEL32_21    *(dp+dpL+dpL+dpL+2) = Interp2(w[8], w[5], w[6]);
#define PIXEL32_31    *(dp+dpL+dpL+dpL+2) = Interp3(w[5], w[6]);
#define PIXEL32_50    *(dp+dpL+dpL+dpL+2) = Interp5(w[8], w[5]);
#define PIXEL32_60    *(dp+dpL+dpL+dpL+2) = Interp6(w[5], w[8], w[6]);
#define PIXEL32_61    *(dp+dpL+dpL+dpL+2) = Interp6(w[5], w[8], w[9]);
#define PIXEL32_82    *(dp+dpL+dpL+dpL+2) = Interp8(w[5], w[8]);
#define PIXEL32_83    *(dp+dpL+dpL+dpL+2) = Interp8(w[8], w[6]);
#define PIXEL33_0     *(dp+dpL+dpL+dpL+3) = w[5];
#define PIXEL33_11    *(dp+dpL+dpL+dpL+3) = Interp1(w[5], w[6]);
#define PIXEL33_12    *(dp+dpL+dpL+dpL+3) = Interp1(w[5], w[8]);
#define PIXEL33_20    *(dp+dpL+dpL+dpL+3) = Interp2(w[5], w[8], w[6]);
#define PIXEL33_50    *(dp+dpL+dpL+dpL+3) = Interp5(w[8], w[6]);
#define PIXEL33_80    *(dp+dpL+dpL+dpL+3) = Interp8(w[5], w[9]);
#define PIXEL33_81    *(dp+dpL+dpL+dpL+3) = Interp8(w[5], w[6]);
#define PIXEL33_82    *(dp+dpL+dpL+dpL+3) = Interp8(w[5], w[8]);

HQX_API void HQX_CALLCONV hq4x_32_rb( uint32_t * sp, uint32_t srb, uint32_t * dp, uint32_t drb, int Xres, int Yres )
{
    int  i, j, k;
    int  prevline, nextline;
    uint32_t w[10];
    int dpL = (drb >> 2);
    int spL = (srb >> 2);
    uint8_t *sRowP = (uint8_t *) sp;
    uint8_t *dRowP = (uint8_t *) dp;
    uint32_t yuv1, yuv2;

    //   +----+----+----+
    //   |    |    |    |
    //   | w1 | w2 | w3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w4 | w5 | w6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w7 | w8 | w9 |
    //   +----+----+----+

    for (j=0; j<Yres; j++)
    {
        if (j>0)      prevline = -spL; else prevline = 0;
        if (j<Yres-1) nextline =  spL; else nextline = 0;

        for (i=0; i<Xres; i++)
        {
            w[2] = *(sp + prevline);
            w[5] = *sp;
            w[8] = *(sp + nextline);

            if (i>0)
            {
                w[1] = *(sp + prevline - 1);
                w[4] = *(sp - 1);
                w[7] = *(sp + nextline - 1);
            }
            else
            {
                w[1] = w[2];
                w[4] = w[5];
                w[7] = w[8];
            }

            if (i<Xres-1)
            {
                w[3] = *(sp + prevline + 1);
                w[6] = *(sp + 1);
                w[9] = *(sp + nextline + 1);
            }
            else
            {
                w[3] = w[2];
                w[6] = w[5];
                w[9] = w[8];
            }

            int pattern = 0;
            int flag = 1;

            yuv1 = rgb_to_yuv(w[5]);

            for (k=1; k<=9; k++)
            {
                if (k==5) continue;

                if ( w[k] != w[5] )
                {
                    yuv2 = rgb_to_yuv(w[k]);
                    if (yuv_diff(yuv1, yuv2))
                        pattern |= flag;
                }
                flag <<= 1;
            }

            switch (pattern)
            {
                case 0:
                case 1:
                case 4:
                case 32:
                case 128:
                case 5:
                case 132:
                case 160:
                case 33:
                case 129:
                case 36:
                case 133:
                case 164:
                case 161:
                case 37:
                case 165:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 2:
                case 34:
                case 130:
                case 162:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 16:
                case 17:
                case 48:
                case 49:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 64:
                case 65:
                case 68:
                case 69:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 8:
                case 12:
                case 136:
                case 140:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 3:
                case 35:
                case 131:
                case 163:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 6:
                case 38:
                case 134:
                case 166:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 20:
                case 21:
                case 52:
                case 53:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 144:
                case 145:
                case 176:
                case 177:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 192:
                case 193:
                case 196:
                case 197:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 96:
                case 97:
                case 100:
                case 101:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 40:
                case 44:
                case 168:
                case 172:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 9:
                case 13:
                case 137:
                case 141:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 18:
                case 50:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL12_0
                            PIXEL13_50
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 80:
                case 81:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_61
                        PIXEL21_30
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 72:
                case 76:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_70
                        PIXEL13_60
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL21_0
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 10:
                case 138:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                            PIXEL11_0
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 66:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 24:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 7:
                case 39:
                case 135:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 148:
                case 149:
                case 180:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 224:
                case 228:
                case 225:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 41:
                case 169:
                case 45:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 22:
                case 54:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 208:
                case 209:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 104:
                case 108:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_70
                        PIXEL13_60
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 11:
                case 139:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 19:
                case 51:
                    {
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL00_81
                            PIXEL01_31
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL00_12
                            PIXEL01_14
                            PIXEL02_83
                            PIXEL03_50
                            PIXEL12_70
                            PIXEL13_21
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 146:
                case 178:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                            PIXEL23_32
                            PIXEL33_82
                        }
                        else
                        {
                            PIXEL02_21
                            PIXEL03_50
                            PIXEL12_70
                            PIXEL13_83
                            PIXEL23_13
                            PIXEL33_11
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_32
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_82
                        break;
                    }
                case 84:
                case 85:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_81
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL03_81
                            PIXEL13_31
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL03_12
                            PIXEL13_14
                            PIXEL22_70
                            PIXEL23_83
                            PIXEL32_21
                            PIXEL33_50
                        }
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_31
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 112:
                case 113:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_82
                        PIXEL21_32
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL30_82
                            PIXEL31_32
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_70
                            PIXEL23_21
                            PIXEL30_11
                            PIXEL31_13
                            PIXEL32_83
                            PIXEL33_50
                        }
                        break;
                    }
                case 200:
                case 204:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_70
                        PIXEL13_60
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                            PIXEL32_31
                            PIXEL33_81
                        }
                        else
                        {
                            PIXEL20_21
                            PIXEL21_70
                            PIXEL30_50
                            PIXEL31_83
                            PIXEL32_14
                            PIXEL33_12
                        }
                        PIXEL22_31
                        PIXEL23_81
                        break;
                    }
                case 73:
                case 77:
                    {
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL00_82
                            PIXEL10_32
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL00_11
                            PIXEL10_13
                            PIXEL20_83
                            PIXEL21_70
                            PIXEL30_50
                            PIXEL31_21
                        }
                        PIXEL01_82
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL11_32
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 42:
                case 170:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                            PIXEL20_31
                            PIXEL30_81
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_21
                            PIXEL10_83
                            PIXEL11_70
                            PIXEL20_14
                            PIXEL30_12
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL21_31
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL31_81
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 14:
                case 142:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL02_32
                            PIXEL03_82
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_83
                            PIXEL02_13
                            PIXEL03_11
                            PIXEL10_21
                            PIXEL11_70
                        }
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 67:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 70:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 28:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 152:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 194:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 98:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 56:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 25:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 26:
                case 31:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL11_0
                        PIXEL12_0
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 82:
                case 214:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 88:
                case 248:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        break;
                    }
                case 74:
                case 107:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_61
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 27:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 86:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 216:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 106:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_61
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 30:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 210:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 120:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 75:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 29:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 198:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 184:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 99:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 57:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 71:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 156:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 226:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 60:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 195:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 102:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 153:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 58:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 83:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL20_61
                        PIXEL21_30
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 92:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 202:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL12_30
                        PIXEL13_61
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 78:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL12_32
                        PIXEL13_82
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 154:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 114:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL20_82
                        PIXEL21_32
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        PIXEL30_82
                        PIXEL31_32
                        break;
                    }
                case 89:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 90:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 55:
                case 23:
                    {
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL00_81
                            PIXEL01_31
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL12_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL00_12
                            PIXEL01_14
                            PIXEL02_83
                            PIXEL03_50
                            PIXEL12_70
                            PIXEL13_21
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 182:
                case 150:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL12_0
                            PIXEL13_0
                            PIXEL23_32
                            PIXEL33_82
                        }
                        else
                        {
                            PIXEL02_21
                            PIXEL03_50
                            PIXEL12_70
                            PIXEL13_83
                            PIXEL23_13
                            PIXEL33_11
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_32
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_82
                        break;
                    }
                case 213:
                case 212:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_81
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL03_81
                            PIXEL13_31
                            PIXEL22_0
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL03_12
                            PIXEL13_14
                            PIXEL22_70
                            PIXEL23_83
                            PIXEL32_21
                            PIXEL33_50
                        }
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_31
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 241:
                case 240:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_82
                        PIXEL21_32
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_0
                            PIXEL23_0
                            PIXEL30_82
                            PIXEL31_32
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL22_70
                            PIXEL23_21
                            PIXEL30_11
                            PIXEL31_13
                            PIXEL32_83
                            PIXEL33_50
                        }
                        break;
                    }
                case 236:
                case 232:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_70
                        PIXEL13_60
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL21_0
                            PIXEL30_0
                            PIXEL31_0
                            PIXEL32_31
                            PIXEL33_81
                        }
                        else
                        {
                            PIXEL20_21
                            PIXEL21_70
                            PIXEL30_50
                            PIXEL31_83
                            PIXEL32_14
                            PIXEL33_12
                        }
                        PIXEL22_31
                        PIXEL23_81
                        break;
                    }
                case 109:
                case 105:
                    {
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL00_82
                            PIXEL10_32
                            PIXEL20_0
                            PIXEL21_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL00_11
                            PIXEL10_13
                            PIXEL20_83
                            PIXEL21_70
                            PIXEL30_50
                            PIXEL31_21
                        }
                        PIXEL01_82
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL11_32
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 171:
                case 43:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                            PIXEL11_0
                            PIXEL20_31
                            PIXEL30_81
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_21
                            PIXEL10_83
                            PIXEL11_70
                            PIXEL20_14
                            PIXEL30_12
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL21_31
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL31_81
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 143:
                case 15:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL02_32
                            PIXEL03_82
                            PIXEL10_0
                            PIXEL11_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_83
                            PIXEL02_13
                            PIXEL03_11
                            PIXEL10_21
                            PIXEL11_70
                        }
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 124:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 203:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 62:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 211:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 118:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 217:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 110:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_32
                        PIXEL13_82
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 155:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 188:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 185:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 61:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 157:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 103:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 227:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 230:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 199:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 220:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        break;
                    }
                case 158:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL12_0
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 234:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL12_30
                        PIXEL13_61
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 242:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_82
                        PIXEL31_32
                        break;
                    }
                case 59:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL11_0
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 121:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 87:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_0
                        PIXEL20_61
                        PIXEL21_30
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 79:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL11_0
                        PIXEL12_32
                        PIXEL13_82
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 122:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 94:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL12_0
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 218:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        break;
                    }
                case 91:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL11_0
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 229:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 167:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 173:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 181:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 186:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 115:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL20_82
                        PIXEL21_32
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        PIXEL30_82
                        PIXEL31_32
                        break;
                    }
                case 93:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL13_31
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        break;
                    }
                case 206:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL12_32
                        PIXEL13_82
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 205:
                case 201:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_70
                        PIXEL13_60
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_10
                            PIXEL21_30
                            PIXEL30_80
                            PIXEL31_10
                        }
                        else
                        {
                            PIXEL20_12
                            PIXEL21_0
                            PIXEL30_20
                            PIXEL31_11
                        }
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 174:
                case 46:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_80
                            PIXEL01_10
                            PIXEL10_10
                            PIXEL11_30
                        }
                        else
                        {
                            PIXEL00_20
                            PIXEL01_12
                            PIXEL10_11
                            PIXEL11_0
                        }
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 179:
                case 147:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_10
                            PIXEL03_80
                            PIXEL12_30
                            PIXEL13_10
                        }
                        else
                        {
                            PIXEL02_11
                            PIXEL03_20
                            PIXEL12_0
                            PIXEL13_12
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 117:
                case 116:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_82
                        PIXEL21_32
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_30
                            PIXEL23_10
                            PIXEL32_10
                            PIXEL33_80
                        }
                        else
                        {
                            PIXEL22_0
                            PIXEL23_11
                            PIXEL32_12
                            PIXEL33_20
                        }
                        PIXEL30_82
                        PIXEL31_32
                        break;
                    }
                case 189:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 231:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 126:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_0
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 219:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 125:
                    {
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL00_82
                            PIXEL10_32
                            PIXEL20_0
                            PIXEL21_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL00_11
                            PIXEL10_13
                            PIXEL20_83
                            PIXEL21_70
                            PIXEL30_50
                            PIXEL31_21
                        }
                        PIXEL01_82
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 221:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_81
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL03_81
                            PIXEL13_31
                            PIXEL22_0
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL03_12
                            PIXEL13_14
                            PIXEL22_70
                            PIXEL23_83
                            PIXEL32_21
                            PIXEL33_50
                        }
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 207:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL02_32
                            PIXEL03_82
                            PIXEL10_0
                            PIXEL11_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_83
                            PIXEL02_13
                            PIXEL03_11
                            PIXEL10_21
                            PIXEL11_70
                        }
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_31
                        PIXEL23_81
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 238:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_32
                        PIXEL13_82
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL21_0
                            PIXEL30_0
                            PIXEL31_0
                            PIXEL32_31
                            PIXEL33_81
                        }
                        else
                        {
                            PIXEL20_21
                            PIXEL21_70
                            PIXEL30_50
                            PIXEL31_83
                            PIXEL32_14
                            PIXEL33_12
                        }
                        PIXEL22_31
                        PIXEL23_81
                        break;
                    }
                case 190:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL12_0
                            PIXEL13_0
                            PIXEL23_32
                            PIXEL33_82
                        }
                        else
                        {
                            PIXEL02_21
                            PIXEL03_50
                            PIXEL12_70
                            PIXEL13_83
                            PIXEL23_13
                            PIXEL33_11
                        }
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_82
                        break;
                    }
                case 187:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                            PIXEL11_0
                            PIXEL20_31
                            PIXEL30_81
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_21
                            PIXEL10_83
                            PIXEL11_70
                            PIXEL20_14
                            PIXEL30_12
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL31_81
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 243:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_82
                        PIXEL21_32
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL22_0
                            PIXEL23_0
                            PIXEL30_82
                            PIXEL31_32
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL22_70
                            PIXEL23_21
                            PIXEL30_11
                            PIXEL31_13
                            PIXEL32_83
                            PIXEL33_50
                        }
                        break;
                    }
                case 119:
                    {
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL00_81
                            PIXEL01_31
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL12_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL00_12
                            PIXEL01_14
                            PIXEL02_83
                            PIXEL03_50
                            PIXEL12_70
                            PIXEL13_21
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 237:
                case 233:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_60
                        PIXEL03_20
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_70
                        PIXEL13_60
                        PIXEL20_0
                        PIXEL21_0
                        PIXEL22_31
                        PIXEL23_81
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL30_0
                        }
                        else
                        {
                            PIXEL30_20
                        }
                        PIXEL31_0
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 175:
                case 47:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                        }
                        else
                        {
                            PIXEL00_20
                        }
                        PIXEL01_0
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_0
                        PIXEL11_0
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_70
                        PIXEL23_60
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_60
                        PIXEL33_20
                        break;
                    }
                case 183:
                case 151:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL03_0
                        }
                        else
                        {
                            PIXEL03_20
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_0
                        PIXEL13_0
                        PIXEL20_60
                        PIXEL21_70
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_20
                        PIXEL31_60
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 245:
                case 244:
                    {
                        PIXEL00_20
                        PIXEL01_60
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_60
                        PIXEL11_70
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_0
                        PIXEL23_0
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL33_20
                        }
                        break;
                    }
                case 250:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_30
                        PIXEL13_10
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        break;
                    }
                case 123:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_10
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 95:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL11_0
                        PIXEL12_0
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_80
                        PIXEL31_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 222:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 252:
                    {
                        PIXEL00_80
                        PIXEL01_61
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_31
                        PIXEL13_31
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_0
                        PIXEL23_0
                        PIXEL32_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL33_20
                        }
                        break;
                    }
                case 249:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_61
                        PIXEL03_80
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_0
                        PIXEL21_0
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL30_0
                        }
                        else
                        {
                            PIXEL30_20
                        }
                        PIXEL31_0
                        break;
                    }
                case 235:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_61
                        PIXEL20_0
                        PIXEL21_0
                        PIXEL22_31
                        PIXEL23_81
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL30_0
                        }
                        else
                        {
                            PIXEL30_20
                        }
                        PIXEL31_0
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 111:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                        }
                        else
                        {
                            PIXEL00_20
                        }
                        PIXEL01_0
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_0
                        PIXEL11_0
                        PIXEL12_32
                        PIXEL13_82
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_61
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 63:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                        }
                        else
                        {
                            PIXEL00_20
                        }
                        PIXEL01_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_0
                        PIXEL11_0
                        PIXEL12_0
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_61
                        PIXEL33_80
                        break;
                    }
                case 159:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL03_0
                        }
                        else
                        {
                            PIXEL03_20
                        }
                        PIXEL11_0
                        PIXEL12_0
                        PIXEL13_0
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_80
                        PIXEL31_61
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 215:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL03_0
                        }
                        else
                        {
                            PIXEL03_20
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_0
                        PIXEL13_0
                        PIXEL20_61
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 246:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_61
                        PIXEL11_30
                        PIXEL12_0
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_0
                        PIXEL23_0
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL33_20
                        }
                        break;
                    }
                case 254:
                    {
                        PIXEL00_80
                        PIXEL01_10
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_10
                        PIXEL11_30
                        PIXEL12_0
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_0
                        PIXEL23_0
                        PIXEL32_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL33_20
                        }
                        break;
                    }
                case 253:
                    {
                        PIXEL00_82
                        PIXEL01_82
                        PIXEL02_81
                        PIXEL03_81
                        PIXEL10_32
                        PIXEL11_32
                        PIXEL12_31
                        PIXEL13_31
                        PIXEL20_0
                        PIXEL21_0
                        PIXEL22_0
                        PIXEL23_0
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL30_0
                        }
                        else
                        {
                            PIXEL30_20
                        }
                        PIXEL31_0
                        PIXEL32_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL33_20
                        }
                        break;
                    }
                case 251:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_10
                        PIXEL03_80
                        PIXEL11_0
                        PIXEL12_30
                        PIXEL13_10
                        PIXEL20_0
                        PIXEL21_0
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL30_0
                        }
                        else
                        {
                            PIXEL30_20
                        }
                        PIXEL31_0
                        break;
                    }
                case 239:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                        }
                        else
                        {
                            PIXEL00_20
                        }
                        PIXEL01_0
                        PIXEL02_32
                        PIXEL03_82
                        PIXEL10_0
                        PIXEL11_0
                        PIXEL12_32
                        PIXEL13_82
                        PIXEL20_0
                        PIXEL21_0
                        PIXEL22_31
                        PIXEL23_81
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL30_0
                        }
                        else
                        {
                            PIXEL30_20
                        }
                        PIXEL31_0
                        PIXEL32_31
                        PIXEL33_81
                        break;
                    }
                case 127:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                        }
                        else
                        {
                            PIXEL00_20
                        }
                        PIXEL01_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL02_0
                            PIXEL03_0
                            PIXEL13_0
                        }
                        else
                        {
                            PIXEL02_50
                            PIXEL03_50
                            PIXEL13_50
                        }
                        PIXEL10_0
                        PIXEL11_0
                        PIXEL12_0
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL20_0
                            PIXEL30_0
                            PIXEL31_0
                        }
                        else
                        {
                            PIXEL20_50
                            PIXEL30_50
                            PIXEL31_50
                        }
                        PIXEL21_0
                        PIXEL22_30
                        PIXEL23_10
                        PIXEL32_10
                        PIXEL33_80
                        break;
                    }
                case 191:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                        }
                        else
                        {
                            PIXEL00_20
                        }
                        PIXEL01_0
                        PIXEL02_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL03_0
                        }
                        else
                        {
                            PIXEL03_20
                        }
                        PIXEL10_0
                        PIXEL11_0
                        PIXEL12_0
                        PIXEL13_0
                        PIXEL20_31
                        PIXEL21_31
                        PIXEL22_32
                        PIXEL23_32
                        PIXEL30_81
                        PIXEL31_81
                        PIXEL32_82
                        PIXEL33_82
                        break;
                    }
                case 223:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                            PIXEL01_0
                            PIXEL10_0
                        }
                        else
                        {
                            PIXEL00_50
                            PIXEL01_50
                            PIXEL10_50
                        }
                        PIXEL02_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL03_0
                        }
                        else
                        {
                            PIXEL03_20
                        }
                        PIXEL11_0
                        PIXEL12_0
                        PIXEL13_0
                        PIXEL20_10
                        PIXEL21_30
                        PIXEL22_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL23_0
                            PIXEL32_0
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL23_50
                            PIXEL32_50
                            PIXEL33_50
                        }
                        PIXEL30_80
                        PIXEL31_10
                        break;
                    }
                case 247:
                    {
                        PIXEL00_81
                        PIXEL01_31
                        PIXEL02_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL03_0
                        }
                        else
                        {
                            PIXEL03_20
                        }
                        PIXEL10_81
                        PIXEL11_31
                        PIXEL12_0
                        PIXEL13_0
                        PIXEL20_82
                        PIXEL21_32
                        PIXEL22_0
                        PIXEL23_0
                        PIXEL30_82
                        PIXEL31_32
                        PIXEL32_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL33_20
                        }
                        break;
                    }
                case 255:
                    {
                        if (Diff(w[4], w[2]))
                        {
                            PIXEL00_0
                        }
                        else
                        {
                            PIXEL00_20
                        }
                        PIXEL01_0
                        PIXEL02_0
                        if (Diff(w[2], w[6]))
                        {
                            PIXEL03_0
                        }
                        else
                        {
                            PIXEL03_20
                        }
                        PIXEL10_0
                        PIXEL11_0
                        PIXEL12_0
                        PIXEL13_0
                        PIXEL20_0
                        PIXEL21_0
                        PIXEL22_0
                        PIXEL23_0
                        if (Diff(w[8], w[4]))
                        {
                            PIXEL30_0
                        }
                        else
                        {
                            PIXEL30_20
                        }
                        PIXEL31_0
                        PIXEL32_0
                        if (Diff(w[6], w[8]))
                        {
                            PIXEL33_0
                        }
                        else
                        {
                            PIXEL33_20
                        }
                        break;
                    }
            }
            sp++;
            dp += 4;
        }

        sRowP += srb;
        sp = (uint32_t *) sRowP;

        dRowP += drb * 4;
        dp = (uint32_t *) dRowP;
    }
}

HQX_API void HQX_CALLCONV hq4x_32( uint32_t * sp, uint32_t * dp, int Xres, int Yres )
{
    uint32_t rowBytesL = Xres * 4;
    hq4x_32_rb(sp, rowBytesL, dp, rowBytesL * 4, Xres, Yres);
}

#define PIXEL1600_0     *dp = w[5];
#define PIXEL1600_11    *dp = Interp1_16(w[5], w[4]);
#define PIXEL1600_12    *dp = Interp1_16(w[5], w[2]);
#define PIXEL1600_20    *dp = Interp2_16(w[5], w[2], w[4]);
#define PIXEL1600_50    *dp = Interp5_16(w[2], w[4]);
#define PIXEL1600_80    *dp = Interp8_16(w[5], w[1]);
#define PIXEL1600_81    *dp = Interp8_16(w[5], w[4]);
#define PIXEL1600_82    *dp = Interp8_16(w[5], w[2]);
#define PIXEL1601_0     *(dp+1) = w[5];
#define PIXEL1601_10    *(dp+1) = Interp1_16(w[5], w[1]);
#define PIXEL1601_12    *(dp+1) = Interp1_16(w[5], w[2]);
#define PIXEL1601_14    *(dp+1) = Interp1_16(w[2], w[5]);
#define PIXEL1601_21    *(dp+1) = Interp2_16(w[2], w[5], w[4]);
#define PIXEL1601_31    *(dp+1) = Interp3_16(w[5], w[4]);
#define PIXEL1601_50    *(dp+1) = Interp5_16(w[2], w[5]);
#define PIXEL1601_60    *(dp+1) = Interp6_16(w[5], w[2], w[4]);
#define PIXEL1601_61    *(dp+1) = Interp6_16(w[5], w[2], w[1]);
#define PIXEL1601_82    *(dp+1) = Interp8_16(w[5], w[2]);
#define PIXEL1601_83    *(dp+1) = Interp8_16(w[2], w[4]);
#define PIXEL1602_0     *(dp+2) = w[5];
#define PIXEL1602_10    *(dp+2) = Interp1_16(w[5], w[3]);
#define PIXEL1602_11    *(dp+2) = Interp1_16(w[5], w[2]);
#define PIXEL1602_13    *(dp+2) = Interp1_16(w[2], w[5]);
#define PIXEL1602_21    *(dp+2) = Interp2_16(w[2], w[5], w[6]);
#define PIXEL1602_32    *(dp+2) = Interp3_16(w[5], w[6]);
#define PIXEL1602_50    *(dp+2) = Interp5_16(w[2], w[5]);
#define PIXEL1602_60    *(dp+2) = Interp6_16(w[5], w[2], w[6]);
#define PIXEL1602_61    *(dp+2) = Interp6_16(w[5], w[2], w[3]);
#define PIXEL1602_81    *(dp+2) = Interp8_16(w[5], w[2]);
#define PIXEL1602_83    *(dp+2) = Interp8_16(w[2], w[6]);
#define PIXEL1603_0     *(dp+3) = w[5];
#define PIXEL1603_11    *(dp+3) = Interp1_16(w[5], w[2]);
#define PIXEL1603_12    *(dp+3) = Interp1_16(w[5], w[6]);
#define PIXEL1603_20    *(dp+3) = Interp2_16(w[5], w[2], w[6]);
#define PIXEL1603_50    *(dp+3) = Interp5_16(w[2], w[6]);
#define PIXEL1603_80    *(dp+3) = Interp8_16(w[5], w[3]);
#define PIXEL1603_81    *(dp+3) = Interp8_16(w[5], w[2]);
#define PIXEL1603_82    *(dp+3) = Interp8_16(w[5], w[6]);
#define PIXEL1610_0     *(dp+dpL) = w[5];
#define PIXEL1610_10    *(dp+dpL) = Interp1_16(w[5], w[1]);
#define PIXEL1610_11    *(dp+dpL) = Interp1_16(w[5], w[4]);
#define PIXEL1610_13    *(dp+dpL) = Interp1_16(w[4], w[5]);
#define PIXEL1610_21    *(dp+dpL) = Interp2_16(w[4], w[5], w[2]);
#define PIXEL1610_32    *(dp+dpL) = Interp3_16(w[5], w[2]);
#define PIXEL1610_50    *(dp+dpL) = Interp5_16(w[4], w[5]);
#define PIXEL1610_60    *(dp+dpL) = Interp6_16(w[5], w[4], w[2]);
#define PIXEL1610_61    *(dp+dpL) = Interp6_16(w[5], w[4], w[1]);
#define PIXEL1610_81    *(dp+dpL) = Interp8_16(w[5], w[4]);
#define PIXEL1610_83    *(dp+dpL) = Interp8_16(w[4], w[2]);
#define PIXEL1611_0     *(dp+dpL+1) = w[5];
#define PIXEL1611_30    *(dp+dpL+1) = Interp3_16(w[5], w[1]);
#define PIXEL1611_31    *(dp+dpL+1) = Interp3_16(w[5], w[4]);
#define PIXEL1611_32    *(dp+dpL+1) = Interp3_16(w[5], w[2]);
#define PIXEL1611_70    *(dp+dpL+1) = Interp7_16(w[5], w[4], w[2]);
#define PIXEL1612_0     *(dp+dpL+2) = w[5];
#define PIXEL1612_30    *(dp+dpL+2) = Interp3_16(w[5], w[3]);
#define PIXEL1612_31    *(dp+dpL+2) = Interp3_16(w[5], w[2]);
#define PIXEL1612_32    *(dp+dpL+2) = Interp3_16(w[5], w[6]);
#define PIXEL1612_70    *(dp+dpL+2) = Interp7_16(w[5], w[6], w[2]);
#define PIXEL1613_0     *(dp+dpL+3) = w[5];
#define PIXEL1613_10    *(dp+dpL+3) = Interp1_16(w[5], w[3]);
#define PIXEL1613_12    *(dp+dpL+3) = Interp1_16(w[5], w[6]);
#define PIXEL1613_14    *(dp+dpL+3) = Interp1_16(w[6], w[5]);
#define PIXEL1613_21    *(dp+dpL+3) = Interp2_16(w[6], w[5], w[2]);
#define PIXEL1613_31    *(dp+dpL+3) = Interp3_16(w[5], w[2]);
#define PIXEL1613_50    *(dp+dpL+3) = Interp5_16(w[6], w[5]);
#define PIXEL1613_60    *(dp+dpL+3) = Interp6_16(w[5], w[6], w[2]);
#define PIXEL1613_61    *(dp+dpL+3) = Interp6_16(w[5], w[6], w[3]);
#define PIXEL1613_82    *(dp+dpL+3) = Interp8_16(w[5], w[6]);
#define PIXEL1613_83    *(dp+dpL+3) = Interp8_16(w[6], w[2]);
#define PIXEL1620_0     *(dp+dpL+dpL) = w[5];
#define PIXEL1620_10    *(dp+dpL+dpL) = Interp1_16(w[5], w[7]);
#define PIXEL1620_12    *(dp+dpL+dpL) = Interp1_16(w[5], w[4]);
#define PIXEL1620_14    *(dp+dpL+dpL) = Interp1_16(w[4], w[5]);
#define PIXEL1620_21    *(dp+dpL+dpL) = Interp2_16(w[4], w[5], w[8]);
#define PIXEL1620_31    *(dp+dpL+dpL) = Interp3_16(w[5], w[8]);
#define PIXEL1620_50    *(dp+dpL+dpL) = Interp5_16(w[4], w[5]);
#define PIXEL1620_60    *(dp+dpL+dpL) = Interp6_16(w[5], w[4], w[8]);
#define PIXEL1620_61    *(dp+dpL+dpL) = Interp6_16(w[5], w[4], w[7]);
#define PIXEL1620_82    *(dp+dpL+dpL) = Interp8_16(w[5], w[4]);
#define PIXEL1620_83    *(dp+dpL+dpL) = Interp8_16(w[4], w[8]);
#define PIXEL1621_0     *(dp+dpL+dpL+1) = w[5];
#define PIXEL1621_30    *(dp+dpL+dpL+1) = Interp3_16(w[5], w[7]);
#define PIXEL1621_31    *(dp+dpL+dpL+1) = Interp3_16(w[5], w[8]);
#define PIXEL1621_32    *(dp+dpL+dpL+1) = Interp3_16(w[5], w[4]);
#define PIXEL1621_70    *(dp+dpL+dpL+1) = Interp7_16(w[5], w[4], w[8]);
#define PIXEL1622_0     *(dp+dpL+dpL+2) = w[5];
#define PIXEL1622_30    *(dp+dpL+dpL+2) = Interp3_16(w[5], w[9]);
#define PIXEL1622_31    *(dp+dpL+dpL+2) = Interp3_16(w[5], w[6]);
#define PIXEL1622_32    *(dp+dpL+dpL+2) = Interp3_16(w[5], w[8]);
#define PIXEL1622_70    *(dp+dpL+dpL+2) = Interp7_16(w[5], w[6], w[8]);
#define PIXEL1623_0     *(dp+dpL+dpL+3) = w[5];
#define PIXEL1623_10    *(dp+dpL+dpL+3) = Interp1_16(w[5], w[9]);
#define PIXEL1623_11    *(dp+dpL+dpL+3) = Interp1_16(w[5], w[6]);
#define PIXEL1623_13    *(dp+dpL+dpL+3) = Interp1_16(w[6], w[5]);
#define PIXEL1623_21    *(dp+dpL+dpL+3) = Interp2_16(w[6], w[5], w[8]);
#define PIXEL1623_32    *(dp+dpL+dpL+3) = Interp3_16(w[5], w[8]);
#define PIXEL1623_50    *(dp+dpL+dpL+3) = Interp5_16(w[6], w[5]);
#define PIXEL1623_60    *(dp+dpL+dpL+3) = Interp6_16(w[5], w[6], w[8]);
#define PIXEL1623_61    *(dp+dpL+dpL+3) = Interp6_16(w[5], w[6], w[9]);
#define PIXEL1623_81    *(dp+dpL+dpL+3) = Interp8_16(w[5], w[6]);
#define PIXEL1623_83    *(dp+dpL+dpL+3) = Interp8_16(w[6], w[8]);
#define PIXEL1630_0     *(dp+dpL+dpL+dpL) = w[5];
#define PIXEL1630_11    *(dp+dpL+dpL+dpL) = Interp1_16(w[5], w[8]);
#define PIXEL1630_12    *(dp+dpL+dpL+dpL) = Interp1_16(w[5], w[4]);
#define PIXEL1630_20    *(dp+dpL+dpL+dpL) = Interp2_16(w[5], w[8], w[4]);
#define PIXEL1630_50    *(dp+dpL+dpL+dpL) = Interp5_16(w[8], w[4]);
#define PIXEL1630_80    *(dp+dpL+dpL+dpL) = Interp8_16(w[5], w[7]);
#define PIXEL1630_81    *(dp+dpL+dpL+dpL) = Interp8_16(w[5], w[8]);
#define PIXEL1630_82    *(dp+dpL+dpL+dpL) = Interp8_16(w[5], w[4]);
#define PIXEL1631_0     *(dp+dpL+dpL+dpL+1) = w[5];
#define PIXEL1631_10    *(dp+dpL+dpL+dpL+1) = Interp1_16(w[5], w[7]);
#define PIXEL1631_11    *(dp+dpL+dpL+dpL+1) = Interp1_16(w[5], w[8]);
#define PIXEL1631_13    *(dp+dpL+dpL+dpL+1) = Interp1_16(w[8], w[5]);
#define PIXEL1631_21    *(dp+dpL+dpL+dpL+1) = Interp2_16(w[8], w[5], w[4]);
#define PIXEL1631_32    *(dp+dpL+dpL+dpL+1) = Interp3_16(w[5], w[4]);
#define PIXEL1631_50    *(dp+dpL+dpL+dpL+1) = Interp5_16(w[8], w[5]);
#define PIXEL1631_60    *(dp+dpL+dpL+dpL+1) = Interp6_16(w[5], w[8], w[4]);
#define PIXEL1631_61    *(dp+dpL+dpL+dpL+1) = Interp6_16(w[5], w[8], w[7]);
#define PIXEL1631_81    *(dp+dpL+dpL+dpL+1) = Interp8_16(w[5], w[8]);
#define PIXEL1631_83    *(dp+dpL+dpL+dpL+1) = Interp8_16(w[8], w[4]);
#define PIXEL1632_0     *(dp+dpL+dpL+dpL+2) = w[5];
#define PIXEL1632_10    *(dp+dpL+dpL+dpL+2) = Interp1_16(w[5], w[9]);
#define PIXEL1632_12    *(dp+dpL+dpL+dpL+2) = Interp1_16(w[5], w[8]);
#define PIXEL1632_14    *(dp+dpL+dpL+dpL+2) = Interp1_16(w[8], w[5]);
#define PIXEL1632_21    *(dp+dpL+dpL+dpL+2) = Interp2_16(w[8], w[5], w[6]);
#define PIXEL1632_31    *(dp+dpL+dpL+dpL+2) = Interp3_16(w[5], w[6]);
#define PIXEL1632_50    *(dp+dpL+dpL+dpL+2) = Interp5_16(w[8], w[5]);
#define PIXEL1632_60    *(dp+dpL+dpL+dpL+2) = Interp6_16(w[5], w[8], w[6]);
#define PIXEL1632_61    *(dp+dpL+dpL+dpL+2) = Interp6_16(w[5], w[8], w[9]);
#define PIXEL1632_82    *(dp+dpL+dpL+dpL+2) = Interp8_16(w[5], w[8]);
#define PIXEL1632_83    *(dp+dpL+dpL+dpL+2) = Interp8_16(w[8], w[6]);
#define PIXEL1633_0     *(dp+dpL+dpL+dpL+3) = w[5];
#define PIXEL1633_11    *(dp+dpL+dpL+dpL+3) = Interp1_16(w[5], w[6]);
#define PIXEL1633_12    *(dp+dpL+dpL+dpL+3) = Interp1_16(w[5], w[8]);
#define PIXEL1633_20    *(dp+dpL+dpL+dpL+3) = Interp2_16(w[5], w[8], w[6]);
#define PIXEL1633_50    *(dp+dpL+dpL+dpL+3) = Interp5_16(w[8], w[6]);
#define PIXEL1633_80    *(dp+dpL+dpL+dpL+3) = Interp8_16(w[5], w[9]);
#define PIXEL1633_81    *(dp+dpL+dpL+dpL+3) = Interp8_16(w[5], w[6]);
#define PIXEL1633_82    *(dp+dpL+dpL+dpL+3) = Interp8_16(w[5], w[8]);

HQX_API void HQX_CALLCONV hq4x_16_rb( uint16_t * sp, uint32_t srb, uint16_t * dp, uint32_t drb, int Xres, int Yres )
{
    int  i, j, k;
    int  prevline, nextline;
    uint16_t w[10];
    int dpL = (drb >> 1);
    int spL = (srb >> 1);
    uint8_t *sRowP = (uint8_t *) sp;
    uint8_t *dRowP = (uint8_t *) dp;
    uint32_t yuv1, yuv2;

    //   +----+----+----+
    //   |    |    |    |
    //   | w1 | w2 | w3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w4 | w5 | w6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w7 | w8 | w9 |
    //   +----+----+----+

    for (j=0; j<Yres; j++)
    {
        if (j>0)      prevline = -spL; else prevline = 0;
        if (j<Yres-1) nextline =  spL; else nextline = 0;

        for (i=0; i<Xres; i++)
        {
            w[2] = *(sp + prevline);
            w[5] = *sp;
            w[8] = *(sp + nextline);

            if (i>0)
            {
                w[1] = *(sp + prevline - 1);
                w[4] = *(sp - 1);
                w[7] = *(sp + nextline - 1);
            }
            else
            {
                w[1] = w[2];
                w[4] = w[5];
                w[7] = w[8];
            }

            if (i<Xres-1)
            {
                w[3] = *(sp + prevline + 1);
                w[6] = *(sp + 1);
                w[9] = *(sp + nextline + 1);
            }
            else
            {
                w[3] = w[2];
                w[6] = w[5];
                w[9] = w[8];
            }

            int pattern = 0;
            int flag = 1;

            yuv1 = rgb16_to_yuv(w[5]);

            for (k=1; k<=9; k++)
            {
                if (k==5) continue;

                if ( w[k] != w[5] )
                {
                    yuv2 = rgb16_to_yuv(w[k]);
                    if (yuv_diff(yuv1, yuv2))
                        pattern |= flag;
                }
                flag <<= 1;
            }

            switch (pattern)
            {
                case 0:
                case 1:
                case 4:
                case 32:
                case 128:
                case 5:
                case 132:
                case 160:
                case 33:
                case 129:
                case 36:
                case 133:
                case 164:
                case 161:
                case 37:
                case 165:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 2:
                case 34:
                case 130:
                case 162:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 16:
                case 17:
                case 48:
                case 49:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 64:
                case 65:
                case 68:
                case 69:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 8:
                case 12:
                case 136:
                case 140:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 3:
                case 35:
                case 131:
                case 163:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 6:
                case 38:
                case 134:
                case 166:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 20:
                case 21:
                case 52:
                case 53:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 144:
                case 145:
                case 176:
                case 177:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 192:
                case 193:
                case 196:
                case 197:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 96:
                case 97:
                case 100:
                case 101:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 40:
                case 44:
                case 168:
                case 172:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 9:
                case 13:
                case 137:
                case 141:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 18:
                case 50:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1612_0
                            PIXEL1613_50
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 80:
                case 81:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_61
                        PIXEL1621_30
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 72:
                case 76:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_70
                        PIXEL1613_60
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1621_0
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 10:
                case 138:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                            PIXEL1611_0
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 66:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 24:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 7:
                case 39:
                case 135:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 148:
                case 149:
                case 180:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 224:
                case 228:
                case 225:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 41:
                case 169:
                case 45:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 22:
                case 54:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 208:
                case 209:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 104:
                case 108:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_70
                        PIXEL1613_60
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 11:
                case 139:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 19:
                case 51:
                    {
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1600_81
                            PIXEL1601_31
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1600_12
                            PIXEL1601_14
                            PIXEL1602_83
                            PIXEL1603_50
                            PIXEL1612_70
                            PIXEL1613_21
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 146:
                case 178:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                            PIXEL1623_32
                            PIXEL1633_82
                        }
                        else
                        {
                            PIXEL1602_21
                            PIXEL1603_50
                            PIXEL1612_70
                            PIXEL1613_83
                            PIXEL1623_13
                            PIXEL1633_11
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_32
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_82
                        break;
                    }
                case 84:
                case 85:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_81
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1603_81
                            PIXEL1613_31
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1603_12
                            PIXEL1613_14
                            PIXEL1622_70
                            PIXEL1623_83
                            PIXEL1632_21
                            PIXEL1633_50
                        }
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_31
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 112:
                case 113:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_82
                        PIXEL1621_32
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1630_82
                            PIXEL1631_32
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_70
                            PIXEL1623_21
                            PIXEL1630_11
                            PIXEL1631_13
                            PIXEL1632_83
                            PIXEL1633_50
                        }
                        break;
                    }
                case 200:
                case 204:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_70
                        PIXEL1613_60
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                            PIXEL1632_31
                            PIXEL1633_81
                        }
                        else
                        {
                            PIXEL1620_21
                            PIXEL1621_70
                            PIXEL1630_50
                            PIXEL1631_83
                            PIXEL1632_14
                            PIXEL1633_12
                        }
                        PIXEL1622_31
                        PIXEL1623_81
                        break;
                    }
                case 73:
                case 77:
                    {
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1600_82
                            PIXEL1610_32
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1600_11
                            PIXEL1610_13
                            PIXEL1620_83
                            PIXEL1621_70
                            PIXEL1630_50
                            PIXEL1631_21
                        }
                        PIXEL1601_82
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1611_32
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 42:
                case 170:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                            PIXEL1620_31
                            PIXEL1630_81
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_21
                            PIXEL1610_83
                            PIXEL1611_70
                            PIXEL1620_14
                            PIXEL1630_12
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1621_31
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1631_81
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 14:
                case 142:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1602_32
                            PIXEL1603_82
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_83
                            PIXEL1602_13
                            PIXEL1603_11
                            PIXEL1610_21
                            PIXEL1611_70
                        }
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 67:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 70:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 28:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 152:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 194:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 98:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 56:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 25:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 26:
                case 31:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1611_0
                        PIXEL1612_0
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 82:
                case 214:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 88:
                case 248:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        break;
                    }
                case 74:
                case 107:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_61
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 27:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 86:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 216:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 106:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_61
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 30:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 210:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 120:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 75:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 29:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 198:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 184:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 99:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 57:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 71:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 156:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 226:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 60:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 195:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 102:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 153:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 58:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 83:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1620_61
                        PIXEL1621_30
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 92:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 202:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1612_30
                        PIXEL1613_61
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 78:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1612_32
                        PIXEL1613_82
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 154:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 114:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1620_82
                        PIXEL1621_32
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        PIXEL1630_82
                        PIXEL1631_32
                        break;
                    }
                case 89:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 90:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 55:
                case 23:
                    {
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1600_81
                            PIXEL1601_31
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1612_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1600_12
                            PIXEL1601_14
                            PIXEL1602_83
                            PIXEL1603_50
                            PIXEL1612_70
                            PIXEL1613_21
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 182:
                case 150:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1612_0
                            PIXEL1613_0
                            PIXEL1623_32
                            PIXEL1633_82
                        }
                        else
                        {
                            PIXEL1602_21
                            PIXEL1603_50
                            PIXEL1612_70
                            PIXEL1613_83
                            PIXEL1623_13
                            PIXEL1633_11
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_32
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_82
                        break;
                    }
                case 213:
                case 212:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_81
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1603_81
                            PIXEL1613_31
                            PIXEL1622_0
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1603_12
                            PIXEL1613_14
                            PIXEL1622_70
                            PIXEL1623_83
                            PIXEL1632_21
                            PIXEL1633_50
                        }
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_31
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 241:
                case 240:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_82
                        PIXEL1621_32
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_0
                            PIXEL1623_0
                            PIXEL1630_82
                            PIXEL1631_32
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1622_70
                            PIXEL1623_21
                            PIXEL1630_11
                            PIXEL1631_13
                            PIXEL1632_83
                            PIXEL1633_50
                        }
                        break;
                    }
                case 236:
                case 232:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_70
                        PIXEL1613_60
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1621_0
                            PIXEL1630_0
                            PIXEL1631_0
                            PIXEL1632_31
                            PIXEL1633_81
                        }
                        else
                        {
                            PIXEL1620_21
                            PIXEL1621_70
                            PIXEL1630_50
                            PIXEL1631_83
                            PIXEL1632_14
                            PIXEL1633_12
                        }
                        PIXEL1622_31
                        PIXEL1623_81
                        break;
                    }
                case 109:
                case 105:
                    {
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1600_82
                            PIXEL1610_32
                            PIXEL1620_0
                            PIXEL1621_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1600_11
                            PIXEL1610_13
                            PIXEL1620_83
                            PIXEL1621_70
                            PIXEL1630_50
                            PIXEL1631_21
                        }
                        PIXEL1601_82
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1611_32
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 171:
                case 43:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                            PIXEL1611_0
                            PIXEL1620_31
                            PIXEL1630_81
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_21
                            PIXEL1610_83
                            PIXEL1611_70
                            PIXEL1620_14
                            PIXEL1630_12
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1621_31
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1631_81
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 143:
                case 15:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1602_32
                            PIXEL1603_82
                            PIXEL1610_0
                            PIXEL1611_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_83
                            PIXEL1602_13
                            PIXEL1603_11
                            PIXEL1610_21
                            PIXEL1611_70
                        }
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 124:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 203:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 62:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 211:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 118:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 217:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 110:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_32
                        PIXEL1613_82
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 155:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 188:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 185:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 61:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 157:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 103:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 227:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 230:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 199:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 220:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        break;
                    }
                case 158:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1612_0
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 234:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1612_30
                        PIXEL1613_61
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 242:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_82
                        PIXEL1631_32
                        break;
                    }
                case 59:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1611_0
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 121:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 87:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_0
                        PIXEL1620_61
                        PIXEL1621_30
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 79:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1611_0
                        PIXEL1612_32
                        PIXEL1613_82
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 122:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 94:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1612_0
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 218:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        break;
                    }
                case 91:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1611_0
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 229:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 167:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 173:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 181:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 186:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 115:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1620_82
                        PIXEL1621_32
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        PIXEL1630_82
                        PIXEL1631_32
                        break;
                    }
                case 93:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1613_31
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        break;
                    }
                case 206:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1612_32
                        PIXEL1613_82
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 205:
                case 201:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_70
                        PIXEL1613_60
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_10
                            PIXEL1621_30
                            PIXEL1630_80
                            PIXEL1631_10
                        }
                        else
                        {
                            PIXEL1620_12
                            PIXEL1621_0
                            PIXEL1630_20
                            PIXEL1631_11
                        }
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 174:
                case 46:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_80
                            PIXEL1601_10
                            PIXEL1610_10
                            PIXEL1611_30
                        }
                        else
                        {
                            PIXEL1600_20
                            PIXEL1601_12
                            PIXEL1610_11
                            PIXEL1611_0
                        }
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 179:
                case 147:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_10
                            PIXEL1603_80
                            PIXEL1612_30
                            PIXEL1613_10
                        }
                        else
                        {
                            PIXEL1602_11
                            PIXEL1603_20
                            PIXEL1612_0
                            PIXEL1613_12
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 117:
                case 116:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_82
                        PIXEL1621_32
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_30
                            PIXEL1623_10
                            PIXEL1632_10
                            PIXEL1633_80
                        }
                        else
                        {
                            PIXEL1622_0
                            PIXEL1623_11
                            PIXEL1632_12
                            PIXEL1633_20
                        }
                        PIXEL1630_82
                        PIXEL1631_32
                        break;
                    }
                case 189:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 231:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 126:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_0
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 219:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 125:
                    {
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1600_82
                            PIXEL1610_32
                            PIXEL1620_0
                            PIXEL1621_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1600_11
                            PIXEL1610_13
                            PIXEL1620_83
                            PIXEL1621_70
                            PIXEL1630_50
                            PIXEL1631_21
                        }
                        PIXEL1601_82
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 221:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_81
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1603_81
                            PIXEL1613_31
                            PIXEL1622_0
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1603_12
                            PIXEL1613_14
                            PIXEL1622_70
                            PIXEL1623_83
                            PIXEL1632_21
                            PIXEL1633_50
                        }
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 207:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1602_32
                            PIXEL1603_82
                            PIXEL1610_0
                            PIXEL1611_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_83
                            PIXEL1602_13
                            PIXEL1603_11
                            PIXEL1610_21
                            PIXEL1611_70
                        }
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_31
                        PIXEL1623_81
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 238:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_32
                        PIXEL1613_82
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1621_0
                            PIXEL1630_0
                            PIXEL1631_0
                            PIXEL1632_31
                            PIXEL1633_81
                        }
                        else
                        {
                            PIXEL1620_21
                            PIXEL1621_70
                            PIXEL1630_50
                            PIXEL1631_83
                            PIXEL1632_14
                            PIXEL1633_12
                        }
                        PIXEL1622_31
                        PIXEL1623_81
                        break;
                    }
                case 190:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1612_0
                            PIXEL1613_0
                            PIXEL1623_32
                            PIXEL1633_82
                        }
                        else
                        {
                            PIXEL1602_21
                            PIXEL1603_50
                            PIXEL1612_70
                            PIXEL1613_83
                            PIXEL1623_13
                            PIXEL1633_11
                        }
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_82
                        break;
                    }
                case 187:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                            PIXEL1611_0
                            PIXEL1620_31
                            PIXEL1630_81
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_21
                            PIXEL1610_83
                            PIXEL1611_70
                            PIXEL1620_14
                            PIXEL1630_12
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1631_81
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 243:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_82
                        PIXEL1621_32
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1622_0
                            PIXEL1623_0
                            PIXEL1630_82
                            PIXEL1631_32
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1622_70
                            PIXEL1623_21
                            PIXEL1630_11
                            PIXEL1631_13
                            PIXEL1632_83
                            PIXEL1633_50
                        }
                        break;
                    }
                case 119:
                    {
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1600_81
                            PIXEL1601_31
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1612_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1600_12
                            PIXEL1601_14
                            PIXEL1602_83
                            PIXEL1603_50
                            PIXEL1612_70
                            PIXEL1613_21
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 237:
                case 233:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_60
                        PIXEL1603_20
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_70
                        PIXEL1613_60
                        PIXEL1620_0
                        PIXEL1621_0
                        PIXEL1622_31
                        PIXEL1623_81
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1630_0
                        }
                        else
                        {
                            PIXEL1630_20
                        }
                        PIXEL1631_0
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 175:
                case 47:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                        }
                        else
                        {
                            PIXEL1600_20
                        }
                        PIXEL1601_0
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_0
                        PIXEL1611_0
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_70
                        PIXEL1623_60
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_60
                        PIXEL1633_20
                        break;
                    }
                case 183:
                case 151:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1603_0
                        }
                        else
                        {
                            PIXEL1603_20
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_0
                        PIXEL1613_0
                        PIXEL1620_60
                        PIXEL1621_70
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_20
                        PIXEL1631_60
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 245:
                case 244:
                    {
                        PIXEL1600_20
                        PIXEL1601_60
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_60
                        PIXEL1611_70
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_0
                        PIXEL1623_0
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1633_20
                        }
                        break;
                    }
                case 250:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_30
                        PIXEL1613_10
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        break;
                    }
                case 123:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_10
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 95:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1611_0
                        PIXEL1612_0
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_80
                        PIXEL1631_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 222:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 252:
                    {
                        PIXEL1600_80
                        PIXEL1601_61
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_31
                        PIXEL1613_31
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_0
                        PIXEL1623_0
                        PIXEL1632_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1633_20
                        }
                        break;
                    }
                case 249:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_61
                        PIXEL1603_80
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_0
                        PIXEL1621_0
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1630_0
                        }
                        else
                        {
                            PIXEL1630_20
                        }
                        PIXEL1631_0
                        break;
                    }
                case 235:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_61
                        PIXEL1620_0
                        PIXEL1621_0
                        PIXEL1622_31
                        PIXEL1623_81
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1630_0
                        }
                        else
                        {
                            PIXEL1630_20
                        }
                        PIXEL1631_0
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 111:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                        }
                        else
                        {
                            PIXEL1600_20
                        }
                        PIXEL1601_0
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_0
                        PIXEL1611_0
                        PIXEL1612_32
                        PIXEL1613_82
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_61
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 63:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                        }
                        else
                        {
                            PIXEL1600_20
                        }
                        PIXEL1601_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_0
                        PIXEL1611_0
                        PIXEL1612_0
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_61
                        PIXEL1633_80
                        break;
                    }
                case 159:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1603_0
                        }
                        else
                        {
                            PIXEL1603_20
                        }
                        PIXEL1611_0
                        PIXEL1612_0
                        PIXEL1613_0
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_80
                        PIXEL1631_61
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 215:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1603_0
                        }
                        else
                        {
                            PIXEL1603_20
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_0
                        PIXEL1613_0
                        PIXEL1620_61
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 246:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_61
                        PIXEL1611_30
                        PIXEL1612_0
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_0
                        PIXEL1623_0
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1633_20
                        }
                        break;
                    }
                case 254:
                    {
                        PIXEL1600_80
                        PIXEL1601_10
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_10
                        PIXEL1611_30
                        PIXEL1612_0
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_0
                        PIXEL1623_0
                        PIXEL1632_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1633_20
                        }
                        break;
                    }
                case 253:
                    {
                        PIXEL1600_82
                        PIXEL1601_82
                        PIXEL1602_81
                        PIXEL1603_81
                        PIXEL1610_32
                        PIXEL1611_32
                        PIXEL1612_31
                        PIXEL1613_31
                        PIXEL1620_0
                        PIXEL1621_0
                        PIXEL1622_0
                        PIXEL1623_0
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1630_0
                        }
                        else
                        {
                            PIXEL1630_20
                        }
                        PIXEL1631_0
                        PIXEL1632_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1633_20
                        }
                        break;
                    }
                case 251:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_10
                        PIXEL1603_80
                        PIXEL1611_0
                        PIXEL1612_30
                        PIXEL1613_10
                        PIXEL1620_0
                        PIXEL1621_0
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1630_0
                        }
                        else
                        {
                            PIXEL1630_20
                        }
                        PIXEL1631_0
                        break;
                    }
                case 239:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                        }
                        else
                        {
                            PIXEL1600_20
                        }
                        PIXEL1601_0
                        PIXEL1602_32
                        PIXEL1603_82
                        PIXEL1610_0
                        PIXEL1611_0
                        PIXEL1612_32
                        PIXEL1613_82
                        PIXEL1620_0
                        PIXEL1621_0
                        PIXEL1622_31
                        PIXEL1623_81
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1630_0
                        }
                        else
                        {
                            PIXEL1630_20
                        }
                        PIXEL1631_0
                        PIXEL1632_31
                        PIXEL1633_81
                        break;
                    }
                case 127:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                        }
                        else
                        {
                            PIXEL1600_20
                        }
                        PIXEL1601_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1602_0
                            PIXEL1603_0
                            PIXEL1613_0
                        }
                        else
                        {
                            PIXEL1602_50
                            PIXEL1603_50
                            PIXEL1613_50
                        }
                        PIXEL1610_0
                        PIXEL1611_0
                        PIXEL1612_0
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1620_0
                            PIXEL1630_0
                            PIXEL1631_0
                        }
                        else
                        {
                            PIXEL1620_50
                            PIXEL1630_50
                            PIXEL1631_50
                        }
                        PIXEL1621_0
                        PIXEL1622_30
                        PIXEL1623_10
                        PIXEL1632_10
                        PIXEL1633_80
                        break;
                    }
                case 191:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                        }
                        else
                        {
                            PIXEL1600_20
                        }
                        PIXEL1601_0
                        PIXEL1602_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1603_0
                        }
                        else
                        {
                            PIXEL1603_20
                        }
                        PIXEL1610_0
                        PIXEL1611_0
                        PIXEL1612_0
                        PIXEL1613_0
                        PIXEL1620_31
                        PIXEL1621_31
                        PIXEL1622_32
                        PIXEL1623_32
                        PIXEL1630_81
                        PIXEL1631_81
                        PIXEL1632_82
                        PIXEL1633_82
                        break;
                    }
                case 223:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                            PIXEL1601_0
                            PIXEL1610_0
                        }
                        else
                        {
                            PIXEL1600_50
                            PIXEL1601_50
                            PIXEL1610_50
                        }
                        PIXEL1602_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1603_0
                        }
                        else
                        {
                            PIXEL1603_20
                        }
                        PIXEL1611_0
                        PIXEL1612_0
                        PIXEL1613_0
                        PIXEL1620_10
                        PIXEL1621_30
                        PIXEL1622_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1623_0
                            PIXEL1632_0
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1623_50
                            PIXEL1632_50
                            PIXEL1633_50
                        }
                        PIXEL1630_80
                        PIXEL1631_10
                        break;
                    }
                case 247:
                    {
                        PIXEL1600_81
                        PIXEL1601_31
                        PIXEL1602_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1603_0
                        }
                        else
                        {
                            PIXEL1603_20
                        }
                        PIXEL1610_81
                        PIXEL1611_31
                        PIXEL1612_0
                        PIXEL1613_0
                        PIXEL1620_82
                        PIXEL1621_32
                        PIXEL1622_0
                        PIXEL1623_0
                        PIXEL1630_82
                        PIXEL1631_32
                        PIXEL1632_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1633_20
                        }
                        break;
                    }
                case 255:
                    {
                        if (Diff16(w[4], w[2]))
                        {
                            PIXEL1600_0
                        }
                        else
                        {
                            PIXEL1600_20
                        }
                        PIXEL1601_0
                        PIXEL1602_0
                        if (Diff16(w[2], w[6]))
                        {
                            PIXEL1603_0
                        }
                        else
                        {
                            PIXEL1603_20
                        }
                        PIXEL1610_0
                        PIXEL1611_0
                        PIXEL1612_0
                        PIXEL1613_0
                        PIXEL1620_0
                        PIXEL1621_0
                        PIXEL1622_0
                        PIXEL1623_0
                        if (Diff16(w[8], w[4]))
                        {
                            PIXEL1630_0
                        }
                        else
                        {
                            PIXEL1630_20
                        }
                        PIXEL1631_0
                        PIXEL1632_0
                        if (Diff16(w[6], w[8]))
                        {
                            PIXEL1633_0
                        }
                        else
                        {
                            PIXEL1633_20
                        }
                        break;
                    }
            }
            sp++;
            dp += 4;
        }

        sRowP += srb;
        sp = (uint16_t *) sRowP;

        dRowP += drb * 4;
        dp = (uint16_t *) dRowP;
    }
}

HQX_API void HQX_CALLCONV hq4x_16( uint16_t * sp, uint16_t * dp, int Xres, int Yres )
{
    uint16_t rowBytesL = Xres * 2;
    hq4x_16_rb(sp, rowBytesL, dp, rowBytesL * 4, Xres, Yres);
}

#define PIXEL2400_0     u24cpy(dp, w[5]);
#define PIXEL2400_11    Interp1_24(dp, w[5], w[4]);
#define PIXEL2400_12    Interp1_24(dp, w[5], w[2]);
#define PIXEL2400_20    Interp2_24(dp, w[5], w[2], w[4]);
#define PIXEL2400_50    Interp5_24(dp, w[2], w[4]);
#define PIXEL2400_80    Interp8_24(dp, w[5], w[1]);
#define PIXEL2400_81    Interp8_24(dp, w[5], w[4]);
#define PIXEL2400_82    Interp8_24(dp, w[5], w[2]);
#define PIXEL2401_0     u24cpy((dp+1), w[5]);
#define PIXEL2401_10    Interp1_24((dp+1), w[5], w[1]);
#define PIXEL2401_12    Interp1_24((dp+1), w[5], w[2]);
#define PIXEL2401_14    Interp1_24((dp+1), w[2], w[5]);
#define PIXEL2401_21    Interp2_24((dp+1), w[2], w[5], w[4]);
#define PIXEL2401_31    Interp3_24((dp+1), w[5], w[4]);
#define PIXEL2401_50    Interp5_24((dp+1), w[2], w[5]);
#define PIXEL2401_60    Interp6_24((dp+1), w[5], w[2], w[4]);
#define PIXEL2401_61    Interp6_24((dp+1), w[5], w[2], w[1]);
#define PIXEL2401_82    Interp8_24((dp+1), w[5], w[2]);
#define PIXEL2401_83    Interp8_24((dp+1), w[2], w[4]);
#define PIXEL2402_0     u24cpy((dp+2), w[5]);
#define PIXEL2402_10    Interp1_24((dp+2), w[5], w[3]);
#define PIXEL2402_11    Interp1_24((dp+2), w[5], w[2]);
#define PIXEL2402_13    Interp1_24((dp+2), w[2], w[5]);
#define PIXEL2402_21    Interp2_24((dp+2), w[2], w[5], w[6]);
#define PIXEL2402_32    Interp3_24((dp+2), w[5], w[6]);
#define PIXEL2402_50    Interp5_24((dp+2), w[2], w[5]);
#define PIXEL2402_60    Interp6_24((dp+2), w[5], w[2], w[6]);
#define PIXEL2402_61    Interp6_24((dp+2), w[5], w[2], w[3]);
#define PIXEL2402_81    Interp8_24((dp+2), w[5], w[2]);
#define PIXEL2402_83    Interp8_24((dp+2), w[2], w[6]);
#define PIXEL2403_0     u24cpy((dp+3), w[5]);
#define PIXEL2403_11    Interp1_24((dp+3), w[5], w[2]);
#define PIXEL2403_12    Interp1_24((dp+3), w[5], w[6]);
#define PIXEL2403_20    Interp2_24((dp+3), w[5], w[2], w[6]);
#define PIXEL2403_50    Interp5_24((dp+3), w[2], w[6]);
#define PIXEL2403_80    Interp8_24((dp+3), w[5], w[3]);
#define PIXEL2403_81    Interp8_24((dp+3), w[5], w[2]);
#define PIXEL2403_82    Interp8_24((dp+3), w[5], w[6]);
#define PIXEL2410_0     u24cpy((dp+dpL), w[5]);
#define PIXEL2410_10    Interp1_24((dp+dpL), w[5], w[1]);
#define PIXEL2410_11    Interp1_24((dp+dpL), w[5], w[4]);
#define PIXEL2410_13    Interp1_24((dp+dpL), w[4], w[5]);
#define PIXEL2410_21    Interp2_24((dp+dpL), w[4], w[5], w[2]);
#define PIXEL2410_32    Interp3_24((dp+dpL), w[5], w[2]);
#define PIXEL2410_50    Interp5_24((dp+dpL), w[4], w[5]);
#define PIXEL2410_60    Interp6_24((dp+dpL), w[5], w[4], w[2]);
#define PIXEL2410_61    Interp6_24((dp+dpL), w[5], w[4], w[1]);
#define PIXEL2410_81    Interp8_24((dp+dpL), w[5], w[4]);
#define PIXEL2410_83    Interp8_24((dp+dpL), w[4], w[2]);
#define PIXEL2411_0     u24cpy((dp+dpL+1), w[5]);
#define PIXEL2411_30    Interp3_24((dp+dpL+1), w[5], w[1]);
#define PIXEL2411_31    Interp3_24((dp+dpL+1), w[5], w[4]);
#define PIXEL2411_32    Interp3_24((dp+dpL+1), w[5], w[2]);
#define PIXEL2411_70    Interp7_24((dp+dpL+1), w[5], w[4], w[2]);
#define PIXEL2412_0     u24cpy((dp+dpL+2), w[5]);
#define PIXEL2412_30    Interp3_24((dp+dpL+2), w[5], w[3]);
#define PIXEL2412_31    Interp3_24((dp+dpL+2), w[5], w[2]);
#define PIXEL2412_32    Interp3_24((dp+dpL+2), w[5], w[6]);
#define PIXEL2412_70    Interp7_24((dp+dpL+2), w[5], w[6], w[2]);
#define PIXEL2413_0     u24cpy((dp+dpL+3), w[5]);
#define PIXEL2413_10    Interp1_24((dp+dpL+3), w[5], w[3]);
#define PIXEL2413_12    Interp1_24((dp+dpL+3), w[5], w[6]);
#define PIXEL2413_14    Interp1_24((dp+dpL+3), w[6], w[5]);
#define PIXEL2413_21    Interp2_24((dp+dpL+3), w[6], w[5], w[2]);
#define PIXEL2413_31    Interp3_24((dp+dpL+3), w[5], w[2]);
#define PIXEL2413_50    Interp5_24((dp+dpL+3), w[6], w[5]);
#define PIXEL2413_60    Interp6_24((dp+dpL+3), w[5], w[6], w[2]);
#define PIXEL2413_61    Interp6_24((dp+dpL+3), w[5], w[6], w[3]);
#define PIXEL2413_82    Interp8_24((dp+dpL+3), w[5], w[6]);
#define PIXEL2413_83    Interp8_24((dp+dpL+3), w[6], w[2]);
#define PIXEL2420_0     u24cpy((dp+dpL+dpL), w[5]);
#define PIXEL2420_10    Interp1_24((dp+dpL+dpL), w[5], w[7]);
#define PIXEL2420_12    Interp1_24((dp+dpL+dpL), w[5], w[4]);
#define PIXEL2420_14    Interp1_24((dp+dpL+dpL), w[4], w[5]);
#define PIXEL2420_21    Interp2_24((dp+dpL+dpL), w[4], w[5], w[8]);
#define PIXEL2420_31    Interp3_24((dp+dpL+dpL), w[5], w[8]);
#define PIXEL2420_50    Interp5_24((dp+dpL+dpL), w[4], w[5]);
#define PIXEL2420_60    Interp6_24((dp+dpL+dpL), w[5], w[4], w[8]);
#define PIXEL2420_61    Interp6_24((dp+dpL+dpL), w[5], w[4], w[7]);
#define PIXEL2420_82    Interp8_24((dp+dpL+dpL), w[5], w[4]);
#define PIXEL2420_83    Interp8_24((dp+dpL+dpL), w[4], w[8]);
#define PIXEL2421_0     u24cpy((dp+dpL+dpL+1), w[5]);
#define PIXEL2421_30    Interp3_24((dp+dpL+dpL+1), w[5], w[7]);
#define PIXEL2421_31    Interp3_24((dp+dpL+dpL+1), w[5], w[8]);
#define PIXEL2421_32    Interp3_24((dp+dpL+dpL+1), w[5], w[4]);
#define PIXEL2421_70    Interp7_24((dp+dpL+dpL+1), w[5], w[4], w[8]);
#define PIXEL2422_0     u24cpy((dp+dpL+dpL+2), w[5]);
#define PIXEL2422_30    Interp3_24((dp+dpL+dpL+2), w[5], w[9]);
#define PIXEL2422_31    Interp3_24((dp+dpL+dpL+2), w[5], w[6]);
#define PIXEL2422_32    Interp3_24((dp+dpL+dpL+2), w[5], w[8]);
#define PIXEL2422_70    Interp7_24((dp+dpL+dpL+2), w[5], w[6], w[8]);
#define PIXEL2423_0     u24cpy((dp+dpL+dpL+3), w[5]);
#define PIXEL2423_10    Interp1_24((dp+dpL+dpL+3), w[5], w[9]);
#define PIXEL2423_11    Interp1_24((dp+dpL+dpL+3), w[5], w[6]);
#define PIXEL2423_13    Interp1_24((dp+dpL+dpL+3), w[6], w[5]);
#define PIXEL2423_21    Interp2_24((dp+dpL+dpL+3), w[6], w[5], w[8]);
#define PIXEL2423_32    Interp3_24((dp+dpL+dpL+3), w[5], w[8]);
#define PIXEL2423_50    Interp5_24((dp+dpL+dpL+3), w[6], w[5]);
#define PIXEL2423_60    Interp6_24((dp+dpL+dpL+3), w[5], w[6], w[8]);
#define PIXEL2423_61    Interp6_24((dp+dpL+dpL+3), w[5], w[6], w[9]);
#define PIXEL2423_81    Interp8_24((dp+dpL+dpL+3), w[5], w[6]);
#define PIXEL2423_83    Interp8_24((dp+dpL+dpL+3), w[6], w[8]);
#define PIXEL2430_0     u24cpy((dp+dpL+dpL+dpL), w[5]);
#define PIXEL2430_11    Interp1_24((dp+dpL+dpL+dpL), w[5], w[8]);
#define PIXEL2430_12    Interp1_24((dp+dpL+dpL+dpL), w[5], w[4]);
#define PIXEL2430_20    Interp2_24((dp+dpL+dpL+dpL), w[5], w[8], w[4]);
#define PIXEL2430_50    Interp5_24((dp+dpL+dpL+dpL), w[8], w[4]);
#define PIXEL2430_80    Interp8_24((dp+dpL+dpL+dpL), w[5], w[7]);
#define PIXEL2430_81    Interp8_24((dp+dpL+dpL+dpL), w[5], w[8]);
#define PIXEL2430_82    Interp8_24((dp+dpL+dpL+dpL), w[5], w[4]);
#define PIXEL2431_0     u24cpy((dp+dpL+dpL+dpL+1), w[5]);
#define PIXEL2431_10    Interp1_24((dp+dpL+dpL+dpL+1), w[5], w[7]);
#define PIXEL2431_11    Interp1_24((dp+dpL+dpL+dpL+1), w[5], w[8]);
#define PIXEL2431_13    Interp1_24((dp+dpL+dpL+dpL+1), w[8], w[5]);
#define PIXEL2431_21    Interp2_24((dp+dpL+dpL+dpL+1), w[8], w[5], w[4]);
#define PIXEL2431_32    Interp3_24((dp+dpL+dpL+dpL+1), w[5], w[4]);
#define PIXEL2431_50    Interp5_24((dp+dpL+dpL+dpL+1), w[8], w[5]);
#define PIXEL2431_60    Interp6_24((dp+dpL+dpL+dpL+1), w[5], w[8], w[4]);
#define PIXEL2431_61    Interp6_24((dp+dpL+dpL+dpL+1), w[5], w[8], w[7]);
#define PIXEL2431_81    Interp8_24((dp+dpL+dpL+dpL+1), w[5], w[8]);
#define PIXEL2431_83    Interp8_24((dp+dpL+dpL+dpL+1), w[8], w[4]);
#define PIXEL2432_0     u24cpy((dp+dpL+dpL+dpL+2), w[5]);
#define PIXEL2432_10    Interp1_24((dp+dpL+dpL+dpL+2), w[5], w[9]);
#define PIXEL2432_12    Interp1_24((dp+dpL+dpL+dpL+2), w[5], w[8]);
#define PIXEL2432_14    Interp1_24((dp+dpL+dpL+dpL+2), w[8], w[5]);
#define PIXEL2432_21    Interp2_24((dp+dpL+dpL+dpL+2), w[8], w[5], w[6]);
#define PIXEL2432_31    Interp3_24((dp+dpL+dpL+dpL+2), w[5], w[6]);
#define PIXEL2432_50    Interp5_24((dp+dpL+dpL+dpL+2), w[8], w[5]);
#define PIXEL2432_60    Interp6_24((dp+dpL+dpL+dpL+2), w[5], w[8], w[6]);
#define PIXEL2432_61    Interp6_24((dp+dpL+dpL+dpL+2), w[5], w[8], w[9]);
#define PIXEL2432_82    Interp8_24((dp+dpL+dpL+dpL+2), w[5], w[8]);
#define PIXEL2432_83    Interp8_24((dp+dpL+dpL+dpL+2), w[8], w[6]);
#define PIXEL2433_0     u24cpy((dp+dpL+dpL+dpL+3), w[5]);
#define PIXEL2433_11    Interp1_24((dp+dpL+dpL+dpL+3), w[5], w[6]);
#define PIXEL2433_12    Interp1_24((dp+dpL+dpL+dpL+3), w[5], w[8]);
#define PIXEL2433_20    Interp2_24((dp+dpL+dpL+dpL+3), w[5], w[8], w[6]);
#define PIXEL2433_50    Interp5_24((dp+dpL+dpL+dpL+3), w[8], w[6]);
#define PIXEL2433_80    Interp8_24((dp+dpL+dpL+dpL+3), w[5], w[9]);
#define PIXEL2433_81    Interp8_24((dp+dpL+dpL+dpL+3), w[5], w[6]);
#define PIXEL2433_82    Interp8_24((dp+dpL+dpL+dpL+3), w[5], w[8]);

HQX_API void HQX_CALLCONV hq4x_24_rb( uint24_t * sp, uint32_t srb, uint24_t * dp, uint32_t drb, int Xres, int Yres )
{
    int  i, j, k;
    int  prevline, nextline;
    uint24_t w[10];
    int dpL = (drb / 3);
    int spL = (srb / 3);
    uint8_t *sRowP = (uint8_t *) sp;
    uint8_t *dRowP = (uint8_t *) dp;
    uint32_t yuv1, yuv2;

    //   +----+----+----+
    //   |    |    |    |
    //   | w1 | w2 | w3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w4 | w5 | w6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w7 | w8 | w9 |
    //   +----+----+----+

    for (j=0; j<Yres; j++)
    {
        if (j>0)      prevline = -spL; else prevline = 0;
        if (j<Yres-1) nextline =  spL; else nextline = 0;

        for (i=0; i<Xres; i++)
        {
            u24cpy(&w[2], *(sp + prevline));
            u24cpy(&w[5], *sp);
            u24cpy(&w[8], *(sp + nextline));

            if (i>0)
            {
                u24cpy(&w[1], *(sp + prevline - 1));
                u24cpy(&w[4], *(sp - 1));
                u24cpy(&w[7], *(sp + nextline - 1));
            }
            else
            {
                u24cpy(&w[1], w[2]);
                u24cpy(&w[4], w[5]);
                u24cpy(&w[7], w[8]);
            }

            if (i<Xres-1)
            {
                u24cpy(&w[3], *(sp + prevline + 1));
                u24cpy(&w[6], *(sp + 1));
                u24cpy(&w[9], *(sp + nextline + 1));
            }
            else
            {
                u24cpy(&w[3], w[2]);
                u24cpy(&w[6], w[5]);
                u24cpy(&w[9], w[8]);
            }

            int pattern = 0;
            int flag = 1;

            yuv1 = rgb24_to_yuv(w[5]);

            for (k=1; k<=9; k++)
            {
                if (k==5) continue;

                if ( w[k] != w[5] )
                {
                    yuv2 = rgb24_to_yuv(w[k]);
                    if (yuv_diff(yuv1, yuv2))
                        pattern |= flag;
                }
                flag <<= 1;
            }

            switch (pattern)
            {
                case 0:
                case 1:
                case 4:
                case 32:
                case 128:
                case 5:
                case 132:
                case 160:
                case 33:
                case 129:
                case 36:
                case 133:
                case 164:
                case 161:
                case 37:
                case 165:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 2:
                case 34:
                case 130:
                case 162:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 16:
                case 17:
                case 48:
                case 49:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 64:
                case 65:
                case 68:
                case 69:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 8:
                case 12:
                case 136:
                case 140:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 3:
                case 35:
                case 131:
                case 163:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 6:
                case 38:
                case 134:
                case 166:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 20:
                case 21:
                case 52:
                case 53:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 144:
                case 145:
                case 176:
                case 177:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 192:
                case 193:
                case 196:
                case 197:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 96:
                case 97:
                case 100:
                case 101:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 40:
                case 44:
                case 168:
                case 172:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 9:
                case 13:
                case 137:
                case 141:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 18:
                case 50:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2412_0
                            PIXEL2413_50
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 80:
                case 81:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_61
                        PIXEL2421_30
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 72:
                case 76:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_70
                        PIXEL2413_60
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2421_0
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 10:
                case 138:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                            PIXEL2411_0
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 66:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 24:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 7:
                case 39:
                case 135:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 148:
                case 149:
                case 180:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 224:
                case 228:
                case 225:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 41:
                case 169:
                case 45:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 22:
                case 54:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 208:
                case 209:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 104:
                case 108:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_70
                        PIXEL2413_60
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 11:
                case 139:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 19:
                case 51:
                    {
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2400_81
                            PIXEL2401_31
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2400_12
                            PIXEL2401_14
                            PIXEL2402_83
                            PIXEL2403_50
                            PIXEL2412_70
                            PIXEL2413_21
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 146:
                case 178:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                            PIXEL2423_32
                            PIXEL2433_82
                        }
                        else
                        {
                            PIXEL2402_21
                            PIXEL2403_50
                            PIXEL2412_70
                            PIXEL2413_83
                            PIXEL2423_13
                            PIXEL2433_11
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_32
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_82
                        break;
                    }
                case 84:
                case 85:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_81
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2403_81
                            PIXEL2413_31
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2403_12
                            PIXEL2413_14
                            PIXEL2422_70
                            PIXEL2423_83
                            PIXEL2432_21
                            PIXEL2433_50
                        }
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_31
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 112:
                case 113:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_82
                        PIXEL2421_32
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2430_82
                            PIXEL2431_32
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_70
                            PIXEL2423_21
                            PIXEL2430_11
                            PIXEL2431_13
                            PIXEL2432_83
                            PIXEL2433_50
                        }
                        break;
                    }
                case 200:
                case 204:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_70
                        PIXEL2413_60
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                            PIXEL2432_31
                            PIXEL2433_81
                        }
                        else
                        {
                            PIXEL2420_21
                            PIXEL2421_70
                            PIXEL2430_50
                            PIXEL2431_83
                            PIXEL2432_14
                            PIXEL2433_12
                        }
                        PIXEL2422_31
                        PIXEL2423_81
                        break;
                    }
                case 73:
                case 77:
                    {
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2400_82
                            PIXEL2410_32
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2400_11
                            PIXEL2410_13
                            PIXEL2420_83
                            PIXEL2421_70
                            PIXEL2430_50
                            PIXEL2431_21
                        }
                        PIXEL2401_82
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2411_32
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 42:
                case 170:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                            PIXEL2420_31
                            PIXEL2430_81
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_21
                            PIXEL2410_83
                            PIXEL2411_70
                            PIXEL2420_14
                            PIXEL2430_12
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2421_31
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2431_81
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 14:
                case 142:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2402_32
                            PIXEL2403_82
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_83
                            PIXEL2402_13
                            PIXEL2403_11
                            PIXEL2410_21
                            PIXEL2411_70
                        }
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 67:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 70:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 28:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 152:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 194:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 98:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 56:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 25:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 26:
                case 31:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2411_0
                        PIXEL2412_0
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 82:
                case 214:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 88:
                case 248:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        break;
                    }
                case 74:
                case 107:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_61
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 27:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 86:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 216:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 106:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_61
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 30:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 210:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 120:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 75:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 29:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 198:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 184:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 99:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 57:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 71:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 156:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 226:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 60:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 195:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 102:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 153:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 58:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 83:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2420_61
                        PIXEL2421_30
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 92:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 202:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2412_30
                        PIXEL2413_61
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 78:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2412_32
                        PIXEL2413_82
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 154:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 114:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2420_82
                        PIXEL2421_32
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        PIXEL2430_82
                        PIXEL2431_32
                        break;
                    }
                case 89:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 90:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 55:
                case 23:
                    {
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2400_81
                            PIXEL2401_31
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2412_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2400_12
                            PIXEL2401_14
                            PIXEL2402_83
                            PIXEL2403_50
                            PIXEL2412_70
                            PIXEL2413_21
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 182:
                case 150:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2412_0
                            PIXEL2413_0
                            PIXEL2423_32
                            PIXEL2433_82
                        }
                        else
                        {
                            PIXEL2402_21
                            PIXEL2403_50
                            PIXEL2412_70
                            PIXEL2413_83
                            PIXEL2423_13
                            PIXEL2433_11
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_32
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_82
                        break;
                    }
                case 213:
                case 212:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_81
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2403_81
                            PIXEL2413_31
                            PIXEL2422_0
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2403_12
                            PIXEL2413_14
                            PIXEL2422_70
                            PIXEL2423_83
                            PIXEL2432_21
                            PIXEL2433_50
                        }
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_31
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 241:
                case 240:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_82
                        PIXEL2421_32
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_0
                            PIXEL2423_0
                            PIXEL2430_82
                            PIXEL2431_32
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2422_70
                            PIXEL2423_21
                            PIXEL2430_11
                            PIXEL2431_13
                            PIXEL2432_83
                            PIXEL2433_50
                        }
                        break;
                    }
                case 236:
                case 232:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_70
                        PIXEL2413_60
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2421_0
                            PIXEL2430_0
                            PIXEL2431_0
                            PIXEL2432_31
                            PIXEL2433_81
                        }
                        else
                        {
                            PIXEL2420_21
                            PIXEL2421_70
                            PIXEL2430_50
                            PIXEL2431_83
                            PIXEL2432_14
                            PIXEL2433_12
                        }
                        PIXEL2422_31
                        PIXEL2423_81
                        break;
                    }
                case 109:
                case 105:
                    {
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2400_82
                            PIXEL2410_32
                            PIXEL2420_0
                            PIXEL2421_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2400_11
                            PIXEL2410_13
                            PIXEL2420_83
                            PIXEL2421_70
                            PIXEL2430_50
                            PIXEL2431_21
                        }
                        PIXEL2401_82
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2411_32
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 171:
                case 43:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                            PIXEL2411_0
                            PIXEL2420_31
                            PIXEL2430_81
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_21
                            PIXEL2410_83
                            PIXEL2411_70
                            PIXEL2420_14
                            PIXEL2430_12
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2421_31
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2431_81
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 143:
                case 15:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2402_32
                            PIXEL2403_82
                            PIXEL2410_0
                            PIXEL2411_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_83
                            PIXEL2402_13
                            PIXEL2403_11
                            PIXEL2410_21
                            PIXEL2411_70
                        }
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 124:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 203:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 62:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 211:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 118:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 217:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 110:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_32
                        PIXEL2413_82
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 155:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 188:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 185:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 61:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 157:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 103:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 227:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 230:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 199:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 220:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        break;
                    }
                case 158:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2412_0
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 234:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2412_30
                        PIXEL2413_61
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 242:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_82
                        PIXEL2431_32
                        break;
                    }
                case 59:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2411_0
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 121:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 87:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_0
                        PIXEL2420_61
                        PIXEL2421_30
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 79:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2411_0
                        PIXEL2412_32
                        PIXEL2413_82
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 122:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 94:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2412_0
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 218:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        break;
                    }
                case 91:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2411_0
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 229:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 167:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 173:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 181:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 186:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 115:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2420_82
                        PIXEL2421_32
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        PIXEL2430_82
                        PIXEL2431_32
                        break;
                    }
                case 93:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2413_31
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        break;
                    }
                case 206:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2412_32
                        PIXEL2413_82
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 205:
                case 201:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_70
                        PIXEL2413_60
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_10
                            PIXEL2421_30
                            PIXEL2430_80
                            PIXEL2431_10
                        }
                        else
                        {
                            PIXEL2420_12
                            PIXEL2421_0
                            PIXEL2430_20
                            PIXEL2431_11
                        }
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 174:
                case 46:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_80
                            PIXEL2401_10
                            PIXEL2410_10
                            PIXEL2411_30
                        }
                        else
                        {
                            PIXEL2400_20
                            PIXEL2401_12
                            PIXEL2410_11
                            PIXEL2411_0
                        }
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 179:
                case 147:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_10
                            PIXEL2403_80
                            PIXEL2412_30
                            PIXEL2413_10
                        }
                        else
                        {
                            PIXEL2402_11
                            PIXEL2403_20
                            PIXEL2412_0
                            PIXEL2413_12
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 117:
                case 116:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_82
                        PIXEL2421_32
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_30
                            PIXEL2423_10
                            PIXEL2432_10
                            PIXEL2433_80
                        }
                        else
                        {
                            PIXEL2422_0
                            PIXEL2423_11
                            PIXEL2432_12
                            PIXEL2433_20
                        }
                        PIXEL2430_82
                        PIXEL2431_32
                        break;
                    }
                case 189:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 231:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 126:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_0
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 219:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 125:
                    {
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2400_82
                            PIXEL2410_32
                            PIXEL2420_0
                            PIXEL2421_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2400_11
                            PIXEL2410_13
                            PIXEL2420_83
                            PIXEL2421_70
                            PIXEL2430_50
                            PIXEL2431_21
                        }
                        PIXEL2401_82
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 221:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_81
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2403_81
                            PIXEL2413_31
                            PIXEL2422_0
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2403_12
                            PIXEL2413_14
                            PIXEL2422_70
                            PIXEL2423_83
                            PIXEL2432_21
                            PIXEL2433_50
                        }
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 207:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2402_32
                            PIXEL2403_82
                            PIXEL2410_0
                            PIXEL2411_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_83
                            PIXEL2402_13
                            PIXEL2403_11
                            PIXEL2410_21
                            PIXEL2411_70
                        }
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_31
                        PIXEL2423_81
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 238:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_32
                        PIXEL2413_82
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2421_0
                            PIXEL2430_0
                            PIXEL2431_0
                            PIXEL2432_31
                            PIXEL2433_81
                        }
                        else
                        {
                            PIXEL2420_21
                            PIXEL2421_70
                            PIXEL2430_50
                            PIXEL2431_83
                            PIXEL2432_14
                            PIXEL2433_12
                        }
                        PIXEL2422_31
                        PIXEL2423_81
                        break;
                    }
                case 190:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2412_0
                            PIXEL2413_0
                            PIXEL2423_32
                            PIXEL2433_82
                        }
                        else
                        {
                            PIXEL2402_21
                            PIXEL2403_50
                            PIXEL2412_70
                            PIXEL2413_83
                            PIXEL2423_13
                            PIXEL2433_11
                        }
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_82
                        break;
                    }
                case 187:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                            PIXEL2411_0
                            PIXEL2420_31
                            PIXEL2430_81
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_21
                            PIXEL2410_83
                            PIXEL2411_70
                            PIXEL2420_14
                            PIXEL2430_12
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2431_81
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 243:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_82
                        PIXEL2421_32
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2422_0
                            PIXEL2423_0
                            PIXEL2430_82
                            PIXEL2431_32
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2422_70
                            PIXEL2423_21
                            PIXEL2430_11
                            PIXEL2431_13
                            PIXEL2432_83
                            PIXEL2433_50
                        }
                        break;
                    }
                case 119:
                    {
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2400_81
                            PIXEL2401_31
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2412_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2400_12
                            PIXEL2401_14
                            PIXEL2402_83
                            PIXEL2403_50
                            PIXEL2412_70
                            PIXEL2413_21
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 237:
                case 233:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_60
                        PIXEL2403_20
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_70
                        PIXEL2413_60
                        PIXEL2420_0
                        PIXEL2421_0
                        PIXEL2422_31
                        PIXEL2423_81
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2430_0
                        }
                        else
                        {
                            PIXEL2430_20
                        }
                        PIXEL2431_0
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 175:
                case 47:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                        }
                        else
                        {
                            PIXEL2400_20
                        }
                        PIXEL2401_0
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_0
                        PIXEL2411_0
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_70
                        PIXEL2423_60
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_60
                        PIXEL2433_20
                        break;
                    }
                case 183:
                case 151:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2403_0
                        }
                        else
                        {
                            PIXEL2403_20
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_0
                        PIXEL2413_0
                        PIXEL2420_60
                        PIXEL2421_70
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_20
                        PIXEL2431_60
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 245:
                case 244:
                    {
                        PIXEL2400_20
                        PIXEL2401_60
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_60
                        PIXEL2411_70
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_0
                        PIXEL2423_0
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2433_20
                        }
                        break;
                    }
                case 250:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_30
                        PIXEL2413_10
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        break;
                    }
                case 123:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_10
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 95:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2411_0
                        PIXEL2412_0
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_80
                        PIXEL2431_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 222:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 252:
                    {
                        PIXEL2400_80
                        PIXEL2401_61
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_31
                        PIXEL2413_31
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_0
                        PIXEL2423_0
                        PIXEL2432_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2433_20
                        }
                        break;
                    }
                case 249:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_61
                        PIXEL2403_80
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_0
                        PIXEL2421_0
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2430_0
                        }
                        else
                        {
                            PIXEL2430_20
                        }
                        PIXEL2431_0
                        break;
                    }
                case 235:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_61
                        PIXEL2420_0
                        PIXEL2421_0
                        PIXEL2422_31
                        PIXEL2423_81
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2430_0
                        }
                        else
                        {
                            PIXEL2430_20
                        }
                        PIXEL2431_0
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 111:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                        }
                        else
                        {
                            PIXEL2400_20
                        }
                        PIXEL2401_0
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_0
                        PIXEL2411_0
                        PIXEL2412_32
                        PIXEL2413_82
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_61
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 63:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                        }
                        else
                        {
                            PIXEL2400_20
                        }
                        PIXEL2401_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_0
                        PIXEL2411_0
                        PIXEL2412_0
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_61
                        PIXEL2433_80
                        break;
                    }
                case 159:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2403_0
                        }
                        else
                        {
                            PIXEL2403_20
                        }
                        PIXEL2411_0
                        PIXEL2412_0
                        PIXEL2413_0
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_80
                        PIXEL2431_61
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 215:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2403_0
                        }
                        else
                        {
                            PIXEL2403_20
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_0
                        PIXEL2413_0
                        PIXEL2420_61
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 246:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_61
                        PIXEL2411_30
                        PIXEL2412_0
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_0
                        PIXEL2423_0
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2433_20
                        }
                        break;
                    }
                case 254:
                    {
                        PIXEL2400_80
                        PIXEL2401_10
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_10
                        PIXEL2411_30
                        PIXEL2412_0
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_0
                        PIXEL2423_0
                        PIXEL2432_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2433_20
                        }
                        break;
                    }
                case 253:
                    {
                        PIXEL2400_82
                        PIXEL2401_82
                        PIXEL2402_81
                        PIXEL2403_81
                        PIXEL2410_32
                        PIXEL2411_32
                        PIXEL2412_31
                        PIXEL2413_31
                        PIXEL2420_0
                        PIXEL2421_0
                        PIXEL2422_0
                        PIXEL2423_0
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2430_0
                        }
                        else
                        {
                            PIXEL2430_20
                        }
                        PIXEL2431_0
                        PIXEL2432_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2433_20
                        }
                        break;
                    }
                case 251:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_10
                        PIXEL2403_80
                        PIXEL2411_0
                        PIXEL2412_30
                        PIXEL2413_10
                        PIXEL2420_0
                        PIXEL2421_0
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2430_0
                        }
                        else
                        {
                            PIXEL2430_20
                        }
                        PIXEL2431_0
                        break;
                    }
                case 239:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                        }
                        else
                        {
                            PIXEL2400_20
                        }
                        PIXEL2401_0
                        PIXEL2402_32
                        PIXEL2403_82
                        PIXEL2410_0
                        PIXEL2411_0
                        PIXEL2412_32
                        PIXEL2413_82
                        PIXEL2420_0
                        PIXEL2421_0
                        PIXEL2422_31
                        PIXEL2423_81
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2430_0
                        }
                        else
                        {
                            PIXEL2430_20
                        }
                        PIXEL2431_0
                        PIXEL2432_31
                        PIXEL2433_81
                        break;
                    }
                case 127:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                        }
                        else
                        {
                            PIXEL2400_20
                        }
                        PIXEL2401_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2402_0
                            PIXEL2403_0
                            PIXEL2413_0
                        }
                        else
                        {
                            PIXEL2402_50
                            PIXEL2403_50
                            PIXEL2413_50
                        }
                        PIXEL2410_0
                        PIXEL2411_0
                        PIXEL2412_0
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2420_0
                            PIXEL2430_0
                            PIXEL2431_0
                        }
                        else
                        {
                            PIXEL2420_50
                            PIXEL2430_50
                            PIXEL2431_50
                        }
                        PIXEL2421_0
                        PIXEL2422_30
                        PIXEL2423_10
                        PIXEL2432_10
                        PIXEL2433_80
                        break;
                    }
                case 191:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                        }
                        else
                        {
                            PIXEL2400_20
                        }
                        PIXEL2401_0
                        PIXEL2402_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2403_0
                        }
                        else
                        {
                            PIXEL2403_20
                        }
                        PIXEL2410_0
                        PIXEL2411_0
                        PIXEL2412_0
                        PIXEL2413_0
                        PIXEL2420_31
                        PIXEL2421_31
                        PIXEL2422_32
                        PIXEL2423_32
                        PIXEL2430_81
                        PIXEL2431_81
                        PIXEL2432_82
                        PIXEL2433_82
                        break;
                    }
                case 223:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                            PIXEL2401_0
                            PIXEL2410_0
                        }
                        else
                        {
                            PIXEL2400_50
                            PIXEL2401_50
                            PIXEL2410_50
                        }
                        PIXEL2402_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2403_0
                        }
                        else
                        {
                            PIXEL2403_20
                        }
                        PIXEL2411_0
                        PIXEL2412_0
                        PIXEL2413_0
                        PIXEL2420_10
                        PIXEL2421_30
                        PIXEL2422_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2423_0
                            PIXEL2432_0
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2423_50
                            PIXEL2432_50
                            PIXEL2433_50
                        }
                        PIXEL2430_80
                        PIXEL2431_10
                        break;
                    }
                case 247:
                    {
                        PIXEL2400_81
                        PIXEL2401_31
                        PIXEL2402_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2403_0
                        }
                        else
                        {
                            PIXEL2403_20
                        }
                        PIXEL2410_81
                        PIXEL2411_31
                        PIXEL2412_0
                        PIXEL2413_0
                        PIXEL2420_82
                        PIXEL2421_32
                        PIXEL2422_0
                        PIXEL2423_0
                        PIXEL2430_82
                        PIXEL2431_32
                        PIXEL2432_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2433_20
                        }
                        break;
                    }
                case 255:
                    {
                        if (Diff24(w[4], w[2]))
                        {
                            PIXEL2400_0
                        }
                        else
                        {
                            PIXEL2400_20
                        }
                        PIXEL2401_0
                        PIXEL2402_0
                        if (Diff24(w[2], w[6]))
                        {
                            PIXEL2403_0
                        }
                        else
                        {
                            PIXEL2403_20
                        }
                        PIXEL2410_0
                        PIXEL2411_0
                        PIXEL2412_0
                        PIXEL2413_0
                        PIXEL2420_0
                        PIXEL2421_0
                        PIXEL2422_0
                        PIXEL2423_0
                        if (Diff24(w[8], w[4]))
                        {
                            PIXEL2430_0
                        }
                        else
                        {
                            PIXEL2430_20
                        }
                        PIXEL2431_0
                        PIXEL2432_0
                        if (Diff24(w[6], w[8]))
                        {
                            PIXEL2433_0
                        }
                        else
                        {
                            PIXEL2433_20
                        }
                        break;
                    }
            }
            sp++;
            dp += 4;
        }

        sRowP += srb;
        sp = (uint24_t *) sRowP;

        dRowP += drb * 4;
        dp = (uint24_t *) dRowP;
    }
}

HQX_API void HQX_CALLCONV hq4x_24( uint24_t * sp, uint24_t * dp, int Xres, int Yres )
{
    uint32_t rowBytesL = Xres * 3;
    hq4x_24_rb(sp, rowBytesL, dp, rowBytesL * 4, Xres, Yres);
}
