@echo off

setlocal

if not exist build mkdir build
pushd build

set CompilerFlags=/FC /nologo /Od /WX /W4 /wd4100 /wd4101 /wd4201 /wd4189  /wd4232 /wd4456 /wd4457 /wd4701 /wd4702 /wd4996 /Zi

set LinkerFlags=/incremental:no /opt:ref

call cl %CompilerFlags% ..\sim8086.c /link %LinkerFlags%
call cl %CompilerFlags% ..\sim8086_v2.c /link %LinkerFlags%

for %%f in (..\perfaware\part1\*.asm) do (
    call nasm ..\perfaware\part1\%%~nf.asm -o %~dp0build\%%~nf
)

