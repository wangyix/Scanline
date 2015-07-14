// texture.vert

uniform mat4 modelMat;

varying vec3 modelPos;
varying vec2 texPos;

varying vec3 modelPosWorld;

void main()
{
    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(gl_Vertex.xyz, 1.0);

    modelPos = (gl_ModelViewMatrix * vec4(gl_Vertex.xyz, 1.0)).xyz;
    texPos = gl_MultiTexCoord0.xy;

    modelPosWorld = (modelMat * vec4(gl_Vertex.xyz, 1.0)).xyz;
}
