# Eclipse Paho MQTT C client

This is a modified version of the Paho MQTT C Client with modifications to make available as a third-party module library for UE4.

See the original [README.md](README.old.md).

## Build for Windows (UE4Editor)

This library needs to be compiled using CMake. See instructions below:

https://github.com/eclipse/paho.mqtt.c/issues/370

## Build for Android

```
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk
```