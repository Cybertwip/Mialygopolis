# Mialygopolis 3D - preparación para una futura compilación Emscripten.
#
# Este archivo NO se incluye desde el CMakeLists.txt actual y no crea objetivos.
# Cuando se active la versión web deberá:
#   1. sustituir SDL/OpenGL nativo por SDL2/WebGL2;
#   2. convertir el bucle principal a emscripten_set_main_loop_arg();
#   3. enlazar source/bindings.cpp con embind o cwrap;
#   4. reemplazar el cliente TCP por WebSocket;
#   5. precargar build/generated/assets y las texturas elegidas;
#   6. mantener el servidor Go como servicio externo.

set(M3D_EMSCRIPTEN_TARGET_NAME "Mialygopolis3DWeb")
set(M3D_EMSCRIPTEN_MEMORY_MB 512)
set(M3D_EMSCRIPTEN_FILESYSTEM "preload")
set(M3D_EMSCRIPTEN_NETWORK_TRANSPORT "websocket")
set(M3D_EMSCRIPTEN_WEBGL_VERSION 2)
set(M3D_EMSCRIPTEN_PTHREADS OFF)
set(M3D_EMSCRIPTEN_BUILD_ENABLED OFF)
