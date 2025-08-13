const vertex_shader_code = `
attribute vec4 position;

void main()
{
    gl_Position = position;
}
`;

const fragment_shader_code = `
precision mediump float;

void main()
{
    gl_FragColor = vec4(1, 0, 0.5, 1);
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

document.addEventListener('DOMContentLoaded', () => {
    WebAssembly.instantiateStreaming(fetch('chess.wasm'), {}).then(w => {
        console.log(w);
        window.wasm = w.instance;
    });

    const canvas = document.getElementById('chess');

    const resize_observer = new ResizeObserver(on_resize);
    try {
        resize_observer.observe(canvas, {box: 'device-pixel-content-box'});
    } catch (error) {
        resize_observer.observe(canvas, {box: 'content-box'});
    }

    const gl = canvas.getContext('webgl');

    if (gl) {
        const vertex_shader = compile_shader(gl, gl.VERTEX_SHADER, vertex_shader_code);
        const fragment_shader = compile_shader(gl, gl.FRAGMENT_SHADER, fragment_shader_code);

        if (vertex_shader && fragment_shader) {
            const program = gl.createProgram();
            gl.attachShader(program, vertex_shader);
            gl.attachShader(program, fragment_shader);
            gl.linkProgram(program);
            if (gl.getProgramParameter(program, gl.LINK_STATUS)) {
                const position_attrib_loc = gl.getAttribLocation(program, "position");
                const position_buffer = gl.createBuffer();
                gl.bindBuffer(gl.ARRAY_BUFFER, position_buffer);
                const positions = [
                    0, 0,
                    0, 0.5,
                    0.7, 0,
                ];
                gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);
                requestAnimationFrame(draw);

                function draw(now) {
                    resize_canvas_to_display_size(gl.canvas);
                    gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
                    gl.clearColor(0.086, 0.125, 0.153, 1);
                    gl.clear(gl.COLOR_BUFFER_BIT);
                    gl.useProgram(program);
                    gl.enableVertexAttribArray(position_attrib_loc);
                    gl.bindBuffer(gl.ARRAY_BUFFER, position_buffer);
                    gl.vertexAttribPointer(position_attrib_loc, 2, gl.FLOAT, false, 0, 0);
                    gl.drawArrays(gl.TRIANGLES, 0, 3);

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
});
