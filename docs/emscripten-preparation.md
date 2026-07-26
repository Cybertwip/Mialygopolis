# Preparación para Emscripten (sin activar)

La compilación web **no está habilitada todavía**. La arquitectura ya separa los puntos que deberán adaptarse:

- `source/bindings.*`: API C estable para sesión, selección, personalización y posición corregida por física.
- `source/npc_system.*`: callback estable para inferencia externa.
- `source/character_catalog.*`: catálogo binario independiente del navegador.
- `source/tools/build_avatar_assets.py`: conversión previa de VRM/FBX, evitando parsers pesados dentro de WebAssembly.
- `source/multiplayer_client.*`: única pieza de red nativa que deberá reemplazarse por WebSocket.
- `source/app.cpp`: el bucle SDL deberá dividirse para `emscripten_set_main_loop_arg`.

`cmake/EmscriptenPreparation.cmake` documenta los valores previstos, pero no se incluye en la compilación nativa y no genera ningún objetivo web.
