// shadow.vert

void main()
{
    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(gl_Vertex.xyz, 1.0);
}
