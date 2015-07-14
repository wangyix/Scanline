// texture.frag

uniform sampler2D colorTex;

uniform float clipping;
uniform float clipZ;

uniform float alphaScale;


varying vec3 modelPos;  // position in viewspace
varying vec2 texPos;

varying vec3 modelPosWorld;


void main()
{
    if (clipping > 0.0f && modelPosWorld.z < clipZ) {
        discard;
    }


    vec4 colorAlpha = texture2D(colorTex, texPos);

    // apply fog
    const vec3 fogColor = vec3(1/255.0, 27/255.0, 34/255.0);
    const float fogStartDist = 0.0;
    const float fogRange = 750.0;
    
    float dist = length(modelPos);
    float fogFactor = clamp((dist-fogStartDist) / fogRange, 0.0, 1.0);
    
    vec3 foggedColor = (1.0 - fogFactor)*colorAlpha.xyz + fogFactor*fogColor;

    
    gl_FragColor = vec4(foggedColor, colorAlpha.w * alphaScale);
}
