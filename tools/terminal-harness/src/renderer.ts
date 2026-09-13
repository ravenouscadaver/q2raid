import type { TerminalKeyDefinition, TerminalManifest } from "./types.js";
import type { TerminalRuntime } from "./runtime.js";
import { createCustomTerminalFontAdapter, drawFallbackText, type TerminalFontAdapter, type TerminalTextStyle } from "./font.js";

interface LayerImages {
  chassis?: HTMLImageElement;
  screen_fx?: HTMLImageElement;
  controls?: HTMLImageElement;
}

export class TerminalRenderer {
  private readonly ctx: CanvasRenderingContext2D;
  private readonly customFont: TerminalFontAdapter | null = createCustomTerminalFontAdapter();
  private images: LayerImages = {};
  private pressedKey: string | null = null;
  cursorX = 487.5;
  cursorY = 512;

  constructor(readonly canvas: HTMLCanvasElement, readonly manifest: TerminalManifest) {
    const ctx = canvas.getContext("2d");
    if (!ctx) throw new Error("2D canvas unavailable");
    this.ctx = ctx;
    canvas.width = manifest.canvas[0];
    canvas.height = manifest.canvas[1];
  }

  async loadDecorativeLayers(basePath: string): Promise<Record<string, boolean>> {
    const status: Record<string, boolean> = {};
    for (const name of ["chassis", "screen_fx", "controls"] as const) {
      const filename = this.manifest.layers[name];
      const image = await this.tryLoadImage(`${basePath}/${filename}`);
      status[filename] = image !== undefined;
      if (image) this.images[name] = image;
    }
    return status;
  }

  setPressedKey(id: string | null): void { this.pressedKey = id; }

  hitTest(x: number, y: number): TerminalKeyDefinition | null {
    for (const key of this.manifest.keyboard_map.keys) {
      const [kx, ky, kw, kh] = key.rect_px;
      if (x >= kx && x <= kx + kw && y >= ky && y <= ky + kh) return key;
    }
    return null;
  }

  draw(runtime: TerminalRuntime): void {
    const ctx = this.ctx;
    const [width, height] = this.manifest.canvas;
    ctx.clearRect(0, 0, width, height);
    this.drawFallbackChassis();
    for (const name of ["chassis", "screen_fx", "controls"] as const) {
      const image = this.images[name];
      if (image) ctx.drawImage(image, 0, 0, width, height);
    }

    this.drawScreen(runtime);
    this.drawKeyboard(runtime);
    this.drawCursor();
  }

  private drawFallbackChassis(): void {
    const ctx = this.ctx;
    const [w, h] = this.manifest.canvas;
    ctx.fillStyle = "rgb(18 20 18)";
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = "rgb(4 14 8)";
    ctx.fillRect(143, 128, 693, 446);
    ctx.fillStyle = "rgb(28 31 27)";
    ctx.fillRect(72, 680, 834, 240);
  }

  private drawScreen(runtime: TerminalRuntime): void {
    const page = runtime.page;
    this.text(page.prompt, 487.5, 205, { size: 24, color: "#72ff94", align: "center" });
    const answerLength = page.answer.length;
    const cells: string[] = [];
    for (let i = 0; i < answerLength; ++i) cells.push(runtime.state.buffer[i] ?? "_");
    this.text(cells.join("  "), 487.5, 260, { size: 32, color: "#d2ffdc", align: "center" });

    const stateText = runtime.state.completed ? "COMPLETE" : runtime.state.active ? "INPUT ACTIVE" : "SESSION CLOSED";
    this.text(stateText, 487.5, 315, { size: 16, color: runtime.state.completed ? "#88ff9f" : "#9ab29f", align: "center" });
  }

  private drawKeyboard(runtime: TerminalRuntime): void {
    const ctx = this.ctx;
    const [r, g, b, a] = this.manifest.keyboard_map.pressed_overlay_rgba;
    for (const key of this.manifest.keyboard_map.keys) {
      const [x, y, w, h] = key.rect_px;
      const enabled = runtime.isKeyEnabled(key);
      if (this.pressedKey === key.id) {
        ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
        ctx.fillRect(x, y, w, h);
      } else if (!enabled) {
        ctx.fillStyle = "rgba(0,0,0,0.52)";
        ctx.fillRect(x, y, w, h);
      }

      const fontSize = key.label.length > 4 ? 8 : key.label.length > 2 ? 10 : 13;
      this.text(key.label, x + 3, y + 3 + (this.pressedKey === key.id ? 2 : 0), {
        size: fontSize,
        color: enabled ? "#b7e6c0" : "#566257"
      });
    }
  }

  private drawCursor(): void {
    const ctx = this.ctx;
    ctx.save();
    ctx.strokeStyle = "#ffdc60";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(this.cursorX - 9, this.cursorY);
    ctx.lineTo(this.cursorX + 9, this.cursorY);
    ctx.moveTo(this.cursorX, this.cursorY - 9);
    ctx.lineTo(this.cursorX, this.cursorY + 9);
    ctx.stroke();
    ctx.restore();
  }

  private text(text: string, x: number, y: number, style: TerminalTextStyle): void {
    if (this.customFont) this.customFont.drawText(this.ctx, text, x, y, style);
    else drawFallbackText(this.ctx, text, x, y, style);
  }

  private tryLoadImage(url: string): Promise<HTMLImageElement | undefined> {
    return new Promise(resolve => {
      const image = new Image();
      image.onload = () => resolve(image);
      image.onerror = () => resolve(undefined);
      image.src = url;
    });
  }
}
