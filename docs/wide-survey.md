# Wide layout survey

This source survey records the base graphics setups on Black 2 at
`84f67acf15`. Overlay numbers below identify the loading paths. The active
overlay and enabled layers can change during an animation or sub-screen
transition, so the runtime capture remains the deciding source for a layout key.

## Battle

- The main engine is on top. Its graphics mode is 0 with BG0 as 3D at
  priority 1. BG1 is a 256×256 text map for messages. BG2 and BG3 are text
  maps for ability banners, 512×256 and 512×512 respectively. Their initial
  horizontal offsets slide each banner in from its own side.
- BG1 is the first alpha target. BG0, BG2, BG3, OBJ and the backdrop are
  the second targets. Splitting a message layer from the world must preserve
  that relationship.
- On the lower engine the regular command UI creates text BG0 and BG1 as
  512×512, text BG2 as 512×256, and extended BG3 as 512×512. The party,
  item and target panels use separate setup paths.
- Base battle overlays are 10 and 167. The battle selection path uses
  overlay 213. Confirm the current overlay, HP box backing, capture
  and blend state at the first command prompt, send-out and faint.
  The trainer VS intro also needs its own capture.

Sources: [battle constants and blend setup](../src/battle/btlv_core.c),
[message and ability layers](../src/battle/btlv_scu.c),
[command layers](../src/battle/graphic/btlv_input.c).

## Pokémon Summary

- The sub engine is on top. Its four 256×256, 16-color text layers use
  priorities 0–3 in BG order. The main engine is on the bottom. Main
  BG0 is 3D at priority 0, drawn by MCSS using an orthographic camera.
  Main BG1–BG3 are 256×256, 16-color text layers at priorities 1–3.
- On the main engine, BG1 loads the shared app bar and BG3 loads the
  screen backdrop. Sub BG3 loads the moving backdrop. The code scrolls
  both BG3 backdrops together. Both engines enable OBJ. Each uses BG2
  alpha blending, with different second-target masks.
- Summary is overlay 207. Verify the actual panel and text ownership on
  the information, moves and ribbons pages. The main BG0 bit alone
  cannot identify a 3D image on the physical top screen.

Source: [Summary graphics, resource loads and render loop](../src/app/p_status/p_sta_sys.c).

## PC boxes

- The normal box setup selects the main engine on top. Both engines use
  mode 0 with 2D BG0. Main BG0–BG2 are 256×256, 16-color text at
  priorities 0–2. Main BG3 is a 512×256, 16-color text map at priority 3.
- Main BG0 loads the shared app bar. BG3 holds the box wallpaper and
  scrolls horizontally during box switches. Its width makes it a WRAP
  candidate, but capture both edges of the live map before choosing the
  wrap bounds. Extending BG3 does not move the grid cursor or OBJ icons.
- Sub BG0 and BG2 are 256×256 text at priorities 1 and 0 respectively.
  Sub BG1 is 256×512 text at priority 2 and switches between vertical
  offsets 0 and 192. Sub BG3 is not enabled by this setup.
- Box is overlay 255. Capture the box grid, a scroll transition and the
  search view separately to identify their active register signatures.

Sources: [box background setup, wallpaper and scrolling](../src/app/box/box2_main.c),
[box display selection](../src/app/box/box2_seq.c).

## Runtime evidence still required

Use REQ-26's scene sequence and record the physical top engine, current
overlay, DISPCNT, DISPCAPCNT, BLDCNT, each BG control and scroll pair, and
which layers carry the visible text and cursor. Record the presented width
and a native-size capture. Static notes cannot decide per-frame signatures
or edge contents for a new MOVE, COPY or WRAP rule.
