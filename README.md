# switch_audio

Switch macOS default audio output device from command line.

## Build
``` sh
clang main.c -framework CoreAudio -framework CoreFoundation -o switch_audio
```

## Usage
```
./switch_audio -l                    # list devices
./switch_audio -n                    # switch to next device
./switch_audio "Device Name"         # switch to specific device
