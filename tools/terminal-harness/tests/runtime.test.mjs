import test from "node:test";
import assert from "node:assert/strict";
import { MockQ2RaidAdapter } from "../dist/adapter.js";
import { TerminalRuntime, validateTerminalDocument } from "../dist/runtime.js";

const doc = validateTerminalDocument({
  schema_version: 1,
  id: "entrance_terminal",
  presentation: "terminal_grunge",
  start_page: "boot",
  resume_policy: "reset_on_close",
  pages: {
    boot: {
      type: "packet_reconstruction",
      prompt: "ACCESS TOKEN CORRUPTED",
      answer: "ALPHA",
      available: ["P", "A", "H", "L"],
      shuffle: false,
      wrong_input: "restart_from_matching_prefix",
      submit_required: true,
      on_success: "entrance_unlock",
      on_failure: "clear_input"
    }
  }
});

function key(id, role, value) {
  return { id, label: id, role, row: "test", rect_px: [0, 0, 1, 1], ...(value === undefined ? {} : { value }) };
}

const A = key("a", "character", "A");
const L = key("l", "character", "L");
const P = key("p", "character", "P");
const H = key("h", "character", "H");
const ENTER = key("enter", "submit");
const BACK = key("backspace", "back");
const Z = key("z", "character", "Z");

function makeRuntime() {
  const adapter = new MockQ2RaidAdapter();
  const runtime = new TerminalRuntime(doc, adapter);
  runtime.open();
  return { runtime, adapter };
}

test("ALPHA requires submit and publishes the current terminal_complete semantic event", () => {
  const { runtime, adapter } = makeRuntime();
  for (const k of [A, L, P, H, A]) runtime.pressKey(k);
  assert.equal(runtime.state.completed, false);
  runtime.pressKey(ENTER);
  assert.equal(runtime.state.completed, true);
  assert.equal(runtime.state.lastResult, "entrance_unlock");
  assert.equal(adapter.events.at(-1)?.name, "terminal_complete");
  assert.equal(adapter.events.at(-1)?.channel, "semantic");
});

test("restart_from_matching_prefix mirrors the current onboarding prefix behavior", () => {
  const { runtime } = makeRuntime();
  runtime.pressKey(A);
  runtime.pressKey(L);
  runtime.pressKey(A);
  assert.equal(runtime.state.buffer, "A");
  runtime.pressKey(P);
  assert.equal(runtime.state.buffer, "");
});

test("unavailable character keys are ignored", () => {
  const { runtime } = makeRuntime();
  runtime.pressKey(Z);
  assert.equal(runtime.state.buffer, "");
});

test("backspace removes one accepted character", () => {
  const { runtime } = makeRuntime();
  runtime.pressKey(A);
  runtime.pressKey(L);
  runtime.pressKey(BACK);
  assert.equal(runtime.state.buffer, "A");
});

test("unsupported terminal page types fail loudly", () => {
  assert.throws(() => validateTerminalDocument({ ...doc, pages: { boot: { ...doc.pages.boot, type: "imaginary_game" } } }), /unsupported type/);
});
