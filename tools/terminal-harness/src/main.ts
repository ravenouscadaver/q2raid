import { MockQ2RaidAdapter } from "./adapter.js";
import { TerminalRenderer } from "./renderer.js";
import { TerminalRuntime, validateManifest, validateTerminalDocument } from "./runtime.js";
import type { TerminalKeyDefinition, TerminalManifest } from "./types.js";

const manifestPath = "/raid/ui/terminal_grunge/terminal_layers.json";
const assetBasePath = "/raid/ui/terminal_grunge";
const canvas = document.querySelector<HTMLCanvasElement>("#terminal")!;
const pathInput = document.querySelector<HTMLInputElement>("#terminal-path")!;
const stateEl = document.querySelector<HTMLPreElement>("#state")!;
const eventsEl = document.querySelector<HTMLPreElement>("#events")!;
const assetsEl = document.querySelector<HTMLPreElement>("#assets")!;
const loadButton = document.querySelector<HTMLButtonElement>("#load")!;
const resetButton = document.querySelector<HTMLButtonElement>("#reset")!;

let manifest: TerminalManifest;
let adapter: MockQ2RaidAdapter;
let runtime: TerminalRuntime;
let renderer: TerminalRenderer;
let pressedTimer: number | undefined;

async function fetchJson(path: string): Promise<unknown> {
  const url = path.startsWith("/") ? path : `/${path}`;
  const response = await fetch(`${url}?t=${Date.now()}`, { cache: "no-store" });
  if (!response.ok) throw new Error(`${url}: HTTP ${response.status}`);
  return response.json();
}

function renderDebug(): void {
  if (!runtime || !adapter) return;
  stateEl.textContent = JSON.stringify({
    terminal: runtime.document.id,
    page: runtime.state.pageId,
    active: runtime.state.active,
    completed: runtime.state.completed,
    buffer: runtime.state.buffer,
    last_result: runtime.state.lastResult
  }, null, 2);
  eventsEl.textContent = adapter.events.length ? adapter.events.map(event =>
    `${event.channel === "semantic" ? "SEMANTIC" : "HARNESS "}  ${event.name}${event.data ? ` ${JSON.stringify(event.data)}` : ""}`
  ).join("\n") : "(none)";
  renderer?.draw(runtime);
}

async function load(): Promise<void> {
  try {
    loadButton.disabled = true;
    manifest = validateManifest(await fetchJson(manifestPath));
    const document = validateTerminalDocument(await fetchJson(pathInput.value.trim()));
    adapter = new MockQ2RaidAdapter();
    runtime = new TerminalRuntime(document, adapter);
    renderer = new TerminalRenderer(canvas, manifest);
    adapter.onChange = renderDebug;
    const assetStatus = await renderer.loadDecorativeLayers(assetBasePath);
    assetsEl.textContent = Object.entries(assetStatus).map(([name, ok]) => `${ok ? "OK     " : "MISSING"}  ${name}`).join("\n") +
      "\n\nMissing decorative PNGs are non-fatal; the primitive fallback remains active.";
    adapter.clear();
    runtime.open();
    renderDebug();
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    stateEl.textContent = `LOAD ERROR\n${message}`;
  } finally {
    loadButton.disabled = false;
  }
}

function canvasPoint(event: PointerEvent): [number, number] {
  const rect = canvas.getBoundingClientRect();
  return [
    (event.clientX - rect.left) * canvas.width / rect.width,
    (event.clientY - rect.top) * canvas.height / rect.height
  ];
}

function press(key: TerminalKeyDefinition): void {
  renderer.setPressedKey(key.id);
  runtime.pressKey(key);
  renderDebug();
  if (pressedTimer !== undefined) window.clearTimeout(pressedTimer);
  pressedTimer = window.setTimeout(() => {
    renderer.setPressedKey(null);
    renderDebug();
  }, 180);
}

function keyFromKeyboard(event: KeyboardEvent): TerminalKeyDefinition | null {
  const keys = manifest.keyboard_map.keys;
  if (event.code.startsWith("Numpad") && /^Numpad[0-9]$/.test(event.code))
    return keys.find(key => key.id === `numpad_${event.code.slice(-1)}`) ?? null;
  if (event.key === "Escape") return keys.find(key => key.id === "esc") ?? null;
  if (event.key === "Backspace") return keys.find(key => key.id === "backspace") ?? null;
  if (event.key === "Enter") return keys.find(key => key.id === "enter") ?? null;
  const value = event.key.length === 1 ? event.key.toUpperCase() : null;
  if (!value) return null;
  return keys.find(key => key.value?.toUpperCase() === value && !key.id.startsWith("numpad_")) ?? null;
}

canvas.addEventListener("pointermove", event => {
  if (!renderer) return;
  const [x, y] = canvasPoint(event);
  renderer.cursorX = x;
  renderer.cursorY = y;
  renderDebug();
});
canvas.addEventListener("pointerdown", event => {
  if (!renderer || !runtime) return;
  const [x, y] = canvasPoint(event);
  const key = renderer.hitTest(x, y);
  if (key) press(key);
});
window.addEventListener("keydown", event => {
  if (!renderer || !runtime || event.repeat) return;
  const key = keyFromKeyboard(event);
  if (!key) return;
  event.preventDefault();
  press(key);
});
loadButton.addEventListener("click", () => void load());
resetButton.addEventListener("click", () => {
  if (!runtime || !adapter) return;
  adapter.clear();
  runtime.reset();
  renderDebug();
});

void load();
