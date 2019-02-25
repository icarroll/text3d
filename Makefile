text3d: text3d.cc
	g++ -Wall -g -m32 text3d.cc -I/mingw32/include/SDL2 -I/mingw32/include/freetype2 -I../reactphysics3d/src -I../lua-5.3.5/src -I../libtess2/Include -L../reactphysics3d/build/lib -L../lua-5.3.5/src -L../libtess2/Build -L/mingw32/lib -Wl,-subsystem,windows -lmingw32 -lSDL2main -lSDL2 -lglew32 -lopengl32 -lfreetype -lreactphysics3d -ltess2 -llua -mwindows -o text3d.exe
