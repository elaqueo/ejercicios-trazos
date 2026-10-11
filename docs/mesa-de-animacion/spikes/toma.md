# Spike HU-85 · Compresión de las hojas guardadas

11 de octubre de 2026 · desktop (i5 Skylake de 4 núcleos, SSD) · preset release

> Pedido del usuario (criterio 4 de HU-85): antes de elegir el formato, comparar algoritmos de compresión sin pérdida para las hojas en disco. El código está en `spikes/mesa-toma` (descartable). Es opcional porque baja zstd y lz4 con FetchContent: `cmake --preset release -DET_SPIKE_TOMA=ON`. La salida completa de la corrida está al final.

## Qué se midió

- **La hoja:** la de Mesa de animación de ese momento, toda la tableta a 600 dpi (7680 × 4800 celdas), con grano 900, HB y el medio recortado (desde HU-113, 300 dpi: ver más abajo). Cada tile guarda solo el depósito: ningún tile usó otros planos.
- **Los trazos:** los grabados con `cartuchera --grabar` (6 trazos), repetidos y desplazados hasta tocar el 10, 26 y 50 % de los tiles, como en HU-81.
- **Qué se guarda de cada hoja:**
  - el estado: índice de tiles más el depósito, u16 por celda;
  - la imagen de pantalla (1734 × 1081, BGRA), para no redibujarla al abrir.
- **Algoritmos:**
  - zlib, el de Qt (`qCompress`), en niveles 1 y 6;
  - zstd 1.5.6, en niveles 1, 3, 9 y 19;
  - lz4 1.10.0, normal y HC 9.
  - Cada uno, con y sin "barajado": separar los bytes bajos de los altos de cada u16 antes de comprimir.
- **Cómo se tomaron los tiempos:**
  - un solo hilo, mediana de 3 corridas al guardar y de 5 al abrir;
  - "abrir 120" estima la toma entera repartida en los 4 núcleos;
  - cada variante se verificó: descomprimida, es idéntica al original.

## Resultados (hoja al 26 %, la de un rough con construcción)

| Algoritmo | Hoja | Razón | Guardar | Abrir | 120 hojas | Abrir 120 |
| --- | --- | --- | --- | --- | --- | --- |
| Sin comprimir | 18,9 MB | 1 : 1 | — | — | 2,2 GB | — |
| zlib 1 (Qt) | 4,7 MB | 4,0 : 1 | 184 ms | 49 ms | 569 MB | 1,5 s |
| zlib 6 (Qt) | 4,6 MB | 4,1 : 1 | 353 ms | 53 ms | 550 MB | 1,6 s |
| **zstd 1** | **4,5 MB** | **4,2 : 1** | **39 ms** | **13 ms** | **541 MB** | **0,4 s** |
| zstd 3 | 4,5 MB | 4,2 : 1 | 88 ms | 14 ms | 544 MB | 0,4 s |
| zstd 9 | 4,4 MB | 4,3 : 1 | 291 ms | 12 ms | 533 MB | 0,4 s |
| zstd 19 | 4,4 MB | 4,3 : 1 | 9 s | 12 ms | 528 MB | 0,4 s |
| lz4 | 5,1 MB | 3,7 : 1 | 23 ms | 4 ms | 609 MB | 0,1 s |
| lz4 HC 9 | 4,8 MB | 3,9 : 1 | 618 ms | 2 ms | 575 MB | 0,1 s |

Al 10 % y al 50 % el orden es el mismo:

| Algoritmo | 10 %: hoja | 10 %: guardar / abrir | 50 %: hoja | 50 %: guardar / abrir |
| --- | --- | --- | --- | --- |
| zlib 1 | 1,8 MB | 71 / 19 ms | 12,9 MB | 507 / 112 ms |
| zstd 1 | 1,7 MB | 14 / 5 ms | 12,4 MB | 134 / 56 ms |
| lz4 | 1,9 MB | 9 / 1 ms | 13,7 MB | 53 / 9 ms |

La imagen de pantalla de la hoja al 26 %:

| Opción | Tamaño | Guardar | Abrir |
| --- | --- | --- | --- |
| Redibujarla al abrir (`renderTone`) | — | — | 225 ms en un hilo, 108 ms en 4 (120 hojas: 6,8 s) |
| zlib 1 | 0,38 MB | 31 ms | 12 ms |
| zstd 1 | 0,36 MB | 8 ms | 3 ms |
| zstd 3 | 0,31 MB | 12 ms | 3 ms |
| lz4 | 0,48 MB | 6 ms | 1 ms |
| PNG (Qt) | 0,34 MB | 105 ms | 21 ms |

**El disco no pesa:** escribir una hoja de 4,6 MB hasta el disco (con `FlushFileBuffers`) tarda 29 ms, y leerla, 2,5 ms con el archivo en la caché del sistema.

## Lectura

- **El depósito comprime poco, alrededor de 4 : 1.**
  - El grafito sigue el grano del papel celda por celda: es ruido, y ningún algoritmo lo achica mucho más.
  - Entre el mejor (zstd 19) y el más rápido (lz4) hay solo un 15 % de tamaño. Los 8 : 1 de HU-81 eran engañosos: contaban los 3 planos vacíos de ilustración.
- **El barajado no sirve.** Empeora todos los casos, porque los dos bytes del u16 son ruido.
- **Los niveles altos no pagan.** zstd 9 o 19 ganan un 2 % de tamaño y tardan de 7 a 230 veces más en guardar.
- **zstd 1 es el mejor equilibrio.**
  - Ocupa lo mismo o menos que zlib 6.
  - Guarda 5 veces más rápido que zlib 1 y abre 4 veces más rápido.
  - La toma de 120 hojas abre en 0,4 s, contra 1,5 s con zlib.
- **lz4 abre más rápido**, pero ocupa un 12 % más. A 0,4 s por toma, abrir no es el cuello de botella.
- **La imagen se guarda.** Comprimida ocupa 0,3 MB, menos del 10 % de la hoja, y se abre en 3 ms en lugar de 108 ms. PNG no conviene como formato nativo: tarda 5 veces más que zlib en guardar.
- **Hoja típica en disco:** unos 4,8 MB (estado más imagen), y 120 hojas al 26 % ocupan unos 580 MB.

## A 300 dpi (HU-113)

El 11 de octubre de 2026 Mesa de animación volvió a 300 dpi con grano 300, y el spike quedó con ese papel (3840 × 2400 celdas). Como los tiles son 4 veces menos, los mismos trazos tocan otro porcentaje: 14, 30 y 51 %.

| Hoja | Sin comprimir | zstd 1 | Razón | Guardar | Abrir | 120 hojas en disco |
| --- | --- | --- | --- | --- | --- | --- |
| 14 % | 2,5 MB | 0,30 MB | 8,4 : 1 | 4 ms | 1,0 ms | 36 MB |
| 30 % | 5,4 MB | 0,74 MB | 7,3 : 1 | 8 ms | 2,5 ms | 88 MB |
| 51 % | 9,0 MB | 1,79 MB | 5,0 : 1 | 17 ms | 5,4 ms | 214 MB |

- La imagen de pantalla al 26 %: 0,23 MB con zstd 1 (6 ms). Redibujarla tarda 97 ms en un hilo.
- El grano bajo es menos ruido: comprime 7 : 1 en vez de 4 : 1. Con la imagen, una hoja de un rough con construcción ocupa ~1 MB en disco, contra ~4,8 MB a 600 dpi.
- zstd 1 sigue siendo la elección: los demás algoritmos se ordenan igual.

## Decisión

El 11 de octubre de 2026 el usuario eligió **zstd nivel 1**. El formato quedó en [arquitectura.md](../arquitectura.md).

## Recomendación

**zstd nivel 1, sin barajado**, para el estado y para la imagen de cada hoja, cada una en su propio bloque dentro del archivo.

- **El costo:** es la primera dependencia fuera de Qt desde HU-68.
  - zstd es BSD y compatible con la licencia MIT del proyecto.
  - Se compila desde su fuente: FetchContent con la versión fija y su hash, o la fuente copiada en `third_party/`. No hace falta vcpkg.
- **La alternativa sin dependencias es zlib 1, el de Qt.** Funciona: guardar es en otro hilo (HU-86) y nunca frena el trazo. Lo que cambia es abrir, que tarda 1,5 s con 120 hojas en lugar de 0,4 s, y un 5 % más de disco.
- **Lo que queda abierto en cualquier caso:** el archivo de hoja lleva en la cabecera el código del algoritmo. Así se puede cambiar más adelante sin romper las tomas guardadas.

**Falta medir:**
- un rough real dibujado en la app, en lugar de 6 trazos repetidos;
- la laptop.

## Salida de la corrida

```text
Muestras: C:\Users\elaqu\AppData\Local\trazos\cartuchera-muestras.csv
  6 trazos, 415 muestras con contacto
Papel 7680 × 4800 celdas (120 × 75 tiles)

1. Estado de la hoja al 11 % de tiles (1010 tiles, 0 con planos además del depósito)
                               MB  razón    guardar     abrir  120 hojas MB  abrir 120 (s)
  sin comprimir               7.9   1 : 1          -         -           947              -
  zlib 1                     1.81   4.4:1      71 ms   19.1 ms           217            0.6
  zlib 6                     1.74   4.5:1     130 ms   20.6 ms           209            0.6
  zstd 1                     1.71   4.6:1      14 ms    4.5 ms           206            0.1
  zstd 3                     1.72   4.6:1      29 ms    4.8 ms           207            0.1
  zstd 9                     1.69   4.7:1     101 ms    4.3 ms           202            0.1
  zstd 19                    1.67   4.7:1    3714 ms    4.2 ms           201            0.1
  lz4                        1.94   4.1:1       9 ms    1.4 ms           233            0.0
  lz4 HC 9                   1.83   4.3:1     236 ms    0.8 ms           219            0.0
  zlib 1 + barajado          1.87   4.2:1      82 ms   23.6 ms           225            0.7
  zlib 6 + barajado          1.78   4.4:1     141 ms   25.0 ms           214            0.7
  zstd 1 + barajado          1.75   4.5:1      22 ms    8.9 ms           210            0.3
  zstd 3 + barajado          1.77   4.5:1      39 ms    8.7 ms           212            0.3
  zstd 9 + barajado          1.69   4.7:1     122 ms    7.3 ms           203            0.2
  zstd 19 + barajado         1.68   4.7:1    2687 ms    8.7 ms           202            0.3
  lz4 + barajado             2.17   3.6:1      16 ms    5.1 ms           260            0.2
  lz4 HC 9 + barajado        1.97   4.0:1     275 ms    4.1 ms           236            0.1

1. Estado de la hoja al 27 % de tiles (2420 tiles, 0 con planos además del depósito)
                               MB  razón    guardar     abrir  120 hojas MB  abrir 120 (s)
  sin comprimir              18.9   1 : 1          -         -          2270              -
  zlib 1                     4.74   4.0:1     184 ms   48.7 ms           569            1.5
  zlib 6                     4.58   4.1:1     353 ms   53.4 ms           550            1.6
  zstd 1                     4.51   4.2:1      39 ms   12.9 ms           541            0.4
  zstd 3                     4.53   4.2:1      88 ms   14.1 ms           544            0.4
  zstd 9                     4.44   4.3:1     291 ms   11.9 ms           533            0.4
  zstd 19                    4.40   4.3:1    9063 ms   12.0 ms           528            0.4
  lz4                        5.08   3.7:1      23 ms    3.6 ms           609            0.1
  lz4 HC 9                   4.79   3.9:1     618 ms    2.2 ms           575            0.1
  zlib 1 + barajado          4.92   3.8:1     211 ms   61.3 ms           591            1.8
  zlib 6 + barajado          4.68   4.0:1     351 ms   61.8 ms           561            1.9
  zstd 1 + barajado          4.60   4.1:1      55 ms   21.7 ms           552            0.7
  zstd 3 + barajado          4.64   4.1:1      96 ms   21.1 ms           557            0.6
  zstd 9 + barajado          4.45   4.3:1     323 ms   20.0 ms           534            0.6
  zstd 19 + barajado         4.42   4.3:1    6903 ms   25.1 ms           530            0.8
  lz4 + barajado             5.67   3.3:1      50 ms   14.3 ms           680            0.4
  lz4 HC 9 + barajado        5.14   3.7:1    1072 ms   16.4 ms           617            0.5

2. Imagen de pantalla de la hoja al 26 % (1734 × 1081, BGRA)
  redibujarla al abrir (renderTone): 225 ms en un hilo, 108 ms en 4; abrir 120: 6.8 s
  guardada: 7.2 MB sin comprimir
  zlib 1                     0.38 MB    19:1      31 ms   11.9 ms
  zstd 1                     0.36 MB    20:1       8 ms    2.7 ms
  zstd 3                     0.31 MB    23:1      12 ms    3.3 ms
  zstd 9                     0.27 MB    26:1      47 ms    2.6 ms
  lz4                        0.48 MB    15:1       6 ms    1.2 ms
  lz4 HC 9                   0.39 MB    18:1     125 ms    1.0 ms
  PNG (Qt)                   0.34 MB    21:1     105 ms   21.1 ms

3. Disco
  escribir la hoja (zstd 3 + barajado, 4.6 MB) hasta el disco: 29.0 ms; leerla (en caché del sistema): 2.5 ms

1. Estado de la hoja al 50 % de tiles (4540 tiles, 0 con planos además del depósito)
                               MB  razón    guardar     abrir  120 hojas MB  abrir 120 (s)
  sin comprimir              35.5   1 : 1          -         -          4259              -
  zlib 1                    12.91   2.7:1     507 ms  112.0 ms          1549            3.4
  zlib 6                    12.55   2.8:1     774 ms  115.0 ms          1506            3.5
  zstd 1                    12.44   2.9:1     134 ms   56.0 ms          1493            1.7
  zstd 3                    12.51   2.8:1     241 ms   28.6 ms          1501            0.9
  zstd 9                    12.32   2.9:1    1075 ms   23.9 ms          1479            0.7
  zstd 19                   12.18   2.9:1   18275 ms   30.8 ms          1462            0.9
  lz4                       13.69   2.6:1      53 ms    8.5 ms          1643            0.3
  lz4 HC 9                  13.01   2.7:1    1323 ms    5.7 ms          1561            0.2
  zlib 1 + barajado         13.30   2.7:1     645 ms  167.0 ms          1597            5.0
  zlib 6 + barajado         12.76   2.8:1     981 ms  149.7 ms          1532            4.5
  zstd 1 + barajado         12.66   2.8:1     142 ms   53.0 ms          1520            1.6
  zstd 3 + barajado         12.73   2.8:1     292 ms   50.0 ms          1528            1.5
  zstd 9 + barajado         12.32   2.9:1     891 ms   46.1 ms          1478            1.4
  zstd 19 + barajado        12.23   2.9:1   16090 ms   55.8 ms          1467            1.7
  lz4 + barajado            14.96   2.4:1      91 ms   27.1 ms          1795            0.8
  lz4 HC 9 + barajado       13.74   2.6:1    1607 ms   20.9 ms          1649            0.6

```
