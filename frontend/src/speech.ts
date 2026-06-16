// Minimal wrapper around the browser Web Speech API (SpeechRecognition).
// Works in Chrome / Edge. Provides a simple start/stop microphone dictation
// that streams interim and final transcripts back to the caller.
//
// In a later milestone this can be swapped for real audio streaming to the
// bridge (board mic -> USB -> STT), but for the web UI this gives an instant,
// working mic button.

/* eslint-disable @typescript-eslint/no-explicit-any */

type SpeechHandlers = {
  onResult: (transcript: string, isFinal: boolean) => void;
  onEnd?: () => void;
  onError?: (message: string) => void;
};

export function isSpeechSupported(): boolean {
  return (
    typeof window !== "undefined" &&
    ((window as any).SpeechRecognition || (window as any).webkitSpeechRecognition)
  );
}

export class Microphone {
  private recognition: any = null;
  private listening = false;

  constructor(private handlers: SpeechHandlers) {
    const Ctor =
      (window as any).SpeechRecognition ||
      (window as any).webkitSpeechRecognition;
    if (!Ctor) return;
    const rec = new Ctor();
    rec.continuous = false;
    rec.interimResults = true;
    rec.lang = "en-US";

    rec.onresult = (event: any) => {
      let transcript = "";
      let isFinal = false;
      for (let i = event.resultIndex; i < event.results.length; i++) {
        transcript += event.results[i][0].transcript;
        if (event.results[i].isFinal) isFinal = true;
      }
      this.handlers.onResult(transcript, isFinal);
    };
    rec.onerror = (e: any) =>
      this.handlers.onError?.(e.error ?? "speech-error");
    rec.onend = () => {
      this.listening = false;
      this.handlers.onEnd?.();
    };
    this.recognition = rec;
  }

  get isListening() {
    return this.listening;
  }

  start() {
    if (!this.recognition || this.listening) return;
    this.listening = true;
    this.recognition.start();
  }

  stop() {
    if (!this.recognition || !this.listening) return;
    this.recognition.stop();
  }
}
