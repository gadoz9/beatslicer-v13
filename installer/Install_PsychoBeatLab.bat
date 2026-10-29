@echo off
REM Installs the Psycho Beat Lab VST3 for all users (needs administrator rights)
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Please right-click this file and choose "Run as administrator".
  pause
  exit /b 1
)
set DEST=%CommonProgramFiles%\VST3
if not exist "%DEST%" mkdir "%DEST%"
REM v9: the plugin was renamed from Beat Slicer. The old file is removed so Cubase does not see the plugin twice.
if exist "%DEST%\BeatSlicer.vst3" rmdir /S /Q "%DEST%\BeatSlicer.vst3"
xcopy /E /I /Y "%~dp0PsychoBeatLab.vst3" "%DEST%\PsychoBeatLab.vst3"

REM v13: the presets folder. Every folder inside it is a library in the plugin's preset list; "User" holds your saved presets.
REM Your presets from before (Documents\BeatSlicer\Presets) are copied into User - nothing is deleted.
set DOCS=
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('MyDocuments')"`) do set DOCS=%%D
if "%DOCS%"=="" set DOCS=%USERPROFILE%\Documents
set PRESETS=%DOCS%\Psycho Beat Lab Presets
if not exist "%PRESETS%\User" mkdir "%PRESETS%\User"
if exist "%DOCS%\BeatSlicer\Presets\*.bspreset" xcopy /Y /D "%DOCS%\BeatSlicer\Presets\*.bspreset" "%PRESETS%\User\" >nul

echo.
echo Installed to %DEST%\PsychoBeatLab.vst3
echo Presets folder: %PRESETS%
echo Open Cubase and rescan plug-ins (Studio ^> Plug-in Manager ^> Update).
pause
