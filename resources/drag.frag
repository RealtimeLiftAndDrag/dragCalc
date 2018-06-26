#version 430 core
layout (std430, binding=2) volatile buffer shader_data
{ 
  ivec4 sumF;
  ivec4 torque;
  uvec4 counter;
};
uniform vec3 center;

in vec3 fragPos;
in vec3 fragNor;


out vec4 color;

void atomicAddTorque(vec3 t, int mult){
	t*= mult;
	atomicAdd(torque.x, int(t.x));
	atomicAdd(torque.y, int(t.y));
	atomicAdd(torque.z, int(t.z));

}
void atomicAddSumF(vec3 f, int mult){
	f*=mult;
	atomicAdd(sumF.x, int(f.x));
	atomicAdd(sumF.y, int(f.y));
	atomicAdd(sumF.z, int(f.z));
}
void main() {
	
	int sigfigMult = 100;
	vec3 f = -fragNor * (dot(vec3(0, 0, -1), fragNor));

	vec3 r = fragPos - center;
	vec3 t = cross(f, r);
	atomicAddTorque(t, sigfigMult);
	atomicAddSumF(f, sigfigMult);

	counter.x += 1;
	atomicAdd(counter.y, 1);
	color = vec4(0.5*(normalize(fragNor) +1 ), 1);

}
