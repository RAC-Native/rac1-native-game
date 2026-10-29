# Wii U Port — Game Logic, Level Transitions, Cutscenes & Main-Branch Sync (`TASKS_LOGIC.md`)

Questo file coordina il lavoro dei subagenti (Gemini 4) sul branch `port/wiiu` in `/home/daino/progetti/rac1-native-game-wiiu`.

## Regole di Coordinamento (Zero Overlap)
1. **Zero sovrapposizione di file o task:** Ogni subagente lavora esclusivamente sui file assegnati alla propria Track. Nessun file può essere modificato da due subagenti contemporaneamente.
2. **Massimo 2 subagenti concorrenti** per rispettare i limiti di RAM (`MemoryMax=6G` per build, `-j3` max).
3. **Aggiornamento progressivo:** Ogni subagente aggiorna la propria sezione in questo file (`ports/wiiu/TASKS_LOGIC.md`) con l'analisi, le modifiche effettuate e i test eseguiti.
4. **Invarianti obbligatorie:**
   - Lavorare **solo** in `/home/daino/progetti/rac1-native-game-wiiu` (branch `port/wiiu`). Non toccare mai `/home/daino/progetti/rac1-native-game`.
   - Non lavorare su micro-ottimizzazioni CPU/GPU per i 60 fps né sugli effetti 3D mancanti (particelle/drop shadows), lasciati all'altro agente.
   - Tutte le verifiche PC (`pc_check.sh`, `gt_check.sh`) devono restare verdi al 100%.

---

## Suddivisione Track e File Ownership

### Track A — Crash Veldin -> Novalis, Planet Travel (`flight`), Level Transitions e Sincronizzazione Game Thread Wii U
- **Stato:** `[COMPLETATO]`
- **File assegnati (esclusivi Track A):**
  - `src/host/orig_main.cpp` (loop `no_graphics` / Wii U, gestione `exiting`, `flight`, `story`, sincronizzazione `wiiu_overlap` / `rac1::game_thread` durante `level_exit_start()` / `level_enter()`, guardia `mrp_moby_ready()` / `mrp_world_ready()` su `no_graphics_skip` e `g_world_pass_ranges`)
  - `src/game/ovl/level_exit.cpp`, `src/game/ovl/level_change.cpp`, `src/game/ovl/arrival.cpp`
  - `runtime/level/level_loader.cpp`
  - `ports/wiiu/game/mrp_world_gx2.cpp`, `ports/wiiu/game/mrp_moby_gx2.cpp`
- **Obiettivi:**
  1. **Race condition Game Thread (`wiiu_overlap`) su Level Exit / Flight Load:**
     - Quando il livello termina (`!overlay_frame()` -> `level_exit_start()`), il frame precedente aveva `wiiu_pend.plain == true` e `exiting == false`. Di conseguenza il frame che avvia `level_exit_start()` gira sul **game thread (Core 2)** in parallelo a `rac1_wiiu_frame()` (Core 1) e al worker del mondo MRP (Core 0).
     - `level_exit_start()` -> `exit_to_loading_screen()` -> `flight_files_read()` -> `mrp_flight_load()` distrugge/ricarica `g_world` e `g_level` mentre Core 0 (`mrp_world_wiiu_finish`) e Core 1 (`mrp_world_gx2_draw` / `mrp_moby_gx2_draw`) stanno ancora disegnando il frame precedente!
     - Inoltre `level_prefetch()` parte concorrentemente.
     - Sincronizzare/attendere il completamento del draw/worker (`rac1::game_thread::wait_draw()` o `mrp_world_wiiu_join()` / non eseguire `level_exit_start()` prima che il draw del frame precedente abbia finito) e verificare tutte le transizioni `level0 -> level1` (sia `LevelExit::Movie` Veldin->Novalis sia `LevelExit::Load` con `flight`).
  2. **Sicurezza MRP su Wii U quando gli asset di un livello/flight mancano o cambiano:**
     - Su Wii U `RAC1_NO_GRAPHICS=1` attiva `no_graphics_skip`, che forza `g_moby_mrp_skip = true` anche se `mrp_moby_ready()` è `false`, e `g_world_pass_ranges_on = true` salta le catene del mondo anche se `mrp_world_ready()` è `false`. Verificare il comportamento con e senza asset MRP per `flight`, `title` e `level1..18`.
     - Verificare `mrp_world_gx2.cpp` e `mrp_moby_gx2.cpp` durante il cambio `level0 -> flight -> level1` e `level0 -> level1` (`GX2DrawDone()`, reset buffer/texture, `mrp_world_wiiu_join()`).
  3. **Test PC multi-livello sul path Wii U:**
     - Testare con `RAC1_NO_GRAPHICS=1 RAC1_NO_GRAPHICS_WALK=1 RAC1_WIIU_GAME_THREAD=1` e `RAC1_GOTO=1@60,2@180` oltre al flusso di uscita da Veldin a Novalis.

### Track B — Cutscene FMV (`.pss` MPEG-2 Video + Audio + Rendering GX2 su TV/GamePad) e Diagnostica Heap Wii U
- **Stato:** `[COMPLETATO]`
- **File assegnati (esclusivi Track B):**
  - `runtime/ipu/movie_hw.cpp`, `runtime/ipu/movie_hw.h`
  - `src/game/lib/movie_player.cpp`, `src/game/ovl/movies.cpp`, `src/game/ovl/cutscene.cpp`
  - `ports/wiiu/game/wiiu_gfx.cpp` (rendering GX2 dei frame video `movie_hw_shown(mw, mh)` su TV e GamePad + fix diagnostica heap `mallinfo()` alla riga 1021)
  - `ports/wiiu/game/wiiu_audio.cpp`, `runtime/iop/snd989_iop.cpp` (verifica/collegamento audio `.pss` `movie_hw_audio_take` su Wii U)
  - `ports/wiiu/game/CMakeLists.txt`
- **Obiettivi:**
  1. **Decodifica Video MPEG-2 (`.pss`) senza dipendere da `libavcodec` di sistema:**
     - Su Wii U `RAC1_HAVE_AVCODEC` non è definito, quindi `movie_hw_available()` ritorna `false` e tutti i filmati `.pss` (intro, storia Veldin->Novalis, cutscene pre-renderizzate) vengono saltati.
     - Il demuxer `.pss` (`Demux`) e il decoder audio SPU2 ADPCM (`TrackDecoder`) in `runtime/ipu/movie_hw.cpp` sono già in C++ portabile.
     - Integrare un decoder MPEG-1/MPEG-2 video portabile e self-contained in C/C++ come fallback quando `RAC1_HAVE_AVCODEC` non è definito (oppure per tutte le piattaforme senza ffmpeg, verificando che decodifichi i flussi MPEG-2 dei `.pss` PS2 di Ratchet & Clank, inclusi I/P/B frames 512x416 / 512x448).
  2. **Rendering GX2 dei filmati in `ports/wiiu/game/wiiu_gfx.cpp`:**
     - Quando `movie_hw_shown(mw, mh)` restituisce un frame RGBA (`const std::vector<uint8_t>*`), caricarlo in una `GX2Texture` (`GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8`, `GX2_TILE_MODE_LINEAR_ALIGNED`) e disegnarlo sull'area 4:3 di TV e GamePad tramite `wiiu_blit.gsh` (o `mrp_2d_gx2`).
  3. **Audio dei filmati `.pss` su Wii U:**
     - Verificare dove `movie_hw_audio_take()` viene consumato (in `audio_out.cpp` su PC vs `wiiu_audio.cpp` su Wii U) e assicurarsi che `wiiu_audio.cpp` mixi correttamente i campioni stereo a 48 kHz dei filmati `.pss`.
  4. **Fix log heap in `wiiu_gfx.cpp`:**
     - In `wiiu_gfx.cpp:1021`, `MEMGetTotalFreeSizeForExpHeap(MEM_BASE_HEAP_MEM2)` riporta sempre `0 KB` perché WUT alloca l'intero MEM2 in un singolo blocco all'avvio per `malloc` di newlib. Aggiungere la lettura di `mallinfo()` (`fordblks` e spazio non allocato nell'arena) per mostrare la reale memoria libera nel log `[wiiu]`.

### Track C — Porting Commit Logica/QoL/2D da `main` a `port/wiiu` e Rumble Wii U
- **Stato:** `[COMPLETATO]`
- **File assegnati (esclusivi Track C):**
  - Cherry-pick / backport dei 5 commit da `main` non ancora presenti in `port/wiiu`:
    - `6ea00d1`: `Debug: a level change during a movie stops the movie first; the free hoverboard's jump (#101)`
    - `3a68baf`: `MRP 2D: no more texture churn on the title and the pause menu (identical re-decodes keep their image, a pool by size, render targets kept by page and size)`
    - `96275ac`: `MRP 2D: the Vendor's panels filtered as the original (its moby programs run for their GS registers); RAC1_2D_RT_SCALE for larger render targets`
    - `6539668`: `Vendor: All Ammo is a real entry of the game's own list, selected when the list opens`
    - `25cb39b`: `QoL sprint: Direct mouse look (#117) and the wheel (#61), aiming without the zoom (#70), controller vibration (#10), a ship selector (#94), Clank's modern movement, a first step (#57)`
  - `ports/wiiu/game/mrp_2d_gx2.cpp` (adattamento al fix anti-churn `3a68baf` + pool render targets `g_rt` + filtro pannelli Vendor `96275ac`, eliminando l'accumulo di 45+ MB di texture nel menu di pausa su Wii U)
  - `ports/wiiu/game/wiiu_input.cpp` / `ports/wiiu/game/wiiu_stubs.cpp` (collegamento vibrazione controller `host_pad_rumble` a `VPADControlMotor` su Wii U GamePad)

---

## Diario di Avanzamento dei Subagenti

### Log Track A
- **Analisi e Root Cause Identificate:**
  1. **Data Race `wiiu_overlap` su Level Exit / Flight Load / Story Cards (`src/host/orig_main.cpp`):**
     - Nell'ultimo frame di un livello, all'inizio dell'host frame `exiting` è ancora `false`, quindi `wiiu_overlap` è `true` e `game_frames()` gira su Core 2 (`draw_reads_gs = false`) in parallelo a `rac1_wiiu_frame()` su Core 1 e a `PrepWorker` (`mrp_world_renderer.cpp`) su Core 0.
     - Sul percorso `LevelExit::Load` (`flight`), `level_exit_start()` invoca `exit_to_loading_screen()` -> `loading_screen_setup()`, sovrascrivendo EE RAM (`0x00100000`, `0x0015ed84`, `OV(0x00173ec0)`, `OV(0x00197bc0)`, `OV(0x0019a980)`, `OV(0x0015ffd8)`) e GS VRAM (`gs_ram_upload` a base 0, letta da `PrepWorker::vram_texture`) mentre Core 0 e Core 1 stanno ancora preparando/disegnando il livello precedente.
     - Sul percorso `LevelExit::Movie` (prima transizione Veldin -> Novalis con story cards/cutscene), poiché su Wii U `steps` è 2..4 (`k = 0..steps-1` in `game_frames()`), `k = 0` imposta `exiting = true` (con `RAC1_ORIG_FADES=0` `screen_fade` non sospende) e **`k = 1` nella stessa chiamata `game_frames()` su Core 2** esegue `level_exit_frame()` -> `card_start()`, sovrascrivendo `0x002c3b00`, `0x004c3b00` e GS VRAM mentre Core 0 e Core 1 stanno ancora leggendo il frame di Veldin.
  2. **Allocazione ridondante di 40-55 MB in MEM2 e lettura SD da 25-35 MB in `LevelLoader::prefetch` / `LevelLoader::load_level_wad` (`runtime/level/level_loader.cpp`, `src/host/orig_main.cpp`):**
     - `LevelLoader::prefetch(level)` (avviata in `level_exit_start()`) e `LevelLoader::load_level_wad(level)` (chiamata in `level_host_load()`) leggevano l'intero WAD del livello (25-35 MB) dalla SD card su un thread `std::async`, allocavano 40-55 MB di MEM2 (`p->file` + `p->core_data`) durante la scena di volo (`flight`), copiavano il core in `host_layout::CORE`, le texture in `host_layout::TEX_HEAP` e decodificavano `g_decoded_sky`. Tutti questi dati sono usati **esclusivamente** dai vecchi renderer OpenGL host (`tfrag_render.cpp`, `tie_render.cpp`, `shrub_render.cpp`, `moby_render.cpp`, `sky_render.cpp`), non compilati su Wii U (dove `engine_image_load(level)` carica già direttamente i ~500 KB dell'overlay tramite `disc_read_level_overlay`).
     - Inoltre `level_prefetch()` in `orig_main.cpp` lanciava un `std::thread` detached che leggeva l'header big-endian senza `le32` e invocava `disc_will_need` (no-op su Wii U, ma con contesa su `g_files_m` ed eviction nella cache `kOpenFiles` da 16 slot).
  3. **Ordine di `mrp_moby_begin_frame` rispetto a `mrp_chain_start` su Wii U (`src/host/orig_main.cpp`):**
     - Nel blocco `walk` di Wii U, `mrp_chain_start()` avviava il chain worker su Core 0 (che legge `mrp_moby_active()` tramite `vu1::g_start_hook`) **prima** che Core 1 chiamasse `mrp_moby_begin_frame()`, causando una data race su `g_frame.active` e `g_level` durante i cambi di livello/modalità e valutando `mrp_moby_ready()` sullo stato del frame precedente.
  4. **Picco transiente di memoria MEM2 nei cambi livello in `mrp_world_gx2.cpp` e `mrp_moby_gx2.cpp`:**
     - In `mrp_world_gx2_draw()` e `mrp_moby_gx2_draw()`, al cambio di `L.loads` (`level0 -> flight -> level1` o `level0 -> level1`), `tex_sync(L)` allocava tutti i texture array del nuovo livello mentre i vertex/index buffer (`g_geo`), il piano merged (`g_mlevel`, `g_mg`) e i colori residenti (`g_cols`) del vecchio livello erano ancora allocati in MEM2. In `mrp_moby_gx2.cpp`, inoltre, `geo_copy()` allocava il nuovo buffer `nb` prima di liberare `buf` anche quando `from == 0`.
- **Modifiche Effettuate:**
  - `src/host/orig_main.cpp`:
    - Aggiunta `sync_main_for_transition()` in `game_frames()` (prima di `RAC1_GOTO`, all'ingresso di `if (exiting)` e prima di `level_exit_start()`): quando `game_frames()` gira sul game thread (`on_game_thread()`), sincronizza con il main thread tramite `rac1::game_thread::on_main` (attendendo la fine di `rac1_wiiu_frame()`) ed esegue `mrp_chain_join()` e `mrp_world_wiiu_join()` prima che la transizione tocchi EE RAM o GS VRAM.
    - Spostata `mrp_moby_begin_frame()` (preceduta da `mrp_chain_join()`) prima di `mrp_moby_ready()` e `mrp_chain_start()` nel blocco `walk` `#if defined(__WIIU__)`, allineando l'ordine a `chain_stage` su PC.
    - Disattivato il thread detached di `level_prefetch()` su `#if defined(__WIIU__)` e corretto l'endianness (`rac1::le32`) nella lettura dell'header LBA.
  - `runtime/level/level_loader.cpp`:
    - Guardate `LevelLoader::prefetch(int level)` e `LevelLoader::load_level_wad(int level)` con `#if defined(__WIIU__)` (early return), eliminando la lettura SD da 25-35 MB e l'allocazione MEM2 da 40-55 MB durante `flight` e `level_host_load`.
  - `ports/wiiu/game/mrp_world_gx2.cpp`:
    - In `mrp_world_gx2_draw()`, quando `L.loads != g_tex.loads || L.loads != g_geo.loads`, esegue un singolo `GX2DrawDone()` e libera preventivamente `tex_free()`, `geo_free()`, `cols_free()`, `g_mlevel` e `g_mg` prima di `tex_sync(L)` e `geo_sync(L)`.
  - `ports/wiiu/game/mrp_moby_gx2.cpp`:
    - In `mrp_moby_gx2_draw()`, quando `L.loads != g_tex.loads || L.loads != g_geo.loads`, esegue `GX2DrawDone()`, `tex_free()` e `geo_free()` prima di `tex_sync(L)`.
    - In `geo_copy()`, quando `from == 0 && buf != nullptr`, libera `buf` dopo `GX2DrawDone()` prima di allocare il nuovo buffer `nb`.
- **Compilazione e Verifiche:**
  - Compilato `rac1-native` in `build/` (0 errori) e gli object file Wii U in `build-wiiu/` (`orig_main.o`, `level_loader.o`, `mrp_world_gx2.o`, `mrp_moby_gx2.o`, 0 errori/warning).
  - Test di determinismo `RAC1_RAMHASH` multi-livello (`RAC1_GOTO=0@60,1@240,2@420` con `flight` e `story cards` Veldin -> Novalis):
    - Path Wii U (`RAC1_NO_GRAPHICS=2 RAC1_NO_GRAPHICS_WALK=1` con `RAC1_WIIU_GAME_THREAD=0` vs `=1`): 100% bit-identical sia con `RAC1_STEPS=1` sia con `RAC1_STEPS=3`.
    - Path PC (`RAC1_GAME_THREAD=0` vs `RAC1_GAME_THREAD=2 RAC1_PIPELINE=1`): 100% bit-identical.

### Log Track B
- **Analisi e Root Cause Identificate:**
  1. **Decodifica Video MPEG-2 (`.pss`) e Gestione Memoria (`runtime/ipu/mpeg2_dec.h`, `runtime/ipu/movie_hw.cpp`):**
     - Verificato il decoder MPEG-2 built-in (`rac1::mpeg2::Decoder` in `runtime/ipu/mpeg2_dec.h`) e l'integrazione in `runtime/ipu/movie_hw.cpp` per la compatibilità Big-Endian (`__WIIU__`) e la gestione dello stack/heap.
     - In `movie_hw_close()`, `m.mpeg2_dec = mpeg2::Decoder{};` allocava un temporaneo di ~31 KB (`VlcTables`) sullo stack del thread chiamante; sostituito con `m.mpeg2_dec.reset()` estendendo `Decoder::reset()` per rilasciare i buffer `ref[0]`, `ref[1]`, `b_buf` ed `es_buf`. Aggiunto `g_shown.shrink_to_fit()` in `movie_hw_show_clear()` per liberare subito i ~900 KB del frame RGBA a fine filmato.
  2. **Rendering GX2 dei filmati `.pss` su TV e GamePad (`ports/wiiu/game/wiiu_gfx.cpp`):**
     - Quando `movie_hw_shown(mw, mh)` restituisce un frame RGBA valido, `movie_sync()` (chiamata dopo `GX2WaitTimeStamp(g_frame_ts)`) alloca lazily `g_movie_tex` (`GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8`, `GX2_TILE_MODE_LINEAR_ALIGNED`, `GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_1)`), copia riga per riga `mw * 4` byte usando lo stride `g_movie_tex.surface.pitch * 4` ed esegue `GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, ...)`.
     - In `draw_frame()`, quando `g_movie_active` è attivo, sincronizza `mrp_world_wiiu_join()` e disegna `g_movie_tex` tramite `movie_blit()` (`shaders/wiiu_blit.gsh`) nel viewport 4:3/widescreen sia sul buffer TV sia sul buffer GamePad (o tramite copia scan buffer in `pad_mode == 1`).
     - Quando il filmato termina (`movie_rgba == nullptr`), `movie_tex_free()` esegue `GX2DrawDone()` e libera `g_movie_tex.surface.image` da MEM2.
     - Esteso `g_blit_vb` a 8 slot da 64 byte (`(g_blit_half + 1u) & 7u`) per evitare qualsiasi aliasing tra blit multipli (TV + GamePad) su frame in overlap.
  3. **Audio dei filmati `.pss` su Wii U (`ports/wiiu/game/wiiu_audio.cpp`):**
     - Verificato il flusso audio tra `orig_main.cpp` (`sound_vsync`), `runtime/iop/snd989_iop.cpp` (`snd989_mirror_next` / `snd989_mirror_alone`) e `wiiu_audio.cpp` (così come `src/host/audio_out.cpp` su PC): i campioni `movie_hw_audio_take()` vengono prelevati a ogni vsync dal game thread, registrati nel mirror IOP e sommati con clamping `[-32768, 32767]` e guadagno `snd989_movie_gain * g_movie_volume` via `movie_hw_audio_add()` direttamente dentro `snd989_mirror_next()`.
     - In `wiiu_audio.cpp`, protetto l'euristica di stallo a 20 ms (`stalled`) con `!rac1::movie_hw_is_open()` in modo che la cadenza a 30 fps (33.3 ms, 2 vsync per frame video) del player `.pss` non inneschi frame `snd989_mirror_alone` spuri durante la riproduzione delle cutscene.
  4. **Diagnostica Heap in `ports/wiiu/game/wiiu_gfx.cpp`:**
     - Aggiornato il log periodico ogni 300 host frame usando `mallinfo()` (`<malloc.h>`) insieme a `MEMGetTotalFreeSizeForExpHeap(MEM_BASE_HEAP_MEM2)`, riportando sia i KB liberi nell'ExpHeap MEM2 sia i KB liberi (`mi.fordblks / 1024`) e usati (`mi.uordblks / 1024`) nell'arena `malloc` di newlib.

### Log Track C
- **Analisi e Modifiche Effettuate:**
  1. **Eliminazione Churn Texture 2D e Render Target in `ports/wiiu/game/mrp_2d_gx2.cpp` (porting commit `3a68baf` e `96275ac`):**
     - **Pool Texture per Dimensione (`g_pool` / `FreeTex`) e Riuso In-Place (`fill_texture`):** `TexEntry` traccia `std::weak_ptr<const gs::TexImage> src` (`TexRef::own`). A fine draw (`tex_evict`), le texture le cui `TexImage` sono scadute (es. la texture 256x128 del menu di pausa rigenerata a ogni frame) o non usate da `kTexKeep` chiamate vengono spostate in `g_pool[{w, h}]` senza deallocare la memoria MEM2. Alla successiva richiesta della stessa dimensione (`texture_of(const mrp2d::TexRef&)`), la texture nel pool viene prelevata in ordine FIFO (verificando `f.last_use + 2 <= g_call` per la sicurezza in-flight GX2) e ricaricata in-place con `GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, ...)`.
     - **Render Target Indicizzati per `(fbp, w, h)` (`g_rt` + `g_rt_cur`) e Scala `RAC1_2D_RT_SCALE` (`g_rt_s`):** Sostituita la mappa `g_rt` per sola pagina `fbp` (che nel menu di pausa causava 2 `GX2DrawDone()` + 2 riallocazioni MEM2 a ogni singolo frame quando la stessa pagina veniva disegnata a due larghezze diverse, oltre a far campionare al GamePad la dimensione sbagliata) con `std::map<RtKey, Rt> g_rt` indicizzata per `(fbp, w, h)` e `g_rt_cur[fbp]` per la dimensione attiva corrente in ordine di draw. Quando una pagina cambia dimensione attiva durante il pass TV (`targets == true`), il target esistente viene pulito su GPU con `GX2ClearColor` senza riallocare; durante il pass GamePad (`targets == false`), `g_rt_cur[fbp]` segue la sequenza delle dimensioni mantenendo intatti entrambi i buffer. Integrato il supporto a `RAC1_2D_RT_SCALE` (`rt_scale_for`, viewport e scissor scalati per `r.s`).
     - **Diagnostica `RAC1_2D_TEX_STATS`:** Portato il tracciamento `TexStats` (`ts_tag`, `ts_made`, `ts_freed`, `ts_rt`, `ts_print`) in `mrp_2d_gx2.cpp` e aggiornato il log `[wiiu] mrp_2d_gx2` con il conteggio `held+pooled`.
  2. **Integrazione Build e Vibrazione GamePad Wii U (`#10`, commit `25cb39b`):**
     - `ports/wiiu/game/CMakeLists.txt`: Aggiunto `${ROOT}/src/host/rumble.cpp` a `HOST_SRC` (`enhance/vibration.cpp` è già incluso automaticamente tramite `rac1_sources_of` da `src/game/CMakeLists.txt`).
     - `src/host/rumble.cpp`: Implementato il backend `#elif defined(__WIIU__)` tramite `<vpad/input.h>` (`VPADControlMotor(VPAD_CHAN_0, pattern, 120)` con pattern PWM a 120 bit / 15 byte proporzionale a `std::max(low, high)` sopra la soglia `0.05f`, e `VPADStopMotor(VPAD_CHAN_0)` a 0 o in `shutdown()`), gestendo il `guid` vuoto su Wii U (`"vpad0"`) e il throttling per non invocare VPAD a ogni frame quando l'intensità non cambia.
     - `ports/wiiu/game/wiiu_input.cpp` & `ports/wiiu/game/wiiu_stubs.cpp`: Aggiunto `VPADStopMotor(VPAD_CHAN_0)` all'uscita (`rac1_wiiu_quit` / Minus tenuto premuto) e lo stub `glfwSetScrollCallback` in `wiiu_stubs.cpp` per il nuovo callback rotella mouse di `orig_main.cpp`.
- **Verifiche:**
  - Controllo sintattico e warning (`-fsyntax-only -Wall -Wextra`) superato con 0 errori e 0 warning sui file modificati sia con `powerpc-eabi-g++` (devkitPPC GCC 16) sia con `g++` host PC.
