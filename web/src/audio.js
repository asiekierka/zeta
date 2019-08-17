/*!
 * Copyright (c) 2018, 2019 Adrian Siekierka
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
		this.volume = (options && options.volume) || 0.2;
	}

	on(freq) {
		if (audioCtx == undefined)
			return;

		let cTime = audioCtx.currentTime;
		if (cTime != this.lastCurrTime) {
			this.lastCurrTime = cTime;
			this.lastTimeMs = time_ms();
		}

		let lastADelay = (time_ms() - this.lastTimeMs) / 1000.0;

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

		this.timeSpeakerOn = time_ms();
	}

	off() {
		if (this.pc_speaker == undefined)
			return;

		let cTime = audioCtx.currentTime;
		if (cTime != this.lastCurrTime) {
			this.lastCurrTime = cTime;
			this.lastTimeMs = time_ms();
		}

		let lastADelay = (time_ms() - this.lastTimeMs) / 1000.0;
		// console.log("pc speaker off " + (audioCtx.currentTime + lastADelay));
		this.pc_speaker.frequency.setValueAtTime(0, audioCtx.currentTime + lastADelay);
	}
}

export class BufferBasedAudio {
	constructor(emu, options) {
		this.emu = emu;
		this.sampleRate = (options && options.sampleRate) || 48000;
		this.bufferSize = (options && options.bufferSize) || 2048;
		this.volume = (options && options.volume) || 0.2;
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
		source.buffer = buffer; // Firefox makes buffer immutable here!
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

		this._queueBufferSource(() => {});
		this._queueNextSpeakerBuffer();
	}

	on(freq) {
		if (audioCtx == undefined) return;
		this._initSpeaker();
		this.emu._audio_stream_append_on(time_ms(), freq);
	}

	off() {
		if (audioCtx == undefined) return;
		this.emu._audio_stream_append_off(time_ms());
	}
}

/*	ZetaAudio.createScriptProcessorBased = function(emu) {
		var pc_speaker = undefined;
		var emu = emu;

		var init_speaker = function() {
			pc_speaker = audioCtx.createScriptProcessor();

			var bufferSize = pc_speaker.bufferSize;
			var buffer = emu._malloc(bufferSize);
			var heap = new Uint8Array(emu.HEAPU8.buffer, buffer, bufferSize);

			emu._audio_stream_init(time_ms(), Math.floor(audioCtx.sampleRate));
			emu._audio_stream_set_volume(Math.floor(0.2 * emu._audio_stream_get_max_volume()));

			pc_speaker.onaudioprocess = function(event) {
				var out = event.outputBuffer.getChannelData(0);
				emu._audio_stream_generate_u8(time_ms() - (bufferSize * 1000 / audioCtx.sampleRate), buffer, bufferSize);
				for (var i = 0; i < bufferSize; i++) {
					out[i] = (heap[i] - 127) / 127.0;
				}
				console.log("audio " + bufferSize);
			};

			pc_speaker.connect(audioCtx.destination);
		};

		return {
			on: function(freq) {
				if (audioCtx == undefined) return;
				if (pc_speaker == undefined) init_speaker();
				emu._audio_stream_append_on(time_ms(), freq);
			},
			off: function() {
				if (pc_speaker == undefined) return;
				emu._audio_stream_append_off(time_ms());
			}
		};
	}; */
