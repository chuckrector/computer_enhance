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
} param_type;

typedef struct
{
    param_type Type;
    int RegisterOrMemoryIndex;
    int ImmediateValue;
    int Offset; // NOTE(chuck): Displacement value or direct address.
    int ByteSizeQualifier; // NOTE(chuck): Prefixes: 0=, 1=byte, 2=word
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
    };
    int Word;
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
} op;

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
};

// NOTE(chuck): Annoyingly, you cannot pass a struct literal as a function argument without casting it. So use a relativelyt short name here and stuff all possible options for all functions into this.
typedef struct
{
    int SignExtend;
    int NameIndex;
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
    Op->Param[ParamIndex].Type = Param_Immediate;
    if((Options.SignExtend == 0) && Op->Word)
    {
        Op->Param[ParamIndex].ImmediateValue = *(u16 *)&IP[0];
        if(Options.SignExtend)
        {
            if(Op->Param[ParamIndex].ImmediateValue & 0x80)
            {
                Op->Param[ParamIndex].ImmediateValue |= 0xff00;
            }
            else
            {
                Op->Param[ParamIndex].ImmediateValue &= ~0xff00;
            }
        }

        if(Op->EmitSize)
        {
            Op->Param[ParamIndex].ByteSizeQualifier = 2; // NOTE(chuck): "word" prefix
        }
    }
    else
    {
        Op->Param[ParamIndex].ImmediateValue = (s8)IP[0];
        if(Op->EmitSize)
        {
            Op->Param[ParamIndex].ByteSizeQualifier = 1; // NOTE(chuck): "byte" prefix
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
    Op->EmitSize = 1;
    if(Op->Mode == REGISTER_MODE_NO_DISPLACEMENT)
    {
        SetParamToReg(IP + 2, Op, DESTINATION, GetRegisterIndex(Op->RegB, Op->Word));
        SetParamToImm(IP + 2, Op, SOURCE, Options);
        Op->ByteLength = Op->Word ? 3 : 2;
    }
    else if(Op->Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
    {
        if(Op->RegB != 0x06)
        {
            SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 0, 0, 0);
            SetParamToImm(IP + 2, Op, SOURCE, Options);
            Op->ByteLength = ((Options.SignExtend == 0) && Op->Word) ? 4 : 3;
        }
        else
        {
            SetParamToMemDirectAddress(IP + 2, Op, DESTINATION, Op->Word);
            SetParamToImm(IP + 4, Op, SOURCE, Options);
            Op->ByteLength = ((Options.SignExtend == 0) && Op->Word) ? 6 : 5;
        }
    }
    else if((Op->Mode == MEMORY_MODE_8BIT_DISPLACEMENT) ||
            (Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
    {
        SetParamToMem(IP + 2, Op, DESTINATION, Op->RegB, 1,
                      Op->Mode == MEMORY_MODE_16BIT_DISPLACEMENT, 1);
        SetParamToImm(IP + 4, Op, SOURCE, Options);
        Op->ByteLength = ((Options.SignExtend == 0) && Op->Word) ? 6 : 5;
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
    op Op = {DecodeOptions.NameIndex, IP, 2, 0};
    SetOpConfig(&Op);
    SharedRegisterOrMemoryVsRegister(IP, &Op);
    return(Op);
}

// ::: 1 0 0 0 0 0 s w | mod 0 0 0  r/m  | (DISP-LO) | (DISP-HI) | data | data if w=1 |
static op AddSubCmp_ImmediateWithRegisterOrMemory(u8 *IP, options DecodeOptions)
{
    op Op = {DecodeOptions.NameIndex, IP, 2, 0};
    SetOpConfig(&Op);
    SharedImmediateToRegisterOrMemory(IP, &Op, (options){.SignExtend = 1});
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

static op_definition OpTable[] =
{
    // TODO(chuck): There is order-dependence here! AddSubCmp_ImmediateWithRegisterOrMemory will fire first if XCHG is listed after it, which is no bueno. I assume this means that I have to order all these encodings by largest prefix and descending, then misc. splotchy masks. Hopefully that works for everything? There's probably a better way to handle this then the prefix masking, such that there is no ambiguity related to ordering.
    // TODO(chuck): Table 4-14. Machine Instruction Encoding Matrix looks interesting but I don't understand how to read it.
    {OP_NAME_XCHG,   0b11111110, 0b10000110, 0, 0, AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_XCHG}},

    {OP_NAME_MOV,    0b11111100, 0b10001000, 0, 0, MovRegisterOrMemoryToOrFromRegister, {0}},
    {OP_NAME_MOV,    0b11110000, 0b10110000, 0, 0, MovImmediateToRegister, {0}},
    {OP_NAME_MOV,    0b11111110, 0b11000110, 0, 0, MovImmediateToRegisterOrMemory, {0}},
    {OP_NAME_MOV,    0b11111110, 0b10100000, 0, 0, MovMemoryToAccumulator, {0}},
    {OP_NAME_MOV,    0b11111110, 0b10100010, 0, 0, MovAccumulatorToMemory, {0}},

    {OP_NAME_ADD,    0b11111100, 0b00000000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_ADD}},
    {OP_NAME_ADD,    0b11111000, 0b10000000, 1, 0b000, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_ADD}},
    {OP_NAME_ADD,    0b11111110, 0b00000100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_ADD}},

    {OP_NAME_SUB,    0b11111100, 0b00101000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_SUB}},
    {OP_NAME_SUB,    0b11111000, 0b10000000, 1, 0b101, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_SUB}},
    {OP_NAME_SUB,    0b11111110, 0b00101100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_SUB}},

    {OP_NAME_CMP,    0b11111100, 0b00111000, 0, 0,     AddSubCmp_RegisterOrMemoryWithRegisterToEither, {.NameIndex=OP_NAME_CMP}},
    {OP_NAME_CMP,    0b11111000, 0b10000000, 1, 0b111, AddSubCmp_ImmediateWithRegisterOrMemory,        {.NameIndex=OP_NAME_CMP}},
    {OP_NAME_CMP,    0b11111110, 0b00111100, 0, 0,     AddSubCmp_ImmediateWithAccumulator,             {.NameIndex=OP_NAME_CMP}},

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

    {OP_NAME_PUSH,   0b11111111, 0b11111111, 1, 0b110, PushPop_RegisterOrMemory, {.NameIndex=OP_NAME_PUSH}},
    {OP_NAME_PUSH,   0b11111000, 0b01010000, 0, 0,     PushPop_Register,         {.NameIndex=OP_NAME_PUSH}},
    {OP_NAME_PUSH,   0b11100111, 0b00000110, 0, 0,     PushPop_SegmentRegister,  {.NameIndex=OP_NAME_PUSH}},

    {OP_NAME_POP,    0b11111111, 0b10001111, 1, 0b000, PushPop_RegisterOrMemory, {.NameIndex=OP_NAME_POP}},
    {OP_NAME_POP,    0b11111000, 0b01011000, 0, 0,     PushPop_Register,         {.NameIndex=OP_NAME_POP}},
    {OP_NAME_POP,    0b11100111, 0b00000111, 0, 0,     PushPop_SegmentRegister,  {.NameIndex=OP_NAME_POP}},
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

static int PrintParam(char *OutputInit, op_param *Param)
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
            EMIT("[%d]", Param->Offset);
        } break;

        default:
        {
            printf("Unknown parameter type: %d\n", Param->Type);
            if(Output != OutputInit)
            {
                printf("Partial output: %s\n", OutputInit);
            }
            ExitProcess(1);
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

    u8 OpStream[1024*4] = {0};
    op OpList[1024] = {0};
    int OpCount = 0;

    char *Filename = Args[1];
    FILE *File = fopen(Filename, "rb");
    size_t ByteLength = fread(OpStream, 1, 1024, File);

    printf("bits 16\n");

    u8 *IP = OpStream;
    u8 *EndOfData = OpStream + ByteLength;
    while(IP < EndOfData)
    {
        op Op = {0, IP, 0};

        int OpTableIndex;
        for(OpTableIndex = 0;
            OpTableIndex < ArrayLength(OpTable);
            ++OpTableIndex)
        {
            op_definition *OpDefinition = OpTable + OpTableIndex;
            if((*IP & OpDefinition->PrefixMask) == OpDefinition->Prefix)
            {
                if(OpDefinition->UseExtension)
                {
                    int OpCodeExtension = ((IP[1] & 0b00111000) >> 3);
                    if(OpCodeExtension == OpDefinition->Extension)
                    {
                        Op = OpDefinition->Decode(IP, OpDefinition->DecodeOptions);
                        break;
                    }
                }
                else
                {
                    Op = OpDefinition->Decode(IP, OpDefinition->DecodeOptions);
                    break;
                }
            }
        }
        if(OpTableIndex == ArrayLength(OpTable))
        {
            Op.Error = 1;
        }

        OpList[OpCount++] = Op;

        IP += Op.Error ? 1 : Op.ByteLength;
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
            printf("%-40s  0x%08llX: %-18s\n", "; ???", (Op->IP - OpStream), OpBytes);
            Result = 1;
        }
        else
        {
            char Line[1024] = {0};
            char *LinePointer = Line;

            LinePointer += sprintf(LinePointer, "  %s ", OpNameLookup[Op->NameIndex]);

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
                    LinePointer += PrintParam(LinePointer, &Op->Param[0]);
                }
            }

            if(Op->ParamCount > 1)
            {
                LinePointer += sprintf(LinePointer, ", ");
                LinePointer += PrintParam(LinePointer, &Op->Param[1]);
            }

            if(HitError)
            {
                Line[0] = ';';
            }
            printf("%-40s", Line);

            char OpBytes[1024] = {0};
            char *OpBytesPointer = OpBytes;
            for(int OpByteIndex = 0;
                OpByteIndex < Op->ByteLength;
                ++OpByteIndex)
            {
                int BytesWritten = sprintf(OpBytesPointer, "%02X ", Op->IP[OpByteIndex]);
                OpBytesPointer += BytesWritten;
                __int64 EmitLength = (OpBytesPointer - OpBytes);
                int OpBytesLength = ArrayLength(OpBytes);
                Assert(EmitLength < OpBytesLength);
            }
            printf("; 0x%08llX: %-32s\n", (Op->IP - OpStream), OpBytes);
        }
    }

    return(Result);
}
