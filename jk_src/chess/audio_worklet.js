class ChessAudioWorklet extends AudioWorkletProcessor {
    constructor() {
        super();
        this.wasm_exports = null;
        this.audio_buffer = null;
        this.port.onmessage = async (event) => {
            const imports = {
                env: {
                    console_log: (size, data) => {
                        // 'AudioWorkletProcessor's don't have access to TextDecoder, so to print
                        // messages from wasm, we'd have to use do postMessage or something.
                        // However, since I don't currently (and haven't ever?) print anything from
                        // the audio function, I'll just let this be an empty stub.
                    }
                }
            };
            this.wasm_exports =
                (await WebAssembly.instantiate(event.data, imports)).instance.exports;
            if (this.wasm_exports) {
                const audio_buffer_offset = this.wasm_exports.init_audio();
                if (audio_buffer_offset) {
                    this.audio_buffer = new Int16Array(
                            this.wasm_exports.memory.buffer, audio_buffer_offset, 38400 / 2);
                } else {
                    console.log('Audio thread failed to initialize wasm module');
                }
            } else {
                console.log('Audio thread failed to initialize wasm module');
            }
        }
    }

    static get parameterDescriptors() {
        return [
            {name: 'sound', automationRate: 'k-rate'},
            {name: 'started_time_0', automationRate: 'k-rate'},
            {name: 'started_time_1', automationRate: 'k-rate'},
        ];
    }

    process(inputs, outputs, params) {
        let channels = null;
        if (outputs.length && outputs[0].length == 2) {
            channels = outputs[0];
        }
        if (this.wasm_exports && channels) {
            const sample_count = Math.min(channels[0].length, channels[1].length);
            this.wasm_exports.fill_audio_buffer(
                params.sound[0],
                params.started_time_0[0],
                params.started_time_1[0],
                currentTime * 48000,
                sample_count,
            );
            for (let i = 0; i < sample_count; i++) {
                channels[0][i] = this.audio_buffer[2 * i] / 32768.0;
                channels[1][i] = this.audio_buffer[2 * i + 1] / 32768.0;
            }
        } else {
            // Fill with silence
            for (const output of outputs) {
                for (const channel of output) {
                    for (let i = 0; i < channel.length; i++) {
                        channel[i] = 0;
                    }
                }
            }
        }
        return true;
    }
}

registerProcessor("worklet", ChessAudioWorklet);
