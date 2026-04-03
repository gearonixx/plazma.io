.PHONY: all env build clean quick fresh format

PROJECT = plazma
BUILD_DIR = build

all: clean build

run:
	env -u QT_QPA_PLATFORMTHEME \
	    -u QT_STYLE_OVERRIDE \
	    -u QT_QUICK_CONTROLS_STYLE ./build/plazma

build:
	mkdir -p $(BUILD_DIR)
	cmake -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)
	cmake --build $(BUILD_DIR) -j$$(nproc)

quick:
	cmake --build $(BUILD_DIR) -j$$(nproc)

q: quick run

clean:
	rm -rf $(BUILD_DIR)

fresh:
	rm -rf tdlib

format:
	find . -name "*.c" -o -name "*.h" -o -name "*.cpp" | xargs -P$$(nproc) clang-format -i