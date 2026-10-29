# Wii U Port — Game Logic, Level Transitions, Cutscenes & Main-Branch Sync (`TASKS_LOGIC.md`)

Questo file coordina il lavoro dei subagenti (Gemini 4) sul branch `port/wiiu` in `/home/daino/progetti/rac1-native-game-wiiu`.

## Regole di Coordinamento (Zero Overlap)
1. **Zero sovrapposizione di file o task:** Ogni subagente lavora esclusivamente sui file assegnati alla propria Track. Nessun file può essere modificato da due subagenti contemporaneamente.
2. **Massimo 2 subagenti concorrenti** per rispettare i limiti di RAM (`MemoryMax=6G` per build, `-j3` max).
3. **Aggiornamento progressivo:** Ogni subagente aggiorna la propria sezione in questo file (`ports/wiiu/TASKS_LOGIC.md`) con l'analisi, le modifiche effettuate e i test eseguiti.
4. **Invarianti obbligatorie:**
   - Lavorare **solo** in `/home/daino/progetti/rac1-native-game-wiiu` (branch `port/wiiu`). Non toccare mai `/home/daino/progetti/rac1-native-game`.
   - Sbloccato il lavoro su ottimizzazioni CPU/GPU (Novalis e livelli pesanti) e sugli effetti 3D mancanti (`mrp_fx`, `mrp_shadows`, shrub billboards) su Wii U.
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

## Track D — FMV (.pss) MPEG-2 VLC & Streaming Demux Fix
- **Stato:** `[COMPLETATO]`
- **File modificati (esclusivi Track D):**
  - `runtime/ipu/mpeg2_dec.h`
  - `runtime/ipu/movie_hw.cpp`
- **Analisi e Modifiche Effettuate:**
  1. **Correzione Tabelle VLC ISO/IEC 13818-2 Annex B (B.1, B.14, B.15), LUT e Riferimento P-frame in `runtime/ipu/mpeg2_dec.h`:**
     - Riallineati gli array canonici `kCoeffRun[111]`, `kCoeffLevel[111]`, `kB14Code[111]`, `kB14Len[111]`, `kB15Code[111]` e `kB15Len[111]` alle tabelle ufficiali ISO/IEC 13818-2 Annex B Table B.14 / Table B.15 (`ff_rl_mpeg1` / `ff_rl_mpeg2`), eliminando tutte le collisioni di prefisso (0 collisioni su tutti i 224 codici `sign=0/1` + `EOB` + `Escape`) e ripristinando le 7 coppie `(run, level)` mancanti (`(0, 36..40)`, `(30, 1)`, `(31, 1)`).
     - Aggiornata `VlcTables` e `decode_block` con `b14_short[1 << 12]` / `b15_short[1 << 12]` (per `total_len <= 12`, indicizzate da `br.show(12)`) e `b14_long[1 << 11]` / `b15_long[1 << 11]` (per `total_len in 13..17`, indicizzate da `br.show(17) < 2048`), garantendo il round-trip esatto al 100% di tutte le entry B.14 e B.15.
     - Corretto `decode_mb_address_increment` (Table B.1) per gli incrementi `10..15` (`v >= 48`), `16..21` (`v >= 36`, `23 - c`) e `22..33` (`v >= 24`, `35 - v`), lo shift di dequantizzazione AC/non-intra (`>> 5`, ovvero `/ 32` come da ISO/IEC 13818-2 §7.4.2.2) e il frame di riferimento forward `ref[0]` in `apply_mc` per i P-frame (`do_dir(0, ref[0])` dopo `std::swap(ref[0], ref[1])`).
  2. **Eliminazione dell'Hitch di Allocazione Sincrona da 40 MB in `runtime/ipu/movie_hw.cpp`:**
     - In `movie_hw_open()`, rimosso `L.d.resize((bytes + 2047) / 2048 * 2048)` (20 MB zero-filled) e `m.video.resize(bytes)` (20 MB zero-filled) che causavano un hitch sincrono di ~1240 ms sul game thread e occupavano 20 MB extra in MEM2.
     - Convertito `L.d` in un buffer sliding-window (`L.d.reserve(kChunk + 65536)` = 320 KB): `load_step()` legge fino a `kChunk` (256 KB) per passo, esegue `L.dx.step()` e rimuove i byte già demuxati (`L.d.erase(L.d.begin(), L.d.begin() + consumed)`), mantenendo la dimensione massima di `L.d` a 256 KB.
     - Sostituito `m.video.resize(bytes)` con `m.video.reserve(bytes)` (singola `malloc` senza zero-fill) e append incrementale in `load_step()` tramite `m.video.insert(m.video.end(), p, p + n)` senza alcuna riallocazione durante la lettura concorrente del decoder.
- **Verifiche:**
  - **Round-trip VLC & Collision Test:** 0 collisioni di prefisso in B.14 e B.15; 100% round-trip verificato su tutte le 111 entry (`sign=0` e `sign=1`), `EOB` ed `Escape`.
  - **Confronto Demux Sliding-Window vs Full-Buffer:** Output `m.video` e `L.tracks` 100% bit-identical su `row_17f8.pss` (7,316,543 B), `row_1950.pss` (4,966,544 B) e `row_1958.pss` (12,799,876 B), con `max_d_size = 262,144` byte (256 KB).
  - **Confronto Decodifica Primi 30 Frame (`mpeg2_dec.h` vs `libavcodec`):**
    - `assets/disc/toc/row_17f8.pss` (Intro FMV): 0 slice fallite, max `|Y_builtin - Y_avcodec| = 0.0994` (`< 0.5`).
    - `assets/disc/toc/row_1950.pss`: 0 slice fallite, max `|Y_builtin - Y_avcodec| = 0.0287` (`< 0.5`).
    - `assets/disc/toc/row_1958.pss` (Veldin FMV, sector 622319): 0 slice fallite, max `|Y_builtin - Y_avcodec| = 0.0212` (`< 0.5`).
  - **Controllo Compilazione PC e Wii U:** `-fsyntax-only -Wall -Wextra` superato con 0 errori e 0 warning sia con `g++` host PC sia con `powerpc-eabi-g++` (devkitPPC).

## Track E — Main Menu (RAC1_BOOT=1) Speedup, Camera View Swap & Default Boot Config on Wii U
- **Stato:** `[COMPLETATO]`
- **File modificati (esclusivi Track E):**
  - `src/host/orig_main.cpp`
  - `src/game/ovl/overlay_entry.cpp`
  - `src/game/boot/title.cpp`
  - `ports/wiiu/game/wiiu_main.cpp`
- **Analisi e Modifiche Effettuate:**
  1. **Guardie `world_tfrag/tie/shrub_lighting` e `_end` in `src/game/boot/title.cpp` (`title_draw`):**
     - Sotto `#if defined(__WIIU__)`, protette `world_tfrag_lighting` / `world_tfrag_end` con `if (!tfrag_off)` (`RAC1_TFRAG_PASS_OFF`), `world_tie_lighting` / `world_tie_end` con `if ((wp_skip & 2ul) == 0)` e `world_shrub_lighting` / `world_shrub_end` con `if ((wp_skip & 4ul) == 0)` (`RAC1_WORLD_PASS_SKIP`), evitando la dereferenziazione di puntatori non inizializzati o obsoleti quando i pass CPU world sono saltati su Wii U.
  2. **Abilitazione `world_pass_skipped` e `tfrag_pass_off` durante `boot_in_title()` su `__WIIU__` in `src/game/ovl/overlay_entry.cpp`:**
     - Sotto `#if defined(__WIIU__)`, rimosso `&& !boot_in_title()` da `world_pass_skipped(int kind)` e `tfrag_pass_off()` (mantenendolo su `#else` per non alterare il test di riferimento `boot` su PC), dato che su Wii U `mrp_world_gx2` disegna direttamente il mondo del Title Screen su GX2.
  3. **Abilitazione `g_moby_mrp_skip`, `g_moby_pass_mrp`, skip hook VU1 moby e `g_world_chain_forced = false` durante `boot_in_title()` su `__WIIU__` in `src/host/orig_main.cpp`:**
     - In `vu1::g_start_hook` e in `game_frames()`, consentito lo skip dei programmi VU1 moby (`0x008..0x00e`) e dello skinning/joints CPU VU0 (`g_moby_mrp_skip`, `g_moby_pass_mrp`) quando `(!boot_active() || boot_in_title())` su `#if defined(__WIIU__)`, eliminando ~180 ms/frame di lavoro CPU sul Title Screen e sul Menu Principale (`mrp_moby_gx2` disegna già sia i moby della scena titolo sia i box 3D del menu `mode == 3`).
     - Sotto `#if defined(__WIIU__)`, impostato `g_world_chain_forced = false` per riavvolgere la catena VU1 world inutilizzata durante `boot_active()`.
  4. **Ripristino `mrp_title_view_swap_in()` / `mrp_title_view_swap_out()` su `__WIIU__` in `src/host/orig_main.cpp`:**
     - Racchiusi `mrp_world_wiiu_sky_capture`, `mrp_moby_prepare`, `mrp_world_wiiu_prepare` e `rac1_wiiu_picture_frame` tra `const bool title_view = wiiu_title && mrp_title_view_swap_in();` e `if (title_view) { mrp_world_wiiu_join(); mrp_title_view_swap_out(); }`, ripristinando la camera 3D della scena titolo dietro il Menu Principale (`mode == 3`).
  5. **Default di Avvio nel Menu Principale e Memory Card su Wii U (`ports/wiiu/game/wiiu_main.cpp`):**
     - Impostati i default `RAC1_BOOT=1`, `RAC1_MEMCARD=1`, `RAC1_MEMCARD_DIR=memcard` e `RAC1_QUIT_ENDS=0` prima della lettura di `run.cfg`, così il gioco su Wii U parte dal Menu Principale con salvataggi su SD attivi e "Quit Game" riporta al Menu Principale.

- **Data:** 2026-10-29
- **Autore:** OpenClaw (Gemini 4 Argon)
- **Stato:** COMPLETATO — Abilitazione QoL e menu `Options > Advanced`, `Options > Controls` e `Options > Graphics` su Wii U
- **Attività svolta:**
  1. **Abilitazione `RAC1_ADVANCED=1` di default su Wii U (`ports/wiiu/game/wiiu_main.cpp`, `src/host/orig_main.cpp`):**
     - Impostato `setenv("RAC1_ADVANCED", "1", 1)` prima della lettura di `run.cfg` in `wiiu_main.cpp`.
     - Sotto `#if defined(__WIIU__)` in `src/host/orig_main.cpp`, abilitati `g_controls_save` (persistenza di `config/controls.txt` su SD) e `console_language(!rac1::g_advanced_disabled)` (persistenza di `config/language.txt`), oltre al passaggio di `extra` (`o.next_item`, `o.prev_item`) in `pad_packet()`.
  2. **Pagina `Options > Advanced` e popup `New Game` (`src/game/enhance/advanced.cpp`, `src/game/enhance/graphics.cpp`):**
     - Attivate tutte le 8 opzioni QoL di `Options > Advanced` (`Quick Select: Normal / Pause`, `Aiming: Original / Modern / Modern, No Zoom`, `Movement: Original / Strafing / RAC 3`, `Fades: Fast / Original`, `Buy All Ammo: Off / On`, `Vibration: Off / Low / High`, `Ship: Original / Ship 1..3`, `Clank: Original / Modern`) con salvataggio in `config/advanced_options.txt` e nei file `save<N>.pc` sulla Memory Card.
     - Attivato il popup `New Game` con le modalità `Original` e `Modern` su Wii U.

---

### Track F — Sblocco Effetti 3D Mancanti (`mrp_fx`, `mrp_shadows`, Shrub Billboards) e Ottimizzazione Prestazioni CPU/GPU/MEM2 su Novalis
- **Stato:** `[COMPLETATO]`
- **File modificati:**
  - `runtime/ipu/movie_hw.cpp`
  - `src/game/lib/libmc.cpp`
  - `src/game/ovl/arrival.cpp`
  - `src/game/ovl/overlay_entry.cpp`
  - `src/game/render/mrp_shadows.h`, `src/game/render/mrp_shadows.cpp`
  - `src/game/render/mrp_fx.h`, `src/game/render/mrp_fx.cpp`
  - `src/host/orig_main.cpp`
  - `ports/wiiu/game/wiiu_main.cpp`, `ports/wiiu/game/wiiu_gfx.cpp`
  - `ports/wiiu/game/mrp_world_gx2.h`, `ports/wiiu/game/mrp_world_gx2.cpp`
- **Modifiche Effettuate:**
  1. **Prestazioni CPU su Novalis (`overlay_entry.cpp`, `arrival.cpp`):**
     - In `relight_flagged()` (`overlay_entry.cpp`), aggiunto budget round-robin `kMaxPerFrame = 64` per kind su `#if defined(__WIIU__)`: su Novalis 514 tie avevano `nib != 0xffffu` fisso e venivano ricalcolati a ogni singolo frame (~45 ms/frame su Espresso), riducendo di 8x il costo CPU.
     - In `ship_sequence_draw()` e `mode2_draw()` (`arrival.cpp`), protetta `render_frame()` con `if (!g_draw_last_step_only || g_mrp_lod_frame_drawn)` per evitare fino a 5 passate complete di rendering nei catch-up frame durante l'arrivo su Novalis.
  2. **Prestazioni GPU e MEM2 (`wiiu_main.cpp`, `wiiu_gfx.cpp`, `movie_hw.cpp`, `libmc.cpp`):**
     - Attivata scala 3D default `RAC1_WIIU_3D_SCALE=0.75` (rendering 3D in MEM1 eDRAM con upscale prima dell'HUD 2D) e corretto il bug in `wiiu_gfx.cpp` (`s3_gfx_seen`) che sovrascriveva `RAC1_WIIU_3D_SCALE` a `1.0` al primo frame.
     - Ridotto il picco di ~40-60 MB MEM2 dei filmati `.pss` in `movie_hw.cpp` liberando progressivamente i blocchi ADPCM decodificati; evitata la creazione/lettura di file salvataggio da 0 byte su SD in `libmc.cpp`.
     - **Fix persistenza salvataggi Memory Card e opzioni su SD (`src/platform/platform.h`, `src/game/lib/libmc.cpp`, `src/host/progress.cpp`):**
       - In `write_file_replace` (`platform.h`) e `progress.cpp`, aggiunto `std::filesystem::remove(path, ec)` prima di `std::filesystem::rename(tmp, path, ec)` sotto `#if defined(__WIIU__)`: su CafeOS/wut `FSARename` fallisce con `EEXIST` se il file di destinazione esiste già, impedendo la sovrascrittura di `save0..4.bin` e dei file `config/*.txt` dopo la prima creazione.
       - In `host_load` (`libmc.cpp`), sostituito `fs::relative(it->path(), root, ec)` con `it->path().lexically_relative(root)` e isolati gli `std::error_code` interni dal contatore del ciclo `recursive_directory_iterator`: su Wii U `fs::relative` invoca `canonical()` su `"fs:/vol/..."` (non riconosciuto come root path POSIX in libstdc++), impostando `ec` e interrompendo il caricamento della Memory Card al primo elemento.
  3. **Sblocco Effetti 3D Mancanti su GX2 (`mrp_fx`, `mrp_shadows`, `mrp_world_gx2`):**
     - Abilitati di default `RAC1_SKIP_DRAW_LISTS=0` e `RAC1_SKIP_SHADOWS=0` in `wiiu_main.cpp`.
     - Implementato `mrp_fx_wiiu_prepare` / `mrp_world_gx2_draw_fx` per disegnare con GX2 superfici VU1 program 7 (acqua/fiumi di Novalis, mari, lava), particelle, thruster glow, colpi blaster, scintille, draw list e drop shadows dei moby (`mrp_shadows`), oltre ai billboard degli arbusti distanti (`cpu_batch` in `mrp_world_gx2_draw`).
  3. **Pagina `Options > Controls` (`src/game/enhance/controls.cpp`, `src/host/orig_main.cpp`):**
     - Nascoste su `__WIIU__` le righe solo tastiera/mouse (`Device`, `Mouse Mode`, `Mouse Sensitivity`, `Aim Sensitivity`), esponendo la rimappatura completa dei 17 comandi del controller (inclusi `Next Weapon` e `Previous Weapon`), `Invert Look` e `Reset to Defaults`, con etichette pulsanti in stile Nintendo (`B`, `A`, `Y`, `X`, `L`, `R`, `ZL`, `ZR`, `+`, `-`).
  4. **Pagina `Options > Graphics`, FOV, Level of Detail e HUD 16:9 (`src/game/enhance/graphics.cpp`, `src/game/render/render_view.cpp`, `src/game/render/mrp_lod.cpp`, `src/game/render/mrp_tfrag_gpu.cpp`, `src/game/ovl/overlay_entry.cpp`, `src/game/ovl/render_passes.cpp`, `ports/wiiu/game/wiiu_gfx.cpp`):**
     - Filtrate le righe di `Options > Graphics` su `__WIIU__` alle 7 opzioni pertinenti (`Render Resolution`: `x0.50 / x0.75 / x1.00`, `Aspect Ratio`: `4:3 Original / 16:9`, `Fit to Screen`: `Black Bars / Stretch`, `HUD`: `4:3 Original / 16:9`, `Field of View`: `80%..130%`, `Level of Detail`: `Original / Medium / High / Maximum / Unlimited`, `Texture Detail`: `Full / Original (PS2)`), escludendo le opzioni finestra PC e l'illuminazione/ombre pesanti PC.
     - Collegati `g_view_fov_scale` (`fov_scale_now()` in `render_proj_x`/`render_proj_y` e nel culling `tfrag_snapshot_widen`/`static_note_pass`/`widen_real`), `g_mrp_lod` (`mrp_lod_scale(g_mrp_lod)` passato a `tfrag_note_pass`, `static_note_pass` e `patch_moby` in `widen_real`), `render_view_hud_wide()`, `render_view_hud_shift()`, `ci.hud` e `ci.layers` al backend GX2 (`wiiu_gfx.cpp` `frame_viewport()`).
