@echo off
rem Builds a release: release.bat [name] (default GBADoom), from cmd or PowerShell.
rem Rebuilds the engine with build.bat, patches the shareware doom1.wad into it and
rem collects the ROMs and GbaWadUtil with what it needs at runtime into release\<name>,
rem then zips that folder to release\<name>.zip.
setlocal
cd /d "%~dp0"

set NAME=%~1
if "%NAME%"=="" set NAME=GBADoom
set OUT=%~dp0release\%NAME%
set UTIL=%~dp0GbaWadUtil

call "%~dp0build.bat"
if errorlevel 1 (
    echo release: build failed
    exit /b 1
)

if exist "%OUT%" rmdir /s /q "%OUT%"
if exist "%OUT%.zip" del "%OUT%.zip"
mkdir "%OUT%\GbaWadUtil"

rem The engine-only ROM goes next to the GbaWadUtil folder, where its window looks for it.
copy /y GBADoom.gba "%OUT%\" >nul || goto failed

"%UTIL%\GbaWadUtil.exe" -in "%UTIL%\doom1.wad" -rom GBADoom.gba -romout "%OUT%\GBADoom1.gba" -gus "%UTIL%\data\dgguspat\timidity.cfg" -oversample 4
if errorlevel 1 goto failed

copy /y README.md "%OUT%\" >nul || goto failed
copy /y CHANGELOG.md "%OUT%\" >nul || goto failed

for %%f in (GbaWadUtil.exe Qt6Core.dll Qt6Gui.dll Qt6Svg.dll Qt6Widgets.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll doom1.wad) do (
    copy /y "%UTIL%\%%f" "%OUT%\GbaWadUtil\" >nul || goto failed
)
xcopy /e /i /q /y "%UTIL%\platforms" "%OUT%\GbaWadUtil\platforms" >nul || goto failed
xcopy /e /i /q /y "%UTIL%\data" "%OUT%\GbaWadUtil\data" >nul || goto failed

"%SystemRoot%\System32\tar.exe" -a -c -f "%OUT%.zip" -C "%~dp0release" "%NAME%"
if errorlevel 1 goto failed

echo release: wrote %OUT% and %OUT%.zip
exit /b 0

:failed
echo release: failed
exit /b 1
