// environment.vert

uniform mat4 modelMat;

varying vec3 modelPos;

void main()
{
    modelPos = (modelMat * vec4(gl_Vertex.xyz, 1.0)).xyz;
    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(gl_Vertex.xyz, 1.0);
}
