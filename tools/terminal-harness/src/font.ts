export interface TerminalTextStyle {
  size: number;
  color: string;
  align?: CanvasTextAlign;
  baseline?: CanvasTextBaseline;
}

export interface TerminalFontAdapter {
  drawText(ctx: CanvasRenderingContext2D, text: string, x: number, y: number, style: TerminalTextStyle): void;
}

// Stub boundary for a future mapped Quake bitmap-font adapter. Returning null is
// deliberate: authoring remains usable before the exact native font atlas and
// glyph geometry are supplied.
export function createCustomTerminalFontAdapter(): TerminalFontAdapter | null {
  return null;
}

export function drawFallbackText(
  ctx: CanvasRenderingContext2D,
  text: string,
  x: number,
  y: number,
  style: TerminalTextStyle
): void {
  ctx.save();
  ctx.font = `${style.size}px ui-monospace, SFMono-Regular, Consolas, monospace`;
  ctx.fillStyle = style.color;
  ctx.textAlign = style.align ?? "left";
  ctx.textBaseline = style.baseline ?? "top";
  ctx.fillText(text, x, y);
  ctx.restore();
}
