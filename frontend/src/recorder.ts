const TARGET_RATE = 16000;

function downsample(buf: Float32Array, inRate: number, outRate: number): Float32Array {
  if (outRate >= inRate) return buf;
  const ratio = inRate / outRate;
  const out = new Float32Array(Math.floor(buf.length / ratio));
  let o = 0;
  let i = 0;
  while (o < out.length) {
    const next = Math.floor((o + 1) * ratio);
    let sum = 0;
    let count = 0;
    for (; i < next && i < buf.length; i++) {
      sum += buf[i];
      count++;
    }
    out[o++] = count ? sum / count : 0;
  }
  return out;
}

export class MicRecorder {
  private ctx: AudioContext | null = null;
  private stream: MediaStream | null = null;
  private node: ScriptProcessorNode | null = null;
  private source: MediaStreamAudioSourceNode | null = null;
  private chunks: Float32Array[] = [];

  static supported(): boolean {
    return !!(navigator.mediaDevices && navigator.mediaDevices.getUserMedia);
  }

  async start(): Promise<void> {
    this.stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    this.ctx = new AudioContext();
    this.source = this.ctx.createMediaStreamSource(this.stream);
    this.node = this.ctx.createScriptProcessor(4096, 1, 1);
    this.chunks = [];
    this.node.onaudioprocess = (e) => {
      this.chunks.push(new Float32Array(e.inputBuffer.getChannelData(0)));
    };
    this.source.connect(this.node);
    this.node.connect(this.ctx.destination);
  }

  async stop(): Promise<{ pcm: ArrayBuffer; rate: number }> {
    const inRate = this.ctx?.sampleRate ?? 48000;
    this.node?.disconnect();
    this.source?.disconnect();
    this.stream?.getTracks().forEach((t) => t.stop());
    await this.ctx?.close();

    let len = 0;
    for (const c of this.chunks) len += c.length;
    const merged = new Float32Array(len);
    let off = 0;
    for (const c of this.chunks) {
      merged.set(c, off);
      off += c.length;
    }

    const ds = downsample(merged, inRate, TARGET_RATE);
    const i16 = new Int16Array(ds.length);
    for (let i = 0; i < ds.length; i++) {
      const s = Math.max(-1, Math.min(1, ds[i]));
      i16[i] = s < 0 ? s * 0x8000 : s * 0x7fff;
    }
    this.chunks = [];
    return { pcm: i16.buffer, rate: TARGET_RATE };
  }
}
