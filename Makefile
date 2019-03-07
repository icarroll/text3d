text3d: text3d.cc libreactphysics3d.a libtess2.a
	g++ -Wall -g text3d.cc -I/usr/include/SDL2 -I/usr/include/freetype2 -Ireactphysics3d/src -Ilibtess2/Include -Lreactphysics3d/build/lib -Llibtess2/Build -lSDL2 -lGLEW -lOpenGL -lfreetype -lreactphysics3d -ltess2 -llua -o text3d

libreactphysics3d.a:
	cd reactphysics3d ; mkdir -p build ; cd build ; cmake .. ; cmake --build .

libtess2.a:
	cd libtess2 ; premake4 gmake ; cd Build ; make -f tess2.make
