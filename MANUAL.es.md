# Periscope — manual

Otros idiomas: [English](MANUAL.md) · [Русский](MANUAL.ru.md). La versión principal es la inglesa; las traducciones se actualizan junto con ella.

Periscope es TELESCOPE, el analizador de audio de OVNI Audio que además concluye, con voz y control por teclado. La pantalla del plugin sigue diciendo TELESCOPE, y este manual también lo llama así cuando describe el analizador. Este manual cubre cada lente, qué mide y cómo leerla, y cómo usar todo con el teclado y un lector de pantalla.

El motor de análisis, las lentes y las notas de medición vienen del [TELESCOPE original de OVNI Audio](https://ovniaudio.com/telescope), versión 0.2.0, que el autor publicó como open source bajo AGPLv3. La capa de teclado y lector de pantalla se agregó en [este fork](https://github.com/fdrv161/periscope/tree/accessible). Donde este manual explica qué mide una lente, sigue el manual del autor, resumido. El original completo, con cada fórmula y resultado de test, está en [`plugins/telescope/docs/manual/README.es.md`](plugins/telescope/docs/manual/README.es.md).

## Contenido

- [Qué es este plugin](#qué-es-este-plugin)
- [Instalación](#instalación)
- [Primeros pasos con un lector de pantalla](#primeros-pasos-con-un-lector-de-pantalla)
- [La ventana y los tres paneles](#la-ventana-y-los-tres-paneles)
- [Referencia de teclas](#referencia-de-teclas)
- [El panel del plugin: bypass, presets, tamaño, idioma, tema](#el-panel-del-plugin)
- Las lentes:
  [Loudness](#1--loudness) ·
  [Dinámica](#2--dinámica) ·
  [Espectro](#3--espectro) ·
  [Espectrograma](#4--espectrograma) ·
  [Cascada](#5--cascada) ·
  [CQT](#6--cqt) ·
  [Espiral](#7--espiral) ·
  [Scope](#8--scope) ·
  [Correlación por banda](#9--correlación-por-banda) ·
  [Espectrograma estéreo](#10--espectrograma-estéreo) ·
  [Campo](#11--campo) ·
  [Balance tonal](#12--balance-tonal) ·
  [Veredicto](#13--veredicto)
- [Analizar un archivo](#analizar-un-archivo)
- [Idiomas](#idiomas)
- [Límites conocidos](#límites-conocidos)
- [Licencia y créditos](#licencia-y-créditos)

---

## Qué es este plugin

Trece **lentes** sobre un solo motor de análisis, más un motor de reglas (Veredicto) que convierte lo medido en frases. Cada frase lleva el número que la sostiene.

- **No toca tu audio.** TELESCOPE lee los canales izquierdo y derecho, los analiza en su propio hilo y no escribe nada de vuelta. Latencia cero, sin cola, salida bit-exacta. El autor lo verifica con tests automáticos. Esto vale mientras los parámetros de host In, Out e In Phase estén en su valor por defecto: no están en la ventana del plugin, pero la lista de parámetros y la automatización de tu DAW los muestran, así que no los toques.
- **El audio nunca espera por una medición.** El análisis corre en un hilo aparte.
- **Entrada estéreo o mono.** Una entrada mono se mide como izquierdo y derecho idénticos (correlación +1, ancho 0). Ni surround ni Ambisonics.
- **No localiza fuentes.** Campo y las vistas polares muestran energía por dirección de paneo, no dónde estaba un sonido en una sala.
- **Sin IA, sin red, sin telemetría.** Veredicto es una tabla fija de umbrales: la misma señal da siempre el mismo informe, y no sale nada de tu computadora.

### Qué agrega este fork

La interfaz original está toda dibujada, así que los lectores de pantalla no encontraban nada en ella. Esta versión pone controles reales, que pueden recibir el foco, encima de la interfaz dibujada:

- una **lista de lentes**, un **panel de lecturas** y un **panel del plugin**, y se pasa de uno a otro con Tab;
- cada número de cada lente como filas de texto ("Integrado, -14.2 LUFS");
- cada ajuste se puede cambiar desde el teclado;
- mensajes hablados cuando termina un análisis en segundo plano;
- voz en siete idiomas: inglés, español, portugués, francés, alemán, italiano y ruso;
- ruso también en pantalla.

Para quien ve la pantalla no cambia nada: el mouse funciona exactamente igual, y un clic no mueve el foco del teclado al plugin, así que los atajos de tu DAW siguen funcionando.

Probado con NVDA en REAPER en Windows. Otros lectores de pantalla que usan UI Automation (Narrador, JAWS) también deberían funcionar.

---

## Instalación

### Windows

Un **plugin VST3 para Windows 10 u 11, de 64 bits**. Es la versión probada.

1. Descargá [`Periscope-windows-vst3.zip`](https://github.com/fdrv161/periscope/releases/latest/download/Periscope-windows-vst3.zip) desde la [última versión publicada](https://github.com/fdrv161/periscope/releases/latest).
2. Descomprimí el ZIP. Adentro hay una carpeta llamada `Periscope.vst3`. Copiá esa carpeta entera a tu carpeta de VST3:
   - la estándar es `C:\Program Files\Common Files\VST3` (Windows pide permiso de administrador para copiar ahí);
   - o cualquier carpeta que hayas agregado a las rutas VST3 de tu DAW. En REAPER: Options, Preferences, Plug-ins, VST, "VST plug-in paths".
3. Reiniciá la DAW o volvé a escanear los plugins. En REAPER: Options, Preferences, Plug-ins, VST, "Re-scan".
4. Insertá **Periscope** en una pista o en el máster. En la lista de plugins aparece como "Periscope (fdrv161)".

Para actualizar, cerrá la DAW y reemplazá la carpeta `Periscope.vst3` por la nueva.

La versión no tiene firma de código. Eso no afecta a un VST3 que carga una DAW.

### macOS (experimental)

VST3 y AU, universal (Apple Silicon e Intel), macOS 11 o posterior. Compila y pasa pluginval, pero **la voz sólo se probó con NVDA en Windows**. JUCE expone los mismos controles a VoiceOver y los avisos usan el mecanismo propio de VoiceOver, así que puede funcionar bien, a medias o nada. Si lo probás, por favor [contá cómo te fue](https://github.com/fdrv161/periscope/issues).

1. Descargá [`Periscope-macos.zip`](https://github.com/fdrv161/periscope/releases/latest/download/Periscope-macos.zip) y abrilo.
2. Copiá `Periscope.vst3` a `~/Library/Audio/Plug-Ins/VST3` y `Periscope.component` a `~/Library/Audio/Plug-Ins/Components`.
3. La versión está firmada ad hoc, sin notarización de Apple, así que macOS la bloquea hasta que quites la marca de cuarentena. En Terminal:

   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Periscope.vst3 ~/Library/Audio/Plug-Ins/Components/Periscope.component
   ```

4. Volvé a escanear los plugins en tu DAW. Logic puede necesitar un reinicio para listar el AU.

En macOS las teclas son las mismas que abajo, con Opción en lugar de Alt.

---

## Primeros pasos con un lector de pantalla

1. Insertá Periscope y abrí su ventana. En REAPER la ventana de FX se abre con el plugin. Si el foco sigue en la lista de la cadena de FX, presioná **F6** (una tecla de OSARA): el foco pasa al plugin, a la lista de lentes.
2. El foco cae en la **lista de lentes**. NVDA dice algo como "Lentes, Loudness".
3. Presioná **flecha arriba** y **flecha abajo** para elegir una lente. La lente cambia enseguida, igual que con un clic.
4. Presioná **Intro** para ir a las **lecturas** de esa lente. NVDA lee la fila actual, por ejemplo "Integrado, -14.2 LUFS".
5. Usá **arriba** y **abajo** para moverte por las filas, **izquierda** y **derecha** para pasar de un grupo a otro (Lecturas, Ajustes, Acciones, etcétera).
6. En un ajuste, presioná **más** o **menos** para cambiarlo, o **Intro** para el modo edición. **Alt+Flecha arriba / Alt+Flecha abajo** también lo cambian un paso, y **Alt+Re Pág / Alt+Av Pág** un paso grande.
7. Presioná **Escape** para volver a la lista de lentes.
8. Presioná **F1** en cualquier lugar para escuchar la ayuda del panel actual.

Arrancá la reproducción con **Espacio**: va a la DAW, así que podés iniciar y detener el transporte sin salir del plugin.

---

## La ventana y los tres paneles

La ventana dibujada tiene una tira de trece lentes a la izquierda, la lente abierta en el medio y un encabezado arriba con bypass, presets, A/B, tamaño de ventana, idioma y tema.

Para el teclado son tres paneles, en este orden de Tab:

1. **Lista de lentes.** Una fila por lente. Elegir una fila abre la lente.
2. **Lecturas.** Una grilla de grupos y filas para la lente abierta: sus números, sus ajustes y sus acciones. Cada lente recuerda dónde estaba el cursor, así que al volver a una lente volvés a la misma fila.
3. **Plugin.** Los controles del encabezado: bypass, presets, A/B, tamaño de ventana, idioma y tema.

Tab avanza y Mayús+Tab retrocede. Después del último panel, Tab vuelve al primero.

Sólo se calculan los módulos que necesita la lente abierta, así que las lentes que no estás mirando no cuestan nada. La excepción es el loudness, que corre siempre, para que el integrado, el loudness range, el histograma y el conteo de clips nunca tengan huecos.

---

## Referencia de teclas

### Lista de lentes

- **Arriba / abajo**: elegir una lente (se abre en el acto).
- **Una letra**: saltar a la siguiente lente cuyo nombre empieza con esa letra.
- **Intro**: ir a las lecturas de la lente.
- **Tab / Mayús+Tab**: panel siguiente o anterior.
- **F1**: ayuda hablada.

### Lecturas

Moverse:

- **Arriba / abajo**: fila anterior o siguiente del grupo.
- **Izquierda / derecha**: grupo anterior o siguiente.
- **Inicio / Fin**: primera o última fila del grupo.
- **Ctrl+Inicio / Ctrl+Fin**: primera o última fila de toda la grilla.
- **Re Pág / Av Pág**: 10 filas hacia arriba o hacia abajo.
- **Una letra o un dígito**: siguiente fila que empieza con él. Con **Mayús**: la anterior.
- **F5**: volver a leer la fila con un valor nuevo. Las lecturas son en vivo, así que así seguís un número que cambia.

Cambiar valores:

- **Más / menos** (teclado principal o numérico; `=` también cuenta como más): un paso.
- **Alt+arriba / Alt+abajo**: un paso.
- **Alt+Re Pág / Alt+Av Pág**: un paso grande.
- **Alt+Inicio / Alt+Fin**: mínimo o máximo.
- **Intro en un ajuste**: modo edición. Ahí **arriba / abajo** cambian el valor, **Re Pág / Av Pág** dan un paso grande, e **Intro** o **Escape** salen del modo edición.
- **Intro en una acción** (Reiniciar, Cargar, etcétera): ejecutarla.
- **Intro en una lectura común**: volver a leerla.

Cuando un valor cambia, sólo se dice el valor nuevo. En el extremo de un rango no se dice nada y el valor no da la vuelta: el silencio significa que estás en el borde. Lo mismo pasa con la primera y la última fila. Sólo tres cosas dan la vuelta: más y menos en la fila Preset, Intro en la fila Idioma y la búsqueda por primera letra.

Otras teclas:

- **Escape**: volver a la lista de lentes (o salir del modo edición).
- **F1**: ayuda hablada. En una fila con ayuda propia (los ajustes de Espectro, por ejemplo) explica esa fila; en el resto da la ayuda del panel.
- **Tab / Mayús+Tab**: panel siguiente o anterior.
- **Ctrl+C**: copia la fila actual como texto, tal como se dice. **Ctrl+Mayús+C**: copia el grupo entero, su nombre y después una fila por línea, por ejemplo todas las lecturas de Espectro. Pegalas en notas o en un mensaje.

### Teclas que van a tu DAW

- **Espacio**: iniciar y detener el transporte.
- **Atajos con Ctrl y Alt** que el plugin no usa (Ctrl+S, Alt+F4, etcétera).
- **Teclas de función** salvo F1 y F5.

Las demás teclas simples se las queda el plugin mientras tiene el foco, así que una flecha o Supr nunca disparan una acción de la DAW por accidente.

### Mensajes hablados

TELESCOPE habla por su cuenta cuando un análisis en segundo plano empieza y termina, y cuando el análisis se retrasa:

- Balance tonal: "Analizando la referencia", después "Referencia analizada, ..." o "No se pudo leer la referencia: ...".
- Veredicto: "Analizando" con el nombre del archivo, después "Archivo analizado, ..." seguido del titular, o "No se pudo leer el archivo: ...".
- Loudness: "El análisis se retrasó y omitió parte del audio, las lecturas pueden no ser exactas", una vez, cuando la computadora no llegó.

Todo lo demás se dice como respuesta a tus teclas.

---

## El panel del plugin

Las filas del tercer panel, en orden:

- **Bypass**: Intro o más y menos lo activan y desactivan. TELESCOPE deja pasar el audio sin cambios en los dos casos.
- **Preset**: el nombre del preset actual, con "modificado" si cambiaste algo. **Más y menos** eligen el preset siguiente o el anterior, los de fábrica y los tuyos en una sola lista, dando la vuelta. **Intro** abre el menú de presets; las flechas e Intro eligen, y después el foco vuelve a esta fila. En el menú, Production tiene los cuatro presets de fábrica (la lente Loudness sin objetivo, Spotify, Apple o YouTube), Sound design está vacío en este plugin, y User muestra tus presets cuando guardaste alguno.
- **Guardar preset**: **Intro** abre un diálogo con un campo de nombre. Escribí un nombre y presioná Intro, o Escape para cancelar. Los caracteres que un nombre de archivo no puede tener (\ / : * ? " < > |) se quitan del nombre, y un preset con el mismo nombre se reemplaza sin preguntar. Un preset guarda todo el estado, incluidos el idioma, la ruta de la referencia y los ajustes de las lentes. Tus presets son archivos en `%APPDATA%\Application Support\OVNI TELESCOPE\User Presets`, carpeta compartida con el TELESCOPE original.
- **Ranura A B**: cuál de las dos ranuras de comparación está activa. Más elige B, menos elige A, Intro cambia.
- **Copiar esta ranura en la otra**: Intro copia la ranura activa sobre la otra.
- **Tamaño de ventana**: pequeño, mediano o grande (S, M y L en pantalla). Intro entra en modo edición; más y menos también funcionan. Cada tamaño tiene su propia disposición, no un dibujo escalado.
- **Idioma**: el idioma de la pantalla y de la voz, ver [Idiomas](#idiomas). Intro pasa al idioma siguiente; más y menos también funcionan.
- **Tema**: oscuro o claro. Más elige claro, Intro cambia.

El tema, el idioma y el tamaño de ventana se recuerdan para ventanas nuevas e instancias nuevas. Se guardan en `%APPDATA%\OVNI\OVNI.settings` (en macOS `~/Library/Application Support/OVNI/OVNI.settings`), un archivo compartido con el TELESCOPE original. Un proyecto guardado conserva su propio idioma.

---

# Las lentes

Cada lente de abajo tiene dos partes: qué mide y cómo leerla (del manual del autor), y **Con el teclado**: los grupos y las filas que vas a escuchar.

## 1 · Loudness

![Lente Loudness: cuatro barras de nivel, el valor integrado y tres minutos de historia](plugins/telescope/docs/manual/images/loudness.png)

**Qué mide.** Qué tan fuerte suena el programa, según el estándar de broadcast (ITU-R BS.1770 y EBU R 128).

- **Integrado** (LUFS): todo el programa desde el último reinicio, con la doble compuerta estándar (absoluta en -70 LUFS, relativa 10 LU por debajo de la media).
- **Momentary** (LUFS): una ventana de 400 ms.
- **Short-term** (LUFS): una ventana de 3 s, actualizada 10 veces por segundo.
- **Loudness range** (LU): EBU Tech 3342, la distancia entre los percentiles 10 y 95 de los valores short-term.
- **True peak** (dBTP): BS.1770 anexo 2, medido antes de la ponderación K, general y por canal.
- **Máximos momentary y short-term**: desde el último reinicio.

**Cómo leerla.** En pantalla hay cuatro barras sobre una misma escala de 0 a -60: momentary y short-term en LUFS, true peak izquierdo y derecho en dBTP. Abajo hay tres minutos de historia del short-term. Si elegís un objetivo de plataforma de streaming, se dibuja su línea y se escribe la distancia a tu integrado.

**Mientras una ventana todavía se llena** (400 ms para momentary, 3 s para short-term) el valor es *parcial*: el mismo cálculo sobre el audio que hay, no una suposición. Cuando la ventana se llena coincide exactamente con el número oficial. El integrado no tiene valor parcial: la compuerta es la compuerta.

**Bueno saberlo.** El true peak se mide con sobremuestreo 4x, que puede leer un poco de menos, dentro de la tolerancia del propio estándar. Si tu máster marca -1.0 dBTP acá, tomalo como un poco más alto.

### Con el teclado

- **Lecturas**: Integrado, Short-term, Momentary, Loudness range, Máx true peak (general, después izquierdo y derecho), Máx short-term, Máx momentary, Historia, Objetivo (la plataforma y qué tan lejos estás de ella). Un valor que todavía se llena va seguido de "parcial". Cuando el análisis tuvo que saltear audio porque la computadora no daba abasto, la pantalla muestra una línea ámbar "análisis atrasado": TELESCOPE lo dice una vez cuando aparece, e Integrado va seguido de "el análisis se retrasó" hasta el próximo reinicio.
- **Historia** cuenta los tres minutos de historia del short-term que hay en pantalla como el ojo lee el gráfico: tramos estables con su duración y su nivel, del más viejo al más nuevo, y después el momento más fuerte y el más suave. Por ejemplo: "últimos 3 minutos, 40 segundos en torno a -14 LUFS, luego 1 minuto 20 segundos en torno a -9 LUFS, luego 1 minuto en torno a -11 LUFS, hasta ahora, lo más fuerte -8.7 LUFS, hace 1 minuto 30 segundos". Muestra dónde el estribillo suena más fuerte que la estrofa, o dónde saltó el loudness. Como máximo cinco tramos: cuando hay más escalones, los chicos se juntan, como el ojo ve menos escalones de lejos.
- **Acciones**:
  - **Objetivo de plataforma**: más y menos recorren: sin objetivo, Spotify, Apple Music, YouTube, Amazon Music, Tidal y Deezer. También es el parámetro de host "Target", así que se puede automatizar.
  - **Pausa**: Intro pausa o reanuda la medición.
  - **Reiniciar**: Intro borra todo y empieza de nuevo.

---

## 2 · Dinámica

![Lente Dinámica: PSR y PLR, el histograma short-term y la línea de tiempo de clips](plugins/telescope/docs/manual/images/dynamics.png)

**Qué mide.** No qué tan fuerte está la mezcla (eso es Loudness) sino cuánto margen le queda.

- **PSR** (dB): el true peak más alto de los últimos 3 segundos menos el loudness short-term. La lectura en vivo.
- **PLR** (dB): el true peak más alto desde el reinicio menos el loudness integrado (AES TD1004).
- **Histograma**: el loudness short-term desde el reinicio, en pasos de 1 LU.
- **Clips**: eventos por encima del umbral de clip (por defecto -1.0 dBTP, rango de -3 a 0) desde el reinicio, y una línea de tiempo de 10 minutos.

**Cómo leerla.** Un seno estable marca PSR = PLR = 0 dB, y es correcto: un tono estable no tiene margen de pico por encima de su propio nivel.

**Un evento de clip no es una muestra pasada de la línea.** Un exceso que dura 50 ms cruza el umbral una vez por ciclo, pero cuenta como un solo evento: un evento empieza en la primera muestra por encima del umbral y termina recién después de 100 ms por debajo.

**Cambiar el umbral reinicia el conteo**, porque un conteo medido contra dos techos distintos no significaría nada.

La línea en pantalla es tu objetivo de PSR, 8 dB hasta que lo cambies (ver Acciones más abajo). Es una referencia, no un juicio. Los juicios son trabajo de Veredicto, con la regla a la vista.

### Con el teclado

- **Lecturas**: PLR, PSR (con "bajo" por debajo de 4 dB, y "bajo el objetivo" por debajo de tu objetivo de PSR, donde la barra se pone ámbar), Tiempo bajo el objetivo de PSR (la parte del tiempo analizado, hasta los últimos 10 minutos, con el PSR por debajo del objetivo, por ejemplo "35 por ciento del tiempo por debajo de 8 dB, en 2 minutos 10 segundos"; los segundos en silencio no cuentan), Factor de cresta (pico de muestra sobre RMS desde el reinicio, por canal, el mayor de los dos, con el RMS contado según AES17, así un seno da 0 dB, como en dpMeter y la mayoría de los medidores), Short-term, Short-term más frecuente (el pico del histograma y cuánto tiempo se mantuvo), La mayor parte del tiempo (dónde está el 80 por ciento central de las lecturas short-term, el ancho del histograma, por ejemplo "de -12 a -8 LUFS"), Clips (cantidad por encima del umbral), Clips en los últimos 10 minutos (y hace cuánto fue el último).
- **Acciones**:
  - **Umbral de clip**: más y menos lo mueven 0.1 dB, un paso grande es 0.5 dB.
  - **Objetivo de PSR**: más y menos lo mueven 0.5 dB, un paso grande es 1 dB, de 3 a 20 dB. Se guarda en el proyecto. Como guía: unos 12 a 14 dB para una mezcla, 7 a 8 para un máster fuerte que todavía respira, 5 a 6 para música de baile.
  - **Pausa** y **Reiniciar**: Intro.

---

## 3 · Espectro

![Lente Espectro: curva FFT con retención de picos](plugins/telescope/docs/manual/images/spectrum.png)

**Qué mide.** El espectro de frecuencias, con una referencia exacta en dB: cada bin está en dBFS respecto de un seno a escala completa, así que un seno de cierta amplitud marca su nivel real con cualquier forma de ventana.

**Cómo leerla.** Vale la pena conocer dos consecuencias:

- **El ruido blanco a escala completa no marca 0 dBFS por bin.** Su energía se reparte en miles de bins, así que cada uno marca mucho menos.
- **Cambiar el tamaño de FFT mueve el piso de ruido, no los tonos.** Duplicar el tamaño parte cada bin en dos: un tono mantiene su nivel, el ruido baja 3 dB por bin.

**Por qué la pendiente por defecto es 3 dB por octava.** Un bin de FFT tiene un ancho fijo en hertz, mientras que la música y el oído trabajan en octavas. El ruido rosa, la referencia neutra, cae 3 dB por octava en un espectro crudo. Con una pendiente de 3, el ruido rosa se ve plano. Una mezcla terminada típica cae un poco más rápido, por eso muchos analizadores usan 4,5. La pendiente es un ajuste de visualización: no cambia los datos, y no se aplica a los modos de bandas, que ya miden en fracciones de octava.

**El suavizado tampoco cambia los datos.** Suaviza la curva dibujada: con Off se ve cada pico estrecho (una imagen cargada, buena para cazar resonancias), con 1/6 de octava sólo jorobas y huecos anchos (el equilibrio general).

**Ponderación.** En lugar de la inclinación de Pendiente, el gráfico puede mostrar el espectro a través de una curva estándar, como lo pondera un sonómetro o el oído: A, B, C y D (sonómetros), K (la curva de sonoridad de LUFS), ITU-R 468 (el ruido como lo percibe el oído), ISO 226 (el contorno de igual sonoridad a 80 fonios) y las curvas de cine X y N inversas, que, como un analizador de sala de cine, además dibujan plano el ruido rosa. Todas las curvas valen 0 dB en 1 kHz, así que un tono de 1 kHz se lee igual con cualquiera. Igual que la pendiente, es un ajuste de visualización: cambia el gráfico, las barras y todos los niveles que dice esta lente, no los datos, Balance tonal ni Veredicto. Elegir Pendiente vuelve al ajuste Pendiente.

**Enmascaramiento.** Una estimación del enmascaramiento auditivo: los rangos de frecuencia que tapan sonidos más fuertes cercanos, así que el oído no los oye en la mezcla, y los rangos bajo el umbral de audición. Usa el modelo psicoacústico de los codificadores AAC (3GPP TS 26.403) sobre bandas de un tercio de Bark. En pantalla, las bandas enmascaradas se oscurecen, las que están bajo el umbral de audición menos. Sigue la curva tal como se dibuja, así que el ajuste Promedio decide qué tan rápido cambia: con Promedio en Off es instante a instante y salta, con un promedio muestra lo que queda enmascarado. Una mezcla bien equilibrada suele mostrar poco enmascaramiento, salvo los graves más profundos y la octava de arriba; mucho en un rango quiere decir que las partes de ahí se pelean por el mismo lugar. Es una estimación, no un modelo completo de la audición.

**Una banda sin bins no es una banda en cero.** Con una FFT chica, las bandas graves pueden ser más angostas que un bin. Eso significa "no se puede medir con esta resolución", no "no hay energía". Para los graves, usá una FFT grande.

### Con el teclado

Pendiente y Suavizado existen para ayudar al ojo: Pendiente hace que una mezcla equilibrada se vea plana, Suavizado decide qué tan grande tiene que ser un detalle para que se vea. La voz los usa igual, y cada nivel que dice es el que muestra el gráfico, así que alguien que ve la pantalla lee los mismos números. Con Pendiente, los niveles por encima de 1 kHz suben y los de abajo bajan: con los 3 dB por octava por defecto, un tono de 440 Hz marca acá unos 3,5 dB menos que en CQT o Espectrograma, y un tono de 4 kHz 6 dB más. Poné Pendiente en 0 para escuchar un tono con su nivel real. Las bandas de tercio de octava nunca se inclinan, como en pantalla; con una curva de ponderación en lugar de Pendiente también se ponderan, como en pantalla.

**La línea.** Una línea plana en el gráfico, a la altura mediana de la curva entre 40 Hz y 16 kHz: donde el ojo ve que está la curva. Con Pendiente en 4,5, una mezcla terminada típica queda a lo largo de ella: un lugar bien por encima tiene más energía que una mezcla equilibrada, uno bien por debajo tiene menos.

- **Lecturas**:
  - **Sonda**: un cursor de frecuencia que se mueve por notas. Más y menos la mueven un semitono, un paso grande es una octava, y el rango va de unos 20 Hz a 20 kHz. Lee la frecuencia, el nombre de la nota, el nivel de los canales izquierdo y derecho (con la retención si Peak hold está activo) y a qué distancia está de la línea, por ejemplo "3 dB por encima de la línea". Los niveles son los del gráfico, con la Pendiente. Con el suavizado activo, el nivel es la altura de la curva suavizada, tal como está dibujada, y la retención es el pico sin suavizar: ése es el número que muestra el recuadro del mouse (el recuadro siempre muestra el nivel sin suavizar). Con el suavizado en Off, la sonda y el recuadro coinciden. La cruz en pantalla sigue a la sonda, así que alguien que ve la pantalla puede ver dónde estás.
  - **Pendiente de la señal**: cómo se inclina la curva en el gráfico de 50 Hz a 10 kHz, por ejemplo "cae 1.5 dB por octava, más oscuro que la línea", "sube 0.3 dB por octava, más brillante que la línea" o "plana, sigue la línea". La Pendiente deja plana una mezcla equilibrada, así que esto es cuánto más oscura o más brillante es la mezcla de lo que espera la Pendiente. Como para el ojo, un pico o una muesca estrecha, por ejemplo un solo tono fuerte, no la cambia. Con Pendiente en 0 es la inclinación propia de la señal: el ruido rosa cae 3 dB por octava, una mezcla típica unos 4,5.
  - **Forma respecto a la línea**: el primer vistazo a la imagen. Primero la altura de la línea, después los tramos de la curva que sobresalen por encima o por debajo de ella (más abajo, qué cuenta como sobresalir), de graves a agudos, con su distancia media, por ejemplo "línea en -42 dB; de 35 a 80 Hz +5 dB, de 85 a 400 Hz -4 dB, de 450 Hz a 2 kHz +3 dB, por encima de 12 kHz -8 dB". Un rango que no se menciona sigue la línea. Siempre mira una vista amplia de 1/3 de octava, sea cual sea el Suavizado, así el panorama no cambia mientras te acercás a los detalles.
  - **Enmascaramiento**: con Enmascaramiento activado en los ajustes, los rangos enmascarados y los que están bajo el umbral de audición, de grave a agudo, por ejemplo "enmascarado: de 870 Hz a 1.3 kHz; bajo el umbral de audición: por encima de 18 kHz", o "nada enmascarado". Con Off, la fila dice "Off". La sonda, los picos y los valles agregan "enmascarado" o "bajo el umbral de audición" cuando su frecuencia lo está.
  - **Picos y valles** (hasta 10 de cada uno): lugares donde la curva sobresale de las frecuencias vecinas, primero los más marcados. Cuánto sobresale una joroba se mide como lo ve el ojo: por encima del más alto de los dos valles que tiene al lado (para un hueco, por debajo de la más baja de las dos crestas). El Suavizado fija su tamaño:
    - Con el suavizado en Off, los picos son resonancias estrechas y notas fuertes: frecuencia, nota con cents, cuántos dB sobresale y su nivel en el gráfico.
    - Con el suavizado activo, son jorobas y huecos: frecuencia, dónde empieza y dónde termina (por ejemplo "de 290 a 400 Hz", práctico para una banda de EQ) y cuántos dB sobresale.

    La lista guarda los 10 más marcados de cada tipo; la fila Suavizado dice cuántos hay en total. Sin suavizado hay decenas de puntas y la lista se queda con las más altas, como las elige el ojo; con 1/6 de octava hay pocas jorobas y casi siempre entran todas. Si nada sobresale, la fila dice "ninguno". Los valles sólo se listan con el suavizado activo. **Intro** en un pico o un valle lleva la sonda ahí y el cursor a la fila de la sonda, así podés recorrer los alrededores por semitonos. **Q** en un pico o un valle dice el Q de una campana de ecualizador del mismo ancho, por ejemplo "Q 5.3". Se cuenta de la forma habitual, por el ancho a la mitad de la altura en dB (fórmulas RBJ, Equalizer APO y la mayoría de los ecualizadores: una octava es Q 1.41); FabFilter Pro-Q da un número unas 1.4 veces mayor. El suavizado ensancha los picos estrechos, así que si después del Q se dice "limitado por el suavizado", apagá el suavizado o poné 1/24 para medir una resonancia estrecha. Mientras suena la música, la lista y la forma se quedan quietas un segundo, para que las filas leídas una tras otra coincidan.
- **Bandas de tercio de octava**: las 30 bandas estándar de 25 Hz a 20 kHz, con el nivel de cada una.
- **Ajustes**: los botones debajo del gráfico, primero los que más importan para escuchar:
  - **Ponderación**: Pendiente, A, B, C, D, K, ITU-R 468, ISO 226, curva X inversa, curva N inversa. Después del valor se dice la nueva pendiente de la señal.
  - **Enmascaramiento**: On u Off.
  - **Pendiente**: de 0 a 4,5 dB por octava. Después del valor se dice la nueva pendiente de la señal, así escuchás el efecto enseguida. Se usa solo cuando Ponderación está en Pendiente; si no, la fila lo dice, y cambiarla vuelve a la pendiente.
  - **Suavizado**: Off, 1/24, 1/12, 1/6 o 1/3 de octava. Con 1/3 de octava queda sólo el equilibrio general, la vista habitual para comparar mezclas. Después del valor se dice cuántos picos y valles sobresalen con ese suavizado, por ejemplo "1/6 de octava, picos: 7, valles: 5": escuchás qué tan cargada está la imagen.
  - **Promedio**: Off, un promedio móvil (se dice su tiempo) o Infinito.
  - **Peak hold**: Off, 12 dB por segundo, 30 dB por segundo, Infinito.
  - **Rango**: 60, 90 o 120 dB, hasta dónde baja el gráfico por debajo de 0 dB. También fija qué cuenta como sobresalir, ver más abajo.
  - **Tamaño de FFT** (de 1024 a 32768 puntos), **Ventana** (Hann, Blackman-Harris, Kaiser), **Solape**, **Canal** (izquierdo, derecho, Mid, Side, izquierdo y derecho), **Visualización** (Curva FFT, Barras de tercio de octava, Barras Bark).
- **F1** en la sonda, la pendiente de la señal, la forma, un pico o valle, o cualquier ajuste lo explica en una o dos frases.

**Qué cuenta como sobresalir.** Un analizador no tiene umbral: el ojo nota una joroba cuando tiene cierta altura en pantalla. El gráfico ocupa siempre los mismos píxeles, así que esa altura depende del Rango. TELESCOPE cuenta un tramo, un pico o un valle cuando sobresale una veinteava parte del Rango: 3 dB con 60 dB, 4,5 dB con 90, 6 dB con 120. Es igual con cualquier suavizado, así que sólo el suavizado decide cuánto detalle hay, como en pantalla. Para escuchar jorobas más chicas, elegí un Rango más chico, igual que haría alguien que ve la pantalla para verlas.

Una forma de usarla mientras mezclás:

1. Para una canción entera, poné Promedio en Infinito y presioná Reiniciar (en la lente Loudness o Dinámica) al empezar.
2. Leé Pendiente de la señal y Forma: ¿la mezcla es más brillante o más oscura que la línea, y qué rangos sobresalen o se hunden?
3. Suavizado en 1/6 de octava: los picos y valles son las jorobas y los huecos dentro de esos rangos, con sus bordes.
4. Suavizado en Off: los picos son resonancias estrechas y notas fuertes. Una resonancia aparece en el nivel y en el Peak hold; un pico que sólo está en la retención fue un golpe corto.
5. Intro en un pico, y después más y menos alrededor para escuchar su ancho. Cuando necesites todos los números del mismo momento, usá Pausa en la lente Loudness o Dinámica: detiene todas las lentes, Espectro incluida.

Los ajustes de FFT se comparten con Espectrograma, Cascada y las demás lentes espectrales: es un solo ajuste, no una copia.

---

## 4 · Espectrograma

![Espectrograma: frecuencia en el tiempo con el nivel como color](plugins/telescope/docs/manual/images/spectrogram.png)

**Qué mide.** El nivel en el tiempo: la frecuencia hacia arriba (de 20 Hz a 20 kHz), el tiempo a lo ancho (10, 30 o 60 segundos, el ahora a la derecha), el nivel como color.

**Cómo leerla.** Las escalas de color están elegidas para que más brillante signifique siempre más fuerte (salvo la paleta "spectrum", donde el tono ordena el nivel). Cada fila toma el máximo de sus bins, así que los picos estrechos no se aplastan. Cambiar el tamaño de FFT, el canal, el rango o la historia borra la imagen, porque mezclar columnas medidas de dos maneras daría una imagen que miente.

**La paleta es un solo ajuste para cuatro lentes**: Espectrograma, Espectrograma estéreo, Cascada y Campo.

### Con el teclado

En pantalla, el mouse sobre la imagen muestra un punto: hace cuánto, la frecuencia y el nivel. La sonda del teclado da los mismos números.

- **Lecturas**:
  - **Sonda**: la frecuencia. Más y menos la mueven un semitono, Alt con Re Pág o Av Pág una octava. Por ejemplo "1.05 kHz, -34 dB, hace 2.5 segundos".
  - **Tiempo de la sonda**: hace cuánto mira. Más retrocede un cuarto de segundo, menos va hacia el ahora, Alt con Re Pág o Av Pág una marca de la escala de tiempo (2, 5 o 10 segundos, como en pantalla), Alt con Inicio el ahora, Alt con Fin el momento más antiguo en pantalla. También dice la frecuencia más fuerte en ese momento, el punto más brillante de la columna: "hace 2.5 segundos, 1.05 kHz, -34 dB, lo más fuerte en ese momento: 6.3 kHz, -20 dB".
- **Ajustes**: Historia, Rango, Canal, Paleta (Ovni, Inferno, Viridis, Spectrum). Se comparten con Espectro, así que también los podés cambiar acá.

La sonda da el punto más fuerte dentro de un cuarto de tono y un cuarto de segundo, así no se pierde un clic corto entre dos pasos. La cruz en pantalla sigue a la sonda, así que alguien que ve la pantalla ve dónde mira; su recuadro lee el único punto debajo de la cruz y puede marcar un poco menos.

**Qué busca quien ve la pantalla** en un espectrograma, y cómo encontrarlo con la sonda:

- una resonancia que suena: una línea estrecha que se queda en una frecuencia y sigue después de la nota. Poné la sonda en esa frecuencia y retrocedé en el tiempo: el nivel se mantiene alto;
- zumbido: líneas en 50 o 60 Hz y sus múltiplos, todo el tiempo;
- clics: líneas verticales cortas, fuertes en todas las frecuencias por un instante. Recorré el tiempo en una frecuencia aguda;
- sibilancia: ráfagas cortas entre unos 4 y 10 kHz.

La imagen se mueve mientras suena el audio. Para leer un momento, detené la reproducción y leelo antes de que se desplace fuera; cuanto más larga la Historia, más tiempo tenés. La Pausa de la lente Loudness no sirve acá: el plugin calcula sólo lo que necesita la lente abierta, así que mientras Loudness está abierta el espectrograma no se escribe. En este plugin quien ve la pantalla tiene el mismo límite.

---

## 5 · Cascada

![Cascada: el espectro en el tiempo en 3D](plugins/telescope/docs/manual/images/waterfall.png)

**Qué mide.** Los mismos datos que el espectrograma, dibujados en profundidad: la frecuencia a lo ancho, el nivel como altura, el tiempo yéndose hacia el fondo.

**Cómo leerla.** El ojo compara alturas mejor que colores, así que acá los picos estrechos se destacan. El costo es que un evento corto puede esconderse detrás de uno posterior. Espectrograma y Cascada se complementan.

### Con el teclado

- **Lecturas**: **Sonda**, lo que muestra el mouse en la línea delantera, la más reciente: frecuencia y nivel ahora, por ejemplo "250 Hz, -18 dB, ahora". Más y menos la mueven un semitono, Alt con Re Pág o Av Pág una octava. Como en pantalla, las líneas de detrás no se leen una por una; la sonda de Espectrograma llega hacia atrás en el tiempo sobre los mismos datos.
- **Ajustes**: Líneas (60, 90, 120), Inclinación (Bajo, Medio, Alto), Historia, Rango, Paleta.

---

## 6 · CQT

![CQT: el espectro por notas sobre un teclado de piano, con el cromagrama abajo](plugins/telescope/docs/manual/images/cqt.png)

**Qué mide.** El espectro por **nota**. Una transformada de Q constante reparte sus bins de forma pareja en octavas en lugar de hertz: 24 bins por octava (dos por semitono) desde La0 (27,5 Hz) hacia arriba. Abajo está el cromagrama, la energía de cada una de las 12 clases de nota, y la tonalidad estimada.

**El grave llega tarde, y la lente lo dice.** La resolución de cuarto de tono a 27,5 Hz necesita más de un segundo de audio (1,24 s a 48 kHz), y el tiempo se reduce a la mitad con cada octava hacia arriba. Es física, no un atajo, y la lente lo muestra.

**La tonalidad nunca se dice sola.** Siempre viene con una confianza y la parte del tiempo en que se mantuvo, por dos razones:

1. Hasta el ruido "gana" alguna tonalidad por azar, con una confianza de alrededor de 0,4 a 0,5. Lo que delata al ruido es la parte del tiempo: unos pocos por ciento, contra casi el 100 por ciento de la música real.
2. Hay música que es ambigua de verdad. Una tríada desnuda Do, Mi, Sol puntúa apenas más para Mi menor que para Do mayor; duplicá el Do en el bajo y Do mayor gana claramente.

El silencio no informa ninguna tonalidad, porque "no sé" es un resultado.

### Con el teclado

- **Lecturas**:
  - **Tonalidad**: por ejemplo "La menor, confianza 0.82, la mejor el 64 por ciento del tiempo".
  - **Notas más fuertes**: las tres notas más fuertes del cromagrama.
  - **Sonda**: lo que muestra el mouse sobre una barra: la nota, su frecuencia y su nivel, por ejemplo "La 2, 110 Hz, -12 dB". Más y menos la mueven un semitono, Alt con Re Pág o Av Pág una octava. Entre dos notas hay una barra más tenue de cuarto de tono; si suena más fuerte que la nota, la sonda la nombra con 50 cents ("La 2 más 50 cents"), así no se pierde una nota desafinada.
  - **Nota 1 a 5**: las cinco notas más fuertes del espectro, cada una con octava, desviación en cents y nivel. Intro pone la sonda en la nota.
- **Ajustes**: Canal, Ventana de croma (cuánto promedia el cromagrama: 0,5, 2 o 5 segundos).

Los nombres de las notas siguen el idioma del plugin (letras, do-re-mi o el sistema alemán con H y B), y los sostenidos se dicen con palabras.

---

## 7 · Espiral

![Espiral: el espectro de Q constante enrollado una vuelta por octava](plugins/telescope/docs/manual/images/spiral.png)

**Qué mide.** El mismo espectro de Q constante, enrollado una vuelta por octava, así la misma nota en distintas octavas queda alineada sobre un mismo radio. Muestra de un vistazo la estructura de octavas de lo que suena, con una rueda de croma y la tonalidad en el centro.

### Con el teclado

- **Lecturas**:
  - **Tonalidad**, **Notas más fuertes**, como en CQT.
  - **Sonda**: lo que muestra el mouse sobre una púa: la nota, su frecuencia y su nivel. Las mismas teclas que la sonda de CQT.
  - **La misma nota en cada octava**: las púas que el ojo ve alineadas sobre un rayo desde el centro, para la nota de la sonda: cada octava donde suena por encima del rango, con su nivel, por ejemplo "La 1, -40 dB; La 2, -12 dB; La 3, -25 dB". Más y menos en esta fila cambian la nota.
- **Ajustes**: Canal, Ventana de croma.

---

## 8 · Scope

![Scope: goniómetro, medidor de correlación y osciloscopio](plugins/telescope/docs/manual/images/scope.png)

**Qué mide.** Las formas clásicas de mirar el estéreo, sobre una ventana deslizante de 100, 300 o 1000 ms.

- **Correlación**: +1 es mono, 0 son canales sin relación, -1 es fuera de fase.
- **Ancho**: 0 es mono, 1 son dos fuentes independientes, por encima de 1 domina la señal lateral.
- **Balance** (dB): positivo significa que el canal derecho suena más fuerte.
- **Pérdida mono** (dB): cuánto nivel se pierde al sumar la mezcla a mono. 0 si izquierdo y derecho son iguales, -3 dB para canales independientes, una pérdida muy grande si se cancelan.

**Modos.** *Lissajous* es el goniómetro: se escala solo según la señal para que veas la forma del estéreo, no el nivel, y dice cuál es el nivel del borde. *Muestras polares* conserva el ángulo y hace del radio el nivel. *Nivel polar* (en los ajustes se llama Hemisferio) es un semicírculo con el mono arriba y el izquierdo y el derecho en la base, la energía dibujada como un rayo por grado; lo que está fuera de fase se pliega sobre la base y se escribe el porcentaje. El osciloscopio muestra 40 ms, con disparo opcional en un cruce por cero ascendente.

**Los casos extremos se informan, nunca como números sin sentido.** Sin señal se lee "sin señal", que no es lo mismo que un mono perfecto. Con un canal en silencio la correlación no está definida y se muestra como 0.

**No es localización.** Dirección de paneo por energía, no de dónde viene un sonido en una sala.

### Con el teclado

- **Lecturas** (Direcciones sólo en el modo Hemisferio, como el semicírculo que lee): Imagen, Historial de correlación (la aguja en todo el tiempo desde el reinicio, hasta 10 minutos, un valor por segundo, grabado con cualquier lente abierta: cuánto estuvo por debajo de cero, cuánto por debajo de 0.5 y su valor mínimo, por ejemplo "en 3 minutos 20 segundos: bajo cero 4 segundos, bajo 0.5 40 segundos, mínimo -0.21"), Correlación (con una palabra que dice lo que significa, como "casi mono" o "fuera de fase"), Ancho, Balance ("1.2 dB más fuerte a la derecha", o "centrado"), Pérdida mono, Energía fuera de fase (en por ciento), Medido sobre (el largo de la ventana), Direcciones: dónde está la energía de izquierda a derecha, como la muestra el semicírculo lleno, por ejemplo "centro: 10 por ciento a la derecha, repartido de 40 por ciento a la izquierda a 60 por ciento a la derecha, lo más fuerte en 80 por ciento a la derecha". La energía fuera de fase no está incluida: tiene su propia fila.
- **Ajustes**: Modo (Lissajous, Polar, Hemisferio), Trigger del osciloscopio (On, Off), Ventana, Caída del hemisferio (12, 24 o 48 dB por segundo) y Escala del hemisferio (Relativa o Decibelios), estas dos sólo en el modo Hemisferio, como en pantalla.

La imagen en sí no se describe con texto: su significado son estos números.

**Imagen** dice en palabras lo que muestran el goniómetro y la aguja de correlación, como los lee alguien que ve la pantalla: "casi mono" (una línea vertical), "estéreo normal" (una nube redonda), "muy ancho" (una nube aplastada), "fuera de fase" o "como una polaridad invertida" (una línea horizontal, la mezcla se anula en mono). Lo que importa es dónde se queda la aguja, no un momento: una bajada corta por debajo de cero por una reverb ancha no hace daño, quedarse por debajo de cero significa que partes de la mezcla desaparecen en mono. Por eso la palabra sale de los últimos 10 segundos, seguida de qué parte de ese tiempo la correlación estuvo por debajo de cero y de su valor mínimo. El bajo y el bombo deberían quedarse cerca del mono; revisalos en la lente Correlación por banda.

---

## 9 · Correlación por banda

![Correlación por banda: correlación en cada banda de tercio de octava](plugins/telescope/docs/manual/images/band-correlation.png)

**Qué mide.** Las cifras estéreo de Scope **por banda de tercio de octava**: correlación, ancho, balance y pérdida mono para cada una de las 30 bandas.

**Para qué.** En una mezcla con el bajo en mono y los agudos abiertos, un solo medidor de correlación da un número intermedio que no dice nada. En la señal de prueba del autor (un tono de 80 Hz en mono más ruido fuera de fase por encima de 2 kHz) el medidor de banda ancha marca +0.24, mientras que la banda de 80 Hz marca +1.00 y la de 4 kHz -1.00. Están pasando dos cosas distintas, y sólo una vista por bandas muestra las dos.

**Una banda sin bins de FFT no es cero.** Con tamaños de FFT chicos las bandas más graves no se pueden medir, y la lente lo dice. En los graves las lecturas además se dispersan más, porque ahí abajo hay menos señal por segundo; ningún analizador la puede inventar.

### Con el teclado

- **Resumen**: Correlación más baja, Más pérdida mono (las peores bandas, lo mismo que la línea encima del gráfico), Bandas medidas.
- **Bandas de tercio de octava**: cada banda con los cuatro números a la vez: "correlación 0.95; ancho 0.2; balance 0.1 dB; pérdida mono -0.1 dB". En pantalla el segundo gráfico muestra sólo uno de los tres; acá tenés todos.
- **Ajustes**: Ventana (0,3, 1 o 3 segundos), Gráfico inferior (qué cifra muestra la pantalla debajo de la correlación).

---

## 10 · Espectrograma estéreo

![Espectrograma estéreo: el color es la fase, el brillo es el nivel](plugins/telescope/docs/manual/images/stereo-spectrogram.png)

**Qué mide.** Un espectrograma donde **el color es la fase** (rojo fuera de fase, verde ancho, blanco mono) y **el brillo es el nivel**. Una celda sin energía es negra, así que el piso de ruido no pinta colores sin sentido.

### Con el teclado

- **Lecturas**: **Sonda** y **Tiempo de la sonda**, como en Espectrograma, con la coherencia que muestra el color: "1.05 kHz, -30 dB, coherencia 0.95, casi mono, hace 2 segundos". 1 es mono, 0 ancho, por debajo de 0 fuera de fase. Una celda sin energía es negra en pantalla y no tiene coherencia, así que sólo se dice su nivel. El nivel es el de los dos canales juntos, así que una señal mono marca 3 dB más de lo que Espectro muestra para un canal.
- **Ajustes**: Historia, Rango, Ventana, Paleta.

Para números por banda de tercio de octava, usá Correlación por banda.

---

## 11 · Campo

![Campo: energía por dirección de paneo y frecuencia](plugins/telescope/docs/manual/images/field.png)

**Qué mide.** La energía por **dirección de paneo y frecuencia**, con caída y estela. El paneo va de -1 (sólo izquierdo) pasando por 0 (centro) hasta +1 (sólo derecho).

**Lo que sólo esta lente muestra.** Una mezcla con dos fuentes paneadas a fondo y una mezcla de ruido decorrelacionado dan casi la misma correlación y el mismo ancho. En Campo la primera son dos manchas en esquinas opuestas y la segunda una nube pareja de lado a lado. Esa diferencia es la razón de ser de la lente.

**Lo que no hace.**

- No mide dónde están las fuentes. A partir de una mezcla estéreo terminada eso no tiene respuesta única, así que el eje es izquierda, centro, derecha, nunca grados, y la pantalla dice "energía por dirección de paneo, no es localización".
- No distingue mono de fuera de fase: los dos ponen la misma energía en cada canal. Correlación por banda y Espectrograma estéreo muestran esa diferencia; las tres lentes se leen juntas.
- Los dB acá son **relativos** a la celda más fuerte. Los niveles absolutos están en Espectro.

### Con el teclado

- **Octavas**: una fila por banda de octava, por ejemplo "250 Hz octava, centro 10 por ciento a la izquierda, repartido de 40 por ciento a la izquierda a 20 por ciento a la derecha, -6 dB". El centro es dónde está la energía en el eje de paneo, el reparto cubre la parte principal (del percentil 10 al 90), y los dB son relativos a la octava más fuerte. Un reparto angosto se dice "estrecho"; una octava sin energía, "silencio".
- **Ajustes**: Decaimiento (0,5, 1 o 2 segundos), Ventana (0,3, 1 o 3 segundos), Paleta.

---

## 12 · Balance tonal

![Balance tonal: curvas del programa y de la referencia con la diferencia abajo](plugins/telescope/docs/manual/images/tonal-balance.png)

**Qué mide.** No "qué tan fuerte suena" sino **"de qué color es, comparado con este otro tema"**. Cargá un tema de referencia, tuyo o comercial. TELESCOPE lo analiza entero, fuera de línea, y compara tu programa en vivo contra él, banda por banda.

**Compara inclinación, no nivel.** Las dos curvas se normalizan por su propio loudness integrado, así que el mismo ruido rosa a -14 y a -20 LUFS no muestra ninguna diferencia. Sin eso el gráfico respondería "cuál suena más fuerte", y eso ya te lo dice Loudness.

**Por eso la diferencia suma cero.** Sumá 6 dB a todo lo que está por encima de 2 kHz y el loudness general también sube. La lente muestra entonces unos +1,7 dB arriba y -4,3 dB en los graves: sigue habiendo 6 dB entre las dos zonas, pero contados a igual loudness, que es como se compara una mezcla con una referencia.

**Cómo leerla.** Una escala fija, así dos capturas de la misma mezcla siguen siendo comparables. La franja de ±3 dB en pantalla es una referencia, no un juicio. Una banda que sólo un lado puede medir nunca se muestra como "sin diferencia": la fila dice por qué no se puede comparar.

**La referencia se guarda como una ruta, no como números.** Al reabrir una sesión el archivo se analiza de nuevo. Si ya no está, la lente dice "referencia no encontrada" y no muestra nada, en lugar de una referencia que ya no existe.

### Con el teclado

- **Resumen**:
  - **Estado**: qué está haciendo la lente, como lo dice la línea de estado en pantalla (midiendo, analizando la referencia, etcétera).
  - **Referencia**: el nombre del archivo de referencia, o "ninguno".
  - **Tramo de la referencia**: la parte de la referencia que se usa, o "archivo completo".
  - **Forma respecto a la referencia**: el primer vistazo a la curva de diferencia, como la forma respecto a la línea de Espectro, con la referencia en lugar de la línea. Los tramos anchos donde tu mezcla está 3 dB o más por encima o por debajo de la referencia (la banda dibujada en pantalla), cada uno con su diferencia media, por ejemplo "de 85 a 180 Hz +4 dB, por encima de 11 kHz -6 dB"; o "todo dentro de 3 dB de la referencia". El loudness ya está igualado, así que el nivel general no aparece aquí.
  - **Pendiente respecto a la referencia**: hacia dónde se inclina la diferencia de 50 Hz a 10 kHz, por ejemplo "más oscuro que la referencia, 0.8 dB por octava", o "misma pendiente que la referencia". Una pendiente dice que todo el balance se inclina hacia un lado; una forma sin pendiente señala regiones sueltas.
  - **Mayor diferencia 1 a 5**: las bandas donde tu programa más se aparta de la referencia, por ejemplo "125 Hz, 3.2 dB más fuerte que la referencia".
- **Comparación con la referencia**: tu mezcla frente a la referencia, en las mismas unidades que las lentes Loudness y Dinámica. Tu mezcla cuenta desde el último reinicio; la referencia es el archivo entero o su tramo.
  - **Loudness**: cuánto bajar o subir la referencia para que las dos suenen igual de fuerte, y después el integrado de cada una. Por ejemplo: "la referencia suena 2.6 dB más fuerte, bajala 2.6 dB; tu mezcla -12.4 LUFS, referencia -9.8 LUFS". Mové el fader de la pista de referencia esa cantidad y compará de oído a la misma sonoridad.
  - **Loudness range**, **PLR** y **Máx true peak**: el tuyo y el de la referencia. Un PLR mucho menor que el de la referencia significa que tu mezcla está más comprimida.
- **Bandas de tercio de octava**: cada una de las 30 bandas con el nivel del programa, el de la referencia y la diferencia.
- **Acciones**:
  - **Reiniciar promedio**: vuelve a empezar el promedio del programa y conserva la referencia.
  - **Cargar archivo de referencia**: abre un diálogo de archivo.
  - **Quitar referencia**.
  - **Inicio del tramo** y **Fin del tramo**: elegir la parte de la referencia contra la que comparar, sin mouse. Más y menos mueven un segundo, un paso grande son 10 segundos, Alt+Inicio y Alt+Fin van al principio o al final del archivo. Los nuevos límites se dicen mientras te movés; la referencia se analiza de nuevo cuando parás.
  - **Usar toda la referencia**: lo mismo que un doble clic en la tira.

Cuando termina el análisis de la referencia, TELESCOPE lo dice: "Referencia analizada, ..." con su nombre y su loudness, o el motivo por el que no se pudo leer.

---

## 13 · Veredicto

![Veredicto: titular, hallazgos por sección y la lista de lo que está dentro de rango](plugins/telescope/docs/manual/images/verdict.png)

Doce lentes muestran. Ésta **dice**. La regla es que cada frase lleva el número que la sostiene.

**Qué es.** Un motor de reglas fijo: sin IA, sin red. Una tabla de umbrales y las condiciones que los leen. La misma señal da el mismo informe, palabra por palabra, hoy y dentro de un año. Cada umbral con su razonamiento está en [`telescope-diccionario.md`](plugins/telescope/docs/telescope-diccionario.md).

**Cómo se lee.** Primero un **titular**: el conteo de lo medido, sin adjetivos, por ejemplo "15 chequeos dentro de rango · 3 para revisar, el primero en 0:20". Después las secciones:

- **Dentro de rango**: cada regla que se chequeó y no saltó, con su número y su límite, por ejemplo "Los transitorios tienen aire: PSR 9.4 dB (piso 8.0 dB)". Una regla que no se pudo chequear no dice nada; "dentro de rango" es una medición, no un valor por defecto.
- **Cómo se va a sentir**: transitorios aplastados, medios graves flacos o embarrados, presencia áspera, falta de aire arriba, un centro hueco, y la tonalidad.
- **Dónde traduce**: chequeos genéricos para teléfono, auriculares, notebook, auto, club (sub en mono) y hi-fi.
- **Qué revisar y dónde**: huecos en bandas, tramos bajos y altos, ráfagas de clips, bandas fuera de fase, desbalance izquierda-derecha, offset de DC y objetivos de plataformas de streaming.

**Cómo se lee un hallazgo.** Primero el número, el término de mezcla entre paréntesis y al final dónde mirar, nunca qué hacer: "630 Hz cae 26.0 dB bajo su propia media entre 0:20 y 0:35. Revisá qué suena en esa banda ahí." La gravedad es problema, advertencia o nota; problema se reserva para lo que está roto de forma objetiva (ráfagas de clips, DC, bandas que se cancelan en mono).

**Contra qué compara.** Con una referencia cargada en Balance tonal, contra la forma de la referencia. Sin ella, contra la tendencia espectral propia de tu material, una recta ajustada a través de tu mezcla. Lo segundo es deliberado, para que ninguna "curva de buena mezcla" escondida pase por medición, pero es tosco: una mezcla con una intención fuerte puede hacer saltar una regla sin que haya nada que arreglar. **Cargar una referencia siempre es mejor.**

**Lo que no es.**

- No da opiniones. Nunca dice "profesional" ni "listo"; los tests del autor revisan cada frase en cada idioma buscando palabras así.
- Los chequeos de dispositivos son definiciones genéricas de clases de sistemas, no mediciones ni simulaciones de parlantes reales.
- **"Sin hallazgos" no es "está terminado".** Cuando no salta nada, dice: "Nada fuera de rango en estas reglas. Miden; lo que no escuchan es tuyo."

**En vivo o archivo.** En modo en vivo Veredicto usa los últimos 10 minutos desde el reinicio, una vez por segundo. La parte espectral sólo se mide mientras Veredicto o Balance tonal están abiertos, así que si abrís Veredicto a mitad de un tema, el informe empieza ahí. Para tiempos exactos sobre un tema entero, usá el modo **archivo**: el análisis de archivo es idéntico al en vivo.

### Con el teclado

- **Resumen**: Estado (la línea de estado en pantalla), Resultado (el titular).
- **Cómo se va a sentir**, **Dónde traduce**, **Qué revisar y dónde**: una fila por hallazgo. Cada fila lee la frase completa con sus números, seguida de su gravedad: problema, advertencia o nota (para los dispositivos: OK, advertencia o no pasa). Una sección vacía es una fila que lo dice.
- **Dentro de rango**: una fila por cada regla que se chequeó y pasó.
- **Acciones**:
  - **Reiniciar análisis**.
  - **Modo**: En vivo o Archivo; más elige Archivo, menos En vivo, Intro cambia.
  - **Cargar un archivo para evaluar**: abre un diálogo de archivo y pasa al modo archivo.

Los nombres de las secciones y los hallazgos se dicen en el idioma del plugin. Cuando termina el análisis de un archivo, TELESCOPE dice "Archivo analizado" y lee el titular.

---

# El resto

## Analizar un archivo

Balance tonal (referencia) y Veredicto (modo archivo) pueden analizar un archivo de audio entero en segundo plano, con Intro en su fila de carga. Con el mouse también podés arrastrar un archivo sobre esas lentes.

- **El análisis fuera de línea es idéntico al en vivo**, bit a bit. El motor cuenta muestras, no bloques del host, y el camino del archivo usa el mismo código.
- **Formatos**: WAV, AIFF, FLAC y Ogg; en Windows también WMA y MP3. Se acepta cualquier archivo; si no se puede leer, se te dice por qué.
- **La frecuencia de muestreo es la del archivo.** No se remuestrea nada, y ni las bandas de tercio de octava ni los LUFS dependen de la frecuencia de muestreo, así que una referencia a 44,1 kHz se compara directamente con una sesión a 48 kHz.
- **Es rápido**: un minuto de audio tarda bastante menos de un segundo en una computadora moderna.
- **Sólo un archivo nuevo empieza un análisis nuevo.** Al reabrir un proyecto el archivo se analiza otra vez, pero deshacer, los presets y A/B no lo hacen, mientras el archivo y el tramo sean los mismos.

## Idiomas

La fila **Idioma** del panel del plugin cambia todo el plugin: la pantalla y la voz. Siete idiomas:

- **inglés** (por defecto) y **español**, revisados por el autor;
- **portugués, francés, alemán, italiano**: traducidos por el autor con los términos de la industria, todavía sin revisión de hablantes nativos;
- **ruso**: agregado en este fork, en pantalla y en la voz.

La voz en español, portugués, francés, alemán e italiano se tradujo para este fork y no fue revisada por hablantes nativos. Si falta una frase, esa frase sale en inglés y el resto queda en tu idioma.

Las unidades (dB, Hz, kHz, ms, LUFS) y los términos de la industria que normalmente quedan en inglés (Loudness, Scope, Short-term, Momentary, True peak, Peak hold, Bypass) quedan en inglés en todos los idiomas, como en las pantallas del autor.

Los números se dicen con punto decimal en todos los idiomas, como en pantalla: "-14.2 LUFS". En el texto de este manual, los decimales fuera de las comillas se escriben con coma, como es habitual en español.

**Los nombres de las notas siguen la convención de cada idioma**: letras (C, D, E) en inglés y ruso, do-re-mi en español, portugués, francés e italiano, y el sistema alemán con H y B.

## Límites conocidos

- La versión para macOS es experimental: compila y pasa la validación, pero no se probó con VoiceOver.
- Probado con NVDA en REAPER. Otros hosts y lectores de pantalla deberían funcionar, pero no se probaron.
- Si otro programa mantiene bloqueado el archivo de ajustes compartido más de un segundo, el tema, el idioma o el tamaño de ventana no se guardan esa vez, y una ventana nueva abre con los valores por defecto.
- Las fuentes principales no tienen cirílico. Con el ruso elegido, el sistema sustituye la fuente, así que el texto en ruso se ve distinto del resto.

## Licencia y créditos

- **TELESCOPE** es de [OVNI Audio](https://ovniaudio.com), gratis. La versión 0.2.0, en la que se basa este fork, se publicó como open source bajo la **GNU AGPLv3**. El autor ya no publica el código fuente, así que los cambios posteriores no llegan a este fork. Sitio del autor: <https://ovniaudio.com/telescope>.
- La voz y el control por teclado, sus traducciones y la traducción al ruso se agregaron en [fdrv161/periscope](https://github.com/fdrv161/periscope/tree/accessible), bajo la misma licencia. El código completo de cada versión está en ese repositorio, en la etiqueta de la versión.
- Avisos de terceros: [`NOTICE.md`](NOTICE.md).

**Cómo se hizo.** La voz y el control por teclado, sus traducciones, la traducción al ruso, los scripts de compilación y esta documentación los escribió Claude, un modelo de IA de Anthropic, trabajando en Claude Code a pedido de [fdrv161](https://github.com/fdrv161). La versión para Windows fue probada a mano con NVDA en REAPER por fdrv161. La versión para macOS no la probó ninguna persona.
