text3d: text3d.cc
	g++ -m32 text3d.cc -I/mingw32/include/SDL2 -I/mingw32/include/freetype2 -L/mingw32/lib -Wl,-subsystem,windows -lmingw32 -lSDL2main -lSDL2 -lglew32 -lopengl32 -lfreetype -mwindows -o text3d.exe
