# switch_audio

Switch macOS default audio output device from command line.

## Build
``` sh
make              # Build optimized version
sudo make install      # Install to /usr/local/bin
# make debug        # Build debug version
# make clean        # Clean build artifacts

```

## Usage
```
./switch_audio -l                    # list devices
./switch_audio -n                    # switch to next device
./switch_audio "Device Name"         # switch to specific device
