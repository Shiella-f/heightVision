@echo off
chcp 936>nul
cmake --build build --config Debug
if errorlevel 1 exit /b %ERRORLEVEL%

call "%~dp0sync_heightvision_artifacts.bat" Debug
if errorlevel 1 exit /b %ERRORLEVEL%
