# Notas de implementacion

- El layout persistente esta fijado por `include/types.hpp`: `Node` mide 4096
  bytes y contiene `k` mas 204 entradas. Cada `Rect` se guarda como
  `(x1, x2, y1, y2)`, el orden indicado en `Tarea1.pdf`.
- `src/disk.cpp` serializa nodos completos con `read`, `write`, `seekg` y
  `seekp`; cada llamada a `readNode` incrementa el contador logico de I/O.
- `src/bulkload.cpp` reserva `nodes[0]` para la raiz y escribe la raiz final al
  terminar el empaquetamiento bottom-up.
- `src/query.cpp` siempre abre el archivo del arbol y recorre desde el nodo 0.
- El MVP asume serializacion cruda little-endian en la misma arquitectura.
