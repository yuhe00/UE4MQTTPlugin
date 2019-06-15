# MQTTPlugin

MQTT client plugin for UE4 that is based on [PahoMQTT](https://github.com/eclipse/paho.mqtt.c). Compatible with Android.

## Prerequisites

- Unreal Engine 4.21.2 or 4.22.2
- NVPACK for UE4 (`Engine\Extras\AndroidWorks\Win64\CodeWorksforAndroid-1R7u1-windows.exe`)

## Build

Remember to pull submodules! :)

```
git submodule update --init --recursive
```

### Windows

```
BuildWin64.bat
```

### Android

```
BuildAndroid.bat
```
