# Stato Lavori — Ottimizzazione e Riscrittura Nativa Wii U (`port/wiiu`)

Ultimo aggiornamento: **30 Ottobre 2026**

Questo documento traccia in modo completo **cosa è stato fatto**, **i risultati misurati sulla console Wii U reale (`192.168.0.16`)**, **cosa stiamo facendo ora**, **cosa manca** e **le note tecniche / trappole da evitare** per poter riprendere il lavoro in qualsiasi momento.

---

## 1. Risultati Misurati su Hardware Reale (Wii U `192.168.0.16`)

Tutti i test sono eseguiti con `ports/wiiu/tools/bench.sh` (`explore` input deterministico, TV 1080p con scala 3D 0.75x):

| Milestone / Commit | Veldin (`RAC1_LEVEL=0`, 1200 frame) | Novalis (`RAC1_LEVEL=1`, 1500 frame) | Note principali |
| :--- | :--- | :--- | :--- |
| **Baseline iniziale** (`de83482`) | **52.6 ms** (~19.0 fps) | **87.8 ms** (~11.4 fps, in slow-motion) | 1 solo step di gioco per host frame quando >50 ms |
| **`m1a`–`m1c`** (`f9d57de`) | **42.6 ms** (~23.5 fps, fine run **33.4 ms**) | **76.7 ms** (velocità reale 100%) | Rimosso doppio `mrp_lod_moby_shadow` su Wii U; `kStallMs = 250` |
| **`m1d`–`m1f`** (`5e9c173`) | **38.3 ms** (6 finestre fisse a **33.2–33.3 ms / 30 fps**) | **62.7 ms** (moby draw loop `20.7 -> 12.7 ms`) | Undo log per-campo in `mrp_lod.cpp`; `PinnedThread`; cache `StaticLevel::tex` |
| **`m1h`–`m1j`** | — | **45.9 ms** (16 finestre; 5 finestre consecutive a **33.4–34.6 ms / 30 fps**) | Overlap `game_thread` (Core 2) + `mrp_chain` (Core 0) + `world prep` (Core 1) |
| **`m1k`–`m1l`** (`0b70fb6`) | **35.3 ms** (10/16 finestre fisse a **32.8–33.5 ms / 30 fps**, 1 a **16.7 ms / 60 fps**) | **44.4 ms** (5 finestre a **32.5–34.2 ms / 30 fps**; finestra finale scesa da **64.9 a 54.9 ms**) | `tfrag_exec_cpu_fast` + drenaggio cooperativo Core 1/Core 0 + fast path `mrp_fx_wiiu_prepare` |
| **`m1m`** (`53e2018`) | **30.3 ms** (~33.0 fps medi! Finestre a **25.4–26.1 ms / 38–39 fps** e **16.7–18.8 ms / 53–60 fps**; GPU busy **17.8 ms**) | Finestra 1140–1200 scesa da **54.9 ms a 36.1 ms (-18.8 ms!)**; GPU busy medio **38.2 → 25.1 ms (-34.3%)**; World GPU **26.98 → 10.06 ms** | Culling Pass 2 per-draw/per-range + shader `_noat` Early-Z in Pass 1 + `LESS` depth in Pass 2 |
| **`m1n`** (`gs_memory` + `mrp_chain`) | *(da misurare insieme a `m1o`)* | `chain join` sceso di **60–100%** (`5.3–7.7 → 2.2–3.0 ms` a 30 fps; **`3.9–7.1 → 0.0–0.1 ms`** nelle viste pesanti!); `fx_texture` `GX2DrawDone` **azzerato (`0.00/frame`)**; frame 960 **49.3 → 44.7 ms**, frame 1080 **47.5 → 41.9 ms** | LUT `in_page` in `addr16`/`addr8`/`addr4`, pre-decodifica CLUT in `texture_at_kick`, ring a 16 slot in `TexSnap`, zero-copy in `Reader::read` |

---

## 2. Cosa è Stato Fatto

### Commit `f9d57de` (già su `origin/port/wiiu`)
1. **Strumentazione GX2 e Benchmark automatico** (`ports/wiiu/game/gx2_counters.h`, `ports/wiiu/game/wiiu_gfx.cpp`, `ports/wiiu/tools/bench.sh`):
   - Contatori per-frame di `GX2DrawDone` (con sito chiamante `__FILE__:__LINE__`), `GX2Invalidate`, draw call e byte di `DCFlushRange`.
   - Separazione dei tempi CPU (`kTmFxPrep`, `kTmFx`) e GPU (`kGmFx`) degli effetti dal mondo.
   - Script `ports/wiiu/tools/bench.sh` per benchmark automatici su console.
2. **Eliminazione doppio passaggio `mrp_lod_moby_shadow` su Wii U** (`src/game/ovl/overlay_entry.cpp`):
   - Su Wii U `mrp_moby_gx2` estrae già le istanze moby direttamente dalla tabella EE; il passaggio shadow extra in `overlay_entry.cpp` duplicava l'intero draw loop dei moby inutilmente.
3. **Fix slow-motion (`kStallMs = 250`)** (`src/host/orig_main.cpp`):
   - Alzato il clamp anti-spirale da 50 ms a 250 ms su `__WIIU__`, permettendo al gioco di recuperare tutti i 60 step logici/secondo anche nelle scene più pesanti di Novalis.

### Commit `5e9c173` (già su `origin/port/wiiu`)
4. **Sostituzione Journal EE a pagine da 4 KB con Undo Log per-campo** (`src/game/render/mrp_lod.cpp`):
   - `widen_real` copiava e ripristinava centinaia di pagine da 4 KB (~1.5 MB/frame su Novalis) solo per salvare `drawn`, `dist_low` e `flags` dei moby. Sostituito con `patch_moby_logged()` / `patch_moby_undo()`.
   - **Impatto misurato su Novalis:** `Game: moby pass (draw loop)` **20.7 ms → 12.7 ms (-8.0 ms)**; `game code` **50.6 ms → 38.2 ms (-12.4 ms)**.
5. **Thread Pinning deterministico su Wii U** (`src/platform/pinned_thread.h`, `src/game/render/mrp_chain.cpp`, `src/game/render/mrp_world_renderer.cpp`):
   - Creato `PinnedThread` basato su `OSCreateThread` con flag di affinità esplicita per core, evitando che `std::thread` finisca sul core di default (Core 2) rubando cicli al `game_thread`.
6. **Cache `StaticLevel::tex` persistente tra frame** (`src/game/render/mrp_world_scene.h`, `src/game/render/mrp_world_scene.cpp`, `src/game/render/mrp_world_static.cpp`):
   - Evitata la copia di `L.tex` (95 KB/frame su Novalis) dentro `SceneFrame::tex` a ogni frame per ties e shrubs.

### Commit `0b70fb6` (già su `origin/port/wiiu`)
7. **Zero allocazioni in `vu1::start_program` e `sky_prepend` O(1)** (`runtime/vu/vu1.cpp`, `src/game/render/mrp_world_scene.cpp`, `src/game/render/mrp_world_renderer.cpp`):
   - Eliminata l'allocazione `std::vector<uint8_t> buf` a ogni `XGKICK` VU1 (passando direttamente `&vu.st.mem[a0 << 4]` quando non wrappa).
   - Inseriti i pacchetti sky in `SceneFrame::packets` prima dei pacchetti tie/shrub/tfrag, eliminando lo spostamento `memmove` di oltre 5.800 `ScenePacket` a ogni frame.
8. **Pipeline Parallela a 3 Core (`RAC1_WIIU_CHAIN_OVERLAP_GT=1`, `RAC1_WIIU_PREP_ON_MAIN=1`, `RAC1_WIIU_TFRAG_PARALLEL=1`)** (`src/host/orig_main.cpp`, `src/game/render/mrp_world_renderer.cpp`, `ports/wiiu/game/wiiu_gfx.cpp`, `src/game/render/mrp_tfrag_gpu.cpp`):
   - **Core 2 (`game_thread`):** avviato immediatamente da `wiiu_overlap` senza aspettare `mrp_chain_join()` su Core 1 (**-13.0 ms** medi su Novalis!).
   - **Core 1 (Main thread):** esegue `mrp_world_wiiu_join()` (`wiiu_prep_head` + `build_triangles`) in parallelo mentre Core 0 esegue `mrp_chain` e Core 2 esegue `game_thread`.
   - **Drenaggio cooperativo Tfrag (`tfrag_exec_cpu_fast`):** eliminati i `std::vector` temporanei per-draw e l'azzeramento da 1.2 MB/frame (`v.assign`/`idx.assign`). Mentre `mrp_chain` gira ancora su Core 0, Core 1 sfrutta il tempo di attesa pre-`mrp_chain_join` per processare draw tfrag; non appena `mrp_chain` termina su Core 0, `PrepWorker` si sveglia su Core 0 (senza alcuna contesa con `mrp_chain`) e drena i restanti draw tfrag in parallelo mentre Core 1 prepara effetti, 2D e comandi GX2.
9. **Fast Path `mrp_fx_wiiu_prepare`** (`src/game/render/mrp_fx.cpp`):
   - Riutilizzo di buffer `static` per i batch VU7 (eliminando ~200–480 allocazioni heap/frame su Novalis).
   - Calcolo di posizione mondo `wp` e `fog` una sola volta per vertice VU7 (invece di ricalcolare `M * pt` e `vu7_geinv * (Me * pt)` per ogni angolo di ogni triangolo nello strip, riducendo del ~70% la matematica VU7).
   - Fattorizzazione dell'unprojection per gli sprite (`p.type == 6`, 4 vertici con la stessa profondità `b.z`) e calcolo diretto di `(VP * wp).w` con prodotto scalare a 3 moltiplicazioni invece di `dmat4 * dvec4` completo.

### Commit `53e2018` — Milestone `m1m` (`RAC1_WIIU_P2_CULL` + `RAC1_WIIU_P2_LESS`) (già su `origin/port/wiiu` e caricato su SD)
10. **Eliminazione Pass 2 (`AFAIL`) e `discard` Pass 1 per le geometrie opache + `LESS` depth in Pass 2** (`ports/wiiu/game/mrp_world_gx2.h`, `src/game/render/mrp_world_backend.inc`, `ports/wiiu/game/mrp_world_merge.inc`, `src/game/render/mrp_world_renderer.cpp`, `ports/wiiu/game/mrp_world_gx2.cpp`):
    - Pre-calcolo di `TexRef::opaque` su tutti i mip level campionati (`alpha >= 0x80` su tutti i texel) e cache dell'opacità alpha per-vertice per slot tie (`tie_slot_opaque`) e tfrag (`tfrag_entry_opaque`).
    - In **Pass 1**, tutti i range tfrag e gli item tie/shrub con `pass2 == false` usano gli shader `_noat` (`mrp_world_fast_noat.gsh` / `mrp_tfrag_fast_noat.gsh`), eliminando `KILL_ENABLE` (`discard`) e abilitando scrittura depth Early-Z e aggiornamento Hi-Z HTILE hardware *prima* del pixel shader.
    - In **Pass 2**, tutti i range/item con `pass2 == false` (~85–95% del mondo) vengono saltati; per il restante 5–15% di cutout (`pass2 == true`), `RAC1_WIIU_P2_LESS` imposta il depth test a `LESS` anziché `LEQUAL`, così l'Early-Z/Hi-Z hardware scarta tutti i pixel interni opachi già scritti in Pass 1.
    - **Risultato misurato su Wii U:**
      - **Veldin:** media globale **30.3 ms (~33.0 fps)**, con finestre a **25.4–26.1 ms (38–39 fps)** e **16.7–18.8 ms (53–60 fps)**; GPU busy **17.8 ms**; World GPU **26.98 → 7.62 ms (3.5× più veloce)**; `GPU wait = 0.0 ms`.
      - **Novalis:** finestra 1140–1200 scesa da **54.9 ms a 36.1 ms (-18.8 ms)**; GPU busy medio **38.2 → 25.1 ms (-34.3%)**; World GPU **26.98 → 10.06 ms**; `GPU wait = 0.0 ms`.

### Milestone `m1n`–`m1o` (`gs_memory.cpp`, `mrp_chain.cpp`, `RAC1_WIIU_RING_NOWAIT`, `bench.sh` zero-FTP)
11. **Accelerazione `gs::LocalMemory` e `mrp_chain` (`m1n`, verificato su console)** (`runtime/gs/gs_memory.h`, `runtime/gs/gs_memory.cpp`, `src/game/render/mrp_chain.cpp`):
    - Tabelle di lookup `static const` `in_page` per `addr16`, `addr8` e `addr4` + scrittura diretta in `LocalMemory::store` senza doppia chiamata `addr*`.
    - Pre-decodifica della CLUT a 256 voci (`T8`) o 16 voci (`T4`) una sola volta per texture in `LocalMemory::texture_at_kick` anziché chiamare `read_pixel(CT32, ...)` per ogni singolo texel (`w * h` volte).
    - Ring cache a 16 slot (`TexSnap::ring[16]` + `ring_hash[16]`) per ogni chiave `(tex0, texa)` in `LocalMemory::snap_`, così quando più texture effetti/HUD si alternano nello stesso slot `TBP` della VRAM GS nello stesso frame, viene riutilizzato lo stesso `std::shared_ptr<const TexImage>` (e il suo `TexImage::id`) senza riallocare e senza mandare in overflow la cache `g_fxtex` da 8 MB (`GX2DrawDone` a `mrp_world_gx2.cpp:2482` ridotto a **`0.00/frame`**!).
    - Zero-copy in `Reader::read` (`mrp_chain.cpp`) quando `d == t + 16` in `g_capture.bytes`.
    - **Risultato misurato su Wii U (`m1n_novalis`):** `chain join` sceso da `5.3–7.7 ms` a **`2.2–3.0 ms`** nelle finestre a 30 fps e da `3.9–7.1 ms` a **`0.0–0.1 ms`** nelle viste pesanti (dove `mrp_chain` su Core 0 termina ora completamente *prima* di `world's worker join` su Core 1!).
12. **Eliminazione degli stalli `GX2DrawDone()` sui wrap dei ring buffer (`RAC1_WIIU_RING_NOWAIT=1`, `m1o`) e `bench.sh` 100% senza FTP** (`ports/wiiu/game/mrp_world_gx2.cpp`, `ports/wiiu/game/mrp_moby_gx2.cpp`, `ports/wiiu/game/mrp_2d_gx2.cpp`, `ports/wiiu/game/wiiu_main.cpp`, `ports/wiiu/tools/bench.sh`, `ports/wiiu/tools/wiiu-run.sh`):
    - I contatori `[gx2]` in `m1n_novalis` hanno rivelato fino a `0.20` chiamate `GX2DrawDone()`/frame (1 stallo completo della pipeline GPU ogni 5 frame, ~4–5 ms medi persi su CPU!) in `mrp_world_gx2.cpp:303` (`geo_alloc`), `mrp_world_gx2.cpp:285` (`plan_alloc`), `mrp_moby_gx2.cpp:126` e `mrp_2d_gx2.cpp:119` a ogni wrap dei ring buffer.
    - Poiché `wiiu_gfx_frame()` attende già `GX2WaitTimeStamp(g_frame_ts)` (la fine del frame precedente `N-1`) prima di qualsiasi draw del frame `N`, i dati all'offset `0` di un ring da 8–16 MB risalgono a 10–50 frame prima e sono già stati letti dalla GPU! Ora i ring buffer tracciano i byte allocati nel frame corrente (`*_frame_bytes`) e chiamano `GX2DrawDone()` sul wrap solo se un singolo frame supera metà ring (`kGeoRingBytes` portato anche da 8 MB a 16 MB).
    - `wiiu_main.cpp` espone ora uno slot `g_embedded_cfg` da 1 KB che `bench.sh` patcha direttamente nell'ELF temporaneo del benchmark prima di `elf2rpl`: **`bench.sh` e `wiiu-run.sh` non aprono più alcuna connessione FTP né toccano mai il `run.cfg` sulla SD**, evitando i crash del plugin `ftpiiu` di Aroma!

---

## 3. Cosa Manca (Prossimi Passi in Ordine di Impatto)

### Priorità 1 — Misurare `m1o` su Console + Testare 1080p Nativo (`RAC1_WIIU_3D_SCALE=1.0`)
Non appena la console è riavviata (ora `bench.sh` non usa più FTP!):
1. Eseguire `ports/wiiu/tools/bench.sh m1o_veldin RAC1_LEVEL=0` e `ports/wiiu/tools/bench.sh m1o_novalis RAC1_LEVEL=1 RAC1_FRAMES=1500` per verificare l'azzeramento totale di `GX2DrawDone` (`0.0/frame`) e il guadagno su `world draws` / `2D draws`.
2. Eseguire il test a **1080p nativo (`RAC1_WIIU_3D_SCALE=1.0`)** su Veldin e Novalis (`m1o_1080p_veldin` e `m1o_1080p_novalis`).

### Priorità 2 — Spostare Tfrag su GPU Latte (Vertex Shader GX2) e Snellire `game_thread` (Core 2)
1. **Tfrag su GPU Latte (Vertex Shader GX2):**
   - Oggi i tfrag generano 3.000–5.000 vertici da 72 byte via CPU a ogni frame (`tfrag_exec_cpu_fast`, ~3.8–5.0 ms CPU).
   - Spostare morph/proiezione tfrag su Vertex Shader GX2 elimina il percorso CPU dei tfrag e riduce `world's worker join`.
2. **Ridurre il carico CPU sul `game_thread` (Core 2: ~12.5 ms/step su Novalis):**
   - `Game code (EE update, other)`: ~5.8 ms/step
   - `Game: moby pass (draw loop)`: ~3.9 ms/step (principalmente `moby_shadows` e `moby_cull_light` VU0)
   - `Game: effects draw, draw lists`: ~1.3 ms/step
   - Ottimizzare `moby_shadows` / `moby_cull_light` e i kernel VU0 / collisioni in `EE update` con **Paired Singles (`ps_*`)**.

### Priorità 3 — Ombre Moderne Leggere su Wii U
- Implementare proiezione blob-shadow / shadow-map leggera su GX2 (sfruttando l'infrastruttura di `src/game/render/mrp_lighting.cpp` già presente su PC) o ottimizzare le drop shadows esistenti.

---

## 4. Cose Tralasciate Volutamente / Trappole da Ricordare

1. **NON spostare mai worker thread a priorità 16 su Core 1 (`RAC1_WIIU_CHAIN_CORE=1`):**
   - Core 1 è il core del main thread di Cafe OS e di GX2. Un worker thread con priorità 16 su Core 1 affama il main thread e congela la console (verificato in `m1e`). I worker a priorità 16 devono stare su **Core 0**, mentre il lavoro su **Core 1** va eseguito in modo sincrono dal main thread (`RAC1_WIIU_PREP_ON_MAIN=1`).
2. **Fingerprint Codegen CMake (`RAC1_CODEGEN_FINGERPRINT`):**
   - `src/game/CMakeLists.txt` calcola un hash SHA-256 di tutti i file `.h` dentro `src/game/`. Se si modifica un `.h` in `src/game/`, la build PC fallisce chiedendo `./setup.sh`.
   - **Soluzione:** per le dichiarazioni interne al renderer usare i file `.inc` (`src/game/render/mrp_world_wiiu.inc`, `src/game/render/mrp_world_backend.inc`), oppure aggiornare `RAC1_CODEGEN_FINGERPRINT` in `assets/generated/src/generated_sources.cmake` tramite uno one-liner Python che chiama `setup.code_sources_fingerprint()`.
3. **Zero FTP durante i benchmark (`bench.sh`):**
   - `ports/wiiu/tools/bench.sh` e `ports/wiiu/tools/wiiu-run.sh` non usano più FTP: incorporano la configurazione nel file `$out/$tag.elf` temporaneo (`g_embedded_cfg`) e inviano `$out/$tag.rpx` via `wiiload`. Usare FTP (`curl -T build-wiiu/rac1_wiiu.rpx ftp://192.168.0.16/fs/vol/external01/wiiu/apps/rac1-native/rac1-native.rpx`) solo una tantum per aggiornare la build su SD per Daino.
4. **Scoperte per il branch `main` (PC):**
   - Tutte le ottimizzazioni nel codice condiviso (`mrp_lod.cpp`, `mrp_world_scene.cpp`, `vu1.cpp`, `mrp_tfrag_gpu.cpp`, `mrp_fx.cpp`, `gs_memory.cpp`, `mrp_chain.cpp`) sono documentate in `MAIN_PERF_FINDINGS.md` (sia in questo worktree sia in `/home/daino/progetti/rac1-native-game/MAIN_PERF_FINDINGS.md`).
