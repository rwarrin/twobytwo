#ifndef TBT_COMMON_H

#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef u32 b32;
typedef size_t mem_size;
typedef uintptr_t umm;
typedef intptr_t smm;

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Assert(Condition) do { if(!(Condition)) { *(int *)0 = 0; } } while(0)
#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: { Assert(!"InvalidCodePath"); } break

#define MAX(A, B) ((A) > (B) ? A : B)
#define MIN(A, B) ((A) < (B) ? A : B)

#define Kilobytes(Size) (Size * 1024LL)
#define Megabytes(Size) (Kilobytes(Size) * 1024LL)
#define Gigabytes(Size) (Megabytes(Size) * 1024LL)
#define Terabytes(Size) (Gigabytes(Size) * 1024LL)

#define SetBit(T, V) (T = T | V)
#define UnsetBit(T, V) (T = T & ~V)
#define IsBitSet(T, V) (T & V)
#define IsBitNotSet(T, V) (!IsBitSet(T, V))

#define ZeroStruct(Struct) (ZeroSize_(Struct, sizeof(Struct)))
#define ZeroArray(Array) (ZeroSize_(Array, sizeof(Array)))
#define ZeroSize(Mem, Size) (ZeroSize_(Mem, Size))
inline void
ZeroSize_(void *Mem, mem_size Size)
{
    u8 *Ptr = (u8 *)Mem;
    while(Size--)
    {
        *Ptr++ = 0;
    }
}

#define SWAP_FUNC(type) inline void Swap(type *A, type *B) \
{ \
    type Temp = *A; \
    *A = *B; \
    *B = Temp; \
}
struct task;
SWAP_FUNC(task *)
SWAP_FUNC(u32)


struct memory_arena
{
    void *Base;
    mem_size Size;
    mem_size Used;

    memory_arena *Next;
    memory_arena *Previous;
};

static memory_arena *
AllocateArena(mem_size Size = 0)
{
    mem_size DefaultArenaSize = Kilobytes(32);
    mem_size NewArenaSize = MAX(Size * 2, DefaultArenaSize);

    memory_arena *Result = (memory_arena *)malloc(sizeof(*Result) + NewArenaSize);
    Assert(Result);

    Result->Base = (void *)(Result + 1);
    Result->Size = NewArenaSize;
    Result->Used = 0;
    Result->Next = Result;
    Result->Previous = Result;
    return(Result);
}

#define PushStruct(Arena, Type, ...) (Type *)PushSize_(Arena, sizeof(Type), ##__VA_ARGS__)
#define PushArray(Arena, Count, Type, ...) (Type *)PushSize_(Arena, sizeof(Type)*Count, ##__VA_ARGS__)
#define PushSize(Arena, Size, ...) (PushSize_(Arena, Size, ##__VA_ARGS__))
inline void *
PushSize_(memory_arena *Arena, mem_size Size, u32 Alignment = 4)
{
    // TODO(rick): Alignment
    memory_arena *ArenaToUse = 0;
    for(memory_arena *TestArena = Arena; TestArena; TestArena = TestArena->Next)
    {
        if((TestArena->Used + Size) < TestArena->Size)
        {
            ArenaToUse = TestArena;
            break;
        }

        if(TestArena->Next == Arena)
        {
            break;
        }
    }

    if(!ArenaToUse)
    {
        // TODO(rick): Switch this to insert at back, use previous.
        ArenaToUse = AllocateArena(Size);
        ArenaToUse->Next = Arena->Next;
        ArenaToUse->Previous = Arena->Next->Previous;
        Arena->Next->Previous = ArenaToUse;
        Arena->Next = ArenaToUse;
    }

    Assert(ArenaToUse);
    Assert((ArenaToUse->Used + Size) < (ArenaToUse->Size));

    void *Result = (u8 *)ArenaToUse->Base + ArenaToUse->Used;
    ArenaToUse->Used += Size;
    return(Result);
}

inline void
Copy(void *DestIn, mem_size Size, void *SrcIn)
{
    u8 *Dest = (u8 *)DestIn;
    u8 *Src = (u8 *)SrcIn;
    while(Size--)
    {
        *Dest++ = *Src++;
    }
}

// IMPORTANT(rick): This function assumes DestIn has enough room for SrcIn to be
// copied!!!
static u32
CopyNullTerminatedString(void *DestIn, void *SrcIn)
{
    u8 *Dest = (u8 *)DestIn;
    u8 *Src = (u8 *)SrcIn;

    u32 Result = 0;
    for(;;)
    {
        *Dest = *Src;
        ++Result;

        if(*Src == 0)
        {
            break;
        }

        ++Dest, ++Src;
    }

    return(Result);
}

#define TBT_COMMON_H
#endif
