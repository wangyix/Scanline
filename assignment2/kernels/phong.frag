// phong.frag

/*
  This fragment implements the Phong Reflection model.
*/

// The input image we will be filtering in this kernel.
uniform sampler2D normalTex;
uniform sampler2D colorTex;
uniform samplerCube cubeMap;
uniform sampler2D reflTex;
uniform sampler2D depthTex;     // for shadowmapping
uniform float depthTexDu;
uniform float depthTexDv;
uniform sampler2D spotTex;      // for shadowmapping

uniform vec3 eyePosWorld;

// enables/disables features
uniform float normalMapping;
uniform float colorMapping;
uniform float cubeMapping;  // if false, then refl map is used (when rendering water)
uniform float clipping;     // if true, everything above clipZ is clipped (when rendering mirrored scene)

uniform float clipZ;

uniform mat4 viewMat;               // for reflTex sampling
uniform mat4 lightViewProjMat;      // viewProj matrix of the shadow-casting light

// point lights
#define NUM_POINTLIGHTS 34
uniform vec3 pointLightPos[NUM_POINTLIGHTS];


varying vec3 modelPos;  // position in world space
varying vec3 normal;    // normal in world space
varying vec2 texPos;

varying vec4 glPos;


vec3 pointLight(in vec3 lightSourcePos, in vec3 N, 
        in vec3 materialDiffuse, in vec3 materialSpecular, in float shininess) {
    
    // no ambient

    vec3 lightDiffuse  = vec3(1.0, 0.7, 0.0);
    vec3 lightSpecular = vec3(1.0, 0.7, 0.0);

    vec3 Lm = normalize(lightSourcePos-modelPos);
	vec3 Rm = normalize(reflect(-Lm,N));
	vec3 V = normalize(eyePosWorld - modelPos);

    float distFactor = max((1.0 - length(lightSourcePos-modelPos) / 50.0), 0.0);

	vec3 colorDiffuse = clamp(max(dot(Lm,N),0.0)*materialDiffuse*lightDiffuse*distFactor,0.0,1.0);
    vec3 colorSpecular = clamp(pow(max(dot(Rm,V),0.0),shininess)*materialSpecular*lightSpecular*distFactor,0.0,1.0);

	return (colorDiffuse + colorSpecular);
}


#define PI 3.1415926535898


void main()
{
    if (clipping > 0.0f && modelPos.z < clipZ) {
        discard;
    }

    // Sample from the normal map, if we're not doing displacement mapping
    vec3 N_old = normalize(normal);
    vec3 N = N_old;

	if (normalMapping > 0.0) {

        // solve for tangent vector

        vec3 dPdx = dFdx(modelPos);
        vec2 duvdx = dFdx(texPos);

        vec3 dPdy = dFdy(modelPos);
        vec2 duvdy = dFdy(texPos);

        // define matrix A to have rows dPdx, dPdy, N
        // we know A*T = [dudx; dudy; 0] and A*B = [dvdx; dvdy; 0]

        // don't worry about dividing by det_A, we have to normalize T later anyway
        //float det_A = length(cross(dPdu_x, dPdu_y));
        vec3 A_inv_col1 = cross(dPdy, N);
        vec3 A_inv_col2 = cross(N, dPdx);
        vec3 T = (duvdx.x * A_inv_col1 + duvdy.x * A_inv_col2);
        vec3 B = (duvdx.y * A_inv_col1 + duvdy.y * A_inv_col2);
        
        T = normalize(T);
        B = normalize(B);

        // perturb normal
        vec3 N_tbn = (2.*texture2D(normalTex, texPos).xyz - 1.);
        vec3 Nnew = normalize(N_tbn.x * T + N_tbn.y * B + N_tbn.z * N);

        N = Nnew;//normalize((N + Nnew) * 0.5);

        /*mat3 TBN = cotangent_frame(N, modelPos, texPos);
        vec3 N_tbn = (2.*texture2D(normalTex, texPos).xyz - 1.);
        N = normalize(TBN * N_tbn);*/
    }


    // material

	vec3 materialAmbient;
	vec3 materialDiffuse;
	vec3 materialSpecular;
	if(colorMapping < 0.0){
		materialAmbient = gl_FrontMaterial.ambient.xyz;
		materialDiffuse = gl_FrontMaterial.diffuse.xyz;
		materialSpecular  = gl_FrontMaterial.specular.xyz;
	}
	else{
		materialAmbient = gl_FrontMaterial.ambient.xyz*texture2D(colorTex, texPos).xyz;
		materialDiffuse = gl_FrontMaterial.diffuse.xyz*texture2D(colorTex, texPos).xyz;
		materialSpecular  = gl_FrontMaterial.specular.xyz*texture2D(colorTex, texPos).xyz;
	}
    float shininess    = gl_FrontMaterial.shininess;




    vec3 color = (0.0, 0.0, 0.0);


    // specular color due to environment map (or refl tex, in case of water)

    if (cubeMapping > 0.0) {
        vec3 eyeToPosDir = normalize(modelPos - eyePosWorld);
        vec3 reflDir = reflect(eyeToPosDir, N);
        vec3 cubeMapTexcoord = vec3(reflDir.x, -reflDir.z, -reflDir.y);
        vec3 envColor = texture(cubeMap, cubeMapTexcoord).xyz;

        color += materialSpecular * envColor;

    } else {
        // calculate component of perturbed normal that's orthogonal to unperturbed normal
        vec3 flatN = N - dot(N, N_old)*N_old;
        vec2 flatNView = viewMat * vec4(flatN, 0.0);
        vec2 reflOffset = normalize(flatNView.xy) * length(flatN) * 0.025;

        // sample reflTex with offset
        vec2 screenTexcoord = 0.5 * (glPos.xy / glPos.w + 1.0);
        vec3 reflColor = texture2D(reflTex, screenTexcoord + reflOffset).xyz;
        
        color += materialSpecular * reflColor;
    }
    

    // lights

    // directional light 0
    {
        vec3 lightDirection = gl_LightSource[0].spotDirection;
        vec3 lightAmbient  = gl_LightSource[0].ambient.xyz;
        vec3 lightDiffuse  = gl_LightSource[0].diffuse.xyz;
        vec3 lightSpecular = gl_LightSource[0].specular.xyz;

	    vec3 Lm = -normalize(lightDirection);
	    vec3 Rm = normalize(reflect(-Lm,N));
	    vec3 V = normalize(eyePosWorld - modelPos);

	    vec3 colorAmbient = materialAmbient*lightAmbient;
	    vec3 colorDiffuse = clamp(max(dot(Lm,N),0.0)*materialDiffuse*lightDiffuse,0.0,1.0);
        vec3 colorSpecular = clamp(pow(max(dot(Rm,V),0.0),shininess)*materialSpecular*lightSpecular,0.0,1.0);

        color += (colorAmbient + colorDiffuse + colorSpecular);
    }

    // directional light 1
    {
        vec3 lightDirection = gl_LightSource[1].spotDirection;
        vec3 lightAmbient  = gl_LightSource[1].ambient.xyz;
        vec3 lightDiffuse  = gl_LightSource[1].diffuse.xyz;
        vec3 lightSpecular = gl_LightSource[1].specular.xyz;

	    vec3 Lm = -normalize(lightDirection);
	    vec3 Rm = normalize(reflect(-Lm,N));
	    vec3 V = normalize(eyePosWorld - modelPos);

	    vec3 colorAmbient = materialAmbient*lightAmbient;
	    vec3 colorDiffuse = clamp(max(dot(Lm,N),0.0)*materialDiffuse*lightDiffuse,0.0,1.0);
        vec3 colorSpecular = clamp(pow(max(dot(Rm,V),0.0),shininess)*materialSpecular*lightSpecular,0.0,1.0);

        color += (colorAmbient + colorDiffuse + colorSpecular);
    }

    // directional light 2
    {
        vec3 lightDirection = gl_LightSource[2].spotDirection;
        vec3 lightAmbient  = gl_LightSource[2].ambient.xyz;
        vec3 lightDiffuse  = gl_LightSource[2].diffuse.xyz;
        vec3 lightSpecular = gl_LightSource[2].specular.xyz;

	    vec3 Lm = -normalize(lightDirection);
	    vec3 Rm = normalize(reflect(-Lm,N));
	    vec3 V = normalize(eyePosWorld - modelPos);

	    vec3 colorAmbient = materialAmbient*lightAmbient;
	    vec3 colorDiffuse = clamp(max(dot(Lm,N),0.0)*materialDiffuse*lightDiffuse,0.0,1.0);
        vec3 colorSpecular = clamp(pow(max(dot(Rm,V),0.0),shininess)*materialSpecular*lightSpecular,0.0,1.0);

        color += (colorAmbient + colorDiffuse + colorSpecular);
    }

    // spotlight 3
    {
        vec3 lightSourcePos = gl_LightSource[3].position.xyz;
        vec3 lightDirection = gl_LightSource[3].spotDirection;
        //float spotCutoff = gl_LightSource[3].spotCutoff;

        vec3 lightAmbient  = gl_LightSource[3].ambient.xyz;
        vec3 lightDiffuse  = gl_LightSource[3].diffuse.xyz;
        vec3 lightSpecular = gl_LightSource[3].specular.xyz;

        vec3 Lm = normalize(lightSourcePos-modelPos);
	    vec3 Rm = normalize(reflect(-Lm,N));
        vec3 toEye = eyePosWorld - modelPos;
	    vec3 V = normalize(toEye);


        // calculate spotcolor using depthTex and spotTex

        vec4 lightProjcoord = lightViewProjMat * vec4(modelPos, 1.0);
        vec3 lightProjcoordNormalized = lightProjcoord.xyz / lightProjcoord.w;
        
        vec2 depthTexcoord = (lightProjcoordNormalized.xy + 1.0) * 0.5;
        float modelDepth = (lightProjcoordNormalized.z + 1.0) * 0.5;    // convert from [-1,1] to [0,1]

        // PCF
        float occludeFactor = 0.0;
        const float bias = 0.00001;
        
        for (int i = -4; i <= 4; i++) {
            for (int j = -4; j <= 4; j++) {
                vec2 texcoordOffset = vec2(i*depthTexDu, j*depthTexDv);
                float occluderDepth = texture2D(depthTex, depthTexcoord + texcoordOffset).x;     // is in [0,1]
                occludeFactor += (modelDepth <= occluderDepth + bias ? 1.0 : 0.0);
            }
        }
        occludeFactor /= 81.0f;
        
        vec3 spotColor = occludeFactor * texture2D(spotTex, depthTexcoord).xyz;


        // distance attenuation
        float distFactor = max((1.0 - length(lightSourcePos-modelPos) / 5000.0), 0.0);
        spotColor *= distFactor;

	    vec3 colorAmbient = materialAmbient*lightAmbient;
	    vec3 colorDiffuse = clamp(max(dot(Lm,N),0.0)*materialDiffuse*lightDiffuse*spotColor,0.0,1.0);
        vec3 colorSpecular = clamp(pow(max(dot(Rm,V),0.0),shininess)*materialSpecular*lightSpecular*spotColor,0.0,1.0);

	    color += (colorAmbient + colorDiffuse + colorSpecular);
    }


    // pointlights
    
    for (int i = 0; i < NUM_POINTLIGHTS; i++) {
        color += pointLight(pointLightPos[i], N, materialDiffuse, materialSpecular, shininess);
    }



    // apply fog
    const vec3 fogColor = vec3(1/255.0, 27/255.0, 34/255.0);
    const float fogStartDist = 0.0;
    const float fogRange = 750.0;
    
    float dist = length(modelPos - eyePosWorld);
    float fogFactor = clamp((dist-fogStartDist) / fogRange, 0.0, 1.0);
    color = (1.0 - fogFactor)*color + fogFactor*fogColor;
    

    /*const vec3 fogColor = vec3(0.5, 0.5, 0.5);
    const float fogLayerTop = 200.0;
    const float fogFullDist = 2000.0;

    // get starting and ending Z of ray to object
    float eyeZ = eyePosWorld.z;
    float pixZ = modelPos.z;
    float pathTopZ = max(eyeZ, pixZ);
    float pathBottomZ = min(eyeZ, pixZ);

    // calculate the portion of that path spent in the fog layer
    float pathFogPortion =  clamp((fogLayerTop - pathBottomZ) / (pathTopZ - pathBottomZ), 0.0, 1.0);
    float distInFog = pathFogPortion * length(modelPos - eyePosWorld);


    // apply fog to pixel color
    float fogFactor = min(distInFog / fogFullDist, 1.0);
    color = (1.0 - fogFactor)*color + fogFactor*fogColor;
    */

	color = clamp(color, 0.0, 1.0);

    gl_FragColor = vec4(color, 1.0);
}
