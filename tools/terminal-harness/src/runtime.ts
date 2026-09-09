import type { PacketReconstructionPage, TerminalDocument, TerminalKeyDefinition, TerminalManifest, TerminalState } from "./types.js";
import type { TerminalRuntimeAdapter } from "./adapter.js";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

export function validateManifest(value: unknown): TerminalManifest {
  assert(value !== null && typeof value === "object", "terminal_layers.json must be an object");
  const manifest = value as Partial<TerminalManifest>;
  assert(Array.isArray(manifest.canvas) && manifest.canvas.length === 2, "manifest.canvas must be [width,height]");
  const [width, height] = manifest.canvas as [number, number];
  assert(Number.isInteger(width) && width > 0 && Number.isInteger(height) && height > 0, "manifest canvas dimensions must be positive integers");
  assert(manifest.layers !== undefined, "manifest.layers is required");
  assert(manifest.keyboard_map !== undefined, "manifest.keyboard_map is required");
  assert(manifest.keyboard_map?.geometry_space === "canvas_pixels", "keyboard geometry_space must be canvas_pixels");
  assert(Array.isArray(manifest.keyboard_map?.keys), "keyboard_map.keys must be an array");
  assert(manifest.keyboard_map!.keys.length === 51, `keyboard_map.keys must contain 51 keys; found ${manifest.keyboard_map!.keys.length}`);

  const ids = new Set<string>();
  for (const [index, key] of manifest.keyboard_map!.keys.entries()) {
    assert(typeof key.id === "string" && key.id.length > 0, `keyboard key ${index} has no id`);
    assert(!ids.has(key.id), `duplicate keyboard key id '${key.id}'`);
    ids.add(key.id);
    assert(Array.isArray(key.rect_px) && key.rect_px.length === 4, `keyboard key '${key.id}' rect_px must be [x,y,w,h]`);
    const [x, y, w, h] = key.rect_px;
    assert([x, y, w, h].every(Number.isInteger), `keyboard key '${key.id}' rect_px must use integers`);
    assert(w > 0 && h > 0 && x >= 0 && y >= 0 && x + w <= width && y + h <= height, `keyboard key '${key.id}' rectangle is outside the ${width}x${height} canvas`);
  }
  return manifest as TerminalManifest;
}

export function validateTerminalDocument(value: unknown): TerminalDocument {
  assert(value !== null && typeof value === "object", "terminal JSON must be an object");
  const doc = value as Partial<TerminalDocument>;
  assert(doc.schema_version === 1, "terminal schema_version must be 1");
  assert(typeof doc.id === "string" && doc.id.length > 0, "terminal id is required");
  assert(typeof doc.presentation === "string" && doc.presentation.length > 0, "terminal presentation is required");
  assert(typeof doc.start_page === "string" && doc.start_page.length > 0, "terminal start_page is required");
  assert(doc.resume_policy === "reset_on_close", "harness v1 supports resume_policy 'reset_on_close'");
  assert(doc.pages !== null && typeof doc.pages === "object", "terminal pages object is required");
  const pages = doc.pages as Record<string, PacketReconstructionPage>;
  assert(pages[doc.start_page] !== undefined, `start_page '${doc.start_page}' is not declared`);

  for (const [pageId, page] of Object.entries(pages)) {
    assert(page.type === "packet_reconstruction", `page '${pageId}' uses unsupported type '${String((page as { type?: unknown }).type)}'`);
    assert(typeof page.prompt === "string", `page '${pageId}' prompt must be a string`);
    assert(typeof page.answer === "string" && page.answer.length > 0, `page '${pageId}' answer is required`);
    assert(Array.isArray(page.available) && page.available.every(item => typeof item === "string" && item.length === 1), `page '${pageId}' available must contain one-character strings`);
    assert(page.shuffle === false, `page '${pageId}' requests shuffle=true; shuffled physical-key semantics are not defined yet`);
    assert(page.wrong_input === "restart_from_matching_prefix", `page '${pageId}' has unsupported wrong_input '${String(page.wrong_input)}'`);
    assert(typeof page.submit_required === "boolean", `page '${pageId}' submit_required must be boolean`);
    assert(typeof page.on_success === "string" && page.on_success.length > 0, `page '${pageId}' on_success is required`);
    assert(page.on_failure === "clear_input", `page '${pageId}' has unsupported on_failure '${String(page.on_failure)}'`);
  }
  return doc as TerminalDocument;
}

export class TerminalRuntime {
  private stateValue: TerminalState;

  constructor(readonly document: TerminalDocument, private readonly adapter: TerminalRuntimeAdapter) {
    this.stateValue = this.initialState();
  }

  private initialState(): TerminalState {
    return { active: false, completed: false, pageId: this.document.start_page, buffer: "", lastResult: null };
  }

  get state(): Readonly<TerminalState> { return this.stateValue; }
  get page(): PacketReconstructionPage { return this.document.pages[this.stateValue.pageId]; }

  open(): void {
    this.stateValue = { ...this.initialState(), active: true };
    this.adapter.record({ channel: "semantic", name: "terminal_open", data: { source: this.document.id } });
  }

  reset(): void { this.open(); }

  cancel(): void {
    if (!this.stateValue.active || this.stateValue.completed) return;
    this.stateValue = this.initialState();
    // Canonical runtime deliberately emits no terminal_cancel semantic signal.
    this.adapter.record({ channel: "harness", name: "session_cancelled", data: { source: this.document.id } });
  }

  isKeyEnabled(key: TerminalKeyDefinition): boolean {
    if (!this.stateValue.active || this.stateValue.completed) return false;
    if (key.role === "cancel" || key.role === "back" || key.role === "submit") return true;
    if (key.value === undefined) return false;
    const value = key.value.toUpperCase();
    return this.page.available.some(candidate => candidate.toUpperCase() === value);
  }

  pressKey(key: TerminalKeyDefinition): void {
    if (!this.isKeyEnabled(key)) return;
    if (key.role === "cancel") { this.cancel(); return; }
    if (key.role === "back") {
      this.stateValue.buffer = this.stateValue.buffer.slice(0, -1);
      this.stateValue.lastResult = null;
      return;
    }
    if (key.role === "submit") { this.submit(); return; }
    if (key.value !== undefined) this.acceptValue(key.value.toUpperCase());
  }

  private acceptValue(value: string): void {
    const answer = this.page.answer.toUpperCase();
    const expected = answer[this.stateValue.buffer.length];
    if (value === expected) {
      this.stateValue.buffer += value;
    } else {
      this.stateValue.buffer = value === answer[0] ? value : "";
      this.stateValue.lastResult = this.page.on_failure;
    }

    if (!this.page.submit_required && this.stateValue.buffer === answer)
      this.complete();
  }

  private submit(): void {
    if (this.stateValue.buffer === this.page.answer.toUpperCase()) {
      this.complete();
      return;
    }
    if (this.page.on_failure === "clear_input")
      this.stateValue.buffer = "";
    this.stateValue.lastResult = this.page.on_failure;
    this.adapter.record({ channel: "harness", name: "content_result", data: { result: this.page.on_failure } });
  }

  private complete(): void {
    this.stateValue.completed = true;
    this.stateValue.active = false;
    this.stateValue.lastResult = this.page.on_success;
    // on_success remains terminal-content data. The current game runtime publishes
    // terminal_complete; it does not yet publish a separate terminal_action signal.
    this.adapter.record({ channel: "harness", name: "content_result", data: { result: this.page.on_success } });
    this.adapter.record({ channel: "semantic", name: "terminal_complete", data: { source: this.document.id } });
  }
}
