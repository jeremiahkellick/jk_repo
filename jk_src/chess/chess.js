const vertex_shader_code = `
uniform vec2 resolution;

attribute vec2 position;
attribute vec2 a_texcoord;

varying vec2 texcoord;

void main()
{
    texcoord = a_texcoord;
    gl_Position = vec4((2.0 * (position / resolution) - 1.0) * vec2(1, -1), 0, 1);
}
`;

const fragment_shader_code = `
precision mediump float;

uniform sampler2D texture;

varying vec2 texcoord;

void main()
{
    gl_FragColor = texture2D(texture, texcoord);
}
`;

function compile_shader(gl, type, source) {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        return shader;
    } else {
        console.log(gl.getShaderInfoLog(shader));
        gl.deleteShader(shader);
        return undefined;
    }
}

let display_width = 640;
let display_height = 480;

// from https://webglfundamentals.org/webgl/lessons/webgl-resizing-the-canvas.html
function resize_canvas_to_display_size(canvas) {
    // Check if the canvas is not the same size
    const need_resize = canvas.width != display_width || canvas.height != display_height;

    if (need_resize) {
        // Make the canvas the same size
        canvas.width  = display_width;
        canvas.height = display_height;
    }

    return need_resize;
}

function on_resize(entries) {
    for (const entry of entries) {
        let width;
        let height;
        let dpr = window.devicePixelRatio;
        if (entry.devicePixelContentBoxSize) {
            // NOTE: Only this path gives the correct answer
            // The other paths are imperfect fallbacks
            // for browsers that don't provide anyway to do this
            width = entry.devicePixelContentBoxSize[0].inlineSize;
            height = entry.devicePixelContentBoxSize[0].blockSize;
            dpr = 1; // it's already in width and height
        } else if (entry.contentBoxSize) {
            if (entry.contentBoxSize[0]) {
                width = entry.contentBoxSize[0].inlineSize;
                height = entry.contentBoxSize[0].blockSize;
            } else {
                width = entry.contentBoxSize.inlineSize;
                height = entry.contentBoxSize.blockSize;
            }
        } else {
            width = entry.contentRect.width;
            height = entry.contentRect.height;
        }
        display_width = Math.round(width * dpr);
        display_height = Math.round(height * dpr);
    }
}

WebAssembly.instantiateStreaming(fetch('/build/chess.wasm'), {}).then(w => {
    window.wasm = w.instance;

    const draw_buffer_offset = wasm.exports.init();

    if (draw_buffer_offset) {
        const canvas = document.getElementById('chess');

        const resize_observer = new ResizeObserver(on_resize);
        try {
            resize_observer.observe(canvas, {box: 'device-pixel-content-box'});
        } catch (error) {
            resize_observer.observe(canvas, {box: 'content-box'});
        }

        const gl = canvas.getContext('webgl2');

        if (gl) {
            const vertex_shader = compile_shader(gl, gl.VERTEX_SHADER, vertex_shader_code);
            const fragment_shader = compile_shader(gl, gl.FRAGMENT_SHADER, fragment_shader_code);

            if (vertex_shader && fragment_shader) {
                const program = gl.createProgram();
                gl.attachShader(program, vertex_shader);
                gl.attachShader(program, fragment_shader);
                gl.linkProgram(program);
                if (gl.getProgramParameter(program, gl.LINK_STATUS)) {
                    gl.useProgram(program);

                    gl.clearColor(0.086, 0.125, 0.153, 1);

                    const position_buffer = gl.createBuffer();
                    const position_attrib_loc = gl.getAttribLocation(program, "position");
                    gl.enableVertexAttribArray(position_attrib_loc);

                    const texcoord_buffer = gl.createBuffer();
                    const texcoord_attrib_loc = gl.getAttribLocation(program, "a_texcoord");
                    gl.enableVertexAttribArray(texcoord_attrib_loc);

                    const texture = gl.createTexture();
                    gl.bindTexture(gl.TEXTURE_2D, texture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
                    const draw_buf = new Uint8Array(
                            wasm.exports.memory.buffer, draw_buffer_offset, 67108864);
                    gl.texImage2D(
                            gl.TEXTURE_2D, 0,
                            gl.RGBA, 4096, 4096, 0, gl.RGBA,
                            gl.UNSIGNED_BYTE, draw_buf);

                    const resolution_uniform_loc = gl.getUniformLocation(program, "resolution");

                    requestAnimationFrame(draw);

                    function draw(now) {
                        resize_canvas_to_display_size(gl.canvas);
                        gl.uniform2f(resolution_uniform_loc, gl.canvas.width, gl.canvas.height);
                        gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);

                        const square_side_length = Math.max(8,
                                Math.floor(Math.min(gl.canvas.width, gl.canvas.height, 4096) / 10));

                        wasm.exports.tick(square_side_length);

                        gl.pixelStorei(gl.UNPACK_ROW_LENGTH, 4096);
                        gl.texSubImage2D(
                                gl.TEXTURE_2D, 0,
                                0, 0, square_side_length * 10, square_side_length * 10,
                                gl.RGBA, gl.UNSIGNED_BYTE, draw_buf);
                        gl.pixelStorei(gl.UNPACK_ROW_LENGTH, 0);

                        gl.clear(gl.COLOR_BUFFER_BIT);

                        const width = Math.min(gl.canvas.width, square_side_length * 10);
                        const height = Math.min(gl.canvas.height, square_side_length * 10);
                        let x = 0;
                        let y = 0;
                        if (gl.canvas.width < gl.canvas.height) {
                            y = Math.floor((gl.canvas.height - height) / 2); 
                        } else {
                            x = Math.floor((gl.canvas.width - width) / 2);
                        }

                        const positions = new Float32Array([
                            x, y,
                            x, y + height,
                            x + width, y,
                            x + width, y + height,
                        ]);
                        gl.bindBuffer(gl.ARRAY_BUFFER, position_buffer);
                        gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);
                        gl.vertexAttribPointer(position_attrib_loc, 2, gl.FLOAT, false, 0, 0);

                        const percent = (square_side_length * 10) / 4096;
                        const texcoords = new Float32Array([
                            0, 0,
                            0, percent,
                            percent, 0,
                            percent, percent,
                        ]);
                        gl.bindBuffer(gl.ARRAY_BUFFER, texcoord_buffer);
                        gl.bufferData(gl.ARRAY_BUFFER, texcoords, gl.STATIC_DRAW);
                        gl.vertexAttribPointer(texcoord_attrib_loc, 2, gl.FLOAT, false, 0, 0);

                        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

                        requestAnimationFrame(draw);
                    }
                } else {
                    console.log(gl.getProgramInfoLog(program));
                    gl.deleteProgram(program);
                }
            }
        } else {
            console.log('WebGL not supported');
        }
    }
});
