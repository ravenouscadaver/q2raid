# Q2Raid Terminal Harness

A local authoring/test harness for terminal JSON. It is deliberately not a second Director: terminal content is loaded from the canonical repository and the harness only emulates the narrow terminal runtime boundary needed to exercise it.

## Run

```text
cd tools/terminal-harness
npm install
npm run dev
```

Open the printed localhost URL. The default document is `raid/terminals/entrance_terminal.json`; change the path in the toolbar to load another terminal document.

`npm test` builds the TypeScript runtime and runs its regression tests.

## Current v1 boundary

- Loads schema-version-1 `packet_reconstruction` terminal content.
- Uses the 51-key geometry in `raid/ui/terminal_grunge/terminal_layers.json`.
- Accepts canvas clicks and physical keyboard input.
- Displays pressed-key feedback, prompt/buffer state, and a semantic/debug event log.
- Loads the approved decorative PNG paths when those private/local runtime assets are present; missing art uses a primitive fallback and is non-fatal.
- Publishes only current terminal semantic events (`terminal_open`, `terminal_complete`). `on_success` / `on_failure` are shown as harness-side content results and do not invent a Director `terminal_action` signal.
- Does not emulate undeclared Director counters, flags, timers, or operations.

## Font boundary

`src/font.ts::createCustomTerminalFontAdapter()` is the intentional hook for a future mapped Quake bitmap-font implementation. It currently returns `null`, so Canvas text falls back to a generic monospace font. The terminal remains authorable without making assumptions about the native font atlas.

## Portability rule

If harness behavior requires a new runtime fact, operation, signal, or terminal JSON field, define that interface canonically first. Do not add a browser-only spelling or behavior and later try to make Quake imitate it.
