#include <windows.h>
#include <stdio.h>
#include "sim8086_v2.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef char s8;
typedef short s16;

#define Assert(Condition) if(!(Condition)) { __debugbreak(); }
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

typedef struct
{
    char *Filename;
    size_t Size;
    u8 *Data;
} file;

typedef struct
{
    int UseMod;
    int MaxLength;    
    char *Format;
    int FormatLength;
} op_flavor;

typedef struct
{
    int OpCode;
    int UseExtension;
    int Extension;
    
    op_flavor Flavor[8];
    int FlavorCount;
} op_definition;

static op_definition OpTable[256] = {0};

static file LoadFile(char *Filename)
{
    file Result = {Filename};

    FILE *File = fopen(Filename, "rb");
    Assert(File);

    Result.Data = malloc(1024*1024);
    Result.Size = fread(Result.Data, 1, 1024*1024, File);
    Result.Data[Result.Size] = 0;
    
    fclose(File);

    return(Result);
}

static op_flavor *GetOpFlavor(u8 *At)
{
    int OpCode = At[0];
    int FlavorIndex = 0;

    op_definition *OpDef = &OpTable[OpCode];
    if(OpDef->UseExtension)
    {
        FlavorIndex = (At[1] & 0b00111000) >> 3;
    }

    op_flavor *Result = &OpDef->Flavor[FlavorIndex];
    return(Result);
}

int main(int ArgCount, char **Args)
{
    int Result = 0;
    
    if(ArgCount != 2)
    {
        printf("Usage: %s <assembly>\n", Args[0]);
    }
    else
    {
        file Guide = LoadFile("8086_decoding_guide.txt");
        char *At = strstr((s8 *)Guide.Data, "00 |");
        
        char Temp[1024] = {0};
        while(*At)
        {
            char *LineStart = At;

            int OpCodeHex = strtol(At, 0, 16);
            op_definition *OpDef = &OpTable[OpCodeHex];
            OpDef->OpCode = OpCodeHex;
            At += 5;

            memcpy(Temp, At, 4);
            memcpy(Temp + 4, At + 5, 4);
            Temp[8] = 0;
            
            int OpCodeBinary = strtol(Temp, 0, 2);
            // NOTE(chuck): Integrity validation. These must be sorted.
            Assert(OpCodeHex == OpCodeBinary);

            Assert(OpDef->FlavorCount < ArrayCount(OpDef->Flavor));
            int FlavorIndex = OpDef->FlavorCount++;
            op_flavor *Flavor = &OpDef->Flavor[FlavorIndex];
            Flavor->MaxLength = 1;
            
            // NOTE(chuck): "2ND BYTE" column
            At = strchr(At, '|') + 2;
            if(At[0] != ' ')
            {
                if(!memcmp(At, "MOD ", 4))
                {
                    Flavor->UseMod = 1;

                    if((At[4] == '0') || (At[4] == '1'))
                    {
                        OpDef->UseExtension = 1;

                        memcpy(Temp, At + 4, 3);
                        Temp[3] = 0;

                        OpDef->Extension = strtol(Temp, 0, 2);
                        // NOTE(chuck): Integrity validation. These must be sorted.
                        Assert(FlavorIndex == OpDef->Extension);
                    }
                }

                ++Flavor->MaxLength;
            }
            
            // NOTE(chuck): "BYTES 3, 4, 5, 6" column
            At = strchr(At, '|') + 2;

            char *NextColumn = strchr(At, '|');
            if(At[0] != ' ')
            {
                char *Param = At;
                while(Param && (Param < NextColumn))
                {
                    Param = strchr(Param, ',');
                    if(Param)
                    {
                        ++Param;
                        ++Flavor->MaxLength;
                    }
                }
            }

            // NOTE(chuck): "FORMAT" column
            Flavor->Format = NextColumn + 2;
            At = strchr(At, '\n');
            Flavor->FormatLength = (int)(At - Flavor->Format);

            if(At)
            {
                ++At;
            }
        }
        
        char *AssemblyFilename = Args[1];
        file Assembly = LoadFile(AssemblyFilename);

        u8 *P = Assembly.Data;
        u8 *End = P + Assembly.Size;
        while(P < End)
        {
            op_flavor *Flavor = GetOpFlavor(P);
            
            int Length = Flavor->MaxLength;
            if(Flavor->UseMod)
            {
                int Mod = (P[1] & 0b11000000) >> 6;
                int RM  = (P[1] & 0b00000111);
                if(Mod == 0b11)
                {
                    Length -= 2;
                }
                else if(Mod == 0b01)
                {
                    --Length;
                }
                else if(Mod == 0b00)
                {
                    if(RM != 0b110)
                    {
                        Length -= 2;
                    }
                }
            }

            Temp[0];
            sprintf(Temp, "%.*s", Flavor->FormatLength, Flavor->Format);
            printf("%-40s ", Temp);

            Temp[0] = 0;
            char *T = Temp;
            for(int ByteIndex = 0;
                ByteIndex < Length;
                ++ByteIndex)
            {
                T += sprintf(T, "%02X ", P[ByteIndex]);
            }
            printf("; %s\n", Temp);

            P += Length;
        }
    }

    return(Result);
}
