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
| **`m1m`** (`RAC1_WIIU_P2_CULL` + `RAC1_WIIU_P2_LESS`) | **30.3 ms** (~33.0 fps medi! Finestre a **25.4–26.1 ms / 38–39 fps** e **16.7–18.8 ms / 53–60 fps**; GPU busy **17.8 ms**) | Finestra finale più pesante scesa da **54.9 ms a 36.1 ms (-18.8 ms!)**; GPU busy medio **38.2 → 24.1 ms (-36.9%)**; World GPU **26.98 → 10.06 ms** | Culling Pass 2 per-draw/per-range + shader `_noat` Early-Z in Pass 1 + `LESS` depth in Pass 2 |

---

## 2. Cosa è Stato Fatto

### Commit `f9d57de` (già su `origin/port/wiiu`)
1. **Strumentazione GX2 e Benchmark automatico** (`ports/wiiu/game/gx2_counters.h`, `ports/wiiu/game/wiiu_gfx.cpp`, `ports/wiiu/tools/bench.sh`):
   - Contatori per-frame di `GX2DrawDone` (con sito chiamante `__FILE__:__LINE__`), `GX2Invalidate`, draw call e byte di `DCFlushRange`.
   - Separazione dei tempi CPU (`kTmFxPrep`, `kTmFx`) e GPU (`kGmFx`) degli effetti dal mondo.
   - Script `ports/wiiu/tools/bench.sh` che salva e ripristina automaticamente il `run.cfg` di Daino (`../rac1-scratch/wiiu/bench/run.cfg.daino`).
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

### Milestone `m1m` (`RAC1_WIIU_P2_CULL` + `RAC1_WIIU_P2_LESS`)
10. **Eliminazione Pass 2 (`AFAIL`) e `discard` Pass 1 per le geometrie opache + `LESS` depth in Pass 2** (`ports/wiiu/game/mrp_world_gx2.h`, `src/game/render/mrp_world_backend.inc`, `ports/wiiu/game/mrp_world_merge.inc`, `src/game/render/mrp_world_renderer.cpp`, `ports/wiiu/game/mrp_world_gx2.cpp`):
    - Pre-calcolo di `TexRef::opaque` su tutti i mip level campionati (`alpha >= 0x80` su tutti i texel) e cache dell'opacità alpha per-vertice per slot tie (`tie_slot_opaque`) e tfrag (`tfrag_entry_opaque`).
    - In **Pass 1**, tutti i range tfrag e gli item tie/shrub con `pass2 == false` usano gli shader `_noat` (`mrp_world_fast_noat.gsh` / `mrp_tfrag_fast_noat.gsh`), eliminando `KILL_ENABLE` (`discard`) e abilitando scrittura depth Early-Z e aggiornamento Hi-Z HTILE hardware *prima* del pixel shader.
    - In **Pass 2**, tutti i range/item con `pass2 == false` (~85–95% del mondo) vengono saltati; per il restante 5–15% di cutout (`pass2 == true`), `RAC1_WIIU_P2_LESS` imposta il depth test a `LESS` anziché `LEQUAL`, così l'Early-Z/Hi-Z hardware scarta tutti i pixel interni opachi già scritti in Pass 1.
    - **Risultato misurato su Wii U:**
      - **Veldin:** media globale **30.3 ms (~33.0 fps)**, con finestre a **25.4–26.1 ms (38–39 fps)** e **16.7–18.8 ms (53–60 fps)**; GPU busy **31.9 → 17.8 ms (-44.2%)**; World GPU **26.98 → 7.62 ms (3.5× più veloce)**; `GPU wait = 0.0 ms`.
      - **Novalis:** finestra finale più pesante scesa da **54.9 ms a 36.1 ms (-18.8 ms)**; GPU busy medio **38.2 → 24.1 ms (-36.9%)**; World GPU nella vista più pesante **26.98 → 10.06 ms**; `GPU wait = 0.0 ms`.

---

## 3. Cosa Manca (Prossimi Passi in Ordine di Impatto)

### Priorità 1 — Eliminare la contesa Core 0 tra `PrepWorker` e `mrp_chain` + Testare 1080p Nativo (`RAC1_WIIU_3D_SCALE=1.0`)
Con la GPU ora scesa a **17.8 ms** su Veldin e **24.1 ms** su Novalis (`GPU wait = 0.0 ms`), il tempo frame residuo su Novalis (**36.1 ms** nella vista più pesante) è interamente CPU:
- Su Core 1: `world's worker join` (**10.0 ms**) + `chain join` (**4.4 ms**). Perché `mrp_world_wiiu_prepare_async` accoda ancora `PrepWorker` su Core 0 mentre `mrp_chain` sta girando su Core 0! Eseguendo lo step di preparazione direttamente su Core 1 in `mrp_world_wiiu_join()` prima di drenare i tfrag, Core 0 resta dedicato al 100% a `mrp_chain` e Core 1 a `mrp_world`, eliminando la contesa su Core 0.
- Testare e misurare **1080p nativo (`RAC1_WIIU_3D_SCALE=1.0`)** su Veldin e Novalis ora che il tempo GPU del mondo è sceso di 3.5×.

### Priorità 2 — Spostare Tfrag su GPU Latte (Vertex Shader GX2) e Snellire `game_thread` (Core 2)
1. **Tfrag su GPU Latte (Vertex Shader GX2):**
   - Oggi i tfrag generano 3.000–5.000 vertici da 72 byte via CPU a ogni frame (`tfrag_exec_cpu_fast`, ~3.8–5.5 ms CPU).
   - Spostare morph/proiezione tfrag su Vertex Shader GX2 elimina il percorso CPU dei tfrag.
2. **Ridurre il carico CPU sul `game_thread` (Core 2: `30.9 ms` nella finestra 14 di Novalis, per `game thread join = 8.8 ms`):**
   - `Game code (EE update, other)`: **12.3 ms** (~5.8 ms/step)
   - `Game: moby pass (draw loop)`: **11.2 ms** (~5.3 ms/step, principalmente `moby_shadows` e `moby_cull_light` VU0)
   - `Game: effects draw, draw lists`: **3.4 ms** (~1.6 ms/step)
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
3. **Instabilità del plugin FTP (`ftpiiu`) di Aroma:**
   - Caricare il binario `.rpx` via FTP troppo frequentemente manda in crash il plugin FTP di Aroma sulla Wii U. Usare `wiiload` (`ports/wiiu/tools/bench.sh`) per tutti i test e caricare `rac1-native.rpx` su SD via FTP solo a fine milestone.
   - `ports/wiiu/tools/bench.sh` salva sempre una copia del `run.cfg` personale di Daino in `../rac1-scratch/wiiu/bench/run.cfg.daino` e la ripristina a fine benchmark.
4. **Scoperte per il branch `main` (PC):**
   - Tutte le ottimizzazioni nel codice condiviso (`mrp_lod.cpp`, `mrp_world_scene.cpp`, `vu1.cpp`, `mrp_tfrag_gpu.cpp`, `mrp_fx.cpp`) sono documentate in `MAIN_PERF_FINDINGS.md` (sia in questo worktree sia in `/home/daino/progetti/rac1-native-game/MAIN_PERF_FINDINGS.md`).
