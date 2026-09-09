export type AdapterEventChannel = "semantic" | "harness";

export interface AdapterEvent {
  channel: AdapterEventChannel;
  name: string;
  data?: Record<string, unknown>;
}

export interface TerminalRuntimeAdapter {
  record(event: AdapterEvent): void;
}

export class MockQ2RaidAdapter implements TerminalRuntimeAdapter {
  readonly events: AdapterEvent[] = [];
  onChange: (() => void) | null = null;

  record(event: AdapterEvent): void {
    this.events.push(event);
    this.onChange?.();
  }

  clear(): void {
    this.events.length = 0;
    this.onChange?.();
  }
}
