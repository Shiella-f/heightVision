@echo off
chcp 936>nul
cmake --build build --config Release
if errorlevel 1 exit /b %ERRORLEVEL%

call "%~dp0sync_heightvision_artifacts.bat" Release
if errorlevel 1 exit /b %ERRORLEVEL%
