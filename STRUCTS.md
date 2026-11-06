# Reconstructed structures (MK8 Deluxe 4.0.1 main)

Offsets refer to the decompressed main NSO (BID 158C0C7399AABD7BF83795C2EF3484C7); IPS offset = offset + 0x100.

Convention: offsets in hexadecimal; "proof" = site in the disassembly; version checked = 4.0.1 static
unless marked "Eden" (verified at runtime). Further details: analysis notes (not published).

## Access chain [VERIFIED]
```
sysRoot  = *(*0x12fb388)            -> [+0x28] = system
system+0x190  table of 10 subsystems (ptr)      system->[0x190]->[0x800] = raceMgr (0x8ace28)
system+0x240  settingsA (0x86f7bc = 0x7e7404()->[0x10])    system+0x248 settingsB (0x86f7d4)
system+0x7f8  Scene (0x8ace60)                   [[0x1306e70]]+0x58 = controller array (0x8bf09c)
menu config: (*0x12fae70 guard) 0x12fae80 -> obj -> [+0x30] = config
```

## Menu event (x1 of 0x4d92e8) [VERIFIED Eden]
| offset | type | meaning | proof |
|---|---|---|---|
| +0x00 | ptr | source (cbz 0x4d931c) | asm |
| +0x08 | u32 | id (0x15/0x16 = 3p/4p -> 0x4d9774; 0x17..0x1a = 5p-8p -> dialog) | asm, step 0 log |
| +0x28 | u32 | type (0/1/2) | asm, step 0 log |

## Menu config (config = singleton+0x30) [VERIFIED]
| 4.0.1 | 3.0.5 | type | meaning | proof |
|---|---|---|---|---|
| +0x1434 | +0x1434 | 96 × 0x10 (3.0.5: 48) | series race list (Set96) | ctor 0x3fcf20 |
| +0x1fa4 | +0x1ca4 | s32 | local player count (ctor 0x3fd32c; 12 writers: 0x4d99a8 direct + 11 via 0x3fd5a4; 25 readers, no cmp #4) | analysis notes |
| +0x1fa8 | +0x1ca8 | s32 | mode (1..8; accessors 0x7c644.. ==N) | 0x4d9920 |
| +0x1fac | +0x1cac | s32 | network mode 0/1/2 | field map |
| +0x1fb0 | +0x1cb0 | s32 | 1/2/3 | field map |

## settings (A = system+0x240, B = system+0x248), reset 0x86ec8c [VERIFIED]
| offset | type | meaning | proof |
|---|---|---|---|
| +0x08 | s32 | mode (4 = special: 0x86f158 uses +0x19c) | 0x86f154 |
| +0x24 | u8 flags | bit4 (0x7e7708) | 0x86ef2c |
| +0x30 + d·0x1c (12) | descriptor | +0x30 body, +0x34 tires, +0x38 glider, +0x3c character, +0x40 u16 variant, +0x44 type (4 = empty); in **settingsB** (0x86f7d4); 0x86f074 recomputes +0x180/+0x184/+0x188[] (zeroes +0x180 if any field is -1) | analysis notes |
| +0x180 | s32 | number of local players (writer: not reached statically) | 0x7e7794 |
| +0x184 | s32 | number of humans | analysis notes |
| +0x188 | u32[5] | locals per console (0x86f154 reads [0], clamp 5) | 0x86f16c |
| +0x19c | s32 | number of locals if mode==4 | 0x86f160 |
| +0x1a0 | s32 | course id | analysis notes |

## raceMgr (0x8ace28) [VERIFIED]
| offset | type | meaning | proof |
|---|---|---|---|
| +0x50 | s32 | number of karts | analysis notes |
| +0x58 | ptr[12] | karts | analysis notes |
| +0x98/+0xa0/+0xa8 | 7 entries each | tables indexed by device (filled by 0x7c5740 with 0x619348) | 0x7c5794 |
| +0x178 | s32 | number of local players (= entries with +0x44 == 0; it stayed 4 because 0x86a594 found no GamePlayer beyond the 4th → entry demoted to CPU; 8 with the mod) | analysis notes |
| +0x17c | s32[4] → **s32[8] with the mod** (+0x17c..+0x19b) | local → entry id (0x86ebf8, `cmp #3` → `#7` B1i); 5 fills with memset 0xff 0x50 | analysis notes |
| +0x18c (vanilla) / **+0x19c (mod)** | s32[16] (12 used) | slot → entry id (setter 0x86af68, readers 0x88236c/0x882408); with the mod moved to +0x19c int[12] (B1) | analysis notes |
| +0x1cc | s32[12] | kart position → entry (init -1 in 0x869040; the "ctor 0x7eae0" from an earlier step was a different class) | analysis notes |
| +0x1fc / +0x1fd | u8 | flags; +0x1fd = "rebind by index" (title 0x4d744c) | analysis notes |
| +0x200..+0x213 | | atomic flags +0x204 (→ 0x86adec rebind after the applet) / +0x208 (→ 0x86ad18) | analysis notes |
| +0x214 / +0x224 | s32[4] / s32[4] | per-local options (getter 0x425888 clamp, 0x8653cc → gp+0x37c/+0x6e4; toggle 0x1197c0) | analysis notes |
| object | 0x238 B | alloc 0x8ac370, ctor 0x8689b8, no free padding; entry 0x2b0 B ×16 (vt+0x70 0x868b0c) | analysis notes |
Entry (0x2b0 B): +0x44 type (0 human local, 1 CPU, 2 absent, 4 empty), +0x48 id, +0x4c local index (-1), +0x200 GamePlayer*, +0x210/+0x214/+0x218 body/tires/glider, +0x21c character, +0x220 variant, +0x224 type (setter 0x42022c), +0x22c descriptor index, +0x230 slot. Setters: 0x4202f4 character, 0x420ac0/0x420b88/0x420c50 parts (they also update the descriptor).

## Scene (0x8ace60) [VERIFIED]
| offset | type | meaning | proof |
|---|---|---|---|
| +0x3c | 5 × 0x18 {+0 slot, +4 active, +8 type, +0xc ?, +0x10 kart} | viewports (clamp 5 at 0x7e7844/0x7e78d8) | 0x7e76b8 |
| +0xb0 | s32[4] | local slot → viewport (0x7e7af8: `cmp #4; csel` ×5) | 0x7e7b14 |
| +0xcc | s32 | split mode 0/1/2 (31 readers; 0x54798c/0x548d20 → layout name) | 0x7e7778 |
| +0xd0 | s32 | 1/2 | 0x7e7778 |
| +0xd4 | s32 | requested scene type (1..7, table 0xf732dc) | 0x7e7d00 |
Ctor: not found statically (writes system+0x7f8): to be found with a watchpoint (step 2).

## InputSystem (ctor 0x7e4554 → 0x7c53ec) [VERIFIED]
| field | meaning | proof |
|---|---|---|
| +0x188 / +0x18c / +0x190 | count / capacity / device list | analysis notes |
| Touch 0x2940 B; NpadDevice 0x7128 B (ctor 0x5fbce0): 9 slots × 0xc40, id {0..7,0x20}, polling 0..[+0x28] | wrapper already at 8+1 | 0x5fbdc8, 0x5fc03c |
| Controller 0x298 B × 4 (ctor 0xa7f1a0 idx 0..3) + handheld idx 8; Misc 0x178 B | game limit | 0x7c54e4-0x7c55bc |
| controller array `[[0x1306e70]]+0x58` ptr[5] (0x8bf158 clamp 5) | filler to be found at runtime | analysis notes |
| global table 0x12fb230 clamp 5 (0x7caf0/0x7ce88), lazy init 0x7cb44 | – | analysis notes |
| vibration: 18 VibrationTargets (9 × 2) at 0x7ca0c0, clamp 9 at 0x7cae18 | already at 9 | analysis notes |
| npad id tables: 0xf72e48 {0,1,2,3,0x20} (5, used at 0x7c54ac); 0xf554ac {0..7,0x20} (9, used at 0x5fbdc8) | – | asm |

## Character selection (pages 0x2c/0x2d/0x2e; base ctor 0x4bc844; Single 0x4bc80c, WiFi 0x4bcad8, Multi 0x4bcc4c; alloc 0xa28) [VERIFIED]
| offset | type | meaning | proof |
|---|---|---|---|
| +0x124 | s32 | stage (vt87 0x4badb0: stage 1 = entry types) | analysis notes |
| +0x1a8 | u32 | per-player input mask (bit i = player i moves the cursor; bit 4 = touch pad; bit31 active page) | 0x4b80c4 |
| +0x1d0 | link | next page (0x2f/0x30/0x31 kart) | 0x4ba948 |
| +0x660/+0x688/+0x6b0/+0x6d8 | 4 × 0x28 | links to the CharaWin pages 0x28..0x2b (17 sites `0x28+i`) | analysis notes |
| +0x728 / +0x730 | ptr | CharaParts page 0x16 / BG 0x19 | 0x4bcc84 |
| +0x740 | ptr[48] | L_Chara parts (characters) | 0x4b5688 |
| +0x8c0 | s32 | number of slots (1/2/4; Multi: 0x4bcc70) | 0x4bcc70 |
| +0x900 | s32 | first free slot / cursor | analysis notes |
| +0x904 | 4 × {state, +0x908 timer} | slot state: 0 nonexistent, 1 empty, 3 choosing, 5 chosen, 6 wait 10 frames, 7 confirmed (72 accesses/39 clamps in 3 classes) | analysis notes |
| +0x928 | 4 × 0x28 | per-slot queues (amiibo) | analysis notes |
| +0x9cc | s32 | current slot | analysis notes |
| +0x9e0..+0xa20 | s32[2] ×9 | copies for pads 0..1 (online) | 0x4b7830 |
Events (vt92 0x4bbd18): ev+0x8 icon, ev+0x28 type (0 move → cfg+0x1fb4[slot] and preview; 2 decide → state 5), ev+0x2c slot.
UI pad i, player i of the cursor selector (0xb8 B, 4 states), slot i, CharaWin page 0x28+i and local i share
the same index; the only map is raceMgr+0x17c. Advance 0x4ba938: slots in state 7 == cfg+0x1fa4 → 0x8ba5fc(this, +0x1d0).
CharaParts page (id 0x14/0x15/0x16, ctor 0x4be890): slot 13 = 0x4be854 → Page_Lyt name from 0x12797f0 (3 char*), onCreate
0x4be8d0 looks up the pane "mn_CharaKartSlct_Multi" (same name in the 6pl/8pl Page_Lyt) and creates w20 (= 4 at 0x4bea58) L_Name_0i parts
(+0x1e8 count, +0x1f0 ptr[]). The menu pages are all built on entering the menu (0x4048b4 → 0x726468 → 0x7240d8).
Kart page (0x2f/0x30/0x31, ctor 0x4c4b74, 0x4a0 B): +0x39c number of slots, +0x3b8 charsel page, +0x460+i·8 kart state (1 choosing,
2→3 25 frames, 3 confirmed, 4→1 10 frames), +0x498 = 1 on advance (0x4c2a44, then 0x8ba5fc(this, +0x1d0)); 0x4c1bfc(page,
slot, part, flag) writes the current part into the entry; 0x4c2b40(page, slot, 1) = cancel from state 3.
Per-player config: 13 contiguous int[4] arrays cfg+0x1fb4 (character), +0x1fc4, +0x1fd4/+0x1fe4/+0x1ff4 (parts), +0x2004..+0x2074.
0x8ba5fc(page, link) = generic page transition (link+0x8 = source id, link+0xc = destination id).

## Cameras / HUD [VERIFIED]
- Camera preset: global 0x1358e40 (init 0x7e98a8), set +0x38 (mode ≠ 1) / +0x50 (mode 1) chosen in
  0x7e8b9c/0x7e8c54/0x7e8dc0/0x7e8e60/0x7e925c.
- Per-view loop 0x7e9b04: viewports 0..3 (`cmp x25,#4` at 0x7e9c0c) × Lakitu (ctor 0x7e80ec, alloc 0x3d0 in 0x110298, +0x169 kart).
- HUD pages: enum `RaceView0..3` (103-106), `RaceViewFront0..3` (108-111); vtable 0x1289240 / 0x1289538, slot 13 = layout name.
- Per-mode positioning: 0x536a88 (`rc_RaceView_Cmn_00`, `P_Partition_x/y`, `L_TVMap`), pause 0x53198c.
- NVN viewport/scissor: indirect `[[0x130b2b8]]`/`[[0x130b2c0]]`/`[[0x130b2c8]]`, wrapper 0x582c94.

## Per-view 3D rendering (step 2) [VERIFIED Eden unless noted]
| object | field | meaning | proof |
|---|---|---|---|
| RenderLayerManager (mgr) | +0x128 ptr[4] | 3D Sub0..3 (slots 2..5) | RLM dump |
| mgr | +0x1d8 / +0x238 + i*0xc0 | shared LookAtCamera / per-view projections | analysis notes |
| mgr | +0x958 / +0x978 | per-view cameras / projections set every frame by 0x3cc218 | analysis notes |
| mgr | +0xa38 13 x float[4] | rects in 1280x720 units, **y=0 at the bottom** | dump run3/run11 |
| mgr | +0xbd8 / +0xbdc | layout mode (0..3) / scale | RLM dump |
| layerSystem ls = *[0x12fa5e0] | +0x40 / +0x48 | number of slots (32) / slot -> layer | dump run11 |
| ls | +0x50c / +0x510 | job bucket thresholds (10 / 0 in race) | dump run11 |
| Layer (gsys) | +0x30 rect, +0x68 camera, +0x70 projection, +0x78 flag (bit0 enabled, 0x6651a8), +0x84 slot, +0x88 job priority | | dump run11 |
| Layer3D (0x1090 B, ctor 0x68d0e4, vtbl 0x12b4fb8) | +0x1020 renderer R, +0x1088 (s8) view index in R | setter 0x68d4f4 | peer log |
| scene renderer R (0x5ec8 B, 0x6a8f10("Main", arg, heap)) | +0xf0 number of views (arg+0 = 4 fixed at 0x3c4b84/0x3c4c34), +0xf8 views 0x3e0 B | engine+0x298 | scene render log |
| nn::g3d ModelObj (0x69bb0c(model, arg, heap)) | arg+0x8 number of views (4 fixed at 0x880704/0x8807d4/0x810d9c/0x880b88), model+0xe0 number of views, model+0x60 n x 0x58 per-view data | analysis notes | model log |
| CameraDirector (0xb00 B, update 0xc8b60) | +0x3d0 + k*0x28 per-kart sources: e[0] = KartCamUnit+8 | analysis notes | |
| KartCamUnit | +0xb0 KartUnit; +0xc8 = 0 for CPUs (fixed descriptor 0,0,10) | 0x117fb4 [static] | desc log |
| KartUnit (ctor 0x16c968) | +0x8 Vehicle, +0x20 KartCamUnit, +0x40 kart index, +0x50 Matrix34f | analysis notes [static] | |
| Vehicle | [+0x28]+0x8: 4 x float3 X side, U up, F forward, T position | run7 log (orthonormal) | |
| LookAtCamera (0x60 B) | +0x8 view Matrix34f, +0x38 pos, +0x44 at, +0x50 up; doUpdateMatrix 0x619a08(cam, cam+8) | analysis notes | |

## Input (step 3, corrections to step 1) [VERIFIED Eden unless noted]
- "InputSystem" = **sead::ControllerMgr** (ctor 0x61900c, singleton **(main+0x12fbcb0)), per-frame calc 0x6192a4.
  +0x170 devices (Touch, NpadDevice = 0x619220(mgr, 0xe)); +0x188 count / +0x18c capacity (7, written at
  0x7e4468) / +0x190 controller list buffer. Order: pos 0..3 pads 0..3, 4 handheld (npad 8), 5 misc (id 0x12);
  with the mod pos 6..9 = pads 4..7. getControllerByOrder(0x11, n) 0x619268: order 4 = handheld (constant at 5 sites).
- NpadDevice: slot s (= npad id, 8 = handheld) at dev + s*0xc40: +0x60 buttons u64, +0x78 attributes (bit0 connected);
  style at dev+0x30+4*s (5 = none); polled pads dev+0x28 (4 -> 8 with P2).
- NpadController (0x298 B, ctor 0xa7f1a0(c, mgr, idx)): +0x114 hold, +0x120/+0x128 sticks, +0x138 id 0x11,
  +0x178 npad idx, +0x17c bit0 connected, +0x17d style. sead::Controller::calc 0x625540.
- PIM (input player manager, system+0x7f0, getter 0x8ace0c): +0x50 FixedPtrArray<Player,4> (with the mod: external buffer of 8,
  G1; GamePlayer 0x7b8 B ctor 0xcb5b4, +0x3d8 game controller read from the UI pads, +0x760 sead::Controller, +0x768 index,
  +0x76d rebind flag, +0x778 order, +0x78c state; child list +0x38/+0x40 cap 5 → 9), 0x86a594(rm, entryId) assigns the free
  GamePlayer (vanilla 0..3), getControllerByOrder(0x11, n): order 4 = handheld → locals 4..7 at orders 5..8 (G8);
  +0x98 int[] / +0xa0 u8[] / +0xa8 int[] per controller position (7 entries, 16 with P4), init 0x7c5d88,
  per-frame 0x7c4e50, reset 0x7c58d0 / 0x7c5794.
- [[0x1306e70]]+0x58 ptr[5] = menu UI pads (0x2c8 B, ctor 0x8bde24): +0x8 idx, +0xc hold, +0x10 trig, +0x14 trig|repeat,
  +0x18 injected, +0x1c active buttons, +0x1d touch active, +0x2c4 "must confirm", +0x2c5 merge (pad 0 in race).
  **Pad 4 is the touch pad** (not any-controller; with the mod idx 8). Chain: idx → 0x86ebf8 → entry → +0x200 GamePlayer →
  gp+0x3d8. Family 0x8be03c EntryIndex, 0x8be068 GetEntry, 0x8be0bc GetCtrl, 0x8be248 IsReady, 0x8be458/0x8be478 SetButtons/
  SetTouch, 0x8be674 Calc, 0x8bf040 SetPad, 0x8bf058 CalcAll, 0x8bf158 GetPad (clamp 5). With the mod: 4 extra static pads for
  idx 4..7. If IsReady is false the menu 0x4ba77c calls the applet 0x49ea60(N, N, 1, 0).
- Table 0x12fb230 (0x7caf4/0x7ce88) is the BGM enum -> string table, not input.

## Race with 8 locals (step 6) [VERIFIED Eden/static]
- Kart manager **M** = G+0x850 (G = race object, vtable 0x11b6768; M vtable 0x11b7610, 0x2978 B, alloc 0x9c190, ctor 0xa8c00):
  +0x50/+0x58 list K (12), +0xa0 K*[4] locals (mod: +0x3000 K*[8]), +0xc0..+0xd8 various K ptrs, +0xe0 s32[4] ids per local
  (mod: +0x3040 [8]), +0xf0 s32[4] view→kart, +0x104 number of registered locals; per frame 0xa8720 (vt+0x30) → 0xaa16c.
- **K** (per kart, vtable 0x11b7748, 0x1700 B, ctor 0xabb8c, from 0x16ce98 per KartUnit): +0x28 M, +0x60 G, +0x258 descriptor
  (vtable 0x11ba508: vt[0] id, vt[0x58] human local, vt[0xa0] flag), +0x1620 kart, +0x1624 id, +0x1640 human local, +0x260 obj 0x2438.
- Scene (0x128 B, alloc 0x8acc24): viewport array 5×0x18 at **+0x38** {+0 idx, +4 slot, +8 active, +0xc type, +0x10 kart's local,
  +0x14 kart}; +0xb0 s32[4] slot→vp; +0xc0 u8[12] visible kart mask; +0xcc mode. 0x7e76b8 loops n times with clamp 5 (with n=8
  it corrupts vp0/+0xb0): the mod rewrites it as a consistent 4P.
- Race pages: Race 0x69 (Race_Watch 0x75), RaceView0..3 = 0x65..0x68, Front 0x6a..0x6d; registered by 0x406870 (id 0x65+slot,
  layer 6+vp) and opened by 0x8b21dc(&id, flag); the Race page has race+0x350/+0x354/+0x358 PtrArray<5> of per-viewport objects
  (0x118 B, 0x54fcc4(obj, vp, &vpEntry, race); +0xf4 kart, +0x100 slot) created in 0x5326dc (called from 0x536a10 onCreate).
  RaceView onCreate 0x547d20: slot = id-0x65 (0x547d5c), vp = 0x86eb84(slot), page+0x1d0 = race+0x358[vp]. Page table
  0x8bcc5c(): +0x8+id*8 factory, +0x448+id*8 owner, 0x88 entries; ids never registered in retail: 0,1,2,0x12,0x1d,0x43,0x5b,0x5e.
- Layer2D (0x1678 B, ctor 0x3c8734): LookAtCamera 0x60 (pos +0x38, z at +0x40 = distance), perspective projection 0xc0
  (0x61bd1c, 0x61bf00(near, far, fovy, aspect); fovy +0xa0, aspect +0xb0, dirty +0x8). 2D Sub i = slot 6+i with the rects of the 4P quadrants.
- layer+0xde4: bit 0x2 hidden (0x703738 while capturing, slot < capture slot), 0x4 R view flag (0x3cd194 from mgr+0x9d6),
  0x8/0x10 texture capture/release +0xc10, 0x20+0x218 pause texture (prio 5). +0x80 display, +0x81 forced display, +0x82
  frame display mask, +0x83 clear. Job: 0x667358 (queues by priority: >10 queue 0 on 2 cores, 0..10 queue 1), Draw 0x683ecc
  (display, ctx, layer, slot*258+0xffff).
- Per-view groups of R: R+0x4228 count, R+0x4230 + i*0x410: +0x408 flag, +0x409 half (0x3cd5c8: 0,0,1,1 in 4P; 0xff elsewhere); R+0x4239.

## 8-player race: GPU order, HUD 5-8, intro, Lakitu (step 6b) [VERIFIED Eden/static unless noted]
- GPU order: the segments of each display are sorted by ascending id (sort 0x66767c, heapsort 0x689cec, comparator 0x684a64
  = a.id - b.id) and submitted by 0x6845f8. Draw/SubDraw job id of a layer = slot*258+0xffff (0x680f8c); so the draw order
  follows the layer slot, not the priority. The extras (14..21) came out above 2D Main/Front/Overlay (10..13): mod = remap by rank.
- HUD player color: 0x86a708(entry+0x4c), index ≥ 4 → color 5 (red); fix site 0x86a720 (`cmp w20,#4`).
- Race page (0x69, ctor 0x53186c, onCreate 0x53198c → base 0x5326dc): +0x320 layout `rc_RaceView_Cmn_00` (partitions
  P_Partition_x/y/y3l/y3r/y4l/y4r, pic1 panes children of N_All_00; y3* at x=∓213, y4* at x=∓320, invisible by default). The
  per-viewport-object updates are unrolled over race+0x358[0..4]: 0x5350a8 (0x5500e4), 0x534848 (end-of-race reduction:
  o[0x111], 0x551294, 0x5512c4, o[0x110]), 0x53467c (0x551378), 0x5345e0 (+0x108 = 0, 0x551914), 0x5349a8 (0x5502f0 if !o[0x111]);
  alarm 0x53635c(race, which, kart) → 0x547aec(page, which, kart). 0x536a10 (another class with the same layout) is not used in the 8-player race.
- 3D Sub0..3 enabling: 0x3cd194 (mgr+0x9d4+i*0x20 && !mgr+0xc72), effective state in sub[i]+0x78 bit0; mgr+0xc72 = 1 with
  event 2 of 0x3d58e4.
- Director (cameras): per-vp stack for vp 0..3 at dir+0x70+vp*0xd0 (+0x78 head, +0x80 count; node = head-0x10: [node] descriptor, +8
  priority, +0xc node type). Bind 0xc89cc (type 1, prio 10, vp 0..3 only); push 0xc9074 (clamp vp ≥ 4 → 0); during the intro the
  head of vp0 is type 2 / prio 20 (desc type 0 = external camera), then returns to type 1. Per-kart descriptors: dir+0x3d0+k*0x28
  {e0 desc0, e1 desc1 (rear-view), ..., e3 rear-view flag}; desc +0x8 type, +0xc pos, +0x18 at, +0x24 up, +0x30 fovy.
  setFovy 0x61bdd0(proj, fovy): +0xa0 fovy, +0xa4 sin, +0xa8 cos (half angle).
- Lakitu: manager (x0 of 0x7e9b04, [..+0x248]) +0x38 count, +0x40 ptr[count] (allocated exactly with 0x5fad70, alignment 0x40,
  never freed), +0x4c fill index. Created in 0x7e9d40 (vt+0x70, from 0x1101f0): only for Scene vp 0..3 with
  a kart and type 1 → with 8 locals count = 4. Factory vt+0xc0 = 0x110298(mgr, desc u16 {kart, slot}), object 0x3d0 B (ctor 0x7e80ec,
  vtable 0x11b9768): +0x168 view slot, +0x169 kart; lak+0xf0+0x28 = mgr, +0xf0+0x30 = mgr+0x30. 0x7e9a68(mgr, kart) looks up by kart.
  SetSlot vt+0xb8 = 0x10f120 (hides if slot ≥ 4 at 0x10f198). Position relative to the camera: preset 0x1358e40 +0x38 (normal)
  / +0x50 (mode Scene+0xcc == 1, 2P), read at sites 0x7e8bc4/0x7e8c80/0x7e8de8/0x7e8ea0/0x7e92b4.

## In-race pause (step 7) [VERIFIED Eden/static]
- Race_Pause id 0x6f (ctor 0x53c860, vtable 0x1288638): vt13 0x53c818 layout name, vt73 0x53c9ec onCreate (L_SwitchBtns_0i panels
  for i < locals, loop up to 0x53d68c), vt75 0x53e0bc assignment (pause+0x51c = pauser = race+0x3f3-1, loop 0..3), vt90 0x53ea20
  events (only event+0x2c == pauser). pause+0x1a8 player mask, +0x1b0 cursor, +0x1e0 rc_Pause_Sg_00 control, +0x1e8/+0x1f0
  buttons (mask +0x88, index +0x98), +0x51d multi mode.
- Key scanner (ctor 0x8b4010, vt+0x20 = 0x8b4038): +0x10 page / +0x18 control; pads 0..4 unrolled; 0x8ba634(page,i) bit i of
  page+0x1a8; 0x8ae0d0(ctl,i) = ctl+0x78 && bit31 and bit i of ctl+0x58 && 0x8ba634(ctl+0x20,i); event via 0x8b4174.
- Base control (0x8ad888): +0x58 player mask = 0x8000000f by default, +0x78 active. L_SwitchBtn: 0x158 B object ctor
  0x51c088 (+0xc8 player, +0xcc type), control at +0x30 (vtable 0x1285598), scanner at +0xd8.
- Page cursor (0x8af3f4 assigns, 0x8afaf0 releases, 0x8b0084 reads pads 0..3 per frame with 0x8b00e0(cur,i)): 4 slots at
  cur+0x28+s*0x20 (state, +0x30 index), pad >= 4 -> slot 0; focus per pad mgr(0x8bdc8c)+0xc68[4]. Graphical cursor: page
  cm_Page_Cursor 0x8bf228() +0x1d0[4] (cm_L_BtnCursor_00..03; the 4.0.1 RomFS has 8), color = ColorChange frame i+1 via
  0x442ca0(obj, frame) (4.0.1: frames 7..10 = colors P5..P8).
- RaceView HUD per side: 0x50efa8(lyt, slot) (only caller 0x547ec4 in onCreate 0x547d20) → PlayerPos animation frame =
  (slot&1)+2 (even = left HUD); rc_RaceView_8P_Ml has only PlayerPos frames 0..3.
- Minimap: race+0x328 L_TVMap control (ctor 0x50786c); 0x533368(race) loads from the per-course/mode table (20 B records)
  race+0x338 scale, +0x33c/+0x340 translation, +0x344; applied with 0x8ae020(ctl+0x30, {s,s}) / 0x8adff0 (0x532c54/0x532c74).
- Driving aids: pause toggle 0x51cd34 → GamePlayer (L gp+0x20c, Y gp+0x208 + kart+0x394, R gp+0x209, X cfg+0x230c).
  Save 0x3fe040 / load 0x3fdd70 (x19 = cfg+0x1fa4, i < locals): guests in int[4] cfg+0x2310 (Y), +0x2320 (L),
  +0x2330 (R) without clamp; right after, cfg+0x2340 game structure (index, byte +0x2344 read at 0x539b34, pointer +0x2348).

## 8-player selection (step 5b) [VERIFIED Eden/static]
- Selection class (0x2c/0x2d/0x2e) with the mod: object 0xcf0 (factories 0x4091c4/0x4091f0/0x40921c). +0xa28 {state,value}[8] (formerly
  +0x904), +0xa68 link[8] to CharaList (formerly +0x660), +0xba8 queues[8] (formerly +0x928, data at entry+0x14, capacity 2), +0xce8 page to
  read the L_Name from (0x16 Multi or names page 0x12). Old areas = sentinels.
- CharaList (variant window, formerly "CharaWin") id 0x28..0x2b, factory 0x40918c, onCreate 0x4b28d4 +0x1e8 = id-0x28; mod slots 4..7
  = id 0x01/0x02/0x5e/0x64. 18 sites `0x28+i` -> 0x8bf0ec (LR listed in the analysis notes). 0x8bcc78 = 0x87 pages (id >= 0x87 -> entry 0).
- Cursor selector (page+0x1b0): single alloc 0x8bafb8 (`mov w0,#0xb8` 0x8bafcc), ctor 0x8af218, init 0x8af2c4; slots at +0x28+V*0x20
  {+0 state, +4 reactivate, +8 index, +0xc prev, +0x10 anim, +0x14 pos}, +0xa8/+0xac keys, +0xb0 focus just taken. Mod: +0xb8 'CUR8',
  +0xbc n (4/8), slots 4..7 at +0x128.. (V = s+4). A pad participates if: bit in page+0x1a8, focus (mgr 0x8bdc8c +0xc68[4]; 4..7 shadowed),
  bit in the item mask +0x58, slot activated, GetPad != 0.
- Per-page index memory: [[GOT 0x1306e70]]+0x60 int[0x88][4] (slots 4..7 shadowed in cur8::mem8). cm_Page_Cursor: 0x1f8 -> 0x230, graphical
  cursors 04..07 at +0x210.. (onCreate 0x49a8fc).
- Per-player config: 13 int[4] cfg+0x1fb4..+0x2083; accesses `ldr x8,[x8,#0x30]; add xB,xB,xI,lsl #2; ldr/str [xB,#imm]`: indices 4..7 shadowed in
  cfg8::sh[13][4] (38 hooked adds).
- CharaParts 8pl "names" page on id 0x12 (factory [GOT 0x1307ad0] = 0x409070, layer 0xa): onCreate 0x4be8fc forced onto the Multi branch;
  N_Chara_00/N_ConSuppo_00 hidden; selection exit vt84 0x4bb6a0. 0x4694fc (variant window position) uses the table
  0xf200a8 {0x14,0x15,-1,0x16}[+0x8c0-1]. Menu 3D models: 0x7e7434()+0x190[4]+0x238 (+0xc0 count, +0xc8 ptr[]), 0x13d424 without
  bounds check before the access.
- Kart page 0x31 with the mod: object 0x4e0, kart state +0x4a0[8] (formerly +0x460); control set B (L_KartSlot/L_Body/L_Tire/L_Glider/
  OKWin 0..7) built with 0x4bfdd0 on names page 0x12 and swapped with set A (+0x3a8, +0x39c, +0x3c0..+0x3ff, +0x450, +0x108/+0x110,
  +0x1b0, **(+0x178)). Part selector: sel+0x1e8 = slot, initial part from cfg+0x1fd4/+0x1fe4/+0x1ff4[slot] (0x476628/0x477400/0x477e38).
  Single "Change" animation (group 0 of [page+0x1d0]+0x70, 0x8ad300) shows the L_KartSlots and hides the grid.
- Part icons in character colors (tc_KP_*.bntx): manager 0x8bf0b0(&3)+0x1b0, ctor 0x40c5f4: queue 64 (0x40c630) and heap
  UIHeap_PartsTex 0x6d0000 (0x40c62c), requests 0x4104a8 (16 per player); beyond capacity 0x410514 writes queue[0].
- PartsCursor (page 0xe, cm_L_KPSlotCursor_00..07): onCreate 0x49acdc (creates i <= 3; mod 7, slots 4..7 at +0x210..), 0x49ae1c(pc,i,on),
  color 0x443738(ctl, i+1) (8 frames), position 0x44355c (clamp G3 0x443590).
- Variant window (CharaList): item vt+0x70 = 0x46c15c -> 0x46c0a4(part,p) -> 0x4552b8 (graphical cursor placement
  cm_Page_Cursor+0x1d0[clamp4(p)], clamp 0x4553f4/0x455468); scale from G_Ptn_00 (0x4694cc: frames 0/1 = 1.0/0.65; 4.0.1 frames 2/3 = 0.4/0.3).
- Page manager: mgr(0x8bdc8c)+0x30[id] = page (set 0x8bd0c8, get 0x8bd0e8 via 0x8bf0ec), +0x28 = 0x87 entries.
- Player color: 0x86a708(local) -> raw (0..3, CPU 4); HUD code 0x420d18 = raw+1 (P5-P8 7..10 with the mod), row code
  0x420da0 (P5-P8 10..13, CPU 5), Rank 0x513b20 (tc_RC_Metal5P..8P 16..19 / tc_RC_5P..8P 20..23).
- Mod file sd:/mk8dx-8p/p58.bin: 0x100 B 'M8P8' v1, cfg8::sh at 0x08, aids [3][4] at 0xd8, FNV-1a at 0xfc.

## Menu 3D models (step 7 B) [VERIFIED Eden/static]
- Menu KartDirector: init 0x13d778, `+0xc0 = settings+0x180` (4 in the menu, `settings+8 == 5`), `+0xc8` KartUnit*[], `+0xd0` "KartWork" heap
  (0x1864000 in the menu, `+0xe9 = 1`; free 0x42ff8 after 4 KartUnits ≈ 6 MiB each), created in the current heap (0x609f64(*[GOT 0x12f9e48]),
  vt+0x80 = maximum allocatable). Resources: 0x884bd8(sys, flag, settings, n) requests slot i for descriptors with type 0 (settings+0x30+i*0x1c,
  type at +0x14); 0x8843a8(sys, i) waits for bit `0xf73d14[i]` of sys+0x5040 without limit; 0x89fc4c(kart) → 0x884b50(sys, entry+0x230) kart
  resources (entry from raceMgr+0x58[+0x1cc[kart]]). In the menu slots 4..7 serve other actors (anim `Shutter`/`LocalRoom_ftp`).
- Menu-kart object = [KartUnit+0x30] (0x13d47c): +0x8..+0x78 presets (3 records {xyz offset, pitch, fovy, sh x, sh y} + 8 floats), +0x80
  camera descriptor (eye +0x8c, at +0x98, up +0xa4, fovy +0xb0, sh +0xb4/+0xb8), +0xc0 slot, +0xc4 type (2 = Multi), +0xd4 visible,
  +0xec position (0,0,0: all models at the same point). Config 0x147678 (preset {3,4,5,6}[slot] at 0x14771c without limit, copy from
  x8+0x38 at 0x147738), configuration unrolled over 4 at 0x13e59c.
- Multi: 4 scene views (3D Sub0..3, mgr+0x128) rect 640×320, aspect 2.0, projection shift proj+0xb4/+0xb8 = preset sh;
  each model has a u16 view mask per shape (0x68fa38 / 0x113c88(node, on, view)); KartUnit +0x362 = 1<<slot (bits 0..3 applied by
  0x16e184), nodes of the active model: list u+0x248+sel*0x78 (sel = u+0x348), wrapper = node+0x10.
- Pedestal viewer `[0x398898()+0x270]+0x70`: 4 entries at +0x568+i*0x138 (+0x8 BaseTable model, +0x34 position), accessors 0x430cc4
  (view bit), 0x430d5c, 0x430d7c, 0x430d8c, 0x430e24, 0x430e58 without index check; thunk 0x427648.. on [0x398898()+0x270].
- Menu flow (2026-11-04): default local players 0x869180(rm, 4) at 0x4f9e6c (startup) and 0x4fa260 (menu) -> entries 0..3 +0x44 = 0,
  4..15 = 2; 0x869a14 builds settings B from the valid entries (+0x44 != 2, parts != -1, type != 4, +0x50 == 0); 0x86f074 (+ copies
  0x86edb4, 0x86f474) limits mode 5 to 4 descriptors; settings A = copy of B (0x7e6e54). Menu scene heap = TaskHeap of the task entry
  (0x603290, 0xd200000), HeapResident 100 MiB carved out after the KartDirector (0x425c70/0x425c9c).
- Loader model lists (sys 0x8ace44, 0x9a50 B; with the mod 0xb4f0): **8** lists of 12 records (Driver 0x5060 ×0xc0, Emblem 0x59f8 ×0xb0,
  Tire 0x62c0 ×0xb0, TireSub 0x6b88 ×0x88, 0x71f0 ×0x80, Body 0x77f8 ×0xc8, Wing 0x8458 ×0xc8, Arm 0x90b8 ×0xc0). Lookups 0x88e6f4/0x88ecfc/
  0x88f0c0/0x88f5b4/0x88fb0c/0x88fecc/0x890374/0x890944 (index or pointer; -1/NULL if full), preloads 0x3dc4f0..0x3dcf2c, per-list fields in
  the analysis notes. With the mod: records 12..15 at sys+0x9a50..0xb030 (rec16), entries 5..8 of the Body/Wing ring at
  sys+0xb030/0xb290 (ring9). Stable heap from BackgroundLoad [sys+0x4ed0] (0x13b00000 -> 0x17500000) and DriverBodySE [sys+0x4ee0]
  (0x279e000 -> 0x350e000, TireSub/0x71f0); parent of BackgroundLoad = startup TaskHeap (0x603290, 0x15b00000 -> 0x19500000);
  ring in [sys+0x320] = CourseResource (0x3d5794, 0xe404000 -> 0x10004000). Ring part 0: Body 0x89d668 (path x2+0x88,
  loads 0x89b290), Wing 0x89dae4 (x2+0x138, 0x89bc70). Further details in the analysis notes.
- Visibility of menu units: selection entry 0x4b8658..0x4b8678 hides [+0x8c0, cap) with 0x14a7ac(i, false) (cap 4 in the
  code, 8 with the mod); selection exit (stage 4 of vt87 0x4badb0) waits for 0x14b5a4 on all dir+0xc0 units (analysis notes).
- Models: class vtable 0x12b53f0 (ctor 0x69b8e0, init 0x69bb0c), per-view draw mask u16 at +0xc; per-view test in job
  0x7210f0 -> 0x6e2430 (`obj+0xc & [R+0x5d14]` against entry+2 = 1<<view); per-shape masks at [*(m+0x28)+i*0x38]+0xc (0x68fa38/0x68fb7c).
  Global "coin" model created at startup (0x2744b8 -> 0x7f8aac) with +0xc = 0xffff (analysis notes).
