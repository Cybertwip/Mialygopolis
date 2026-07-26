# Enlace futuro de inferencia para NPC

`inference.json` prepara el contrato entre el cliente nativo y un proceso Go de inferencia. Está **deshabilitado por defecto** y no altera la compilación actual.

El cliente ya expone `NpcInferenceCallback` en `npc_system.h`. Cuando se conecte el sistema de IA, el adaptador debe recibir una solicitud JSON con:

- `request_id`
- `npc_id`
- `npc_name`
- `persona`
- `player_text`
- `world`

Mientras no exista proveedor, el sistema utiliza los textos de marcador incluidos para cada NPC.

El transporte propuesto es TCP con JSON por línea en `127.0.0.1:7788`. El servidor Go contiene la interfaz `AIProvider`, pero todavía no inicia ni configura un modelo.
