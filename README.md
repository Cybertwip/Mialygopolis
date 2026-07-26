# Mialygopolis 3D

Prototipo nativo en C++20 que integra:

- **Cubiquity** para el mundo vóxel.
- **CharacterStudio y character-assets** para personajes VRM modulares.
- **Assimp** para convertir las animaciones FBX de caminar y correr.
- **SDL2 + OpenGL 3.3** para ventana, entrada y renderizado local.
- **Servidor Go TCP** para lobby, chat, roles y sincronización multijugador.
- **Dos mundos Cubiquity**: Ciudad Romana y Cielo Griego.

La aplicación está completamente en español y actualmente se enfoca en ejecución nativa. La capa C de `source/bindings.*` conserva una separación adecuada para una futura adaptación con Emscripten.

## Funciones incluidas

- Roster de **20 personajes iniciales** construidos sobre las dos familias VRM completas disponibles.
- Estudio de personajes basado directamente en los manifiestos de CharacterStudio:
  - cabello;
  - cuerpo;
  - ojos;
  - abrigos;
  - torso;
  - piernas;
  - calzado;
  - accesorios;
  - todas las variantes de textura, color, piel y ojos incluidas.
- Restricciones de prendas incompatibles tomadas de los manifiestos.
- Animaciones FBX enlazadas: caminar, correr, saludar, celebrar y bailar; salto cinemático adicional.
- Colliders de cápsula contra vóxeles, edificios, terreno, NPC y jugadores remotos, con deslizamiento por ejes.
- Movimiento relativo a la dirección de la cámara/mouse.
- Caché compartida de mallas y texturas, actualización incremental por categoría y precarga progresiva del roster.
- Lobby y chat con roles:
  - `admin` / administrador;
  - `moderator` / moderador;
  - `gamer` / jugador.
- Sincronización TCP de posición, orientación, velocidad, fase de animación y personaje seleccionado.
- Visualización de jugadores remotos dentro del mundo seleccionado.
- Ciudad Romana predeterminada con foro, templo, basílica, murallas, villas, fuente, estatuas, cipreses y acueducto.
- Cielo Griego con templos del Olimpo, jardines, laureles, cascadas, nubes de mármol, portales e islas flotantes.
- Ocho NPC iniciales —cuatro por mundo— con colliders, interacción por proximidad y textos provisionales.
- Callback nativo y contrato Go preparados para inferencia futura de diálogo; deshabilitados por defecto.
- Configuración preliminar de Emscripten documentada, sin habilitar todavía ningún objetivo web.
- En macOS, la entrada de texto/IME solo se activa al abrir el chat, evitando el menú de acentos al mantener una tecla de movimiento.

## Dependencias locales

En macOS con Homebrew:

```bash
brew install cmake ninja sdl2 assimp go
```

También se necesita Python 3 para convertir los activos durante la compilación.

## Compilar y ejecutar

La forma rápida:

```bash
./run-local.sh
```

Equivalente manual:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
./build/Mialygopolis3D

# Iniciar directamente en el segundo mundo
./build/Mialygopolis3D --world --greek-heaven
```

La primera compilación genera en `build/generated/assets` un catálogo nativo y todos los rasgos animados. Los árboles dentro de `3rdparty` no se modifican.

## Controles

### Selector

- `←` / `→`: cambiar personaje.
- `Enter`: abrir el estudio de personajes.
- Doble clic: abrir el estudio.

### Estudio de personajes

- `↑` / `↓`: cambiar categoría.
- `←` / `→`: cambiar modelo o prenda.
- `[` / `]`: cambiar textura o color.
- `R`: combinación aleatoria.
- `Enter`: entrar al lobby.
- `Retroceso` o `Esc`: volver al roster.

### Lobby y chat

- `←` / `→`: elegir entre **Ciudad Romana** y **Cielo Griego**.
- `T`: abrir el chat.
- `Enter`: enviar el mensaje cuando el chat está activo; de lo contrario, entrar al mundo.
- `Esc`: volver al estudio.

### Mundo

- `WASD` o flechas: movimiento relativo a la dirección de la cámara.
- `Shift`: correr.
- Botón derecho + mouse: orientar la cámara.
- Rueda: zoom.
- `1`: saludar.
- `2`: celebrar.
- `3`: bailar.
- `Espacio`: saltar.
- `E`: hablar con un NPC cercano o pedir otra respuesta provisional.
- `T`: chat.
- `Esc`: cerrar el diálogo o regresar al lobby.

## Servidor multijugador

Inicia el servidor local:

```bash
./run-server.sh
```

Luego abre uno o más clientes:

```bash
./run-local.sh --lobby --name Aera
./run-local.sh --lobby --name Moderador --token moderator
./run-local.sh --lobby --name Administrador --token admin
```

O inicia servidor y cliente juntos:

```bash
./run-multiplayer.sh
```

El servidor escucha de forma predeterminada en `127.0.0.1:7777`. Para usar otra dirección:

```bash
./run-server.sh -listen 0.0.0.0:7777
./run-local.sh --server 192.168.1.10:7777 --name Jugador2
```

Los tokens predeterminados del prototipo son `admin` y `moderator`. Se pueden cambiar:

```bash
./run-server.sh -admin-token MI_TOKEN_ADMIN -moderator-token MI_TOKEN_MOD
```

Comandos del chat:

- `/help`
- `/kick nombre` — administrador o moderador.
- `/announce mensaje` — administrador o moderador.
- `/role nombre admin|moderator|gamer` — solo administrador.

> El servidor es un prototipo local. Antes de exponerlo a Internet deben añadirse TLS, autenticación persistente, límites por IP y almacenamiento durable.

## NPC, inferencia futura y Emscripten

Los NPC se definen en `source/npc_system.*`. Cada uno incluye identidad, título, persona para IA, posición, collider y varias respuestas provisionales.

La futura inferencia Go se configura en:

```text
source/ai/inference.json
source/server/ai_provider.go
```

Está deshabilitada y usa los textos provisionales como fallback. `NpcInferenceCallback` permite conectar el cliente nativo a un puente Go sin acoplar el juego a un modelo específico.

La preparación web se encuentra en:

```text
cmake/EmscriptenPreparation.cmake
docs/emscripten-preparation.md
source/config/runtime.json
```

Estos archivos no se incluyen en el objetivo actual y **no habilitan una compilación Emscripten**.

## Verificación

```bash
./build/Mialygopolis3D --smoke-test \
  --asset-dir build/generated/assets \
  --source-assets 3rdparty/character-assets

ctest --test-dir build --output-on-failure
```

La prueba informa también tiempos de primera carga frente a retorno desde caché.
