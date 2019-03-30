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

ZetaAudio = {};

(function() {
	var audioCtx = undefined;

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

	ZetaAudio.createOscillatorBased = function() {
		var lastCurrTime = 0;
		var lastTimeMs = 0;
		var timeSpeakerOn = 0;
		var audioGain = undefined;
		var pc_speaker = undefined;

		return {
			on: function(freq) {
				if (audioCtx == undefined)
					return;

				var cTime = audioCtx.currentTime;
				if (cTime != lastCurrTime) {
					lastCurrTime = cTime;
					lastTimeMs = time_ms();
				}

				var lastADelay = (time_ms() - lastTimeMs) / 1000.0;

			//	console.log("pc speaker " + freq + " " + (audioCtx.currentTime + lastADelay));
				if (pc_speaker == undefined) {
					audioGain = audioCtx.createGain();
					pc_speaker = audioCtx.createOscillator();
					pc_speaker.type = 'square';
					pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
					pc_speaker.connect(audioGain);
					audioGain.connect(audioCtx.destination);
					audioGain.gain.setValueAtTime(0.2, audioCtx.currentTime);
					pc_speaker.start(0);
				} else {
					pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
				}

				timeSpeakerOn = time_ms();
			},

			off: function() {
				if (pc_speaker == undefined)
					return;

				var cTime = audioCtx.currentTime;
				if (cTime != lastCurrTime) {
					lastCurrTime = cTime;
					lastTimeMs = time_ms();
				}

				var lastADelay = (time_ms() - lastTimeMs) / 1000.0;
			//	console.log("pc speaker off " + (audioCtx.currentTime + lastADelay));
				pc_speaker.frequency.setValueAtTime(0, audioCtx.currentTime + lastADelay);
			}
		};
	};

	ZetaAudio.createBufferBased = function(emu, options) {
		var pc_speaker = undefined;
		var emu = emu;
		var sampleRate = (options && options.sampleRate) || 48000;
		var bufferSize = (options && options.bufferSize) || 4096;

		var init_speaker = function() {
			pc_speaker = audioCtx.createBufferSource();
			var buffer = audioCtx.createBuffer(1, bufferSize * 2, sampleRate);
			pc_speaker.buffer = buffer;
			pc_speaker.connect(audioCtx.destination);

			var nativeBuffer = emu._malloc(bufferSize);
			var nativeHeap = new Uint8Array(emu.HEAPU8.buffer, nativeBuffer, bufferSize);

			emu._audio_stream_init(time_ms(), sampleRate);
			emu._audio_stream_set_volume(Math.floor(0.2 * emu._audio_stream_get_max_volume()));
			var startedAt = 0;
			var startedAtMs = 0;
			var lastTime = 0;

			var write = function(offset) {
				var out = buffer.getChannelData(0);
				var sm = Math.min(time_ms(), ((audioCtx.currentTime - startedAt) * 1000) + startedAtMs);
				emu._audio_stream_generate_u8(sm, nativeBuffer, bufferSize);
				for (var i = 0; i < bufferSize; i++) {
					out[offset + i] = (nativeHeap[i] - 127) / 127.0;
				}
			}

			var tick = function() {
				var ltPos = Math.floor( (lastTime / (bufferSize / sampleRate)) % 2 );
				var ctPos = Math.floor( (audioCtx.currentTime / (bufferSize / sampleRate)) % 2 );
				if (ltPos != ctPos) {
					write(ltPos * bufferSize);
				}
				lastTime = audioCtx.currentTime;
			};

			pc_speaker.loop = true;
			startedAt = audioCtx.currentTime;
			startedAtMs = time_ms();
			pc_speaker.start();
			lastTime = startedAt - 0.001;

			tick();
			setInterval(tick, (bufferSize / sampleRate) * (1000.0 / 2));
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
	};

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

})();
