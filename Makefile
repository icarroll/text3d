text3d: text3d.cc
	g++ -m32 text3d.cc -I/mingw32/include/SDL2 -I/mingw32/include/freetype2 -I../reactphysics3d/src -L../reactphysics3d/build/lib -L/mingw32/lib -Wl,-subsystem,windows -lmingw32 -lSDL2main -lSDL2 -lglew32 -lglu32 -lopengl32 -lfreetype -lreactphysics3d -mwindows -o text3d.exe
