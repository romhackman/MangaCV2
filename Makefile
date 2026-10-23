.PHONY: all configure build static clean rebuild check run help

BUILD_DIR = build
DIST_DIR  = dist
CXX_PATH  = /ucrt64/bin/g++.exe

all: configure build

configure:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX_PATH)

build:
	cmake --build $(BUILD_DIR) --parallel

# Un seul .exe, sans DLL (necessite les bibliotheques statiques MSYS2)
static:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX_PATH) -DSTATIC_LINK=ON
	cmake --build $(BUILD_DIR) --parallel

check:
	g++ --version
	cmake --version
	ninja --version
	pkg-config --modversion sdl2
	pkg-config --modversion SDL2_ttf
	pkg-config --modversion libcurl
	pkg-config --modversion libharu

clean:
	cmake -E rm -rf $(BUILD_DIR)

rebuild: clean all

run: all
	./$(DIST_DIR)/MangaCV2.exe

help:
	@echo "make           configure + compile (dist/MangaCV2.exe)"
	@echo "make static    exe autonome, sans DLL"
	@echo "make check     verifie les outils et bibliotheques"
	@echo "make clean | rebuild | run"
