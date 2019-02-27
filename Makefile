text3d: text3d.cc
	g++ -Wall -g text3d.cc -I/usr/include/SDL2 -I/usr/include/freetype2 -I../reactphysics3d/src -I../libtess2/Include -L../reactphysics3d/build/lib -L../libtess2/Build -lSDL2 -lGLEW -lOpenGL -lfreetype -lreactphysics3d -ltess2 -llua -o text3d
