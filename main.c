// main.c
// Compile with: clang main.c -framework CoreAudio -framework CoreFoundation -o switch_audio

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Function declarations
static OSStatus setDefaultOutputDevice(AudioDeviceID deviceID);

static AudioDeviceID getCurrentDefaultOutputDevice() {
    AudioDeviceID deviceID;
    UInt32 size = sizeof(deviceID);
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, &deviceID);
    if (err != noErr) {
        return kAudioObjectUnknown;
    }
    return deviceID;
}

static char* getDeviceName(AudioDeviceID deviceID) {
    CFStringRef deviceName = NULL;
    UInt32 size = sizeof(deviceName);
    AudioObjectPropertyAddress addr = {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus err = AudioObjectGetPropertyData(deviceID, &addr, 0, NULL, &size, &deviceName);
    if (err != noErr || deviceName == NULL) {
        return NULL;
    }

    char* nameBuf = malloc(256);
    if (!nameBuf) {
        CFRelease(deviceName);
        return NULL;
    }

    if (CFStringGetCString(deviceName, nameBuf, 256, kCFStringEncodingUTF8)) {
        CFRelease(deviceName);
        return nameBuf;
    } else {
        CFRelease(deviceName);
        free(nameBuf);
        return NULL;
    }
}

static bool deviceSupportsOutput(AudioDeviceID deviceID) {
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
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
    UInt32 size = 0;
    OSStatus err;
    AudioObjectPropertyAddress propertyAddress;

    // Get size needed for device list
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;

    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size);
    if (err != noErr) {
        fprintf(stderr, "Error getting device list size: %d\n", (int)err);
        return;
    }

    UInt32 deviceCount = size / sizeof(AudioDeviceID);
    AudioDeviceID *devices = (AudioDeviceID*)malloc(size);
    if (!devices) {
        fprintf(stderr, "Error allocating memory for device list\n");
        return;
    }

    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, devices);
    if (err != noErr) {
        fprintf(stderr, "Error getting device list: %d\n", (int)err);
        free(devices);
        return;
    }

    AudioDeviceID currentDefault = getCurrentDefaultOutputDevice();

    printf("Available Audio Output Devices:\n");
    printf("================================\n");

    int outputDeviceCount = 0;
    for (UInt32 i = 0; i < deviceCount; ++i) {
        AudioDeviceID dev = devices[i];

        // Only show devices that support output
        if (!deviceSupportsOutput(dev)) {
            continue;
        }

        char* deviceName = getDeviceName(dev);
        if (deviceName) {
            outputDeviceCount++;
            if (dev == currentDefault) {
                printf("* %s (current default)\n", deviceName);
            } else {
                printf("  %s\n", deviceName);
            }
            free(deviceName);
        }
    }

    if (outputDeviceCount == 0) {
        printf("No output devices found.\n");
    } else {
        printf("\nFound %d output device(s).\n", outputDeviceCount);
        printf("* indicates current default device\n");
    }

    free(devices);
}

static void switchToNextDevice() {
    UInt32 size = 0;
    OSStatus err;
    AudioObjectPropertyAddress propertyAddress;

    // Get size needed for device list
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;

    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size);
    if (err != noErr) {
        fprintf(stderr, "Error getting device list size: %d\n", (int)err);
        return;
    }

    UInt32 deviceCount = size / sizeof(AudioDeviceID);
    AudioDeviceID *devices = (AudioDeviceID*)malloc(size);
    if (!devices) {
        fprintf(stderr, "Error allocating memory for device list\n");
        return;
    }

    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, devices);
    if (err != noErr) {
        fprintf(stderr, "Error getting device list: %d\n", (int)err);
        free(devices);
        return;
    }

    // Get current default device
    AudioDeviceID currentDefault = getCurrentDefaultOutputDevice();

    // Build array of output devices
    AudioDeviceID *outputDevices = malloc(deviceCount * sizeof(AudioDeviceID));
    int outputCount = 0;

    for (UInt32 i = 0; i < deviceCount; i++) {
        if (deviceSupportsOutput(devices[i])) {
            outputDevices[outputCount] = devices[i];
            outputCount++;
        }
    }

    if (outputCount <= 1) {
        printf("Only one or no output devices available. Cannot switch.\n");
        free(devices);
        free(outputDevices);
        return;
    }

    // Find current device in the output list and get next one
    int currentIndex = -1;
    for (int i = 0; i < outputCount; i++) {
        if (outputDevices[i] == currentDefault) {
            currentIndex = i;
            break;
        }
    }

    // Calculate next device index (wrap around to beginning if at end)
    int nextIndex = (currentIndex + 1) % outputCount;
    AudioDeviceID nextDevice = outputDevices[nextIndex];

    // Get device names for display
    char* currentName = getDeviceName(currentDefault);
    char* nextName = getDeviceName(nextDevice);

    // Switch to next device
    err = setDefaultOutputDevice(nextDevice);
    if (err != noErr) {
        fprintf(stderr, "Failed to set default output device: %d\n", (int)err);
    } else {
        printf("Switched from \"%s\" to \"%s\"\n",
               currentName ? currentName : "Unknown",
               nextName ? nextName : "Unknown");
    }

    if (currentName) free(currentName);
    if (nextName) free(nextName);
    free(devices);
    free(outputDevices);
}

static OSStatus setDefaultOutputDevice(AudioDeviceID deviceID) {
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
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
    UInt32 size = 0;
    OSStatus err;
    AudioObjectPropertyAddress propertyAddress;

    // Get size needed for device list using modern API
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;

    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size);
    if (err != noErr) {
        fprintf(stderr, "Error getting device list size: %d\n", (int)err);
        return kAudioObjectUnknown;
    }

    UInt32 deviceCount = size / sizeof(AudioDeviceID);
    AudioDeviceID *devices = (AudioDeviceID*)malloc(size);
    if (!devices) return kAudioObjectUnknown;

    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, devices);
    if (err != noErr) {
        fprintf(stderr, "Error getting device list: %d\n", (int)err);
        free(devices);
        return kAudioObjectUnknown;
    }

    AudioDeviceID found = kAudioObjectUnknown;

    for (UInt32 i = 0; i < deviceCount; ++i) {
        AudioDeviceID dev = devices[i];

        // Only consider devices that support output
        if (!deviceSupportsOutput(dev)) {
            continue;
        }

        char* deviceName = getDeviceName(dev);
        if (deviceName) {
            // Compare with wanted name
            if (strcmp(deviceName, wantedName) == 0) {
                found = dev;
                free(deviceName);
                break;
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
