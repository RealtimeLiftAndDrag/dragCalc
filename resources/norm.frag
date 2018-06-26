#version 330 core
in vec3 fragPos;
in vec3 fragNor;
in vec2 fragTex;
in vec3 lightPos;

out vec4 color;

void main() {
	vec3 normal = 0.5*(normalize(fragNor) +1 );
    

    color = vec4(normal, 1);
}
