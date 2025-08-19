let initialized = false;
let wasm_exports = null;
let ai_request_bytes = null;
let ai_response_offset = null;
let next_request_id = 0;

function yield_to_event_loop() {
    return new Promise(resolve => setTimeout(resolve, 0));
}

onmessage = async e => {
    if (e.data.type == 'wasm') {
        this.wasm_exports = (await WebAssembly.instantiate(e.data.buffer)).instance.exports;
        if (this.wasm_exports) {
            if (this.wasm_exports.ai_alloc_memory()) {
                ai_request_bytes = new Uint8Array(
                        this.wasm_exports.memory.buffer, this.wasm_exports.get_ai_request(), 56);
                ai_response_offset = this.wasm_exports.get_ai_response_ai_thread();
                initialized = true;
            } else {
                console.log('Failed to allocate memory for AI');
            }
        } else {
            console.log('Failed to instantiate AI thread wasm');
        }
    } else if (e.data.type == 'ai_request') {
        if (initialized) {
            const request_id = next_request_id++;
            const source_bytes = new Uint8Array(e.data.buffer, 0, 56);
            for (let i = 0; i < 56; i++) {
                ai_request_bytes[i] = source_bytes[i];
            }

            if (this.wasm_exports.ai_begin_request(performance.now())) {
                let cancelled = false;
                while (!cancelled && this.wasm_exports.ai_tick(performance.now())) {
                    await yield_to_event_loop();
                    cancelled = request_id + 1 < next_request_id;
                }
                if (!cancelled) {
                    const ai_response_buffer = this.wasm_exports.memory.buffer.slice(
                            ai_response_offset, ai_response_offset + 64);
                    postMessage(ai_response_buffer, [ai_response_buffer]);
                }
            }
        }
    }
};
