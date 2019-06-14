@echo off
cd ThirdParty\PahoMQTT
"%NDKROOT%\ndk-build" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk
cd ..\..