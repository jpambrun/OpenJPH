/****************************************************************************/
// This software is released under the 2-Clause BSD license, included
// below.
//
// Copyright (c) 2019, Aous Naman 
// Copyright (c) 2019, Kakadu Software Pty Ltd, Australia
// Copyright (c) 2019, The University of New South Wales, Australia
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/****************************************************************************/
// This file is part of the OpenJPH software implementation.
// File: ojph_transform_avx.cpp
// Author: Aous Naman
// Date: 28 August 2019
/****************************************************************************/

#include <cstdio>

#include "ojph_defs.h"
#include "ojph_arch.h"
#include "ojph_transform.h"
#include "ojph_transform_local.h"

#ifdef OJPH_COMPILER_MSVC
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

namespace ojph {
  namespace local {

    //////////////////////////////////////////////////////////////////////////
    void avx_irrev_vert_wvlt_step(const float* src1, const float* src2,
                                  float *dst, int step_num, int repeat)
    {
      __m256 factor = _mm256_set1_ps(LIFTING_FACTORS::steps[step_num]);
      for (int i = (repeat + 7) >> 3; i > 0; --i, dst+=8, src1+=8, src2+=8)
      {
        __m256 s1 = _mm256_load_ps(src1);
        __m256 s2 = _mm256_load_ps(src2);
        __m256 d = _mm256_load_ps(dst);
        d = _mm256_add_ps(d, _mm256_mul_ps(factor, _mm256_add_ps(s1, s2)));
        _mm256_store_ps(dst, d);
      }
    }

    /////////////////////////////////////////////////////////////////////////
    void avx_irrev_vert_wvlt_K(const float* src, float* dst,
                               bool L_analysis_or_H_synthesis, int repeat)
    {
      float f = LIFTING_FACTORS::K_inv;
      f = L_analysis_or_H_synthesis ? f : LIFTING_FACTORS::K;
      __m256 factor = _mm256_set1_ps(f);
      for (int i = (repeat + 7) >> 3; i > 0; --i, dst+=8, src+=8)
      {
        __m256 s = _mm256_load_ps(src);
        _mm256_store_ps(dst, _mm256_mul_ps(factor, s));
      }
    }


    /////////////////////////////////////////////////////////////////////////
    void avx_irrev_horz_wvlt_fwd_tx(float* src, float *ldst, float *hdst,
                                    int width, bool even)
    {
      if (width > 1)
      {
        const int L_width = (width + (even ? 1 : 0)) >> 1;
        const int H_width = (width + (even ? 0 : 1)) >> 1;

        //extension
        src[-1] = src[1];
        src[width] = src[width-2];
        // predict
        const float* sp = src + (even ? 1 : 0);
        float *dph = hdst;
        __m256 factor = _mm256_set1_ps(LIFTING_FACTORS::steps[0]);
        for (int i = (H_width + 3) >> 2; i > 0; --i)
        { //this is doing twice the work it needs to do
          //it can be definitely written better
          __m256 s1 = _mm256_loadu_ps(sp - 1);
          __m256 s2 = _mm256_loadu_ps(sp + 1);
          __m256 d = _mm256_loadu_ps(sp);
          s1 = _mm256_mul_ps(factor, _mm256_add_ps(s1, s2));
          __m256 d1 = _mm256_add_ps(d, s1);
          sp += 8;
          __m128 t1 = _mm256_extractf128_ps(d1, 0);
          __m128 t2 = _mm256_extractf128_ps(d1, 1);
          __m128 t = _mm_shuffle_ps(t1, t2, _MM_SHUFFLE(2, 0, 2, 0));
          _mm_store_ps(dph, t);
          dph += 4;
        }

        // extension
        hdst[-1] = hdst[0];
        hdst[H_width] = hdst[H_width-1];
        // update
        __m128 factor128 = _mm_set1_ps(LIFTING_FACTORS::steps[1]);
        sp = src + (even ? 0 : 1);
        const float* sph = hdst + (even ? 0 : 1);
        float *dpl = ldst;
        for (int i = (L_width + 3) >> 2; i > 0; --i, sp+=8, sph+=4, dpl+=4)
        {
          __m256 d1 = _mm256_loadu_ps(sp); //is there an advantage here?
          __m128 t1 = _mm256_extractf128_ps(d1, 0);
          __m128 t2 = _mm256_extractf128_ps(d1, 1);
          __m128 d = _mm_shuffle_ps(t1, t2, _MM_SHUFFLE(2, 0, 2, 0));

          __m128 s1 = _mm_loadu_ps(sph - 1);
          __m128 s2 = _mm_loadu_ps(sph);
          s1 = _mm_mul_ps(factor128, _mm_add_ps(s1, s2));
          d = _mm_add_ps(d, s1);
          _mm_store_ps(dpl, d);
        }

        //extension
        ldst[-1] = ldst[0];
        ldst[L_width] = ldst[L_width-1];
        //predict
        factor = _mm256_set1_ps(LIFTING_FACTORS::steps[2]);
        const float* spl = ldst + (even ? 1 : 0);
        dph = hdst;
        for (int i = (H_width + 7) >> 3; i > 0; --i, spl+=8, dph+=8)
        {
          __m256 s1 = _mm256_loadu_ps(spl - 1);
          __m256 s2 = _mm256_loadu_ps(spl);
          __m256 d = _mm256_loadu_ps(dph);
          s1 = _mm256_mul_ps(factor, _mm256_add_ps(s1, s2));
          d = _mm256_add_ps(d, s1);
          _mm256_store_ps(dph, d);
        }

        // extension
        hdst[-1] = hdst[0];
        hdst[H_width] = hdst[H_width-1];
        // update
        factor = _mm256_set1_ps(LIFTING_FACTORS::steps[3]);
        sph = hdst + (even ? 0 : 1);
        dpl = ldst;
        for (int i = (L_width + 7) >> 3; i > 0; --i, sph+=8, dpl+=8)
        {
          __m256 s1 = _mm256_loadu_ps(sph - 1);
          __m256 s2 = _mm256_loadu_ps(sph);
          __m256 d = _mm256_loadu_ps(dpl);
          s1 = _mm256_mul_ps(factor, _mm256_add_ps(s1, s2));
          d = _mm256_add_ps(d, s1);
          _mm256_store_ps(dpl, d);
        }

        //multipliers
        float *dp = ldst;
        factor = _mm256_set1_ps(LIFTING_FACTORS::K_inv);
        for (int i = (L_width + 7) >> 3; i > 0; --i, dp+=8)
        {
          __m256 d = _mm256_load_ps(dp);
          _mm256_store_ps(dp, _mm256_mul_ps(factor, d));
        }
        dp = hdst;
        factor = _mm256_set1_ps(LIFTING_FACTORS::K);
        for (int i = (H_width + 7) >> 3; i > 0; --i, dp+=8)
        {
          __m256 d = _mm256_load_ps(dp);
          _mm256_store_ps(dp, _mm256_mul_ps(factor, d));
        }
      }
      else
      {
        if (even)
          ldst[0] = src[0];
        else
          hdst[0] = src[0];
      }
    }

    /////////////////////////////////////////////////////////////////////////
    void avx_irrev_horz_wvlt_bwd_tx(float* dst, float *lsrc, float *hsrc,
                                    int width, bool even)
    {
      if (width > 1)
      {
        const int L_width = (width + (even ? 1 : 0)) >> 1;
        const int H_width = (width + (even ? 0 : 1)) >> 1;

        //multipliers
        float *dp = lsrc;
        __m256 factor = _mm256_set1_ps(LIFTING_FACTORS::K);
        for (int i = (L_width + 7) >> 3; i > 0; --i, dp+=8)
        {
          __m256 d = _mm256_load_ps(dp);
          _mm256_store_ps(dp, _mm256_mul_ps(factor, d));
        }
        dp = hsrc;
        factor = _mm256_set1_ps(LIFTING_FACTORS::K_inv);
        for (int i = (H_width + 7) >> 3; i > 0; --i, dp+=8)
        {
          __m256 d = _mm256_load_ps(dp);
          _mm256_store_ps(dp, _mm256_mul_ps(factor, d));
        }

        //extension
        hsrc[-1] = hsrc[0];
        hsrc[H_width] = hsrc[H_width-1];
        //inverse update
        factor = _mm256_set1_ps(LIFTING_FACTORS::steps[7]);
        const float *sph = hsrc + (even ? 0 : 1);
        float *dpl = lsrc;
        for (int i = (L_width + 7) >> 3; i > 0; --i, sph+=8, dpl+=8)
        {
          __m256 s1 = _mm256_loadu_ps(sph - 1);
          __m256 s2 = _mm256_loadu_ps(sph);
          __m256 d = _mm256_loadu_ps(dpl);
          s1 = _mm256_mul_ps(factor, _mm256_add_ps(s1, s2));
          d = _mm256_add_ps(d, s1);
          _mm256_store_ps(dpl, d);
        }

        //extension
        lsrc[-1] = lsrc[0];
        lsrc[L_width] = lsrc[L_width-1];
        //inverse perdict
        factor = _mm256_set1_ps(LIFTING_FACTORS::steps[6]);
        const float *spl = lsrc + (even ? 0 : -1);
        float *dph = hsrc;
        for (int i = (H_width + 7) >> 3; i > 0; --i, dph+=8, spl+=8)
        {
          __m256 s1 = _mm256_loadu_ps(spl);
          __m256 s2 = _mm256_loadu_ps(spl + 1);
          __m256 d = _mm256_loadu_ps(dph);
          s1 = _mm256_mul_ps(factor, _mm256_add_ps(s1, s2));
          d = _mm256_add_ps(d, s1);
          _mm256_store_ps(dph, d);
        }

        //extension
        hsrc[-1] = hsrc[0];
        hsrc[H_width] = hsrc[H_width-1];
        //inverse update
        factor = _mm256_set1_ps(LIFTING_FACTORS::steps[5]);
        sph = hsrc + (even ? 0 : 1);
        dpl = lsrc;
        for (int i = (L_width + 7) >> 3; i > 0; --i, dpl+=8, sph+=8)
        {
          __m256 s1 = _mm256_loadu_ps(sph - 1);
          __m256 s2 = _mm256_loadu_ps(sph);
          __m256 d = _mm256_loadu_ps(dpl);
          s1 = _mm256_mul_ps(factor, _mm256_add_ps(s1, s2));
          d = _mm256_add_ps(d, s1);
          _mm256_store_ps(dpl, d);
        }

        //extension
        lsrc[-1] = lsrc[0];
        lsrc[L_width] = lsrc[L_width-1];
        //inverse perdict and combine
        factor = _mm256_set1_ps(LIFTING_FACTORS::steps[4]);
        dp = dst + (even ? 0 : -1);
        spl = lsrc + (even ? 0 : -1);
        sph = hsrc;
        int width = L_width + (even ? 0 : 1);
        for (int i = (width + 7) >> 3; i > 0; --i, spl+=8, sph+=8)
        {
          __m256 s1 = _mm256_loadu_ps(spl);
          __m256 s2 = _mm256_loadu_ps(spl + 1);
          __m256 d = _mm256_load_ps(sph);
          s2 = _mm256_mul_ps(factor, _mm256_add_ps(s1, s2));
          d = _mm256_add_ps(d, s2);

          __m128 a0 = _mm256_extractf128_ps(s1, 0);
          __m128 a1 = _mm256_extractf128_ps(s1, 1);
          __m128 a2 = _mm256_extractf128_ps(d, 0);
          __m128 a3 = _mm256_extractf128_ps(d, 1);
          _mm_storeu_ps(dp, _mm_unpacklo_ps(a0, a2)); dp += 4;
          _mm_storeu_ps(dp, _mm_unpackhi_ps(a0, a2)); dp += 4;
          _mm_storeu_ps(dp, _mm_unpacklo_ps(a1, a3)); dp += 4;
          _mm_storeu_ps(dp, _mm_unpackhi_ps(a1, a3)); dp += 4;

//          s2 = _mm256_unpackhi_ps(s1, d);
//          s1 = _mm256_unpacklo_ps(s1, d);
//          d = _mm256_permute2f128_ps(s1, s2, (2 << 4) | 0);
//          _mm256_storeu_ps(dp, d);
//          d = _mm256_permute2f128_ps(s1, s2, (3 << 4) | 1);
//          _mm256_storeu_ps(dp + 1, d);
        }
      }
      else
      {
        if (even)
          dst[0] = lsrc[0];
        else
          dst[0] = hsrc[0];
      }
    }
  }
}
