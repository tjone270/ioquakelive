/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// q_math_sse.c -- SSE implementations of Q_ftol and Q_SnapVector
// Replaces the old asm/ftola.asm and asm/snapvector.asm files.

#include "q_shared.h"
#include <emmintrin.h>

/*
 * Truncate float to long using SSE cvttss2si.
 * This is the truncation (round-toward-zero) variant, matching
 * the original qftolsse assembly.
 */
long Q_ftol(float f) {
    return (long)_mm_cvttss_si32(_mm_set_ss(f));
}

/*
 * Round each component of a vec3_t to the nearest integer.
 * Uses SSE2 cvtps2dq (round-to-nearest) matching the original
 * qsnapvectorsse assembly.
 */
void Q_SnapVector(vec3_t vec) {
    __m128 v = _mm_loadu_ps(vec);             // load 3 floats (4th is garbage)
    __m128i vi = _mm_cvtps_epi32(v);          // round to nearest int
    __m128 rounded = _mm_cvtepi32_ps(vi);     // convert back to float

    // Write back only 3 components
    vec[0] = _mm_cvtss_f32(rounded);
    vec[1] = _mm_cvtss_f32(_mm_shuffle_ps(rounded, rounded, _MM_SHUFFLE(1,1,1,1)));
    vec[2] = _mm_cvtss_f32(_mm_shuffle_ps(rounded, rounded, _MM_SHUFFLE(2,2,2,2)));
}
