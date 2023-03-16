#include <stdio.h>

#define Assert(Condition) if(!Condition) { __debugbreak(); }

#define MOV_REGISTER_TO_REGISTER  0x88 // 0b100010 00
#define MOV_IMMEDIATE_TO_REGISTER 0xb0 // 0b1011 0 000

#define MEMORY_MODE_MAYBE_NO_DISPLACEMENT 0x00
#define MEMORY_MODE_8BIT_DISPLACEMENT     0x01
#define MEMORY_MODE_16BIT_DISPLACEMENT    0x02
#define REGISTER_MODE_NO_DISPLACEMENT     0x03

static char *RegisterLookup[16] =
{
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
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

static void PrintRegister(int RegisterCode, int OperatesOnWordData)
{
    char *RegisterName = RegisterLookup[(OperatesOnWordData * 8) + RegisterCode];
    printf("%s", RegisterName);
}

static void PrintEffectiveAddress(int Register, int DisplacementValue)
{
    printf("[%s", EffectiveAddressLookup[Register]);
    if(DisplacementValue)
    {
        printf(" + %d", DisplacementValue);
    }
    printf("]");
}

int main(int ArgCount, char **Args)
{
    int Result = 0;

    unsigned char Data[1024] = {0};

    char *Filename = Args[1];
    FILE *File = fopen(Filename, "rb");
    size_t ByteLength = fread(Data, 1, 1024, File);
    
    printf("bits 16\n");

    unsigned char *InstructionPointer = Data;
    unsigned char *EndOfData = Data + ByteLength;
    while(InstructionPointer < EndOfData)
    {
        if((InstructionPointer[0] & 0xfc) == MOV_REGISTER_TO_REGISTER)
        {
            int DirectionIsDestination = InstructionPointer[0] & 0x02; // D
            int OperatesOnWordData = InstructionPointer[0] & 0x01; // W
            int Mode = InstructionPointer[1] >> 6; // MOD
            int Register = (InstructionPointer[1] & 0x38) >> 3; // REG
            int RegisterOrMemoryMode = InstructionPointer[1] & 0x07; // R/M
            if(Mode == REGISTER_MODE_NO_DISPLACEMENT)
            {
                printf("mov ");
                PrintRegister(RegisterOrMemoryMode, OperatesOnWordData);
                printf(", ");
                PrintRegister(Register, OperatesOnWordData);
                printf("\n");

                InstructionPointer += 2;
            }
            else if(Mode == MEMORY_MODE_MAYBE_NO_DISPLACEMENT)
            {
                if(RegisterOrMemoryMode != 0x06)
                {
                    printf("mov ");
                    if(DirectionIsDestination)
                    {
                        PrintRegister(Register, OperatesOnWordData);
                        printf(", ");
                        PrintEffectiveAddress(RegisterOrMemoryMode, 0);
                    }
                    else
                    {
                        PrintEffectiveAddress(RegisterOrMemoryMode, 0);
                        printf(", ");
                        PrintRegister(Register, OperatesOnWordData);
                    }
                    printf("\n");

                    InstructionPointer += 2;
                }
                else
                {
                    printf("Direct address mode is not yet supported.\n");
                    Result = 3;

                    InstructionPointer += 2;
                }
            }
            else if((Mode == MEMORY_MODE_8BIT_DISPLACEMENT) || (Mode == MEMORY_MODE_16BIT_DISPLACEMENT))
            {
                printf("mov ");

                if(DirectionIsDestination)
                {
                    PrintRegister(Register, OperatesOnWordData);
                    printf(", ");

                    int DisplacementValue = 0;
                    if(Mode == MEMORY_MODE_16BIT_DISPLACEMENT)
                    {
                        DisplacementValue = *(unsigned short *)&InstructionPointer[2];
                        InstructionPointer += 4;
                    }
                    else
                    {
                        DisplacementValue = InstructionPointer[2];
                        InstructionPointer += 3;
                    }

                    PrintEffectiveAddress(RegisterOrMemoryMode, DisplacementValue);
                }
                else
                {
                    int DisplacementValue = 0;
                    if(Mode == MEMORY_MODE_16BIT_DISPLACEMENT)
                    {
                        DisplacementValue = *(unsigned short *)&InstructionPointer[2];
                        InstructionPointer += 4;
                    }
                    else
                    {
                        DisplacementValue = InstructionPointer[2];
                        InstructionPointer += 3;
                    }

                    PrintEffectiveAddress(RegisterOrMemoryMode, DisplacementValue);
                    printf(", ");
                    PrintRegister(Register, OperatesOnWordData);
                }

                printf("\n");
            }
            else
            {
                printf("Unsupported MOV register mode: 0x%x\n", Mode);
                Result = 2;

                InstructionPointer += 2;
            }
        }
        else if((InstructionPointer[0] & 0xf0) == MOV_IMMEDIATE_TO_REGISTER)
        {
            int OperatesOnWordData = (InstructionPointer[0] & 0x08) == 0x08; // W
            int Register = InstructionPointer[0] & 0x07; // REG

            printf("mov ");
            PrintRegister(Register, OperatesOnWordData);
            printf(", ");

            int Value = 0;
            if(OperatesOnWordData)
            {
                Value = *(unsigned short *)&InstructionPointer[1];
                InstructionPointer += 3;
            }
            else
            {
                Value = InstructionPointer[1];
                InstructionPointer += 2;
            }

            printf("%d\n", Value);
        }
        else
        {
            printf("Unrecognized op-code prefix in byte: 0x%02x\n", InstructionPointer[0]);
            Result = 1;

            ++InstructionPointer;
        }
    }

    return(Result);
}
