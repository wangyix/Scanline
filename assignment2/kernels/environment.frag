// environment.frag

uniform vec3 eyePosWorld;
uniform samplerCube cubeMap;

// used for drawing axes
uniform float overrideWithColor;
uniform vec3 color;

varying vec3 modelPos;


#define PI 3.1415926535898

void main()
{
    if (overrideWithColor > 0.0f) {
        gl_FragColor = vec4(color, 1.0);
        return;
    }
    
    vec3 viewDir = normalize(modelPos - eyePosWorld);
    vec3 cubeMapTexcoord = vec3(viewDir.x, -viewDir.z, -viewDir.y);
    vec3 color = texture(cubeMap, cubeMapTexcoord).xyz;


    // fog

    const vec3 fogColor = vec3(1/255.0, 27/255.0, 34/255.0);
    
    float phi = acos(viewDir.z);

    float fogPhiStart = 70.0 / 180.0 * PI;
    float fogPhiRange = 20.0 / 180.0 * PI;

    float fogFactor = clamp((phi-fogPhiStart) / fogPhiRange, 0.0, 1.0);



    // apply fog to pixel color
    color = (1.0 - fogFactor)*color + fogFactor*fogColor;


    gl_FragColor = vec4(color, 1.0);
}
