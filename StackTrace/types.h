#pragma once
#include <Windows.h>
#include <winnt.h>
#include <intrin.h>
#include <tlhelp32.h> 

#include <cctype>
#include <psapi.h>

#include "context.h"
#include "unwind.h"

#define MAX_STACK_FRAMES 64

typedef struct _STACK_FRAME
{
    DWORD64 address;
} STACK_FRAME;

typedef struct _STACK_TRACE
{
    STACK_FRAME frames[MAX_STACK_FRAMES];
    DWORD frameCount;
} STACK_TRACE;