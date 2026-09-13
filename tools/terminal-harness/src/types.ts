export type KeyRole = "character" | "digit" | "back" | "submit" | "cancel" | "navigation" | "action";

export interface TerminalKeyDefinition {
  id: string;
  label: string;
  role: KeyRole;
  row: string;
  value?: string;
  rect_px: [number, number, number, number];
}

export interface TerminalManifest {
  canvas: [number, number];
  layers: {
    engine_backing: string;
    chassis: string;
    screen_fx: string;
    controls: string;
  };
  regions: {
    screen: Array<[number, number]>;
    keyboard: Array<[number, number]>;
    keypad: Array<[number, number]>;
  };
  keyboard_map: {
    geometry_space: "canvas_pixels";
    mask_encoding: Record<string, string>;
    pressed_overlay_rgba: [number, number, number, number];
    keys: TerminalKeyDefinition[];
  };
}

export interface PacketReconstructionPage {
  type: "packet_reconstruction";
  prompt: string;
  answer: string;
  available: string[];
  shuffle: boolean;
  wrong_input: "restart_from_matching_prefix";
  submit_required: boolean;
  on_success: string;
  on_failure: "clear_input";
}

export interface TerminalDocument {
  schema_version: 1;
  id: string;
  presentation: string;
  camera?: string;
  start_page: string;
  resume_policy: "reset_on_close";
  pages: Record<string, PacketReconstructionPage>;
}

export interface TerminalState {
  active: boolean;
  completed: boolean;
  pageId: string;
  buffer: string;
  lastResult: string | null;
}
