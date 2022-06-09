# pbrAsteroid
Procedural 3D asteroid in OpenGL using physically based rendering and tesselation(based on https://github.com/Nadrin/PBR). Run exe from folder above data folder (data folder is necessary besides executable). Video can be found at https://youtu.be/XnucS9sqZQQ.

# Controls
WASD - forward, backward, strafe left/right

QE - upper/lower

ZC - camera tilt

# Build and run

; build
cd pbrAsteroid
mkdir build
cd build
cmake ..
cmake-gui #<optional>
cmake --build . --config Release --target pbrAsteroid --

; run
cd ..
build/pbrAsteroid.exe

Only fbx and obj support in assimp are required. Support for other formats can be turned off in the cmake-gui as well as ASSIMP_NO_EXPORT. Compiler options can be passed after -- in the build line (-jN for example).

# Known problems
I didn't have opportunity to debug this (still rather raw code) on many enough systems, therefore there is no guarantee that it will run everywhere. So this code is provided as is, with no warranty of any kind. I hope that this code can be source for new ideas.
