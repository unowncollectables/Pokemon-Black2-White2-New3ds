# Wi-Fi Stage 1 — link surgery + transport bottom

Branch `ghost/bigbuild-wifi`, base Build 20 (`096110bec`). This stage
replaces the stub wall with the game's own network stack, as one atomic
set (the Stage 0 doc's condition). It does **not** make any online
feature work yet: the radio seam at the very bottom is fail-honest
(see below), so online menus now run the real DWC/GFL_Net code and fail
through the game's own error paths instead of dead-ending on stub
answers. Offline play is untouched: every newly linked object hangs
off a symbol the baseline image already referenced (the BFS closure
grew by following existing references, never by adding entry points),
and the only newly-real host primitive, `OS_CreateThread`, has no
callers outside the network libraries (censused).

## What changed

- `scripts/n3ds_link_closure.py`: the network libraries
  (`lib/dwc`, `lib/nitrowifi`, `lib/dpw`, `lib/poke_net`) are eligible
  for the link closure, except `lib/dwc/src/vct/` (voice chat stays
  stubbed — no mic path). `src/gfl/net.c` and `src/system/net_err.c`
  are no longer excluded.
- `n3ds/nitro/link_stubs.c`: 163 stubs retired — the whole `DWC_`
  family (incl. the seven `DWC_Nd*`), `DWCi_*`, `NHTTP*`,
  `POKE_NET_GDS_*`, `DpwTr_*`/`DpwBt_*`, `Gdb_*`, `Sc_*`,
  `NdReportResults`, `NCFG_ReadConfigEx`, `Crypt_VerifySignature`
  (the fail-closed typed stub) and `Crypt_SetAllocator`. The real
  implementations link from the libraries above; ELF spot checks:
  `DWC_NdInitAsync` 0x210 (stub was 0x30), `DWC_LoginAsync` 0x188,
  `Crypt_VerifySignature` 0x54.
- `n3ds/nitro/net_stubs.c`: emptied. The GFL_Net session fiction
  (86 symbols incl. `GFL_NetBoot`, `GFL_NetIsInitialized`) is retired;
  the real `src/gfl/net*.c` + `src/system/net_err.c` link instead.
  `GFL_NetBoot` (0xb4) runs at boot by the game's own design — its
  Wi-Fi prepare is `DWCRapCommon_StartLibrary`.
- `scripts/n3ds_game_objects.py`: three library files that call the
  MWCC builtin `__memcpy` by name (`lib/dpw/src/dpw_tr.c`,
  `lib/dpw/src/dpw_battle_tower.c`, `lib/poke_net/src/poke_net.c`)
  get `-Wno-error=implicit-function-declaration` so they compile;
  the port already defines the `__memcpy` symbol in
  `n3ds/nitro/nitro_stubs.c`.
- `n3ds/nitro/os_thread_host.c`: `OS_CreateThread` is real now —
  a libctru worker thread per OSThread (joinable via
  `OS_JoinThread`, `OS_IsThreadTerminated` reports completion).
  DS priorities map below the game thread. This unblocks the SVL
  token thread `DWC_NdInitAsync` starts and joins, and the NHTTP/CPS
  workers. Callers in the closure are exclusively network-library
  objects, so offline behaviour cannot change.
- `n3ds/nitro/net_bottom.c` (new): the transport bottom.
  Real: RC4 (SDK context layout; lib/nitro's rc4.c is MWCC asm), the
  MSL ctype tables (`include/ctype.h` documents the layout), the
  middleware signature strings (their `*_version.c` files use MWCC
  section pragmas), a real OS alarm service (one libctru thread;
  CPS ACK, WCM keep-alive, NVRAM poll), `errno`, `strcmpi`/`strnicmp`,
  the three bucket-set functions gbucket.c's header says the ROM
  dead-stripped (`NewBucketSet`/`FreeBucketSet`/`DumpBucketSet`), and
  `gsCryptRSAEncryptBuffer` (PKCS #1 v1.5 on the tree's gsLargeInt;
  gsCrypt.c in the tree carries only the verify half).
  Fail-honest: the DS radio seam — `NWM_*` and the WM driver-lifecycle
  calls under WCM (`WM_Init`/`WM_Enable`/`WM_PowerOn`/`WM_StartDCF`/…)
  — answers synchronously with the documented failure codes. Both WCM
  backends switch on those codes and propagate the error to DWC and
  the game's error screens; nothing fakes a scan or an association,
  and no caller waits for a callback that will not come.

## Remaining for a connection-attempting Mystery Gift (Stage 2)

1. The radio seam, for real: fabricate NWM scan/connect results from
   the 3DS's own association (ac:u state, SSID/BSSID of the connected
   AP) so WCM believes it associated, then CPS over libctru `soc:U`
   BSD sockets with the community DNS override — the shape
   SoulSilver's `ss3ds_net.c` proved. `soc:U`/`ac:u` must be confirmed
   in this build's service access (the .3dsx runs under the shell's
   existing service set; the CIA service list is the Builder's).
2. `OS_SendMessage`/`OS_ReceiveMessage` semantics under real threads:
   the host queue implementation predates real workers; the SVL/NHTTP
   message flows need a thread-safe pass.
3. DWC's per-frame process pump: confirm the game's own loop drives
   `DWC_ProcessInet`/`DWC_SVLProcess` from the GFL_Net state machine
   now that it is real (expected — `net_state.c` was written for it —
   but unverified until the radio seam exists).
4. Only then: Mystery Gift → Receive via WFC against a self-hosted
   endpoint first (per the study's server stance), then the community
   resolver.

## Build

Full clean build (`PBW2_LINK_GAME=1 bash scripts/n3ds_build.sh -j2 b2`)
green; reloc check OK. `.3dsx` 10,033,660 B vs Build 20's 9,214,136 B
(+819,524 B — the real stack). White 2 objects compile in the same
pass (game 1218/1218 both); the w2 link was not run in this stage.
