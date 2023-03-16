#include <stdio.h>

#define Assert(Condition) if(!Condition) { __debugbreak(); }
#define MOV_REG_TO_REG 0x88
#define REGISTER_MODE 0xc0

static char *RegisterLookup[16] =
{
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
};

static void PrintRegister(int RegisterCode, int OperatesOnWordData)
{
    char *RegisterName = RegisterLookup[(OperatesOnWordData * 8) + RegisterCode];
    printf("%s", RegisterName);
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
    for(;
        InstructionPointer < EndOfData;
        InstructionPointer += 2)
    {
        int OpCode = InstructionPointer[0] & 0xfc;
        int DirectionIsDestination = InstructionPointer[0] & 0x02;
        int OperatesOnWordData = InstructionPointer[0] & 0x01;
        int Mode = InstructionPointer[1] & 0xc0;
        int RegisterOne = (InstructionPointer[1] & 0x38) >> 3;
        int RegisterTwo = InstructionPointer[1] & 0x07;
        
        if(OpCode == MOV_REG_TO_REG)
        {
            if(Mode == REGISTER_MODE)
            {
                printf("mov ");
                PrintRegister(RegisterTwo, OperatesOnWordData);
                printf(", ");
                PrintRegister(RegisterOne, OperatesOnWordData);
                printf("\n");
            }
            else
            {
                printf("Unsupported MOV register mode: 0x%x\n", Mode);
                Result = 2;
            }
        }
        else
        {
            printf("Unrecognized op-code: %d\n", OpCode);
            Result = 1;
        }
    }

    return(Result);
}
