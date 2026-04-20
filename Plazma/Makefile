.PHONY: all env build clean quick fresh format

PROJECT = plazma
BUILD_DIR = build

all: clean build

run:
	@# A prior plazma that survived (e.g. was left running in another terminal
	@# or died mid-abort before the mpv teardown fix landed) keeps the TDLib
	@# binlog fd-locked and silently breaks login on the next launch. Kill any
	@# existing instances of THIS build first. -x matches the exact name so we
	@# never hit something unrelated.
	@pkill -x plazma 2>/dev/null; \
	 for i in 1 2 3 4 5; do pgrep -x plazma >/dev/null || break; sleep 0.2; done; \
	 pgrep -x plazma >/dev/null && pkill -9 -x plazma 2>/dev/null || true
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