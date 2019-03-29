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

	// TODO: Rewrite to use a timeout + BufferSourceNode; ScriptProcessorNode is "abandoned"
	ZetaAudio.createStreamBased = function(emu) {
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
	};
})();
