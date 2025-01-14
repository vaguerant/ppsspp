#pragma once
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void OSConsoleWrite(const char *msg, int size);
void OSReport(const char *fmt, ...);
void OSPanic(const char *file, int line, const char *fmt, ...);
void OSFatal(const char *msg);
int __os_snprintf(char *buf, int n, const char *format, ... );
typedef void* (*GetSymbolNameFunc)(u32 addr, char* out, int out_size);
typedef void (*PrintfFunc)(const char * fmt, ...);

#define DISASM_FLAG_SIMPLE 0x001
#define DISASM_FLAG_SPACES 0x020
#define DISASM_FLAG_SHORT  0x040
#define DISASM_FLAG_ADDR   0x080
#define DISASM_FLAG_FUNCS  0x100
void* OSGetSymbolName(u32 addr, char* out, int out_size);
void* OSGetSymbolNameEx(u32 addr, char* out, int out_size);
void DisassemblePPCRange(const void *start, const void *end, PrintfFunc printf_cb, GetSymbolNameFunc getSymbolName_cb, int flags);
BOOL DisassemblePPCOpcode(const void *opcode, char *out, int out_size, GetSymbolNameFunc getSymbolName_cb, int flags);
void OSSetDABR(BOOL allCores, void* addr, BOOL reads, BOOL writes);

#ifdef __cplusplus
}
#endif

#define DEBUG_DISASM(start, count) DisassemblePPCRange((void*)start, (u32*)(start) + (count? count - 1 : 0), (void*)printf, (void*)OSGetSymbolName, 0)
/* #define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0) */
#define DEBUG_LINE() do{printf("%s:%4d %s().\n", __FILE__, __LINE__, __FUNCTION__);fflush(stdout);}while(0)
#define DEBUG_BREAK() do{DEBUG_LINE();__asm__ volatile (".int 0x0FE00016");}while(0)
#define DEBUG_BREAK_ONCE() do{static bool debug_break_done; if(!debug_break_done){debug_break_done=true; DEBUG_LINE(); __asm__ volatile (".int 0x0FE00016");}}while(0)
#define DEBUG_CRASH() do{DEBUG_LINE(); *(u32*)0 = 0;}while(0)
#define DEBUG_STR(X) do{printf( "%s: %s\n", #X, (char*)(X));fflush(stdout);}while(0)
#define DEBUG_VAR(X) do{printf( "%-20s: 0x%08" PRIX32 "\n", #X, (uint32_t)(X));fflush(stdout);}while(0)
#define DEBUG_VAR2(X) do{printf( "%-20s: 0x%08" PRIX32 " (%i)\n", #X, (uint32_t)(X), (int)(X));fflush(stdout);}while(0)
#define DEBUG_INT(X) do{printf( "%-20s: %10" PRIi32 "\n", #X, (int32_t)(X));fflush(stdout);}while(0)
#define DEBUG_FLOAT(X) do{printf( "%-20s: %10.3f\n", #X, (float)(X));fflush(stdout);}while(0)
#define DEBUG_VAR64(X) do{printf( "%-20s: 0x%016" PRIX64 "\n", #X, (uint64_t)(X));fflush(stdout);}while(0)
#define DEBUG_PTR(X) do{printf( "%-20s: 0x%016" PRIXPTR "\n", #X, (uintptr_t)(X));fflush(stdout);}while(0)
#define DEBUG_MAGIC(X) do{printf( "%-20s: '%c''%c''%c''%c' (0x%08X)\n", #X, (u32)(X)>>24, (u32)(X)>>16, (u32)(X)>>8, (u32)(X),(u32)(X));fflush(stdout);}while(0)

/* #define DEBUG_ERROR(X) do{if(X)dump_result_value(X);}while(0) */
#define PRINTFPOS(X,Y) "\x1b["#X";"#Y"H"
#define PRINTFPOS_STR(X,Y) "\x1b[" X ";" Y "H"
#define PRINTF_LINE(X) "\x1b[" X ";0H"
