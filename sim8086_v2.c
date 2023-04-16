#include <windows.h>
#include <stdio.h>

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

typedef enum
{
    Param_Unknown,
    Param_Immed8,
    Param_Immed16,
    Param_Reg8,
    Param_Reg16,
    Param_Mem8,
    Param_Mem16,
    Param_RegOrMem8,
    Param_RegOrMem16,
    Param_SegReg,
    Param_Literal,
    Param_ShortLabel,
} param_type;

typedef struct
{
    param_type Type;
    char *Literal;
    size_t LiteralLength;
} op_param;

typedef struct
{
    int UseMod;
    int MaxBytes;

    char *Name;
    int NameLength;

    char *Format;
    int FormatLength;

    op_param Param[2];
    int ParamCount;
} op_variant;

typedef struct
{
    int OpCode;
    int UseExtension;
    int Extension;
    
    op_variant OpVariant[8];
    int OpVariantCount;
} op_definition;

static char *RegisterLookup[16] =
{
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
};

static char *EffectiveAddressLookup[8] =
{
    "bx+si",
    "bx+di",
    "bp+si",
    "bp+di",
    "si",
    "di",
    "bp",
    "bx"
};

static char *SegmentRegisterLookup[4] =
{
    "es",
    "cs",
    "ss",
    "ds",
};

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

typedef struct
{
    char *Description;
    param_type Type;
} param_description_type;

static param_description_type ParamDescriptionTypeLookup[] =
{
    {"IMMED8", Param_Immed8},
    {"IMMED16", Param_Immed16},
    {"REG8", Param_Reg8},
    {"REG16", Param_Reg16},
    {"MEM8", Param_Mem8},
    {"MEM16", Param_Mem16},
    {"REG8/MEM8", Param_RegOrMem8},
    {"REG16/MEM16", Param_RegOrMem16},
    {"SEGREG", Param_SegReg},
    {"SHORT-LABEL", Param_ShortLabel},
};

static void ParseParamType(op_variant *OpVariant, int ParamIndex, char *ParamDescription, char *AtEnd)
{
    op_param *Param = &OpVariant->Param[ParamIndex];

    // NOTE(chuck): Parse one and only one of the params.
    char *P = ParamDescription;
    while((P < AtEnd) && (*P != '\n') && (*P != ' ') && (*P != ';') && (*P != ','))
    {
        ++P;
    }
    size_t ParamDescriptionLength = (size_t)(P - ParamDescription);

    int Found = 0;
    for(int Index = 0;
        Index < ArrayCount(ParamDescriptionTypeLookup);
        ++Index)
    {
        param_description_type *ParamDescriptionType = &ParamDescriptionTypeLookup[Index];
        size_t ParamDescriptionTypeLength = strlen(ParamDescriptionType->Description);
        if((ParamDescriptionTypeLength == ParamDescriptionLength) &&
           !memcmp(ParamDescription, ParamDescriptionType->Description, ParamDescriptionLength))
        {
            Param->Type = ParamDescriptionType->Type;
            Found = 1;
            break;
        }
    }
    
    if(!Found)
    {
       Param->Type = Param_Literal;
       Param->Literal = ParamDescription;
       Param->LiteralLength = ParamDescriptionLength;
    }
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
        file Guide = LoadFile("C:/dev/computer_enhance/8086_decoding_guide.txt");
        char *At = strstr((s8 *)Guide.Data, "00 |");
        char *AtEnd = At + Guide.Size;
        
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

            Assert(OpDef->OpVariantCount < ArrayCount(OpDef->OpVariant));
            int OpVariantIndex = OpDef->OpVariantCount++;
            op_variant *OpVariant = &OpDef->OpVariant[OpVariantIndex];
            OpVariant->MaxBytes = 1;
            
            // NOTE(chuck): "2ND BYTE" column
            At = strchr(At, '|') + 2;
            if(At[0] != ' ')
            {
                if(!memcmp(At, "MOD ", 4))
                {
                    OpVariant->UseMod = 1;

                    if((At[4] == '0') || (At[4] == '1'))
                    {
                        OpDef->UseExtension = 1;

                        memcpy(Temp, At + 4, 3);
                        Temp[3] = 0;

                        OpDef->Extension = strtol(Temp, 0, 2);
                        // NOTE(chuck): Integrity validation. These must be sorted.
                        Assert(OpVariantIndex == OpDef->Extension);
                    }
                }

                ++OpVariant->MaxBytes;
            }
            
            // NOTE(chuck): "BYTES 3, 4, 5, 6" column
            At = strchr(At, '|') + 2;

            char *NextColumn = strchr(At, '|');
            if(At[0] != ' ')
            {
                char *Param = At;
                do
                {
                    ++OpVariant->MaxBytes;
                    Param = strchr(Param, ',');
                    if(Param)
                    {
                        ++Param;
                    }
                } while(Param && (Param < NextColumn));
            }

            char *EndOfLine = strchr(At, '\n');
            if(!EndOfLine)
            {
                EndOfLine = AtEnd;
            }

            // NOTE(chuck): "FORMAT" column
            OpVariant->Name = NextColumn + 2;
            char *NameEnd = strchr(OpVariant->Name, ' ');
            if(NameEnd > EndOfLine)
            {
                NameEnd = EndOfLine;
            }
            OpVariant->NameLength = (int)(NameEnd - OpVariant->Name);
            char *Slash = strchr(OpVariant->Name, '/');
            int SlashIndex = (int)(Slash - OpVariant->Name);
            if(OpVariant->NameLength > SlashIndex)
            {
                OpVariant->NameLength = SlashIndex;
            }
            if(NameEnd == EndOfLine)
            {
                OpVariant->Format = NameEnd;
                OpVariant->FormatLength = 0;
            }
            else
            {
                OpVariant->Format = NameEnd + 1;

                char *SecondParam = strchr(OpVariant->Format, ',');
                ParseParamType(OpVariant, 0, OpVariant->Format, AtEnd);

                OpVariant->ParamCount = 1;
                if(SecondParam && (SecondParam < EndOfLine))
                {
                    ++OpVariant->ParamCount;
                    ++SecondParam;
                    ParseParamType(OpVariant, 1, SecondParam, AtEnd);
                }
                OpVariant->FormatLength = (int)(EndOfLine - OpVariant->Format);
            }
            
            // int L = (int)(At - LineStart);
            // printf("%02X %d :: %.*s\n", OpDef->OpCode, OpVariant->MaxBytes, L, LineStart);//OpVariant->FormatLength, OpVariant->Format);

            At = EndOfLine;
            if(At < AtEnd)
            {
                Assert(*At == '\n');
                ++At;
            }
        }

        // printf("---\n");

        char *AssemblyFilename = Args[1];
        file Assembly = LoadFile(AssemblyFilename);

        u8 *P = Assembly.Data;
        u8 *End = P + Assembly.Size;
        while(P < End)
        {
            int OpCode = P[0];
            int OpVariantIndex = 0;
            int OpByteCount = 1;
            
            char *T = Temp;

            op_definition *OpDef = &OpTable[OpCode];
            if(OpDef->UseExtension)
            {
                OpVariantIndex = (P[1] & 0b00111000) >> 3;
            }
            op_variant *OpVariant = &OpDef->OpVariant[OpVariantIndex];
            T += sprintf(T, "%.*s ", OpVariant->NameLength, OpVariant->Name);
            if(OpVariant->UseMod)
            {
                ++OpByteCount;
            }
            
            int SD  = (P[0] & 0b00000010) >> 1; // NOTE(chuck): S or D
            int W   = (P[0] & 0b00000001);
            int Mod = (P[1] & 0b11000000) >> 6;
            int Reg = (P[1] & 0b00111000) >> 3;
            int RM  = (P[1] & 0b00000111);
            
            int SourceAndDest[2] = {RM, Reg};

            int Special = 0;
            if(OpVariant->ParamCount == 2)
            {
                if((OpVariant->Param[1].Type == Param_Immed8) ||
                   (OpVariant->Param[1].Type == Param_Immed16))
                {
                    // NOTE(chuck): Do nothing, e.g. SUB REG16/MEM16,IMMED8/IMMED16
                    Special = 1;
                }
                else if(((OpVariant->Param[0].Type == Param_RegOrMem8) ||
                         (OpVariant->Param[0].Type == Param_RegOrMem16)) &&
                         (OpVariant->Param[1].Type == Param_Literal))
                {
                    // NOTE(chuck): e.g. 0xd3 SHR REG16/MEM16,CL
                    Special = 1;
                }
                else if((OpVariant->Param[0].Type == Param_Reg16) &&
                        (OpVariant->Param[1].Type == Param_Mem16))
                {
                    // NOTE(chuck): LEA, LES, and LDS have no D bit.
                    SourceAndDest[0] = Reg;
                    SourceAndDest[1] = RM;
                    Special = 1;
                }
            }
            if(!Special)
            {
                if(OpVariant->UseMod && (OpVariant->ParamCount > 1) && SD)
                {
                    SourceAndDest[0] = Reg;
                    SourceAndDest[1] = RM;
                }
            }

            int Length = OpVariant->MaxBytes;
            // NOTE(chuck): AAM/AAD are the only two composed of two literal bytes.
            if((OpCode == 0xd4) || (OpCode == 0xd5))
            {
                OpByteCount = 2;
            }
            // NOTE(chuck): CALL NEAR-PROC and JMP NEAR-LABEL and RETF/RET (intersegment)
            else if((OpCode == 0xe8) || (OpCode == 0xe9) || (OpCode == 0xca) || (OpCode == 0xc2))
            {
                OpByteCount = 3;
            }
            // NOTE(chuck): CALL FAR_PROC and JMP FAR-LABEL
            else if((OpCode == 0x9a) || (OpCode == 0xea))
            {
                OpByteCount = 5;
            }
            else if((OpCode >= 0xe0) && (OpCode <= 0xe3) || // LOOP*/JCXZ
                    (OpCode >= 0x70) && (OpCode <= 0x7f)) // J* jumps
            {
                op_param *Param = &OpVariant->Param[0];
                Assert(Param->Type == Param_ShortLabel);

                T += sprintf(T, "$%+d", *(s8 *)&P[OpByteCount + 1]);

                OpByteCount = 2;
            }
            else
            {
                // NOTE(chuck): Only display "byte" or "word" prefix when the text form would be ambiguous.
                int ShowSize = 0;
                if(OpVariant->ParamCount == 2)
                {
                    if((OpVariant->Param[0].Type != Param_Reg8) &&
                       (OpVariant->Param[0].Type != Param_Reg16) &&
                       (OpVariant->Param[0].Type != Param_Literal))
                    {
                        if((OpVariant->Param[1].Type == Param_Immed8) ||
                           (OpVariant->Param[1].Type == Param_Immed16) ||
                           (OpVariant->Param[1].Type == Param_Literal))
                        {
                            if((OpVariant->Param[0].Type == Param_Reg8) ||
                               (OpVariant->Param[0].Type == Param_Mem8))
                            {
                                ShowSize = 1;
                            }
                            else if((OpVariant->Param[0].Type == Param_Reg16) ||
                                    (OpVariant->Param[0].Type == Param_Mem16))
                            {
                                ShowSize = 2;
                            }
                            else if(OpVariant->UseMod && (Mod < 0b11))
                            {
                                if(OpVariant->Param[0].Type == Param_RegOrMem8)
                                {
                                    ShowSize = 1;
                                }
                                else if(OpVariant->Param[0].Type == Param_RegOrMem16)
                                {
                                    ShowSize = 2;
                                }
                            }
                        }
                    }
                }
                else if(OpVariant->ParamCount == 1)
                {
                    if(OpVariant->Param[0].Type == Param_Mem8)
                    {
                        ShowSize = 1;
                    }
                    else if(OpVariant->Param[0].Type == Param_Mem16)
                    {
                        ShowSize = 2;
                    }
                    else if(OpVariant->UseMod && (Mod < 0b11))
                    {
                        if(OpVariant->Param[0].Type == Param_RegOrMem8)
                        {
                            ShowSize = 1;
                        }
                        else if(OpVariant->Param[0].Type == Param_RegOrMem16)
                        {
                            ShowSize = 2;
                        }
                    }

                }
                if(ShowSize == 1)
                {
                    T += sprintf(T, "byte ");
                }
                else if(ShowSize == 2)
                {
                    if(OpCode != 0xff) // NOTE(chuck): CALL doesn't need clarification
                    {
                        T += sprintf(T, "word ");
                    }
                }

                for(int ParamIndex = 0;
                    ParamIndex < OpVariant->ParamCount;
                    ++ParamIndex)
                {
                    int SourceOrDest = SourceAndDest[ParamIndex];

                    op_param *Param = &OpVariant->Param[ParamIndex];
                    if(Param->Type == Param_Immed8)
                    {
                        T += sprintf(T, "%d", *(u8 *)&P[OpByteCount]);
                        ++OpByteCount;
                    }
                    else if(Param->Type == Param_Immed16)
                    {
                        T += sprintf(T, "%d", *(u16 *)&P[OpByteCount]);
                        OpByteCount += 2;
                    }
                    else if(Param->Type == Param_Reg8)
                    {
                        T += sprintf(T, "%s", RegisterLookup[SourceOrDest]);
                    }
                    else if(Param->Type == Param_Reg16)
                    {
                        T += sprintf(T, "%s", RegisterLookup[8 + SourceOrDest]);
                    }
                    else if((Param->Type == Param_Mem8) || (Param->Type == Param_Mem16))
                    {
                        if(OpVariant->UseMod)
                        {
                            if(Mod == 0b00)
                            {
                                if(RM == 0b110)
                                {
                                    T += sprintf(T, "[%d]", *(u16 *)&P[OpByteCount]);
                                    OpByteCount += 2;
                                }
                                else
                                {
                                    T += sprintf(T, "[%s]", EffectiveAddressLookup[RM]);
                                }
                            }
                            else if(Mod == 0b01)
                            {
                                T += sprintf(T, "[%s%+d]", EffectiveAddressLookup[RM], *(s8 *)&P[OpByteCount]);
                                ++OpByteCount;
                            }
                            else if(Mod == 0b10)
                            {
                                if(Param->Type == Param_Mem8)
                                {
                                    T += sprintf(T, "[%s%+d]", EffectiveAddressLookup[RM], *(s8 *)&P[OpByteCount]);
                                    ++OpByteCount;
                                }
                                else
                                {
                                    T += sprintf(T, "[%s%+d]", EffectiveAddressLookup[RM], *(s16 *)&P[OpByteCount]);
                                    OpByteCount += 2;
                                }
                            }
                            else
                            {
                                T += sprintf(T, "[%s]", EffectiveAddressLookup[RM]);
                            }
                        }
                        else
                        {
                            if(Param->Type == Param_Mem8)
                            {
                                T += sprintf(T, "[%d]", *(u8 *)&P[OpByteCount]);
                                ++OpByteCount;
                            }
                            else if(Param->Type == Param_Mem16)
                            {
                                T += sprintf(T, "[%d]", *(u16 *)&P[OpByteCount]);
                                OpByteCount += 2;
                            }
                        }
                    }
                    else if(Param->Type == Param_RegOrMem8)
                    {
                        if(Mod == 0b00)
                        {
                            if(RM != 0b110)
                            {
                                T += sprintf(T, "[%s]", EffectiveAddressLookup[RM]);
                            }
                            else
                            {
                                T += sprintf(T, "[%d]", *(u16 *)&P[OpByteCount]);
                                OpByteCount += 2;
                            }
                        }
                        else if(Mod == 0b01)
                        {
                            T += sprintf(T, "[%s%+d]", EffectiveAddressLookup[RM], *(s8 *)&P[OpByteCount]);
                            ++OpByteCount;
                        }
                        else if(Mod == 0b10)
                        {
                            T += sprintf(T, "[%s%+d]", EffectiveAddressLookup[RM], *(s16 *)&P[OpByteCount]);
                            OpByteCount += 2;
                        }
                        else
                        {
                            T += sprintf(T, "%s", RegisterLookup[SourceOrDest]);
                        }
                    }
                    else if(Param->Type == Param_RegOrMem16)
                    {
                        if(Mod == 0b00)
                        {
                            if(RM != 0b110)
                            {
                                T += sprintf(T, "[%s]", EffectiveAddressLookup[RM]);
                            }
                            else
                            {
                                T += sprintf(T, "[%d]", *(u16 *)&P[OpByteCount]);
                                OpByteCount += 2;
                            }
                        }
                        else if(Mod == 0b01)
                        {
                            T += sprintf(T, "[%s%+d]", EffectiveAddressLookup[RM], *(s8 *)&P[OpByteCount]);
                            OpByteCount += 1;
                        }
                        else if(Mod == 0b10)
                        {
                            T += sprintf(T, "[%s%+d]", EffectiveAddressLookup[RM], *(s16 *)&P[OpByteCount]);
                            OpByteCount += 2;
                        }
                        else
                        {
                            T += sprintf(T, "%s", RegisterLookup[8 + SourceOrDest]);
                        }
                    }
                    else if(Param->Type == Param_SegReg)
                    {
                        T += sprintf(T, "%s", SegmentRegisterLookup[Reg]);
                    }
                    else if(Param->Type == Param_Literal)
                    {
                        T += sprintf(T, "%.*s", (int)Param->LiteralLength, Param->Literal);
                    }                
                    
                    if(ParamIndex < (OpVariant->ParamCount - 1))
                    {
                        T += sprintf(T, ", ");
                    }
                }
            }
            Length = OpByteCount;

            *T = 0;
            printf("%-20s", Temp);

            // sprintf(Temp, "%.*s", OpVariant->FormatLength, OpVariant->Format);
            printf(" ; ");
            // printf("%-40s; ", Temp);

            T = Temp;
            for(int ByteIndex = 0;
                ByteIndex < Length;
                ++ByteIndex)
            {
                T += sprintf(T, "%02X ", P[ByteIndex]);
            }

            printf("%-18s", Temp);

            for(int ByteIndex = 0;
                ByteIndex < Length;
                ++ByteIndex)
            {
                printf("(");

                for(int BitIndex = 0;
                    BitIndex < 8;
                    ++BitIndex)
                {
                    if(P[ByteIndex] & (1 << (7 - BitIndex)))
                    {
                        printf("1");
                    }
                    else
                    {
                        printf("0");
                    }

                    if((ByteIndex == 1) && ((BitIndex == 1) || (BitIndex == 4)))
                    {
                        printf(" ");
                    }
                }

                printf(") ");
            }

            printf("\n");

            P += Length;
        }
    }

    return(Result);
}
