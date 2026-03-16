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
// vm_local.h -- native DLL module loader (QVM bytecode support removed)

#include "q_shared.h"
#include "qcommon.h"

// Max number of arguments to pass from engine to a module's vmMain function.
// command number + 12 arguments
#define MAX_VMMAIN_ARGS 13

// Max number of arguments to pass from a module to engine's syscall handler.
// syscall number + 15 arguments
#define MAX_VMSYSCALL_ARGS 16

struct vm_s {
    intptr_t (*systemCall)(intptr_t* parms);

    char name[MAX_QPATH];

    // for dynamic linked modules
    void* dllHandle;
    vmMainProc entryPoint;
    void (*destroy)(vm_t* self);

    int callLevel;      // counts recursive VM_Call
};

extern vm_t* currentVM;
extern int vm_debugLevel;
