@echo off
chcp 936>nul
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T v142 -DCMAKE_PREFIX_PATH="C:/Qt5.12.4/5.12.4/msvc2017_64/lib/cmake"
