# Servidor de lobby de Mialygopolis

Servidor TCP pequeño escrito en Go. Mantiene usuarios conectados, historial reciente de chat, roles y estados de movimiento.

## Ejecutar

Desde la raíz del proyecto:

```bash
./run-server.sh
```

Opciones principales:

```bash
go run . \
  -listen 127.0.0.1:7777 \
  -admin-token admin \
  -moderator-token moderator
```

El protocolo es una secuencia de líneas UTF-8 separadas por tabuladores. Los campos de texto usan escape URL. Se diseñó como una primera capa sencilla que después puede reemplazarse por WebSocket/QUIC al preparar la versión web.

Cada actualización de movimiento incluye el identificador del mundo. El cliente solo renderiza jugadores remotos que estén en el mismo mundo seleccionado.
