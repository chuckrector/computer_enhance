@echo off

setlocal

set CURRENT_DIRECTORY=%~dp0%
set NASM=%CURRENT_DIRECTORY%nasm
set PART1=%CURRENT_DIRECTORY%perfaware\part1

if not exist build mkdir build
pushd build

del sim8086.log

for %%f in (%PART1%\*.asm) do (
    if exist %%~nf.disassembled.asm del %%~nf.disassembled.asm

    echo. >> sim8086.log 2>&1
    echo call sim8086 %PART1%\%%~nf > %%~nf.disassembled.asm >> sim8086.log 2>&1
    call sim8086 %PART1%\%%~nf > %%~nf.disassembled.asm

    rem NOTE(chuck): "errorlevel 1" means 1 *or higher*. "errorlevel 0" isn't what you'd think.
    if errorlevel 1 (
        type %%~nf.disassembled.asm >> sim8086.log 2>&1
        echo FAIL [stage 1] disassemble %%~nf
    ) else (
        echo. >> sim8086.log 2>&1
        echo call %NASM% %%~nf.disassembled.asm -o %%~nf >> sim8086.log 2>&1
        call %NASM% %%~nf.disassembled.asm -o %%~nf >> sim8086.log 2>&1

        if errorlevel 1 (
            echo FAIL [stage 2] reassemble  %%~nf.disassembled.asm
        ) else (
            echo PASS [stage 2] reassemble  %%~nf.disassembled.asm
        )
    )
)

echo %CURRENT_DIRECTORY%sim8086.log
