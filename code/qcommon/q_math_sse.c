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

// q_math_sse.c -- Q_ftol and Q_SnapVector implementations
// SSE on x86/x86_64, generic C fallbacks on other architectures (ARM, etc.)

#include "q_shared.h"

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)

#include <emmintrin.h>

long Q_ftol(float f) {
    return (long)_mm_cvttss_si32(_mm_set_ss(f));
}

void Q_SnapVector(vec3_t vec) {
    __m128 v = _mm_loadu_ps(vec);
    __m128i vi = _mm_cvtps_epi32(v);
    __m128 rounded = _mm_cvtepi32_ps(vi);
    vec[0] = _mm_cvtss_f32(rounded);
    vec[1] = _mm_cvtss_f32(_mm_shuffle_ps(rounded, rounded, _MM_SHUFFLE(1,1,1,1)));
    vec[2] = _mm_cvtss_f32(_mm_shuffle_ps(rounded, rounded, _MM_SHUFFLE(2,2,2,2)));
}

#else

#include <math.h>

long Q_ftol(float f) {
    return (long)f;
}

void Q_SnapVector(vec3_t vec) {
    vec[0] = rintf(vec[0]);
    vec[1] = rintf(vec[1]);
    vec[2] = rintf(vec[2]);
}

#endif
