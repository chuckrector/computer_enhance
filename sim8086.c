// NOTE(chuck): Homework for listing 41. Supports jump labels too. I emit byte/word prefixes too aggressively in some cases I think? But NASM doesn't care so I don't care. :^)

#include <windows.h>
#include <stdio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef char s8;
typedef short s16;

#define DEBUG

#define Assert(Condition) if(!(Condition)) { __debugbreak(); }
#define ArrayLength(A) (sizeof(A) / sizeof((A)[0]))

#define TOP_6BITS 0xfc
#define TOP_7BITS 0xfe

#define ADD_REGISTER_OR_MEMORY_TO_REGISTER  0x00
#define ADD_IMMEDIATE_TO_REGISTER_OR_MEMORY 0x80
#define ADD_IMMEDIATE_TO_ACCUMULATOR        0x04

#define MOV_REGISTER_OR_MEMORY_TO_REGISTER  0x88 // 0b100010 00
#define MOV_IMMEDIATE_TO_REGISTER 0xb0 // 0b1011 0 000
#define MOV_IMMEDIATE_TO_REGISTER_OR_MEMORY 0xc6 // 0b1100011 0
#define MOV_MEMORY_TO_ACCUMULATOR 0xa0 // 0b1010000 0
#define MOV_ACCUMULATOR_TO_MEMORY 0xa2 // 0b1010001 0

#define MEMORY_MODE_MAYBE_NO_DISPLACEMENT 0x00
#define MEMORY_MODE_8BIT_DISPLACEMENT     0x01
#define MEMORY_MODE_16BIT_DISPLACEMENT    0x02
#define REGISTER_MODE_NO_DISPLACEMENT     0x03

#define REGISTER_NAME_AL 0
#define REGISTER_NAME_CL 1
#define REGISTER_NAME_DL 2
#define REGISTER_NAME_BL 3
#define REGISTER_NAME_AH 4
#define REGISTER_NAME_CH 5
#define REGISTER_NAME_DH 6
#define REGISTER_NAME_BH 7
#define REGISTER_NAME_AX 8
#define REGISTER_NAME_CX 9
#define REGISTER_NAME_DX 10
#define REGISTER_NAME_BX 11
#define REGISTER_NAME_SP 12
#define REGISTER_NAME_BP 13
#define REGISTER_NAME_SI 14
#define REGISTER_NAME_DI 15
static char *RegisterLookup[16] =
{
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
};

#define SEGMENT_REGISTER_NAME_ES 0
#define SEGMENT_REGISTER_NAME_CS 0
#define SEGMENT_REGISTER_NAME_SS 0
#define SEGMENT_REGISTER_NAME_DS 0
static char *SegmentRegisterLookup[4] =
{
    "es",
    "cs",
    "ss",
    "ds",
};

typedef enum
{
    Param_Unknown,
    Param_Immediate,
    Param_Register,
    Param_Memory,
    Param_MemoryDirectAddress,
    Param_SegmentRegister,
    Param_DirectIntersegment,
} param_type;

typedef struct
{
    param_type Type;
    int RegisterOrMemoryIndex;
    int ImmediateValue;
    int Offset; // NOTE(chuck): Displacement value or direct address.
    int ByteSizeQualifier; // NOTE(chuck): Prefixes: 0=, 1=byte, 2=word
    int IP; // NOTE(chuck): Direct intersegment
    int CS; // NOTE(chuck): Direct intersegment
} op_param;

#define DESTINATION 0
#define SOURCE 1
typedef struct
{
    int NameIndex;
    u8 *IP;
    int ParamCount;
    op_param Param[2];
    int ByteLength;
    int Error; // NOTE(chuck): Something bogus was encountered. Mark it and keep on trucking so that I can see more of the instruction stream on the off-chance that it helps provide more insight.

    union
    {
        int Dest;
        int Sign;
        int VariableShift;
    };
    union
    {
        int Word;
        int RepeatWhileZero;
    };
    int Mode;
    union
    {
        int RegA;
        int Ext;
    };
    int RegB;
    int EmitSize;
    int IsRelativeJump;
    int IsJumpTarget;
    int JumpTargetIndex; // NOTE(chuck): This is patched in on the second pass for label printing.

    int UseSegmentOverride;
    int SegmentOverride;
} op;

static u8 OpStream[1024*4] = {0};
static op OpList[1024*4] = {0};
static int OpCount = 0;

#define OP_NAME_UNKNOWN 0
#define OP_NAME_MOV 1
#define OP_NAME_ADD 2
#define OP_NAME_SUB 3
#define OP_NAME_CMP 4

#define OP_NAME_JE 5
#define OP_NAME_JL 6
#define OP_NAME_JLE 7
#define OP_NAME_JB 8
#define OP_NAME_JBE 9
#define OP_NAME_JP 10
#define OP_NAME_JO 11
#define OP_NAME_JS 12
#define OP_NAME_JNZ 13
#define OP_NAME_JNL 14
#define OP_NAME_JG 15
#define OP_NAME_JNB 16
#define OP_NAME_JA 17
#define OP_NAME_JNP 18
#define OP_NAME_JNO 19
#define OP_NAME_JNS 20
#define OP_NAME_LOOP 21
#define OP_NAME_LOOPZ 22
#define OP_NAME_LOOPNZ 23
#define OP_NAME_JCXZ 24

#define OP_NAME_PUSH 25
#define OP_NAME_POP 26
#define OP_NAME_XCHG 27
#define OP_NAME_IN 28
#define OP_NAME_OUT 29
#define OP_NAME_XLAT 30
#define OP_NAME_LEA 31
#define OP_NAME_LDS 32
#define OP_NAME_LES 33
#define OP_NAME_LAHF 34
#define OP_NAME_SAHF 35
#define OP_NAME_PUSHF 36
#define OP_NAME_POPF 37
#define OP_NAME_ADC 38
#define OP_NAME_INC 39
#define OP_NAME_AAA 40
#define OP_NAME_DAA 41
#define OP_NAME_SBB 42
#define OP_NAME_AAS 43
#define OP_NAME_DAS 44
#define OP_NAME_AAM 45
#define OP_NAME_AAD 46
#define OP_NAME_CBW 47
#define OP_NAME_CWD 48
#define OP_NAME_INTO 49
#define OP_NAME_CLC 50
#define OP_NAME_CMC 51
#define OP_NAME_STC 52
#define OP_NAME_CLD 53
#define OP_NAME_STD 54
#define OP_NAME_CLI 55
#define OP_NAME_STI 56
#define OP_NAME_HLT 57
#define OP_NAME_WAIT 58
#define OP_NAME_IRET 59
#define OP_NAME_DEC 60
#define OP_NAME_NEG 61
#define OP_NAME_MUL 62
#define OP_NAME_IMUL 63
#define OP_NAME_DIV 64
#define OP_NAME_IDIV 65
#define OP_NAME_NOT 66
#define OP_NAME_SHL 67
#define OP_NAME_SHR 68
#define OP_NAME_SAR 69
#define OP_NAME_ROL 70
#define OP_NAME_ROR 71
#define OP_NAME_RCL 72
#define OP_NAME_RCR 73
#define OP_NAME_AND 74
#define OP_NAME_TEST 75
#define OP_NAME_OR 76
#define OP_NAME_XOR 77
#define OP_NAME_REP 78
#define OP_NAME_MOVS 79
#define OP_NAME_CMPS 80
#define OP_NAME_SCAS 81
#define OP_NAME_LODS 82
#define OP_NAME_STOS 83
#define OP_NAME_CALL 84
#define OP_NAME_JMP 85
#define OP_NAME_RET 86
#define OP_NAME_INT 87
#define OP_NAME_INT3 88
#define OP_NAME_LOCK 89
#define SEGMENT_OVERRIDE 90

static char *OpNameLookup[] =
{
    "<unknown>",
    "mov",
    "add",
    "sub",
    "cmp",
    "je",
    "jl",
    "jle",
    "jb",
    "jbe",
    "jp",
    "jo",
    "js",
    "jnz",
    "jnl",
    "jg",
    "jnb",
    "ja",
    "jnp",
    "jno",
    "jns",
    "loop",
    "loopz",
    "loopnz",
    "jcxz",
    "push",
    "pop",
    "xchg",
    "in",
    "out",
    "xlat",
    "lea",
    "lds",
    "les",
    "lahf",
    "sahf",
    "pushf",
    "popf",
    "adc",
    "inc",
    "aaa",
    "daa",
    "sbb",
    "aas",
    "das",
    "aam",
    "aad",
    "cbw",
    "cwd",
    "into",
    "clc",
    "cmc",
    "stc",
    "cld",
    "std",
    "cli",
    "sti",
    "hlt",
    "wait",
    "iret",
    "dec",
    "neg",
    "mul",
    "imul",
    "div",
    "idiv",
    "not",
    "shl",
    "shr",
    "sar",
    "rol",
    "ror",
    "rcl",
    "rcr",
    "and",
    "test",
    "or",
    "xor",
    "rep",
    "movs",
    "cmps",
    "scas",
    "lods",
    "stos",
    "call",
    "jmp",
    "ret",
    "int",
    "int3",
    "lock",
    "",
};

static int IsPrefix(op *Op)
{
    int Result = (Op->NameIndex == OP_NAME_REP) || (Op->NameIndex == OP_NAME_LOCK) || (Op->NameIndex == SEGMENT_OVERRIDE);
    return(Result);
}

// NOTE(chuck): Annoyingly, you cannot pass a struct literal as a function argument without casting it. So use a relativelyt short name here and stuff all possible options for all functions into this.
typedef struct
{
    int SignExtend;
    int NameIndex;
    int SwapParams; // NOTE(chuck): in/out reuse code but "out" uses the reverse order.
    int ParamCount;
    int ByteLength;
    int NoSignExtension; // TODO(chuck): Tacking this on without much thought. Confusing to have this and SignExtend. Figure out a better way to handle this.
    int ForceSigned;
} options;

typedef struct
{
    int NameIndex;
    int PrefixMask;
    int Prefix;
    int UseExtension;
    int Extension;
    op (*Decode)(u8 *IP, options Options);
    options DecodeOptions;
} op_definition;

static int GetRegisterIndex(int RegisterCode, int IsWord)
{
    IsWord = IsWord ? 1 : 0;
    int Result = (IsWord * 8) + RegisterCode;
    return(Result);
}

static void SetOpConfig(op *Op)
{
    Op->Dest = (Op->IP[0] & 0b00000010) >> 1; // NOTE(chuck): This is unioned with "Sign".
    Op->Word = (Op->IP[0] & 0b00000001);
    Op->Mode = (Op->IP[1] & 0b11000000) >> 6;
    Op->RegA = (Op->IP[1] & 0b00111000) >> 3;
    Op->RegB = (Op->IP[1] & 0b00000111);
}

static void SetParamToImm(u8 *IP, op *Op, int ParamIndex, options Options)
{
    int MaybeSignExtendAndWord = 0;
    if(Options.NoSignExtension)
    {
        MaybeSignExtendAndWord = Op->Word;
    }
    else
    {
        MaybeSignExtendAndWord = (Options.SignExtend == 0) && Op->Word;
    }

    Op->Param[ParamIndex].Type = Param_Immediate;
    if(MaybeSignExtendAndWord)
    {
        if(Options.ForceSigned)
        {
            Op->Param[ParamIndex].ImmediateValue = *(s16 *)&IP[0];
        }
        else
        {
            Op->Param[ParamIndex].ImmediateValue = *(u16 *)&IP[0];
        }

        if(Op->EmitSize)
        {
            Op->Param[ParamIndex].ByteSizeQualifier = 2; // NOTE(chuck): "word" prefix
        }
    }
    else
    {
        if(Options.ForceSigned)
        {
            Op->Param[ParamIndex].ImmediateValue = (s8)IP[0];
        }
        else
        {
            Op->Param[ParamIndex].ImmediateValue = IP[0];
        }

        if(Op->EmitSize)
        {
            Op->Param[ParamIndex].ByteSizeQualifier = 1; // NOTE(chuck): "byte" prefix
        }
    }

    if(Options.SignExtend)
    {
        if(Op->Param[ParamIndex].ImmediateValue & 0x80)
        {
            Op->Param[ParamIndex].ImmediateValue |= 0xffffff00;
        }
        else
        {
            Op->Param[ParamIndex].ImmediateValue &= ~0xffffff00;
        }
    }
}

static void SetParamToReg(u8 *IP, op *Op, int ParamIndex, int RegisterIndex)
{
    Op->Param[ParamIndex].Type = Param_Register;
    Op->Param[ParamIndex].RegisterOrMemoryIndex = RegisterIndex;
}

static void SetParamToMem(u8 *IP, op *Op, int ParamIndex, int EffectiveAddressIndex,
                          int ReadDisplacement, int WordDisplacement, int SignedDisplacement)
{
    Op->Param[ParamIndex].Type = Param_Memory;
    Op->Param[ParamIndex].RegisterOrMemoryIndex = EffectiveAddressIndex;
    if(ReadDisplacement)
    {
        if(WordDisplacement)
        {
            if(SignedDisplacement)
            {
                Op->Param[ParamIndex].Offset = *(s16 *)&IP[0];
            }
            else
            {
                Op->Param[ParamIndex].Offset = *(u16 *)&IP[0];
            }
        }
        else
        {
            if(SignedDisplacement)
            {
                Op->Param[ParamIndex].Offset = (s8)IP[0];
            }
            else
            {
                Op->Param[ParamIndex].Offset = IP[0];
            }
        }
    }
}

static void SetParamToMemDirectAddress(u8 *IP, op *Op, int ParamIndex, int Word)
{
    Op->Param[ParamIndex].Type = Param_MemoryDirectAddress;
    if(Word)
    {
        Op->Param[ParamIndex].Offset = *(u16 *)&IP[0];
    }
    else
    {
        Op->Param[ParamIndex].Offset = IP[0];
    }
}

static void SharedRegisterOrMemoryVsRegister(u8 *IP, op *Op)
{
    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT) // 0b11
    {
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegB, Op->Word));
        SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegA, Op->Word));
        Op->ByteLength = 2; // TODO(chuck): Maybe just increment pointers like any standard file format parser does instead. Not sure why I did this everywhere.
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            if(Op->Dest)
            {
                SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
                SetParamToMem(IP + 2, Op, SOURCE, Op->RegB, 0, 0, 0);
            }
            else
            {
                SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
                SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegA, Op->Word));
            }

            Op->ByteLength = 2;
        }
        else // NOTE(chuck): Direct address
        {
            if(Op->Dest)
            {
                SetParamToReg(IP, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
                SetParamToMemDirectAddress(IP + 2, Op, SOURCE, Op->Word);
                Op->ByteLength = Op->Word ? 4 : 3;
            }
            else
            {
                Op->Error = 1;
                Op->ByteLength = 1; // NOTE(chuck): This is important for the disassembled listing of the op bytes.
            }
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        int WordDisplacement = (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        Op->ByteLength = WordDisplacement ? 4 : 3;
        if(Op->Dest)
        {
            SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
            SetParamToMem(IP + 2, Op, SOURCE, Op->RegB, 1, WordDisplacement, 1);
        }
        else
        {
            SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 1, WordDisplacement, 1);
            SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegA, Op->Word));
        }
    }
}

static op Xchg_RegisterOrMemoryWithRegister(u8 *IP, options DecodeOptions)
{
    op Opp = {DecodeOptions.NameIndex, IP, 2, 0};
    SetOpConfig(&Opp);

    op *Op = &Opp;
    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT)
    {
        // NOTE(chuck): XCHG-specific
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
        SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegB, Op->Word));
        Op->ByteLength = 2;
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            if(Op->Dest)
            {
                SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
                SetParamToMem(IP + 2, Op, SOURCE, Op->RegB, 0, 0, 0);
            }
            else
            {
                SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
                SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegA, Op->Word));
            }

            Op->ByteLength = 2;
        }
        else // NOTE(chuck): Direct address
        {
            if(Op->Dest)
            {
                SetParamToReg(IP, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
                SetParamToMemDirectAddress(IP + 2, Op, SOURCE, Op->Word);
                Op->ByteLength = 4;//Op->Word ? 4 : 3;
            }
            else
            {
                Op->Error = 1;
                Op->ByteLength = 1;
            }
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        int WordDisplacement = (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        Op->ByteLength = WordDisplacement ? 4 : 3;
        if(Op->Dest)
        {
            SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
            SetParamToMem(IP + 2, Op, SOURCE, Op->RegB, 1, WordDisplacement, 1);
        }
        else
        {
            SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 1, WordDisplacement, 1);
            SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegA, Op->Word));
        }
    }

    return(Opp);
}

// ::: 1 0 0 0 1 0 d w | mod  reg   r/m  |    (DISP-LO)    |    (DISP-HI)    |
static op MovRegisterOrMemoryToOrFromRegister(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_MOV, IP, 2, 0};
    SetOpConfig(&Op);
    SharedRegisterOrMemoryVsRegister(IP, &Op);
    return(Op);
}

static void SharedImmediateToRegister(u8 *IP, op *Op, int RegisterIndex, options Options)
{
    SetParamToReg(IP, Op, DESTINATION, RegisterIndex);
    SetParamToImm(IP + 1, Op, SOURCE, Options);
    Op->ByteLength = Op->Word ? 3 : 2;
}

// ::: 1 0 1 1 w  reg  |      data       |   data if w=1   |
static op MovImmediateToRegister(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_MOV, IP, 2, 0};
    Op.Word = IP[0] & 0b00001000;
    Op.RegA = IP[0] & 0b00000111;
    SharedImmediateToRegister(IP, &Op, GetRegisterIndex(Op.RegA, Op.Word), (options){0});
    return(Op);
}

static void SharedImmediateToRegisterOrMemory(u8 *IP, op *Op, options Options)
{
    int MaybeSignExtendAndWord = 0;
    if(Options.NoSignExtension)
    {
        MaybeSignExtendAndWord = Op->Word;
    }
    else
    {
        MaybeSignExtendAndWord = (Options.SignExtend == 0) && Op->Word;
    }

    Op->EmitSize = 1;
    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT)
    {
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegB, Op->Word));
        SetParamToImm(IP + 2, Op, SOURCE, Options);
        Op->ByteLength = MaybeSignExtendAndWord ? 4 : 3;
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
            SetParamToImm(IP + 2, Op, SOURCE, Options);
            Op->ByteLength = MaybeSignExtendAndWord ? 4 : 3;
        }
        else
        {
            SetParamToMemDirectAddress(IP + 2, Op, DESTINATION, Op->Word);
            SetParamToImm(IP + 4, Op, SOURCE, Options);
            Op->ByteLength = MaybeSignExtendAndWord ? 6 : 5;
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        // NOTE(chuck): It took me a while to realize there were back-to-back variable-sized encodings here. I do not claim to be an intelligent man!
        int ByteLength = 2;

        int WordDisplacement = (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 1, WordDisplacement, 1);
        ByteLength += WordDisplacement ? 2 : 1;

        SetParamToImm(IP + ByteLength, Op, SOURCE, Options);
        ByteLength += MaybeSignExtendAndWord ? 2 : 1;

        Op->ByteLength = ByteLength;
    }
}

// ::: 1 1 0 0 0 1 1 w | mod 0 0 0  r/m  | (DISP-LO) | (DISP-HI) | data | data if w=1 |
static op MovImmediateToRegisterOrMemory(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_MOV, IP, 2, 0};
    SetOpConfig(&Op);
    SharedImmediateToRegisterOrMemory(IP, &Op, (options){0});
    return(Op);
}

// ::: 1 0 1 0 0 0 0 w |     addr-lo     |     addr-hi     |
static op MovMemoryToAccumulator(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_MOV, IP, 2, 0};
    SetOpConfig(&Op);
    SetParamToReg(IP + 1, &Op, DESTINATION, REGISTER_NAME_AX);
    SetParamToMemDirectAddress(IP + 1, &Op, SOURCE, Op.Word);
    Op.ByteLength = Op.Word ? 3 : 2;
    return(Op);
}

// ::: 1 0 1 0 0 0 1 w |     addr-lo     |     addr-hi     |
static op MovAccumulatorToMemory(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_MOV, IP, 2, 0};
    Op.Word = IP[0] & 1;
    SetParamToMemDirectAddress(IP + 1, &Op, DESTINATION, Op.Word);
    SetParamToReg(IP + 1, &Op, SOURCE, REGISTER_NAME_AX);
    Op.ByteLength = Op.Word ? 3 : 2;
    return(Op);
}

// ::: 0 0 0 0 0 0 d w | mod  reg   r/m  |    (DISP-LO)    |    (DISP-HI)    |
static op AddSubCmp_RegisterOrMemoryWithRegisterToEither(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, DecodeOptions.ParamCount, 0};
    SetOpConfig(&Op);
    SharedRegisterOrMemoryVsRegister(IP, &Op);
    return(Op);
}

static op IncDec(u8 *IP, options DecodeOptions)
{
    op Opp = {DecodeOptions.NameIndex, IP, 1, 0};

    op *Op = &Opp;
    Op->Word = (Op->IP[0] & 0b00000001);
    Op->Mode = (Op->IP[1] & 0b11000000) >> 6;
    Op->RegB = (Op->IP[1] & 0b00000111);

    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT) // 0b11
    {
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegB, Op->Word));
        Op->ByteLength = 2;
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
            Op->Param[DESTINATION].ByteSizeQualifier = Op->Word;
            Op->ByteLength = 2;
            Op->EmitSize = 1;
        }
        else // NOTE(chuck): Direct address
        {
            SetParamToMemDirectAddress(IP + 2, Op, DESTINATION, Op->Word);
            Op->Param[DESTINATION].ByteSizeQualifier = Op->Word;
            Op->ByteLength = Op->Word ? 4 : 3;
            Op->EmitSize = 1;
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        int WordDisplacement = (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 1, WordDisplacement, 1);
        Op->Param[DESTINATION].ByteSizeQualifier = Op->Word;
        Op->ByteLength = WordDisplacement ? 4 : 3;
        Op->EmitSize = 1;
    }

    return(Opp);
}

static op Rotate(u8 *IP, options DecodeOptions)
{
    op Opp = {DecodeOptions.NameIndex, IP, 2, 0};
    SetOpConfig(&Opp);

    op *Op = &Opp;
    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT) // 0b11
    {
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegB, Op->Word));
        Op->ByteLength = 2;
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
            Op->Param[DESTINATION].ByteSizeQualifier = Op->Word;
            Op->EmitSize = 1;
            Op->ByteLength = 2;
        }
        else // NOTE(chuck): Direct address
        {
            SetParamToMemDirectAddress(IP + 2, Op, DESTINATION, 1);//Op->Word);
            Op->Param[DESTINATION].ByteSizeQualifier = Op->Word;
            Op->EmitSize = 1;
            Op->ByteLength = 4;//Op->Word ? 4 : 3;
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        int WordDisplacement = (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 1, WordDisplacement, 1);
        Op->Param[DESTINATION].ByteSizeQualifier = Op->Word;
        Op->EmitSize = 1;
        Op->ByteLength = WordDisplacement ? 4 : 3;
    }

    if(Op->VariableShift)
    {
        SetParamToReg(IP, Op, SOURCE, REGISTER_NAME_CL);
    }
    else
    {
        Op->Param[SOURCE].Type = Param_Immediate;
        Op->Param[SOURCE].ImmediateValue = 1;
    }

    return(Opp);
}

// NOTE(chuck): Copy/pasted from AddSubCmp_RegisterOrMemoryWithRegisterToEither and tweaked.
static op Lea(u8 *IP, options DecodeOptions)
{
    op Opp = {DecodeOptions.NameIndex, IP, 2, 0};
    SetOpConfig(&Opp);
    Opp.Word = 1; // NOTE(chuck): LEA-specific

    op *Op = &Opp;
    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT)
    {
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegB, Op->Word));
        SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegA, Op->Word));
        Op->ByteLength = 2; // TODO(chuck): Maybe just increment pointers like any standard file format parser does instead. Not sure why I did this everywhere.
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            // if(Op->Dest)
            // {
            //     SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
            //     SetParamToMem(IP + 2, Op, SOURCE, Op->RegB, 0, 0, 0);
            // }
            // else
            // {
                SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
                SetParamToReg(IP + 2, Op, SOURCE, GetRegisterIndex(Op->RegA, Op->Word));
            // }

            Op->ByteLength = 2;
        }
        else // NOTE(chuck): Direct address
        {
            // if(Op->Dest)
            // {
            //     SetParamToReg(IP, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
            //     SetParamToMemDirectAddress(IP + 2, Op, SOURCE, Op->Word);
            //     Op->ByteLength = Op->Word ? 4 : 3;
            // }
            // else
            // {
                Op->Error = 1;
                Op->ByteLength = 1;
            // }
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        int WordDisplacement = (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        Op->ByteLength = WordDisplacement ? 4 : 3;
        // if(Op->Dest)
        // {
        //     SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
        //     SetParamToMem(IP + 2, Op, SOURCE, Op->RegB, 1, WordDisplacement, 1);
        // }
        // else
        // {
            // NOTE(chuck): LEA uses reverse ordering from ADD/SUB/BMP
            SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegA, Op->Word));
            SetParamToMem(IP + 2, Op, SOURCE, Op->RegB, 1, WordDisplacement, 1);
        // }
    }

    return(Opp);
}

// ::: 1 0 0 0 0 0 s w | mod 0 0 0  r/m  | (DISP-LO) | (DISP-HI) | data | data if w=1 |
static op AddSubCmp_ImmediateWithRegisterOrMemory(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 2, 0};
    SetOpConfig(&Op);
    options Options =
    {
        .SignExtend = Op.Sign,
        .NoSignExtension = DecodeOptions.NoSignExtension,
    };
    SharedImmediateToRegisterOrMemory(IP, &Op, Options);
    return(Op);
}

static op AddSubCmp_ImmediateWithAccumulator(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 2, 0};
    Op.Word = IP[0] & 0b00000001;
    SharedImmediateToRegister(IP, &Op,
                              Op.Word ? REGISTER_NAME_AX : REGISTER_NAME_AL,
                              (options){.SignExtend = Op.Word ? 0 : 1});
    return(Op);
}

static op SharedRelativeJump(u8 *IP, int NameIndex)
{
    op Op = {NameIndex, IP, 1, 0};
    Op.Param[0].Type = Param_Immediate;
    Op.Param[0].ImmediateValue = (s8)IP[1];
    Op.ByteLength = 2;
    Op.IsRelativeJump = 1;
    return(Op);
}

// TODO(chuck): Make a single handler for these kinda things? This is kinda tedious and annoying.
static op Je(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JE);  }
static op Jl(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JL);  }
static op Jle(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JLE); }
static op Jb(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JB);  }
static op Jbe(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JBE); }
static op Jp(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JP);  }
static op Jo(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JO);  }
static op Js(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JS);  }
static op Jnz(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JNZ); }
static op Jnl(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JNL); }
static op Jg(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JG);  }
static op Jnb(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JNB); }
static op Ja(u8 *IP, options DecodeOptions)     { return SharedRelativeJump(IP, OP_NAME_JA);  }
static op Jnp(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JNP); }
static op Jno(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JNO); }
static op Jns(u8 *IP, options DecodeOptions)    { return SharedRelativeJump(IP, OP_NAME_JNS); }
static op Loop(u8 *IP, options DecodeOptions)   { return SharedRelativeJump(IP, OP_NAME_LOOP);   }
static op Loopz(u8 *IP, options DecodeOptions)  { return SharedRelativeJump(IP, OP_NAME_LOOPZ);  }
static op Loopnz(u8 *IP, options DecodeOptions) { return SharedRelativeJump(IP, OP_NAME_LOOPNZ); }
static op Jcxz(u8 *IP, options DecodeOptions)   { return SharedRelativeJump(IP, OP_NAME_JCXZ);   }

static op PushPop_RegisterOrMemory(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 1, 0};
    SetOpConfig(&Op);

    if(Op.Mode == REGISTER_MODE_NO_DISPLACEMENT)
    {
        SetParamToReg(IP, &Op, DESTINATION, GetRegisterIndex(Op.RegB, 1));
        Op.ByteLength = 2;
    }
    else if(Op.Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op.RegB != 0x06)
        {
            SetParamToMem(IP + 2, &Op, DESTINATION, Op.RegB, 0, 0, 0);
            Op.EmitSize = 1;
            Op.ByteLength = 2;
        }
        else // NOTE(chuck): Direct address
        {
            SetParamToMemDirectAddress(IP + 2, &Op, DESTINATION, 1);
            Op.ByteLength = 4;
            Op.EmitSize = 1;
        }
    }
    else if((Op.Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op.Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        int WordDisplacement = (Op.Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        Op.ByteLength = WordDisplacement ? 4 : 3;
        SetParamToMem(IP + 2, &Op, DESTINATION, Op.RegB, 1, WordDisplacement, 1);
        Op.EmitSize = 1;
    }

    return(Op);
}

static op PushPop_Register(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 1, 0};
    int Register = IP[0] & 0b111;
    SetParamToReg(IP, &Op, DESTINATION, GetRegisterIndex(Register, 1));
    Op.ByteLength = 1;
    return(Op);
}

static op PushPop_SegmentRegister(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 1, 0};
    int SegmentRegister = (IP[0] & 0b00011000) >> 3;
    Op.Param[DESTINATION].Type = Param_SegmentRegister;
    Op.Param[DESTINATION].RegisterOrMemoryIndex = SegmentRegister;
    Op.ByteLength = 1;
    return(Op);
}

static op XchgAccumulator(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_XCHG, IP, 2, 0};
    int Register = (IP[0] & 0b00000111);
    Op.Param[DESTINATION].Type = Param_Register;
    Op.Param[DESTINATION].RegisterOrMemoryIndex = REGISTER_NAME_AX;
    SetParamToReg(IP, &Op, SOURCE, GetRegisterIndex(Register, 1));
    Op.ByteLength = 1;
    return(Op);
}

static op InOut_FixedPort(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 2, 0};
    int Word = (IP[0] & 0b00000001);

    // NOTE(chuck): For "out"
    int A = DESTINATION;
    int B = SOURCE;
    if(DecodeOptions.SwapParams)
    {
        int Temp = A;
        A = B;
        B = Temp;
    }

    Op.Param[A].Type = Param_Register;
    Op.Param[A].RegisterOrMemoryIndex = Word ? REGISTER_NAME_AX : REGISTER_NAME_AL;
    SetParamToImm(IP + 1, &Op, B, (options){0});
    Op.ByteLength = 2;
    return(Op);
}

static op InOut_VariablePort(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 2, 0};
    int Word = (IP[0] & 0b00000001);

    // NOTE(chuck): For "out"
    int A = DESTINATION;
    int B = SOURCE;
    if(DecodeOptions.SwapParams)
    {
        int Temp = A;
        A = B;
        B = Temp;
    }

    Op.Param[A].Type = Param_Register;
    Op.Param[A].RegisterOrMemoryIndex = Word ? REGISTER_NAME_AX : REGISTER_NAME_AL;
    Op.Param[B].Type = Param_Register;
    Op.Param[B].RegisterOrMemoryIndex = REGISTER_NAME_DX;
    Op.ByteLength = 1;
    return(Op);
}

static op LiteralBytes(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 0, 0};
    Op.ByteLength = DecodeOptions.ByteLength;
    return(Op);
}

static op Rep(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 0, 0};
    Op.RepeatWhileZero = (IP[0] & 1);
    Op.ByteLength = 1;
    return(Op);
}

static op CallDirectIntersegment(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 1, 0};
    Op.Param[DESTINATION].Type = Param_DirectIntersegment;
    Op.Param[DESTINATION].IP = *(u16 *)&IP[1];
    Op.Param[DESTINATION].CS = *(u16 *)&IP[3];
    Op.ByteLength = 5;
    return(Op);
}

static op CallJmp_IndirectWithinSegment(u8 *IP, options DecodeOptions)
{
    op Opp = {DecodeOptions.NameIndex, IP, 1, 0};
    SetOpConfig(&Opp);

    op *Op = &Opp;
    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT) // 0b11
    {
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegB, Op->Word));
        Op->ByteLength = 2;
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
            Op->ByteLength = Op->Word ? 4 : 3;
        }
        else // NOTE(chuck): Direct address
        {
            SetParamToMemDirectAddress(IP + 2, Op, DESTINATION, Op->Word);
            Op->ByteLength = Op->Word ? 4 : 3;
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        int WordDisplacement = (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT);
        Op->ByteLength = WordDisplacement ? 4 : 3;
        SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 1, WordDisplacement, 1);
    }

    return(Opp);
}

static op Ret(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_RET, IP, 0, 0};
    Op.ByteLength = DecodeOptions.ByteLength;
    if(Op.ByteLength == 3)
    {
        Op.ParamCount = 1;
        Op.Word = 1;
        SetParamToImm(IP + 1, &Op, DESTINATION, (options){0});
    }
    return(Op);
}

static op Int(u8 *IP, options DecodeOptions)
{
    op Op = {OP_NAME_INT, IP, 1, 0};
    Op.ByteLength = 2;
    SetParamToImm(IP + 1, &Op, DESTINATION, (options){0});
    return(Op);
}

static op SegmentOverride(u8 *IP, options DecodeOptions)
{
    op Op = {SEGMENT_OVERRIDE, IP, 0, 0};
    Op.SegmentOverride = (IP[0] & 0b00011000) >> 3;
    Op.ByteLength = 1;
    return(Op);
}

static op_definition OpTable[] =
{
    // TODO(chuck): There is order-dependence here! AddSubCmp_ImmediateWithRegisterOrMemory will fire first if XCHG is listed after it, which is no bueno. I assume this means that I have to order all these encodings by largest prefix and descending, then misc. splotchy masks. Hopefully that works for everything? There's probably a better way to handle this then the prefix masking, such that there is no ambiguity related to ordering.
    // TODO(chuck): Table 4-14. Machine Instruction Encoding Matrix looks interesting but I don't understand how to read it.
    {OP_NAME_XCHG,   0b11111110, 0b10000110, 0, 0, Xchg_RegisterOrMemoryWithRegister, {.NameIndex=OP_NAME_XCHG}},

    {OP_NAME_MOV,    0b11111100, 0b10001000, 0, 0, MovRegisterOrMemoryToOrFromRegister, {0}},
    {OP_NAME_MOV,    0b11110000, 0b10110000, 0, 0, MovImmediateToRegister, {0}},
    {OP_NAME_MOV,    0b11111110, 0b11000110, 0, 0, MovImmediateToRegisterOrMemory, {0}},
    {OP_NAME_MOV,    0b11111110, 0b10100000, 0, 0, MovMemoryToAccumulator, {0}},
    {OP_NAME_MOV,    0b11111110, 0b10100010, 0, 0, MovAccumulatorToMemory, {0}},

    {OP_NAME_ADD,    0b11111100, 0b00000000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_ADD, .ParamCount=2}},
    {OP_NAME_ADD,    0b11111000, 0b10000000, 1, 0b000, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_ADD}},
    {OP_NAME_ADD,    0b11111110, 0b00000100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_ADD}},

    {OP_NAME_ADC,    0b11111100, 0b00010000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_ADC, .ParamCount=2}},
    {OP_NAME_ADC,    0b11111000, 0b10000000, 1, 0b010, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_ADC}},
    {OP_NAME_ADC,    0b11111110, 0b00010100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_ADC}},

    {OP_NAME_SUB,    0b11111100, 0b00101000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_SUB, .ParamCount=2}},
    {OP_NAME_SUB,    0b11111000, 0b10000000, 1, 0b101, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_SUB}},
    {OP_NAME_SUB,    0b11111110, 0b00101100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_SUB}},

    {OP_NAME_SBB,    0b11111100, 0b00011000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_SBB, .ParamCount=2}},
    {OP_NAME_SBB,    0b11111000, 0b10000000, 1, 0b011, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_SBB}},
    {OP_NAME_SBB,    0b11111110, 0b00011100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_SBB}},

    {OP_NAME_CMP,    0b11111100, 0b00111000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_CMP, .ParamCount=2}},
    {OP_NAME_CMP,    0b11111000, 0b10000000, 1, 0b111, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_CMP}},
    {OP_NAME_CMP,    0b11111110, 0b00111100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_CMP}},

    {OP_NAME_AND,    0b11111100, 0b00100000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_AND, .ParamCount=2}},
    {OP_NAME_AND,    0b11111000, 0b10000000, 1, 0b100, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_AND, .NoSignExtension=1}},
    {OP_NAME_AND,    0b11111110, 0b00100100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_AND}},

    {OP_NAME_TEST,   0b11111100, 0b10000100, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_TEST, .ParamCount=2}},
    {OP_NAME_TEST,   0b11111110, 0b11110110, 1, 0b000, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_TEST, .NoSignExtension=1}},
    {OP_NAME_TEST,   0b11111110, 0b10101000, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_TEST}},

    {OP_NAME_OR,     0b11111100, 0b00001000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_OR, .ParamCount=2}},
    {OP_NAME_OR,     0b11111000, 0b10000000, 1, 0b001, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_OR, .NoSignExtension=1}},
    {OP_NAME_OR,     0b11111110, 0b00001100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_OR}},

    {OP_NAME_XOR,    0b11111100, 0b00110000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_XOR, .ParamCount=2}},
    {OP_NAME_XOR,    0b11111000, 0b10000000, 1, 0b110, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_XOR, .NoSignExtension=1}},
    {OP_NAME_XOR,    0b11111110, 0b00110100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_XOR}},

    {OP_NAME_LEA,    0b11111111, 0b10001101, 0, 0,     Lea, {.NameIndex=OP_NAME_LEA}},
    {OP_NAME_LDS,    0b11111111, 0b11000101, 0, 0,     Lea, {.NameIndex=OP_NAME_LDS}},
    {OP_NAME_LES,    0b11111111, 0b11000100, 0, 0,     Lea, {.NameIndex=OP_NAME_LES}},

    {OP_NAME_JE,     0b11111111, 0b01110100, 0, 0, Je, {0}},
    {OP_NAME_JL,     0b11111111, 0b01111100, 0, 0, Jl, {0}},
    {OP_NAME_JLE,    0b11111111, 0b01111110, 0, 0, Jle, {0}},
    {OP_NAME_JB,     0b11111111, 0b01110010, 0, 0, Jb, {0}},
    {OP_NAME_JBE,    0b11111111, 0b01110110, 0, 0, Jbe, {0}},
    {OP_NAME_JP,     0b11111111, 0b01111010, 0, 0, Jp, {0}},
    {OP_NAME_JO,     0b11111111, 0b01110000, 0, 0, Jo, {0}},
    {OP_NAME_JS,     0b11111111, 0b01111000, 0, 0, Js, {0}},
    {OP_NAME_JNZ,    0b11111111, 0b01110101, 0, 0, Jnz, {0}},
    {OP_NAME_JNL,    0b11111111, 0b01111101, 0, 0, Jnl, {0}},
    {OP_NAME_JG,     0b11111111, 0b01111111, 0, 0, Jg, {0}},
    {OP_NAME_JNB,    0b11111111, 0b01110011, 0, 0, Jnb, {0}},
    {OP_NAME_JA,     0b11111111, 0b01110111, 0, 0, Ja, {0}},
    {OP_NAME_JNP,    0b11111111, 0b01111011, 0, 0, Jnp, {0}},
    {OP_NAME_JNO,    0b11111111, 0b01110001, 0, 0, Jno, {0}},
    {OP_NAME_JNS,    0b11111111, 0b01111001, 0, 0, Jns, {0}},
    {OP_NAME_LOOP,   0b11111111, 0b11100010, 0, 0, Loop, {0}},
    {OP_NAME_LOOPZ , 0b11111111, 0b11100001, 0, 0, Loopz, {0}},
    {OP_NAME_LOOPNZ, 0b11111111, 0b11100000, 0, 0, Loopnz, {0}},
    {OP_NAME_JCXZ,   0b11111111, 0b11100011, 0, 0, Jcxz, {0}},
    
    {OP_NAME_XLAT,   0b11111111, 0b11010111, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_XLAT, .ByteLength=1}},

    {OP_NAME_LAHF,   0b11111111, 0b10011111, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_LAHF, .ByteLength=1}},
    {OP_NAME_SAHF,   0b11111111, 0b10011110, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_SAHF, .ByteLength=1}},
    {OP_NAME_PUSHF,  0b11111111, 0b10011100, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_PUSHF, .ByteLength=1}},
    {OP_NAME_POPF,   0b11111111, 0b10011101, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_POPF, .ByteLength=1}},
    {OP_NAME_AAA,    0b11111111, 0b00110111, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_AAA, .ByteLength=1}},
    {OP_NAME_DAA,    0b11111111, 0b00100111, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_DAA, .ByteLength=1}},
    {OP_NAME_AAS,    0b11111111, 0b00111111, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_AAS, .ByteLength=1}},
    {OP_NAME_DAS,    0b11111111, 0b00101111, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_DAS, .ByteLength=1}},

    {OP_NAME_INTO,   0b11111111, 0b11001110, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_INTO, .ByteLength=1}},
    {OP_NAME_IRET,   0b11111111, 0b11001111, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_IRET, .ByteLength=1}},
    {OP_NAME_CLC,    0b11111111, 0b11111000, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_CLC, .ByteLength=1}},
    {OP_NAME_CMC,    0b11111111, 0b11110101, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_CMC, .ByteLength=1}},
    {OP_NAME_STC,    0b11111111, 0b11111001, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_STC, .ByteLength=1}},
    {OP_NAME_CLD,    0b11111111, 0b11111100, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_CLD, .ByteLength=1}},
    {OP_NAME_STD,    0b11111111, 0b11111101, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_STD, .ByteLength=1}},
    {OP_NAME_CLI,    0b11111111, 0b11111010, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_CLI, .ByteLength=1}},
    {OP_NAME_STI,    0b11111111, 0b11111011, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_STI, .ByteLength=1}},
    {OP_NAME_HLT,    0b11111111, 0b11110100, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_HLT, .ByteLength=1}},
    {OP_NAME_WAIT,   0b11111111, 0b10011011, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_WAIT, .ByteLength=1}},
    {OP_NAME_CBW,    0b11111111, 0b10011000, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_CBW, .ByteLength=1}},
    {OP_NAME_CWD,    0b11111111, 0b10011001, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_CWD, .ByteLength=1}},

    // {OP_NAME_CALL,   0b11111111, 0b11101000, 0, 0, AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_CALL, .ParamCount=1}},
    {OP_NAME_CALL,   0b11111111, 0b11111111, 1, 0b010, CallJmp_IndirectWithinSegment, {.NameIndex=OP_NAME_CALL}},
    {OP_NAME_CALL,   0b11111111, 0b10011010, 0, 0, CallDirectIntersegment, {.NameIndex=OP_NAME_CALL}},

    {OP_NAME_JMP,    0b11111111, 0b11111111, 1, 0b100, CallJmp_IndirectWithinSegment, {.NameIndex=OP_NAME_JMP}},
    {OP_NAME_JMP,    0b11111111, 0b11101010, 0, 0, CallDirectIntersegment, {.NameIndex=OP_NAME_JMP}},

    {OP_NAME_RET,    0b11111111, 0b11000011, 0, 0, Ret, {.ByteLength=1}},
    {OP_NAME_RET,    0b11111111, 0b11000010, 0, 0, Ret, {.ByteLength=3}},
    {OP_NAME_INT,    0b11111111, 0b11001101, 0, 0, Int, {0}},
    {OP_NAME_INT3,   0b11111111, 0b11001100, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_INT3, .ByteLength=1}},
    {OP_NAME_LOCK,   0b11111111, 0b11110000, 0, 0, LiteralBytes, {.NameIndex=OP_NAME_LOCK, .ByteLength=1}},

    {OP_NAME_REP,    0b11111110, 0b11110010, 0, 0, Rep, {.NameIndex=OP_NAME_REP}},
    {OP_NAME_MOVS,   0b11111110, 0b10100100, 0, 0, Rep, {.NameIndex=OP_NAME_MOVS}},
    {OP_NAME_CMPS,   0b11111110, 0b10100110, 0, 0, Rep, {.NameIndex=OP_NAME_CMPS}},
    {OP_NAME_SCAS,   0b11111110, 0b10101110, 0, 0, Rep, {.NameIndex=OP_NAME_SCAS}},
    {OP_NAME_LODS,   0b11111110, 0b10101100, 0, 0, Rep, {.NameIndex=OP_NAME_LODS}},
    {OP_NAME_STOS,   0b11111110, 0b10101010, 0, 0, Rep, {.NameIndex=OP_NAME_STOS}},

    {OP_NAME_INC,    0b11111110, 0b11111110, 1, 0b000, IncDec, {.NameIndex=OP_NAME_INC}},
    {OP_NAME_INC,    0b11111000, 0b01000000, 0, 0,     PushPop_Register,         {.NameIndex=OP_NAME_INC}},

    {OP_NAME_DEC,    0b11111110, 0b11111110, 1, 0b001, IncDec, {.NameIndex=OP_NAME_DEC}},
    {OP_NAME_DEC,    0b11111000, 0b01001000, 0, 0,     PushPop_Register,         {.NameIndex=OP_NAME_DEC}},

    {OP_NAME_NEG,    0b11111110, 0b11110110, 1, 0b011, IncDec, {.NameIndex=OP_NAME_NEG}},

    {OP_NAME_MUL,    0b11111110, 0b11110110, 1, 0b100, IncDec, {.NameIndex=OP_NAME_MUL}},
    {OP_NAME_IMUL,   0b11111110, 0b11110110, 1, 0b101, IncDec, {.NameIndex=OP_NAME_IMUL}},
    {OP_NAME_DIV,    0b11111110, 0b11110110, 1, 0b110, IncDec, {.NameIndex=OP_NAME_DIV}},
    {OP_NAME_IDIV,   0b11111110, 0b11110110, 1, 0b111, IncDec, {.NameIndex=OP_NAME_IDIV}},

    {OP_NAME_NOT,    0b11111110, 0b11110110, 1, 0b010, IncDec, {.NameIndex=OP_NAME_NOT}},

    {OP_NAME_SHL,    0b11111100, 0b11010000, 1, 0b100, Rotate, {.NameIndex=OP_NAME_SHL}},
    {OP_NAME_SHR,    0b11111100, 0b11010000, 1, 0b101, Rotate, {.NameIndex=OP_NAME_SHR}},
    {OP_NAME_SAR,    0b11111100, 0b11010000, 1, 0b111, Rotate, {.NameIndex=OP_NAME_SAR}},
    {OP_NAME_ROL,    0b11111100, 0b11010000, 1, 0b000, Rotate, {.NameIndex=OP_NAME_ROL}},
    {OP_NAME_ROR,    0b11111100, 0b11010000, 1, 0b001, Rotate, {.NameIndex=OP_NAME_ROR}},
    {OP_NAME_RCL,    0b11111100, 0b11010000, 1, 0b010, Rotate, {.NameIndex=OP_NAME_RCL}},
    {OP_NAME_RCR,    0b11111100, 0b11010000, 1, 0b011, Rotate, {.NameIndex=OP_NAME_RCR}},

    {OP_NAME_AAM,    0b11111111, 0b11010100, 2, 0b00001010, LiteralBytes, {.NameIndex=OP_NAME_AAM, .ByteLength=2}},
    {OP_NAME_AAD,    0b11111111, 0b11010101, 2, 0b00001010, LiteralBytes, {.NameIndex=OP_NAME_AAD, .ByteLength=2}},

    {OP_NAME_PUSH,   0b11111111, 0b11111111, 1, 0b110, PushPop_RegisterOrMemory, {.NameIndex=OP_NAME_PUSH}},
    {OP_NAME_PUSH,   0b11111000, 0b01010000, 0, 0,     PushPop_Register,         {.NameIndex=OP_NAME_PUSH}},
    {OP_NAME_PUSH,   0b11100111, 0b00000110, 0, 0,     PushPop_SegmentRegister,  {.NameIndex=OP_NAME_PUSH}},

    {OP_NAME_POP,    0b11111111, 0b10001111, 1, 0b000, PushPop_RegisterOrMemory, {.NameIndex=OP_NAME_POP}},
    {OP_NAME_POP,    0b11111000, 0b01011000, 0, 0,     PushPop_Register,         {.NameIndex=OP_NAME_POP}},
    {OP_NAME_POP,    0b11100111, 0b00000111, 0, 0,     PushPop_SegmentRegister,  {.NameIndex=OP_NAME_POP}},

    {OP_NAME_IN,     0b11111110, 0b11100100, 0, 0, InOut_FixedPort,    {.NameIndex=OP_NAME_IN}},
    {OP_NAME_IN,     0b11111110, 0b11101100, 0, 0, InOut_VariablePort, {.NameIndex=OP_NAME_IN}},
    {OP_NAME_OUT,    0b11111110, 0b11100110, 0, 0, InOut_FixedPort,    {.NameIndex=OP_NAME_OUT, .SwapParams=1}},
    {OP_NAME_OUT,    0b11111110, 0b11101110, 0, 0, InOut_VariablePort, {.NameIndex=OP_NAME_OUT, .SwapParams=1}},
    {OP_NAME_XCHG,   0b11111000, 0b10010000, 0, 0, XchgAccumulator,    {.NameIndex=OP_NAME_XCHG}},
    
    {SEGMENT_OVERRIDE, 0b11100111, 0b00100110, 0, 0, SegmentOverride, {0}},
};

static char *EffectiveAddressLookup[8] =
{
    "bx + si",
    "bx + di",
    "bp + si",
    "bp + di",
    "si",
    "di",
    "bp",
    "bx"
};

static int PrintParam(char *OutputInit, op *Op, op_param *Param)
{
    char *Output = OutputInit;
    int BytesWritten = 0;
    int TotalBytesWritten = 0;

#define EMIT(...) \
    BytesWritten = sprintf(Output, __VA_ARGS__); \
    Output += BytesWritten; \
    TotalBytesWritten += BytesWritten;

    switch(Param->Type)
    {
        case Param_Immediate:
        {
            EMIT("%d", Param->ImmediateValue);
        } break;

        case Param_Register:
        {
            EMIT("%s", RegisterLookup[Param->RegisterOrMemoryIndex]);
        } break;

        case Param_SegmentRegister:
        {
            EMIT("%s", SegmentRegisterLookup[Param->RegisterOrMemoryIndex]);
        } break;
        
        case Param_Memory:
        {
            if(Op->UseSegmentOverride)
            {
                EMIT("%s:", SegmentRegisterLookup[Op->SegmentOverride]);
            }

            EMIT("[%s", EffectiveAddressLookup[Param->RegisterOrMemoryIndex]);
            int Offset = Param->Offset;
            if(Offset)
            {
                if(Offset < 0)
                {
                    EMIT(" - ");
                    Offset *= -1;
                }
                else
                {
                    EMIT(" + ");
                }
                EMIT("%d", Offset);
            }
            EMIT("]");
        } break;

        case Param_MemoryDirectAddress:
        {
            if(Op->UseSegmentOverride)
            {
                EMIT("%s:", SegmentRegisterLookup[Op->SegmentOverride]);
            }

            EMIT("[%d]", Param->Offset);
        } break;

        case Param_DirectIntersegment:
        {
            EMIT("%d:%d", Param->CS, Param->IP);
        } break;

        default:
        {
            EMIT("<unknown parameter type: %d>", Param->Type);
            if(Output != OutputInit)
            {
                EMIT(" <partial output: %s>", OutputInit);
            }
        } break;
    }

#undef EMIT

    return TotalBytesWritten;
}

typedef struct
{
    op *Op;
    __int64 Offset;
} jump;

static int CompareJumps(const void *A, const void *B)
{
    jump *AJump = (jump *)A;
    jump *BJump = (jump *)B;

    int Result = 0;

    if(AJump->Offset < BJump->Offset)
    {
        Result = -1;
    }
    else if(AJump->Offset > BJump->Offset)
    {
        Result = 1;
    }

    return(Result);
}

int main(int ArgCount, char **Args)
{
    int Result = 0;
    int HitError = 0;

    char *Filename = Args[1];
    FILE *File = fopen(Filename, "rb");
    size_t ByteLength = fread(OpStream, 1, ArrayLength(OpStream), File);

    printf("bits 16\n");

    u8 *IP = OpStream;
    u8 *EndOfData = OpStream + ByteLength;
    while(IP < EndOfData)
    {
        op Op = {0, IP, 0};

        int OpTableLength = ArrayLength(OpTable);
        int OpTableIndex;
        int Found = 0;
        for(OpTableIndex = 0;
            OpTableIndex < OpTableLength;
            ++OpTableIndex)
        {
            op_definition *OpDefinition = OpTable + OpTableIndex;
            if((IP[0] & OpDefinition->PrefixMask) == OpDefinition->Prefix)
            {
                if(OpDefinition->UseExtension)
                {
                    if(OpDefinition->UseExtension == 1)
                    {
                        int OpCodeExtension = ((IP[1] & 0b00111000) >> 3);
                        if(OpCodeExtension == OpDefinition->Extension)
                        {
                            Op = OpDefinition->Decode(IP, OpDefinition->DecodeOptions);
                            Found = 1;
                            break;
                        }
                    }
                    else if(OpDefinition->UseExtension == 2)
                    {
                        if(IP[1] == OpDefinition->Extension)
                        {
                            Op = OpDefinition->Decode(IP, OpDefinition->DecodeOptions);
                            Found = 1;
                            break;
                        }
                    }
                }
                else
                {
                    Op = OpDefinition->Decode(IP, OpDefinition->DecodeOptions);
                    Found = 1;
                    break;
                }
            }
        }
        if(!Found)
        {
            Op.Error = 1;
            Op.ByteLength = 1;
        }

        OpList[OpCount++] = Op;

        Assert(Op.ByteLength);
        IP += Op.ByteLength;
    }

    // TODO(chuck): Rethink the jump targetting. This seems too complicated?
    jump JumpTargets[100] = {0};
    int JumpCount = 0;

    for(int MaybeJumpIndex = 0;
        MaybeJumpIndex < OpCount;
        ++MaybeJumpIndex)
    {
        op *MaybeJump = &OpList[MaybeJumpIndex];
        if(MaybeJump->IsRelativeJump)
        {
            __int64 InstructionOffset = (MaybeJump->IP - OpStream);
            __int64 JumpBase = (InstructionOffset + MaybeJump->ByteLength);
            __int64 JumpTo = JumpBase + MaybeJump->Param[0].ImmediateValue;

            for(int MaybeJumpTargetIndex = 0;
                MaybeJumpTargetIndex < OpCount;
                ++MaybeJumpTargetIndex)
            {
                op *MaybeJumpTarget = &OpList[MaybeJumpTargetIndex];
                __int64 CurrentOffset = MaybeJumpTarget->IP - OpStream;
                if(JumpTo == CurrentOffset)
                {
                    // NOTE(chuck): Only add if this is a unique target. Don't assign the JumpTargetIndex yet because the target may not be the first label lexically.
                    if(!MaybeJumpTarget->IsJumpTarget)
                    {
                        MaybeJumpTarget->IsJumpTarget = 1;
                        JumpTargets[JumpCount++] = (jump){MaybeJumpTarget, CurrentOffset};
                    }
                    break;
                }
            }
        }
    }

    qsort(JumpTargets, JumpCount, sizeof(jump), CompareJumps);

    int LabelCount = 0;
    for(int OpIndex = 0;
        OpIndex < OpCount;
        ++OpIndex)
    {
        op *Op = &OpList[OpIndex];
        __int64 CurrentOffset = (Op->IP - OpStream);

        if(Op->IsJumpTarget)
        {
            printf("\nlabel%d:\n", LabelCount++);
        }
        
        if(Op->Error)
        {
            HitError = 1; // NOTE(chuck): Trip this so that all subsequent output is commented. I just want to see it continue attempting to decode the rest of the stream, even if most of it is garbage, in case it is useful for debugging.

            char OpBytes[32] = {0};
            sprintf(OpBytes, "%02X", Op->IP[0]);
            printf("%-40s  0x%08llX: %s\n", "; ???", (Op->IP - OpStream), OpBytes);
            Result = 1;
        }
        else
        {
            char Line[1024] = {0};
            char *LinePointer = Line;

            // NOTE(chuck): Copy the segment override into the next op so that PrintParam() knows what to do.
            if(Op->NameIndex == SEGMENT_OVERRIDE)
            {
                OpList[OpIndex + 1].UseSegmentOverride = 1;
                OpList[OpIndex + 1].SegmentOverride = Op->SegmentOverride;
            }

            if(Op->NameIndex >= ArrayLength(OpNameLookup))
            {
                LinePointer += sprintf(LinePointer, "  <corrupted?> ");
            }
            else
            {
                // TODO(chuck): This is fudgeville!
                if(OpIndex > 0 && !IsPrefix(&OpList[OpIndex - 1]))
                {
                    LinePointer += sprintf(LinePointer, "  ");
                }

                LinePointer += sprintf(LinePointer, "%s", OpNameLookup[Op->NameIndex]);

                // TODO(chuck): This is fudgeville!
                if((Op->NameIndex == OP_NAME_MOVS) ||
                   (Op->NameIndex == OP_NAME_CMPS) ||
                   (Op->NameIndex == OP_NAME_SCAS) ||
                   (Op->NameIndex == OP_NAME_LODS) ||
                   (Op->NameIndex == OP_NAME_STOS))
                {
                    LinePointer += sprintf(LinePointer, "%c", Op->Word ? 'w' : 'b');
                }

                if(Op->NameIndex != SEGMENT_OVERRIDE)
                {
                    LinePointer += sprintf(LinePointer, " ");
                }
            }

            if(Op->ParamCount > 0)
            {
                if(Op->IsRelativeJump)
                {
#if 0
                    LinePointer += sprintf(LinePointer, "$+0");
                    if(Op->Param[0].ImmediateValue >= 0)
                    {
                        LinePointer += sprintf(LinePointer, "+");
                    }
                    LinePointer += sprintf(LinePointer, "%d", Op->Param[0].ImmediateValue + 2);
#else
                    __int64 InstructionOffset = (Op->IP - OpStream);
                    __int64 JumpBase = (InstructionOffset + Op->ByteLength);
                    __int64 JumpTo = JumpBase + Op->Param[0].ImmediateValue;

                    int JumpTargetIndex;
                    for(JumpTargetIndex = 0;
                        JumpTargetIndex < JumpCount;
                        ++JumpTargetIndex)
                    {
                        jump *JumpTarget = &JumpTargets[JumpTargetIndex];
                        if(JumpTo == JumpTarget->Offset)
                        {
                            LinePointer += sprintf(LinePointer, "label%d", JumpTargetIndex);
                            break;
                        }
                    }
                    if(JumpTargetIndex == JumpCount)
                    {
                        HitError = 1;
                    }
#endif
                }
                else
                {
                    // NOTE(chuck): Don't bother with tracking which side. Just blast it on the left.
                    if(Op->EmitSize)
                    {
                        LinePointer += sprintf(LinePointer, Op->Word ? "word " : "byte ");
                    }
                    LinePointer += PrintParam(LinePointer, Op, &Op->Param[0]);
                }
            }

            if(Op->ParamCount > 1)
            {
                LinePointer += sprintf(LinePointer, ", ");
                LinePointer += PrintParam(LinePointer, Op, &Op->Param[1]);
            }

            if(HitError)
            {
                Line[0] = ';';
            }

            if(IsPrefix(Op))
            {
                printf("%s", Line);
            }
            else
            {
                // TODO(chuck): This is fudgeville!
                if((OpIndex > 0) && IsPrefix(&OpList[OpIndex - 1]))
                {
                    printf("%-34s", Line);
                }
                else
                {
                    printf("%-40s", Line);
                }
            }

            if(!IsPrefix(Op))
            {
                char OpBytes[1024] = {0};
                char *OpBytesPointer = OpBytes;
                unsigned char *IP = Op->IP;
                int ByteLength = Op->ByteLength;

                // TODO(chuck): This is fudgeville!
                if((OpIndex > 0) && IsPrefix(&OpList[OpIndex - 1]))
                {
                    IP = OpList[OpIndex - 1].IP;
                    ++ByteLength;
                }

                for(int OpByteIndex = 0;
                    OpByteIndex < ByteLength;
                    ++OpByteIndex)
                {
                    int BytesWritten = sprintf(OpBytesPointer, " %02X", IP[OpByteIndex]);
                    OpBytesPointer += BytesWritten;
                    __int64 EmitLength = (OpBytesPointer - OpBytes);
                    int OpBytesLength = ArrayLength(OpBytes);
                    Assert(EmitLength < OpBytesLength);
                }
                printf("; 0x%08llX:%s\n", (IP - OpStream), OpBytes);
            }
        }
    }

    return(Result);
}
