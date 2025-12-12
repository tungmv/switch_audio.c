// main.c
// Compile with: clang main.c -framework CoreAudio -framework CoreFoundation -o switch_audio

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static OSStatus getAudioDeviceList(AudioDeviceID **devices, UInt32 *deviceCount) {
    AudioObjectPropertyAddress propertyAddress = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    UInt32 size = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size);
    if (err != noErr) {
        return err;
    }

    *deviceCount = size / sizeof(AudioDeviceID);
    *devices = malloc(size);
    if (!*devices) {
        return kAudioHardwareBadDeviceError;
    }

    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, *devices);
    if (err != noErr) {
        free(*devices);
        *devices = NULL;
    }
    return err;
}

// Function declarations
static OSStatus setDefaultOutputDevice(AudioDeviceID deviceID);

static AudioDeviceID getCurrentDefaultOutputDevice() {
    AudioDeviceID deviceID;
    UInt32 size = sizeof(deviceID);
    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    return (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, &deviceID) == noErr) 
           ? deviceID : kAudioObjectUnknown;
}

static char* getDeviceName(AudioDeviceID deviceID) {
    CFStringRef deviceName = NULL;
    UInt32 size = sizeof(deviceName);
    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioObjectPropertyName,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    if (AudioObjectGetPropertyData(deviceID, &addr, 0, NULL, &size, &deviceName) != noErr || !deviceName) {
        return NULL;
    }

    // Get the actual required buffer size
    CFIndex nameLength = CFStringGetLength(deviceName);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(nameLength, kCFStringEncodingUTF8) + 1;

    char* nameBuf = malloc(maxSize);
    if (nameBuf && CFStringGetCString(deviceName, nameBuf, maxSize, kCFStringEncodingUTF8)) {
        CFRelease(deviceName);
        return nameBuf;
    }

    CFRelease(deviceName);
    free(nameBuf);
    return NULL;
}

static bool deviceSupportsOutput(AudioDeviceID deviceID) {
    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioDevicePropertyStreamConfiguration,
        .mScope = kAudioDevicePropertyScopeOutput,
        .mElement = kAudioObjectPropertyElementMain
    };

    UInt32 size = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(deviceID, &addr, 0, NULL, &size);
    if (err != noErr) {
        return false;
    }

    AudioBufferList* bufferList = (AudioBufferList*)malloc(size);
    if (!bufferList) {
        return false;
    }

    err = AudioObjectGetPropertyData(deviceID, &addr, 0, NULL, &size, bufferList);
    if (err != noErr) {
        free(bufferList);
        return false;
    }

    bool hasOutput = bufferList->mNumberBuffers > 0;
    for (UInt32 i = 0; i < bufferList->mNumberBuffers; i++) {
        if (bufferList->mBuffers[i].mNumberChannels == 0) {
            hasOutput = false;
            break;
        }
    }

    free(bufferList);
    return hasOutput;
}

static void listAudioDevices() {
    AudioDeviceID *devices;
    UInt32 deviceCount;

    if (getAudioDeviceList(&devices, &deviceCount) != noErr) {
        fprintf(stderr, "Error getting device list\n");
        return;
    }

    AudioDeviceID currentDefault = getCurrentDefaultOutputDevice();

    printf("Available Audio Output Devices:\n");
    printf("================================\n");

    for (UInt32 i = 0; i < deviceCount; ++i) {
        AudioDeviceID dev = devices[i];

        // Only show devices that support output
        if (!deviceSupportsOutput(dev)) {
            continue;
        }

        char* deviceName = getDeviceName(dev);
        if (deviceName) {
            if (dev == currentDefault) {
                printf("* %s\n", deviceName);
            } else {
                printf("  %s\n", deviceName);
            }
            free(deviceName);
        }
    }

    free(devices);
}

static void switchToNextDevice() {
    AudioDeviceID *devices;
    UInt32 deviceCount;

    if (getAudioDeviceList(&devices, &deviceCount) != noErr) {
        fprintf(stderr, "Error getting device list\n");
        return;
    }

    AudioDeviceID currentDefault = getCurrentDefaultOutputDevice();
    AudioDeviceID outputDevices[deviceCount]; // VLA for small arrays
    int outputCount = 0;
    int currentIndex = -1;

    // Single pass: build output array and find current device
    for (UInt32 i = 0; i < deviceCount; i++) {
        if (deviceSupportsOutput(devices[i])) {
            if (devices[i] == currentDefault) {
                currentIndex = outputCount;
            }
            outputDevices[outputCount++] = devices[i];
        }
    }

    free(devices);

    if (outputCount <= 1) {
        printf("Only one or no output devices available. Cannot switch.\n");
        return;
    }

    int nextIndex = (currentIndex + 1) % outputCount;
    AudioDeviceID nextDevice = outputDevices[nextIndex];

    char* currentName = getDeviceName(currentDefault);
    char* nextName = getDeviceName(nextDevice);

    if (setDefaultOutputDevice(nextDevice) == noErr) {
        printf("Switched from \"%s\" to \"%s\"\n",
               currentName ?: "Unknown", nextName ?: "Unknown");
    } else {
        fprintf(stderr, "Failed to set default output device\n");
    }

    free(currentName);
    free(nextName);
}

static OSStatus setDefaultOutputDevice(AudioDeviceID deviceID) {
    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };
    UInt32 size = sizeof(deviceID);
    return AudioObjectSetPropertyData(kAudioObjectSystemObject,
                                      &addr,
                                      0,
                                      NULL,
                                      size,
                                      &deviceID);
}

static AudioDeviceID findDeviceByName(const char* wantedName) {
    AudioDeviceID *devices;
    UInt32 deviceCount;

    if (getAudioDeviceList(&devices, &deviceCount) != noErr) {
        return kAudioObjectUnknown;
    }

    size_t wantedLen = strlen(wantedName);

    AudioDeviceID found = kAudioObjectUnknown;

    for (UInt32 i = 0; i < deviceCount && found == kAudioObjectUnknown; ++i) {
        AudioDeviceID dev = devices[i];

        // Only consider devices that support output
        if (!deviceSupportsOutput(dev)) {
            continue;
        }

        char* deviceName = getDeviceName(dev);
        if (deviceName) {
            // Compare with wanted name using length check first
            if (strlen(deviceName) == wantedLen && memcmp(deviceName, wantedName, wantedLen) == 0) {
                found = dev;
            }
            free(deviceName);
        }
    }

    free(devices);
    return found;
}



static void printUsage(const char* progName) {
    printf("Usage: %s [OPTIONS] [DEVICE_NAME]\n\n", progName);
    printf("Switch macOS default audio output device\n\n");
    printf("Options:\n");
    printf("  -l, --list    List available audio output devices\n");
    printf("  -n, --next    Switch to next available device\n");
    printf("  -h, --help    Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s -l                          # List available devices\n", progName);
    printf("  %s \"External Headphones\"      # Switch to headphones\n", progName);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Handle list option
    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0) {
        listAudioDevices();
        return 0;
    }

    // Handle next device option
    if (strcmp(argv[1], "-n") == 0 || strcmp(argv[1], "--next") == 0) {
        switchToNextDevice();
        return 0;
    }

    // Handle help option
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printUsage(argv[0]);
        return 0;
    }

    // Handle device switching
    if (argc != 2) {
        fprintf(stderr, "Error: Please provide exactly one device name.\n\n");
        printUsage(argv[0]);
        return 1;
    }

    const char* deviceName = argv[1];

    AudioDeviceID dev = findDeviceByName(deviceName);
    if (dev == kAudioObjectUnknown) {
        fprintf(stderr, "Device \"%s\" not found.\n", deviceName);
        fprintf(stderr, "Use '%s -l' to list available devices.\n", argv[0]);
        return 1;
    }

    OSStatus err = setDefaultOutputDevice(dev);
    if (err != noErr) {
        fprintf(stderr, "Failed to set default output device: %d\n", (int)err);
        return 1;
    }

    printf("Switched default output to \"%s\".\n", deviceName);
    return 0;
}
