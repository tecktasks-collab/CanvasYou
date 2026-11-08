@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "%~dp0"
cl /nologo /EHsc /W3 /Fe:Window.build.exe scratch.cpp renderer.cpp manage_screens.cpp edit_view.cpp document.cpp history.cpp audio.cpp templates.cpp gpu_client.cpp store.cpp boot.cpp init.cpp window_main.cpp svg_icon.cpp sqlite3.obj user32.lib gdi32.lib gdiplus.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib winmm.lib mfplat.lib mf.lib mfuuid.lib dwmapi.lib /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b 1
copy /Y Window.build.exe Window.exe
exit /b 0
