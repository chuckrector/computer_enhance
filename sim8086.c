#include <stdio.h>

#define Assert(Condition) if(!Condition) { __debugbreak(); }
#define OPCODE_MOV_REG_TO_REG 0x88

int main(int ArgCount, char **Args)
{
    int Result = 0;

    unsigned char Data[1024] = {0};

    char *Filename = Args[1];
    FILE *File = fopen(Filename, "rb");
    size_t ByteLength = fread(Data, 1, 1024, File);

    unsigned char *InstructionPointer = Data;
    unsigned char *EndOfData = Data + ByteLength;
    for(;
        InstructionPointer < EndOfData;
        InstructionPointer += 2)
    {
        int OpCode = InstructionPointer[0] & 0xfc;
        int RegisterOperandIsDestination = InstructionPointer[0] & 0x02;
        int OperatesOnWordData = InstructionPointer[0] & 0x01;
        int RegisterMode = InstructionPointer[1] & 0xc0;
        int RegisterOperand = InstructionPointer[1] & 0x38;
        int RegisterMemory = InstructionPointer[1] & 0x07;

        if(OpCode == OPCODE_MOV_REG_TO_REG)
        {
            printf("mov ??, ??\n");
        }
        else
        {
            printf("Unrecognized op-code: %d\n", OpCode);
            Result = 1;
        }
    }

    return(Result);
}
