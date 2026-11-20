# Preview.6 TODO checkpoint

This release keeps eight playable missions. The ninth mission remains under development and is not included. Campaign saves and PC preferences remain compatible.

| Requested fix | Result |
| --- | --- |
| Enemies visible through fog | Hidden enemies are excluded from bodies, health bars, minimap dots, and pointer targeting. |
| Optional cursor lock | Saved in `options.ini`; active only during focused gameplay and released by overlays, focus loss, and minimization. |
| Idle and newly recruited troops | Recruits inherit Attack. Completed movement returns troops to target acquisition; miners can move to another available ore deposit. |
| Castle hover animations | Original label holds, true loops, filters, blend modes, and alpha hit areas are retained. Selection is a one-update state before returning to hover. |
| Toolbar descriptions and costs | Original tooltip artwork, font, icons, XML fields, timing, and HUD layering. |
| Borderless desktop coverage | Covers the full monitor, including the taskbar/dock area. |
| Aspect modes | Default 16:9, Stretched, and Dynamic [Experimental], with inverse mouse mapping and adapted camera/HUD/fog/minimap. |
| HIGH to PC | Original button artwork and font, with the requested PC label and settings action. |
| Miners left/right | Garrison or resume miners without changing military orders. |
| Troop toolbar after castle selection | Selecting troops or using SPACE restores their toolbar. |

Windows checks cover 720p/1080p castle states and tooltips, 15 aspect/size combinations, cursor confinement and release, full-monitor borderless, settings persistence, and unfinished-content recovery. Nine CTest probes cover the seven campaign modules plus toolbar and original blend behavior. An isolated original-player comparison checks 49 castle frames at neutral, hover, early transition, label hold, and late loop states. These comparisons do not establish full combat or campaign parity.

Portable release checks execute the actual Linux/macOS app on their native runners and cover both Mac Option keys and combination suppression, Linux Insert, eight mission entries, settings, aspect snapshots, borderless, original tooltip rendering, pause, audio decoding/playback with dummy devices, and save round trips. Physical audio, extra desktop/window-manager combinations, and a full original-game playthrough remain outside those checks.


## Preview.7 follow-up

The larger PC label uses the original embedded font at 13 pixels inside the same 48x17 button. The toolbar now draws the original up/over/down states for army orders, miner controls, and MENU. Action slots play their authored 20-frame hover glow and pressed artwork. An independent 30 Hz UI clock keeps that glow at its original rate during gameplay fast-forward. Recruitment buttons use the original building highlight and idle-overlay visibility, including the authored alpha and flipped lower-row overlays. Dragging outside cancels the visible pressed state; dragging back while held restores it. Original hit masks stay aligned with the graphics.

`tools/check_toolbar.py` exercises held/dragged/cancelled presses, action pulses, recruitment highlights, and pause at 720p and 1080p. `toolbar_probe` checks timing, release resets, and fast-forward independence. The ninth mission is covered separately by [its Windows checkpoint](deads-checkpoint.md). Full campaign parity remains in progress.
