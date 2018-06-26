#version 430 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 fragPos;
out vec3 fragNor;


void main() {
    gl_Position = P * V * M * vec4(vertPos, 1.0);
    fragPos = vec4(V * M * vec4(vertPos, 1.0)).xyz;
    fragNor = vec4(V * M * vec4(vertNor, 0.0)).xyz;
}
