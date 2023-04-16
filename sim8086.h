typedef unsigned char u8;
typedef unsigned short u16;
typedef char s8;
typedef short s16;

#define Assert(Condition) if(!(Condition)) { __debugbreak(); }
#define ArrayLength(A) (sizeof(A) / sizeof((A)[0]))

#define MEMORY_MODE_MAYBE_NO_DISPLACEMENT 0x00
#define MEMORY_MODE_8BIT_DISPLACEMENT     0x01
#define MEMORY_MODE_16BIT_DISPLACEMENT    0x02
#define REGISTER_MODE_NO_DISPLACEMENT     0x03

#define FLAG_CARRY     (1 << 0)
#define FLAG_PARITY    (1 << 2)
#define FLAG_AUX_CARRY (1 << 4)
#define FLAG_ZERO      (1 << 6)
#define FLAG_SIGN      (1 << 7)
#define FLAG_TRAP      (1 << 8)
#define FLAG_INTERRUPT (1 << 9)
#define FLAG_DIRECTION (1 << 10)
#define FLAG_OVERFLOW  (1 << 11)

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
#define SEGMENT_REGISTER_NAME_CS 1
#define SEGMENT_REGISTER_NAME_SS 2
#define SEGMENT_REGISTER_NAME_DS 3
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
        int SegmentRegister;
    };
    int RegB;
    int EmitSize;
    int IsRelativeJump;
    int IsJumpTarget;
    int JumpTargetIndex; // NOTE(chuck): This is patched in on the second pass for label printing.

    int UseSegmentOverride;
    int SegmentOverride;
    char Suffix; // NOTE(chucK): For "retf"
    int IsFar; // NOTE(chuck): For "call far"
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
    char Suffix; // NOTE(chuck): For "retf"
    int IsFar; // NOTE(chuck): For "call far"
} options;

typedef struct
{
    u8 *OpStream;
    u8 *IP;
} parsing_context;

typedef struct
{
    int NameIndex;
    int PrefixMask;
    int Prefix;
    int UseExtension;
    int Extension;
    op (*Decode)(parsing_context *Context, options Options);
    options DecodeOptions;
} op_definition;

typedef struct
{
    op *Op;
    __int64 Offset;
} jump;

