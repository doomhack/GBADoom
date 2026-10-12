@echo off
rem Builds the GBA target from cmd or PowerShell: build, build clean, build TARGET=foo.
rem devkitPro stays off the global PATH; its make and shell tools are added for this
rem build only. The devkitARM rules add the compiler and gbafix from DEVKITPRO.
setlocal
set DEVKITPRO=C:/devkitPro
set DEVKITARM=C:/devkitPro/devkitARM
set PATH=C:\devkitPro\msys2\usr\bin;%PATH%
cd /d "%~dp0"
make %*
