#version 330 core

/* Fullscreen triangle — no VBO needed.
   gl_VertexID 0,1,2 generates a triangle that covers the entire viewport. */
out vec2 v_uv;

void main() {
    /* Positions in clip space that produce a fullscreen triangle */
    vec2 pos = vec2(
        float((gl_VertexID & 1) << 2) - 1.0,
        float((gl_VertexID & 2) << 1) - 1.0
    );
    gl_Position = vec4(pos, 0.0, 1.0);
    v_uv = pos * 0.5 + 0.5;
}
