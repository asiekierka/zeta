/**
 * Copyright (c) 2018, 2019, 2020, 2021 Adrian Siekierka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { time_ms } from "./util.js";

let audioCtx = undefined;

document.addEventListener('mousedown', function(event) {
	if (audioCtx == undefined) {
		audioCtx = new (window.AudioContext || window.webkitAudioContext) ();
	}
});

document.addEventListener('keydown', function(event) {
	if (audioCtx == undefined) {
		audioCtx = new (window.AudioContext || window.webkitAudioContext) ();
	}
});

export class OscillatorBasedAudio {
	constructor(options) {
		this.lastCurrTime = 0;
		this.lastTimeMs = 0;
		this.timeSpeakerOn = 0;
		this.audioGain = undefined;
		this.pc_speaker = undefined;
		this.volume = Math.min(1.0, Math.max(0.0, (options && options.volume) || 0.1));
		this.noteDelay = 1;
	}

	on(time, cycles, freq) {
		if (audioCtx == undefined)
			return;

		let cTime = audioCtx.currentTime;
		if (cTime != this.lastCurrTime) {
			this.lastCurrTime = cTime;
			this.lastTimeMs = time;
		}

		let lastADelay = (time - this.lastTimeMs) / 1000.0;

		// console.log("pc speaker " + freq + " " + (audioCtx.currentTime + lastADelay));
		if (this.pc_speaker == undefined) {
			this.audioGain = audioCtx.createGain();
			this.pc_speaker = audioCtx.createOscillator();
			this.pc_speaker.type = 'square';
			this.pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
			this.pc_speaker.connect(this.audioGain);
			this.audioGain.connect(audioCtx.destination);
			this.audioGain.gain.setValueAtTime(this.volume, audioCtx.currentTime);
			this.pc_speaker.start(0);
		} else {
			this.pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
		}

		this.timeSpeakerOn = time;
	}

	off(time, cycles) {
		if (this.pc_speaker == undefined)
			return;

		let cTime = audioCtx.currentTime;
		if (cTime != this.lastCurrTime) {
			this.lastCurrTime = cTime;
			this.lastTimeMs = time;
		}

		let lastADelay = (time - this.lastTimeMs) / 1000.0;
		// console.log("pc speaker off " + (audioCtx.currentTime + lastADelay));
		this.pc_speaker.frequency.setValueAtTime(0, audioCtx.currentTime + lastADelay);
	}

	setNoteDelay(delay) {
		this.noteDelay = delay;
	}

	setVolume(volume) {
		this.volume = Math.min(1.0, Math.max(0.0, volume));

		if (this.pc_speaker != undefined) {
			this.audioGain.gain.setValueAtTime(this.volume, audioCtx.currentTime);
		}
	}
}

export class BufferBasedAudio {
	constructor(emu, options) {
		this.emu = emu;
		this.sampleRate = (options && options.sampleRate) || 48000;
		this.bufferSize = (options && options.bufferSize) || 2048;
		this.noteDelay = (options && options.noteDelay) || undefined;
		this.volume = Math.min(1.0, Math.max(0.0, (options && options.volume) || 0.1));
		this.timeUnit = (this.bufferSize / this.sampleRate);
		this.initialized = false;
		this.lastTimeMs = 0;
	}

	_queueBufferSource(populateFunc) {
		this.time += this.timeUnit;
		if (this.time < audioCtx.currentTime) this.time = audioCtx.currentTime;

		const source = audioCtx.createBufferSource();
		const buffer = audioCtx.createBuffer(1, this.bufferSize, this.sampleRate);
		if (populateFunc) populateFunc(buffer, source);
		source.buffer = buffer; // Firefox makes buffer immutable here! :^)
		source.onended = () => this._queueNextSpeakerBuffer();
		source.connect(audioCtx.destination);
		source.start(this.time);
	}

	_queueNextSpeakerBuffer() {
		const self = this;

		this._queueBufferSource((buffer, source) => {
			const bufferSize = self.bufferSize;
			const nativeBuffer = self.nativeBuffer;
			const nativeHeap = self.nativeHeap;
			const out0 = buffer.getChannelData(0);

			self.emu._audio_stream_generate(time_ms(), nativeBuffer, self.bufferSize * 2);
			for (let i = 0; i < bufferSize; i++) {
				out0[i] = (nativeHeap[i] - 32768) / 32768.0;
			}
			for (var channel = 1; channel < buffer.numberOfChannels; channel++) {
				buffer.getChannelData(channel).set(out0);
			}
		})
	}

	_initSpeaker() {
		if (this.initialized) return true;
		if (audioCtx == undefined) return false;
		this.initialized = true;

		this.time = audioCtx.currentTime;

		this.emu._audio_generate_init();
		this.emu._audio_stream_init(time_ms(), this.sampleRate, false, true);
		this.emu._audio_stream_set_volume(Math.floor(this.volume * this.emu._audio_stream_get_max_volume()));

		if (this.noteDelay) {
			this.emu._audio_set_note_delay(this.noteDelay);
		}

		this.nativeBuffer = this.emu._malloc(this.bufferSize * 2);
		this.nativeHeap = new Uint16Array(this.emu.HEAPU8.buffer, this.nativeBuffer, this.bufferSize);

		// If only one audio buffer is queued, it ending will cause the
		// *beginning* of queueing the next buffer - creating a short,
		// audible stutter every (this.timeUnit) seconds. If we instead
		// queue two audio buffers, one after another, this will create
		// a delay of length = (this.timeUnit) seconds.
		this._queueBufferSource(() => {});
		this._queueNextSpeakerBuffer();
		return true;
	}

	setNoteDelay(delay) {
		this.noteDelay = delay;
		if (this.initialized) {
			this.emu._audio_set_note_delay(delay);
		}
	}

	setVolume(volume) {
		this.volume = Math.min(1.0, Math.max(0.0, volume));
		if (this.initialized) {
			this.emu._audio_stream_set_volume(Math.floor(this.volume * this.emu._audio_stream_get_max_volume()));
		}
	}

	on(time, cycles, freq) {
		if (!this._initSpeaker()) return;
		this.emu._audio_stream_append_on(time, cycles, freq);
	}

	off(time, cycles) {
		if (!this._initSpeaker()) return;
		this.emu._audio_stream_append_off(time, cycles);
	}
}
