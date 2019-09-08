@echo off
mkdir ThirdParty\PahoMQTT\build
cd ThirdParty\PahoMQTT\build
cmake -G "Visual Studio 15 2019 Win64" -DCMAKE_INSTALL_PREFIX=install ..
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
call msbuild ALL_BUILD.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64
cd ..\..\..