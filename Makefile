PROGRAM = switch_audio
SOURCE = main.c
CC = clang

# Optimization flags
CFLAGS = -O2 -flto -march=native -Wall -Wextra
FRAMEWORKS = -framework CoreAudio -framework CoreFoundation

# Default target
$(PROGRAM): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) $(FRAMEWORKS) -o $(PROGRAM)

# Clean build artifacts
clean:
	rm -f $(PROGRAM)

# Install to /usr/local/bin
install: $(PROGRAM)
	cp $(PROGRAM) /usr/local/bin/

# Uninstall from /usr/local/bin
uninstall:
	rm -f /usr/local/bin/$(PROGRAM)

# Debug build
debug: CFLAGS = -g -O0 -Wall -Wextra -DDEBUG
debug: $(PROGRAM)

# Release build (same as default but explicit)
release: CFLAGS = -O2 -flto -march=native -Wall -Wextra -DNDEBUG
release: $(PROGRAM)

.PHONY: clean install uninstall debug release
