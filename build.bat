@echo off
:: uMRC Build batch file for MinGW on Windows
::
:: Use with MinGW32 and/or MinGW64
::
:: Point path_to_x64_gcc and path_to_x86_gcc
:: to your actual gcc.exe paths.
::
::
set path_to_x86_gcc=C:\mingw32\bin\gcc.exe
set path_to_x64_gcc=C:\mingw64\bin\gcc.exe
::
::

for /F %%A in ('echo prompt $E ^| cmd') do set "ESC=%%A" 

set arc=x86
set gcc=%path_to_x86_gcc%
if not "%1"=="" (
   set arc=%1
   set gcc=%path_to_x64_gcc%
)

if not exist "bin" (
   md bin
)
if not exist "bin\screens" (
   md bin\screens
)
if not exist "bin\themes" (
   md bin\themes
)

echo Building %ESC%[1msetup%ESC%[0m...
cd setup
%gcc% setup.c ..\common\common.c -o setup.exe -fcommon
copy setup.exe ..\bin

cd ..

echo Building %ESC%[1mumrc-bridge%ESC%[0m...
cd umrc-bridge
%gcc% bridge.c ..\common\common.c -I ..\lib\include -L ..\lib\%arc% -o umrc-bridge.exe -lssl -lcrypto -lcrypt32 -lWs2_32 -lMswsock -lAdvApi32 -fcommon
copy umrc-bridge.exe ..\bin

cd ..

echo Building %ESC%[1mumrc-client%ESC%[0m...
cd umrc-client
%gcc% main.c func.c ..\common\common.c -o umrc-client -L ..\lib\odoors\win-%arc% -lODoorW -lWs2_32 -lMswsock -lAdvApi32 -fcommon
copy umrc-client.exe ..\bin

cd ..

echo Copying assets...
copy lib\odoors\win-%arc%\*.dll bin /Y
copy lib\%arc%\*.dll bin /Y
copy changelog.txt bin /Y
copy LICENSE bin /Y
copy README.md bin /Y
copy assets\win-%arc%\* bin /Y
copy assets\screens\* bin\screens /Y
copy assets\themes\* bin\themes /Y

echo All jobs done. Check the %ESC%[32;1mbin%ESC%[0m subfolder for the output files.

pause
