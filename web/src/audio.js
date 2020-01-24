/*!
 * Copyright (c) 2018, 2019, 2020 Adrian Siekierka
 *
 * This file is part of Zeta.
 *
 * Zeta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zeta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zeta.  If not, see <http://www.gnu.org/licenses/>.
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
		this.volume = Math.min(1.0, Math.max(0.0, (options && options.volume) || 0.2));
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
		this.volume = Math.min(1.0, Math.max(0.0, (options && options.volume) || 0.2));
		this.timeUnit = (this.bufferSize / this.sampleRate);
		this.nativeBuffer = this.emu._malloc(this.bufferSize);
		this.nativeHeap = new Uint8Array(this.emu.HEAPU8.buffer, this.nativeBuffer, this.bufferSize);
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
			const out0 = buffer.getChannelData(channel);

			self.emu._audio_stream_generate_u8(time_ms(), nativeBuffer, self.bufferSize);
			for (let i = 0; i < bufferSize; i++) {
				out0[i] = (nativeHeap[i] - 127) / 127.0;
			}
			for (var channel = 1; channel < buffer.numberOfChannels; channel++) {
				buffer.getChannelData(channel).set(out0);
			}
		})
	}

	_initSpeaker() {
		if (this.initialized) return;
		this.initialized = true;

		this.time = audioCtx.currentTime;

		this.emu._audio_stream_init(time_ms(), this.sampleRate);
		this.emu._audio_stream_set_volume(Math.floor(this.volume * this.emu._audio_stream_get_max_volume()));

		if (this.noteDelay) {
			this.emu._audio_set_note_delay(this.noteDelay);
		}

		this._queueBufferSource(() => {});
		this._queueNextSpeakerBuffer();
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
		if (audioCtx == undefined) return;
		this._initSpeaker();
		this.emu._audio_stream_append_on(time, cycles, freq);
	}

	off(time, cycles) {
		if (audioCtx == undefined) return;
		this.emu._audio_stream_append_off(time, cycles);
	}
}
