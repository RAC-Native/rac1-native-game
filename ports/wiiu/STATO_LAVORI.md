# Stato Lavori — Ottimizzazione e Riscrittura Nativa Wii U (`port/wiiu`)

Ultimo aggiornamento: **30 Ottobre 2026 (notte, timeline dei core e split del job del mondo)**

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
| **`m1o`** (`d2cee34`) | **29.4 ms** (ultima finestra 23.7 ms) — ⚠️ terreno/rocce corrotti | **43.5 ms** — ⚠️ terreno/rocce corrotti | `RAC1_WIIU_RING_NOWAIT` (GX2DrawDone = 0.0), `bench.sh` senza FTP |
| **`m1p`** (fix varying SPI `*_fast_noat`) | **29.3 ms** (ultima **22.9 ms**, GPU 17.8 ms) ✅ rendering corretto | **42.9 ms** (ultima **38.2 ms**, GPU 23.9 ms) ✅ | **1080p nativo (scale 1.0):** Veldin **35.1 ms** (ultima **33.3 ms = 30 fps**, GPU 22.9 ms); Novalis **43.4 ms** (ultima 37.1 ms, GPU **29.9 ms**: la GPU non è più il collo di bottiglia, Novalis è CPU-bound su Core 2) |

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

13. **Fix rendering rotto di terreno e rocce (`m1p`)** (`ports/wiiu/shaders/mrp_world_fast.glsl`, `ports/wiiu/game/mrp_world_gx2.cpp`):
    - Da `m1m` in poi, nel Pass 1 i draw opachi usano `*_fast_noat.gsh`. CafeGLSL assegna gli ID SPI dei varying in **ordine alfabetico** per ogni stadio: il VS esportava `vAt` sempre, il PS lo ometteva con `RAC1_NO_ALPHA_TEST` → tutti gli input del PS slittavano di 1 slot (colori/UV/unità texture sbagliati).
    - Fix: `vAt` in `#ifndef RAC1_NO_ALPHA_TEST` anche nel VS + `gx2_fix_noat_pair()` che riallinea in RAM `spi_ps_input_cntls` (funziona anche con `.gsh` vecchi sulla SD; nel log: `aligned 8 SPI PS inputs`).
    - **Regola:** ogni varying condizionato da `#ifdef` deve esserlo identicamente in VS **e** PS. Verificare con lo script Python di dump dei blocchi GFD (tipo 3 VS / tipo 6 PS) che confronta `spi_vs_out_id` e `spi_ps_input_cntls`.

14. **Tfrag nel vertex shader (`RAC1_WIIU_TFRAG_VS`, default on; da misurare su console)** (`src/game/render/mrp_tfrag_gpu_vs.inc`, `ports/wiiu/shaders/mrp_tfrag_vs.vert`, `shaders/mrp_tfrag_vs_fast.gsh` + `_noat`, `mrp_world_gx2.cpp`, `mrp_world_renderer.cpp`):
    - Nessun piano tfrag di Veldin/Novalis ha step seriali e ogni slot è scritto una volta sola: `tfrag_vs_build` risolve a ogni caricamento di livello ogni piano in tabelle statiche (record X / M / raw, selezioni MS sotto F), caricate come texture RGBA32UI (Novalis: piani 15.3 MB + immagini 10 MB in MEM2; la copia CPU dei piani liberata dopo l'upload).
    - Per frame la CPU scrive solo un blocco (draw, set di viste con la matrice clip = VP × vista mondo in double, costanti, run) e un u32 per angolo di triangolo; il VS calcola ogni angolo come la CPU route (selezioni, morph, STQ, Q, F, RGBA) e collassa i triangoli con ADC. Niente più `tfrag_exec_cpu_fast` / drenaggio su Core 1 e Core 0.
    - **Verifica PC:** `RAC1_MRPW_TFRAG_CPU=1 RAC1_MRPW_TFRAG_VS_CHECK=1` (gemello C++ contro la CPU route) e `=2` (lo stesso `.vert` compilato dal GL del PC, transform feedback): decisioni ADC identiche, STQ/RGBA/F/run esatti, posizioni entro il rumore float, 0 piani rifiutati (Veldin e Novalis, forme 0/3/4).
    - SPI controllato con `rac1-scratch/gpuperf3/sem.py`: le due coppie nuove hanno gli stessi semantic e flat di `mrp_tfrag_fast(_noat).gsh` (MATCH); blocco/sampler del PS dove sono quelli di `mrp_tfrag.gsh` (verificato anche al caricamento).
15. **Cielo dopo il primo passo del terreno (`RAC1_WIIU_SKY_LATE`, default on; da misurare)** (`mrp_world_gx2.cpp`, `TfragRange::opaque` in `mrp_world_backend.inc` / `mrp_world_renderer.cpp draw_opaque`):
    - I 5 gusci del cielo coprivano tutto lo schermo senza depth test (3.8 ms GPU Veldin, 5.4 Novalis a 1080p). Ora il cielo è disegnato dopo il Pass 1 dei batch tfrag iniziali i cui pixel sono tutti alpha >= 0x80 (texture e vertici opachi, blend `ALPHA_1 0x44` o niente), con depth test `LEQUAL` senza scrittura e viewport depth range [1, 1]: passa solo dove nulla è stato disegnato, Hi-Z scarta i tile coperti. Immagine identica per costruzione; Pass 2 dei tfrag e il resto dopo il cielo come prima.

---

### Moby pass del game thread (30 Ottobre 2026, da misurare su console)
14. **Mappa del moby pass + draw loop senza flag VU0 (`RAC1_WIIU_FAST_MOBY_LOOP`, default 1 su Wii U, 0 su PC)** (`src/game/ovl/moby_draw.cpp`, `src/game/ovl/moby_draw_fast.inc`):
    - Su Novalis il moby pass (12.7 ms/host frame) è stimato ~7 ms drop shadows, ~4 ms draw loop, 1–2 ms il resto (profilo `m1b` con `samples.py --under widen_real`; mappa completa e chi legge cosa in `PERFORMANCE.md`).
    - Il draw loop su Wii U era 142 KB di codice (le op VU0 dello shim SSE2 inline + `fin_lanes`), 4.4× la I-cache del 750: ora le op senza flag MAC/status (stessi valori: clamp VU decisi sulla FPU, `vuc` per i casi rari), una piccola funzione per (op, forma, maschera): 31 KB + ~12 KB.
    - Prova PC (walk Wii U-like con drop shadows attive): RAMHASH identico parola per parola `=1` vs `=0` (Veldin→Novalis forward 1500 frame, Novalis explore 1200).
15. **Nuovo stadio profiler `Game: moby drop shadows`** (`src/profiler/host_profiler.*`, `src/game/ovl/moby_pass.cpp`): la riga `[game]` separa le drop shadows dal moby pass. `samples.py --under NOME` per il dettaglio di una zona.

### Drop shadows sulla GPU (30 Ottobre 2026, sera — da misurare e guardare su console)
16. **`RAC1_WIIU_SHADOWS_GPU` (default 1 su Wii U, 0 su PC)** (`src/game/render/mrp_shadows_gpu.inc`, `mrp_shadows.cpp`, `src/game/ovl/moby_shadows.cpp`, `ports/wiiu/game/mrp_world_gx2.cpp` `mrp_world_gx2_draw_shadows`, `mrp_moby_gx2.cpp` `part`, `wiiu_gfx.cpp`, shader `mrp_shadow_vol` / `mrp_shadow_dark`):
    - Il codice del gioco (`shadow_points` con le pose rifatte, `drop_shadows_draw`, VU0 0x190, pacchetti) non gira più: l'host prende le stesse forme (sfere e capsule della lista ombre della classe, posate con i giunti della scena MRP: `J p = (p B^-1) S`; `moby_pose_matrices` solo per i moby senza posa nella scena), la direzione (0x1aeb80 + 16 × moby +0xbd) e lo slab (moby +0x84 / +0x88: niente ombre attraverso i pavimenti).
    - La GPU disegna i prismi (i tubi dell'originale) in una maschera RG8 contro il depth del frame (LEQUAL senza scrittura: facce verso la camera contate nel rosso, le altre nel verde, dentro dove rosso > verde = z-pass dell'originale), poi scurisce ×0.75 con bordo morbido (13 tap, ~2.5 px a 1080p) solo nei rettangoli delle ombre. Fra i moby della prima catena e quelli differiti, come il gioco. Nessuno shader del mondo toccato.
    - Prova PC: forme identiche al gioco (scene vs gioco: differenza max 0.000015 su 7964 forme); immagine PC delle forme uguale all'originale al pixel salvo il poligono; walk Wii U-like `=1` vs `=0`: righe hero/camera identiche, RAMHASH diverso solo in 0x15f000–0x161fff (0x15f410 tag texture, 0x160430–0x16046f, 0x1611c0 puntatore pacchetti), 0x1ac780.. (voci ombra), buffer pacchetti, scratchpad.
    - CPU (PC): game thread 0.12–0.22 → 0.002–0.006 ms/frame; console attesa: da ~4.4 ms (Novalis) a ~0.1–0.2 ms. Prismi sul render thread ~0.2–0.4 ms stimati su core 1. GPU attesa 0.1–0.3 ms (riga `[gpu]`: "drop shadows", "mobys deferred").
    - Prima prova su console: Novalis game code 34–37 → 27.6–29.5 ms/host frame, GPU "drop shadows" 0.37–0.55 ms; ma nessuna ombra visibile in modo normale (forme rosse solo con `_DEBUG=1`). Causa probabile: passo 1/255 con sottrazione su R8 (0.99998 troncato a 0). Corretto con maschera RG8, solo addizioni, passo 1.25/255; nuovi debug `_DEBUG=2` (rettangoli scuriti interi), `3` (facce frontali bianche col depth test), `4` (conteggi grezzi). Cambiato solo `mrp_shadow_dark.gsh`. Da riprovare.
    - Servono sulla SD i due `.gsh` nuovi (`shaders/mrp_shadow_vol.gsh`, `mrp_shadow_dark.gsh`); senza, torna il codice del gioco. Riga nel debug menu (Picture): "Drop shadows" GPU / game's.

### Timeline dei core e "world's worker join" (30 Ottobre 2026, notte — da misurare su console)
17. **Cosa è davvero "world's worker join" (~20 ms nelle viste pesanti di Novalis):** con `RAC1_WIIU_PREP_ON_MAIN=1` (default) non è un'attesa: il primo `mrp_world_wiiu_join()` di `wiiu_gfx_frame` *esegue* l'intero job del mondo sul main thread (Core 1): `wiiu_prep_head` (~8–10 ms) + `wiiu_prep_tail` (`build_triangles` ~6–9 ms). Non aspetta la catena (cielo e registri GS già risolti al prepare, drenaggio tfrag sparito col VS). Il worker su Core 0 resta inutilizzato e Core 0 è fermo dopo la lettura della catena (`n1080_tvs`: 7.8 + 6.6 ms = media delle 4 finestre).
18. **Timeline dei core (`src/platform/core_timeline.h`, righe `[core0]` col timing attivo)** (`wiiu_gfx.cpp` `rac1_wiiu_cycle`, `orig_main.cpp`, `mrp_chain.cpp`, `mrp_world_renderer.cpp`): marcatori `OSGetTime` (store atomico, niente lock/allocazioni) di main thread, game thread, lettore della catena e job del mondo; offset dall'inizio del prepare, media su 30 cicli; cosa faceva il job quando il main è arrivato ai join; tempo del lettore per tipo (walk, VU1 nei range / fuori, kick, texture, upload GS) e i 10 programmi VU1 più pesanti. Sul PC tutte funzioni inline vuote.
19. **`RAC1_WIIU_PREP_SPLIT=1` (default 0):** prima metà sul main a inizio disegno (Core 0 sta leggendo la catena), seconda metà al worker su Core 0 appena il lettore è libero, in parallelo a chain join / effetti / 2D / attese GPU e swap; il join prima dei draw del mondo la esegue sul main se il worker non l'ha ancora presa (claim atomico). Atteso su Novalis pesante: join ~20 → ~10 ms, host frame ~50 → ~44–46 ms (poi il limite è il game thread: prepare seriale ~7.3 ms + step 30–40 ms).
20. **`RAC1_WIIU_WJOIN_LATE=1` (default 0, solo con `RAC1_WIIU_PREP_ON_MAIN=0`):** tutto il job sul worker di Core 0, atteso solo prima dei draw del mondo (chain join, effetti, 2D, attese GPU/swap prima).
    - Bench: `BUILD=…/build-wiiu-c0`, default vs `RAC1_WIIU_PREP_SPLIT=1` vs `RAC1_WIIU_PREP_ON_MAIN=0 RAC1_WIIU_WJOIN_LATE=1` (righe in `PERFORMANCE.md`).
    - Proposte non applicate (decide Daino): prepare dei moby su Core 2 in parallelo al prepare del mondo / copia della catena su Core 0 prima di `run_async` (fino a 3–5 ms in meno sul percorso del game thread); prima metà divisa (decisioni tfrag su Core 0); `build_triangles` su due core.

## 3. Cosa Manca (Prossimi Passi in Ordine di Impatto)

### Priorità 1 — Decidere default scala 3D e far confermare visivamente a Daino
- A 1080p nativo (`RAC1_WIIU_3D_SCALE=1.0`) la GPU sta sotto i 33 ms anche su Novalis (29.9 ms). Valutare di portare il default in `wiiu_main.cpp` da `0.75` a `1.0` (o `0.875`) dopo conferma visiva di Daino sulla TV.
- Far verificare a Daino (build su SD) terreno/rocce, cielo e cutout (foglie, recinzioni) nel Pass 2.

### Priorità 2 — Misurare Tfrag VS + cielo tardivo, poi il costo CPU del mondo e `game_thread` (Core 2)
0. **Prima di tutto:** bench con le righe `[core0]` (default, `RAC1_WIIU_PREP_SPLIT=1`, `RAC1_WIIU_PREP_ON_MAIN=0 RAC1_WIIU_WJOIN_LATE=1`) su Novalis e Veldin open; scegliere il default; dalla riga del lettore della catena decidere se vale la pena tagliare i programmi VU1 eseguiti fuori dai range.
1. **Tfrag su GPU Latte (Vertex Shader GX2): fatto (punto 14), da misurare** — `bench.sh` col default contro `RAC1_WIIU_TFRAG_VS=0`; poi riprovare `RAC1_WIIU_PREP_ON_MAIN=0` (Core 0 ora libero dopo la catena) e `RAC1_WIIU_PREP_TIME=1` per la ripartizione di `world's worker join` (ties/shrubs frame, scena, `build_triangles`).
   - Restano su Core 1: `static_frame_build_at` + `scene_from_static_k` + `build_triangles` (worker join) e il piano merged + ~600 draw GX2 (world draws). Proposta successiva: matrici modello statiche dei tie/shrub in una texture per livello, VP per frame nel blocco, `clip`/`rows` calcolati nel VS (niente `InstGpu` da 160 byte per istanza riscritte e byte-swappate ogni frame: ~3400 istanze × 192 B su Novalis).
2. **Ridurre il carico CPU sul `game_thread` (Core 2: ~12.5 ms/step su Novalis):**
   - `Game code (EE update, other)`: ~5.8 ms/step
   - `Game: moby pass (draw loop)`: ~3.9 ms/step (principalmente `moby_shadows` e `moby_cull_light` VU0)
   - `Game: effects draw, draw lists`: ~1.3 ms/step
   - Ottimizzare `moby_shadows` / `moby_cull_light` e i kernel VU0 / collisioni in `EE update` con **Paired Singles (`ps_*`)**.

### Priorità 2b — Proposte che cambiano lo stato del gioco (decide Daino)
- ~~**Drop shadows native (MRP/GPU) invece del codice del gioco**~~: fatto (punto 16, `RAC1_WIIU_SHADOWS_GPU`), da misurare e da far guardare a Daino.
- **Ombre statiche precalcolate (`RAC1_WIIU_SHADOWS_BAKED`, proposta di Daino, passo successivo):** le ombre di tie / shrub / tfrag calcolate una volta per livello (colori dei vertici residenti o piccola lightmap, mai committate) con la stessa direzione (tabella 0x1aeb80); per frame restano solo le ombre dei moby.
- **`vuc` decisa sulla FPU in `ovl_common.h`** (bit-exact, ma è un header: fingerprint del codegen + rebuild completo): accelera tutti i port scritti a mano con le regole VU (pose `moby_pose_matrices`, collisioni, particelle).

### Priorità 3 — Ombre Moderne Leggere su Wii U
- Drop shadows dei moby sulla GPU: fatto (punto 16). Restano le ombre statiche precalcolate (`RAC1_WIIU_SHADOWS_BAKED`, sopra).

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
5. **`run.cfg` sulla SD azzerato (0 byte) da una vecchia versione di `bench.sh`** (quella che scambiava `run.cfg` via FTP): senza `RAC1_FRAMES=0` il gioco headless si chiudeva dopo 300 frame (subito dopo l'intro). Ora `wiiu_main.cpp` imposta `RAC1_FRAMES=0` di default (un `run.cfg` lo può ancora sovrascrivere) e `run.cfg` è stato ripristinato da `rac1-scratch/wiiu/play.cfg`. I benchmark mettono sempre il proprio `RAC1_FRAMES` nella config incorporata.
6. **Debug menu GamePad:** la pagina Picture ha 15 righe e l'ultima ("Exit game") si sovrapponeva alla riga di stato in basso, quindi non si poteva toccare. Ora l'altezza delle righe si adatta (`row_h()` in `debug_menu.cpp`). **Da verificare con Daino:** i controlli che "non andavano" (forse per lo stesso problema dei tocchi, o per un input sul GamePad; nel test `boot_test` il gioco raggiungeva il titolo e il menù riceveva i tocchi).
7. **Shader nuovi = coppie VS+PS compilate insieme (regola SPI del punto 13):** `mrp_tfrag_vs.vert` dichiara gli stessi varying di `mrp_tfrag.vert` fast (stesso `#ifndef RAC1_NO_ALPHA_TEST` per `vAt`, incluso da `mrp_world_fast.glsl`). Controllare ogni nuova `.gsh` con `python3 rac1-scratch/gpuperf3/sem.py <gsh>` (deve dire MATCH). Il VS tfrag è grande (~1500 gruppi ALU inlined, 27 GPR): se CafeGLSL cambia, ricontrollare che compili.
