#version 330

// shadow.frag

layout(location=0) out float depth;

void main()
{
    //gl_FragColor = vec4(0, 0, 0, 0);
    depth = gl_FragCoord.z;
}
