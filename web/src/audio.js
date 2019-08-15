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
	constructor() {
		this.lastCurrTime = 0;
		this.lastTimeMs = 0;
		this.timeSpeakerOn = 0;
		this.audioGain = undefined;
		this.pc_speaker = undefined;
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
			this.audioGain.gain.setValueAtTime(0.2, audioCtx.currentTime);
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
		this.pc_speaker = undefined;
		this.sampleRate = (options && options.sampleRate) || 48000;
		this.bufferSize = (options && options.bufferSize) || 4096;
		this.volume = (options && options.volume) || 0.2;
	}

	_initSpeaker() {
		this.pc_speaker = audioCtx.createBufferSource();
		let buffer = audioCtx.createBuffer(1, this.bufferSize * 2, this.sampleRate);
		this.pc_speaker.buffer = buffer;
		this.pc_speaker.connect(audioCtx.destination);

		let nativeBuffer = this.emu._malloc(this.bufferSize);
		let nativeHeap = new Uint8Array(this.emu.HEAPU8.buffer, nativeBuffer, this.bufferSize);

		this.emu._audio_stream_init(time_ms(), this.sampleRate);
		this.emu._audio_stream_set_volume(Math.floor(this.volume * this.emu._audio_stream_get_max_volume()));
		let startedAt = 0;
		let startedAtMs = 0;
		let lastTime = 0;

		let write = function(offset) {
			let out = buffer.getChannelData(0);
			let sm = Math.min(time_ms(), ((audioCtx.currentTime - startedAt) * 1000) + startedAtMs);
			this.emu._audio_stream_generate_u8(sm, nativeBuffer, this.bufferSize);
			for (let i = 0; i < this.bufferSize; i++) {
				out[offset + i] = (nativeHeap[i] - 127) / 127.0;
			}
		}

		let tick = function() {
			let ltPos = Math.floor( (lastTime / (this.bufferSize / this.sampleRate)) % 2 );
			let ctPos = Math.floor( (audioCtx.currentTime / (this.bufferSize / this.sampleRate)) % 2 );
			if (ltPos != ctPos) {
				write(ltPos * this.bufferSize);
			}
			lastTime = audioCtx.currentTime;
		};

		this.pc_speaker.loop = true;
		startedAt = audioCtx.currentTime;
		startedAtMs = time_ms();
		this.pc_speaker.start();
		lastTime = startedAt - 0.001;

		tick();
		setInterval(tick, (this.bufferSize / this.sampleRate) * (1000.0 / 2));
	}

	on(freq) {
		if (audioCtx == undefined) return;
		if (this.pc_speaker == undefined) this._initSpeaker();
		this.emu._audio_stream_append_on(time_ms(), freq);
	}

	off() {
		if (this.pc_speaker == undefined) return;
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
