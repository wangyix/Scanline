// main.cpp

#include "Camera.h"
//
// For this project, we use OpenGL, GLUT
// and GLEW (to load OpenGL extensions)
//

#include "stglew.h"

#include <stdio.h>
#include <string.h>
#include <fstream>

#include "Obj.h"

//
// Globals used by this application.
// As a rule, globals are Evil, but this is a small application
// and the design of GLUT makes it hard to avoid them.
//

// Window size, kept for screenshots
static int gWindowSizeX = 0;
static int gWindowSizeY = 0;

// File locations
std::vector<std::string> objFilePaths;

STShaderProgram *shader;

// mouse
int gPreviousMouseX = -1;
int gPreviousMouseY = -1;
int gMouseButton = -1;

// camera
Camera camera;
glm::vec3 defaultCameraPos;
glm::vec3 defaultCameraLook;

bool smooth = true; // smooth/flat shading for mesh
bool axes = false;

// object manipulation
glm::vec3 axisOfRotation;        // 0=X, 1=Y, 2=Z
glm::vec3 axisOfTranslation;     // 0=X, 1=Y, 2=Z

// objects
std::vector<Obj> objs;
int selectedObj;        // index of currently-selected obj


// shadowmap stuff
#define SHADOWMAP_TEX_WIDTH 512
#define SHADOWMAP_TEX_HEIGHT 512

GLuint shadowFbo;
GLuint depthTex;
GLuint spotTex;

STShaderProgram *shadowMapShader;


// cubemap stuff
GLuint cubeMap;
STShaderProgram *environmentShader;

// reflection stuff
#define REFTEX_SIZE 512

GLuint reflectionFbo;
GLuint reflectionTex;
GLuint reflectionDepthTex;


// billboard stuff
STShaderProgram *textureShader;
GLuint orbBillboardTex;
GLuint beamBillboardTex;

struct Dirlight {
    float az;
    float el;
    float scale;
    glm::vec3 color;        // scale*color is used for diffuse

    Dirlight() : az(0.0f), el(0.0f), scale(1.0f), color() {}
    
    bool readLightsFile(int id) {
        std::string filename = "light";
        filename.append(std::to_string(static_cast<long long>(id))).append(".txt");
        std::ifstream in(filename.c_str(), std::ios::in);
        if(!in){
            std::cout << "no light file" << filename << std::endl;
            return false;
        }
        std::cout << "light file" << filename << "found" << std::endl;
        in >> az >> el >> scale;
        in.close();
        return true;
    }
    bool writeLightsFile(int id) {
        std::string filename = "light";
        filename.append(std::to_string(static_cast<long long>(id))).append(".txt");
        std::ofstream out(filename.c_str(), std::ios::out);
        if (!out) {
            std::cout << "cannot open file" << filename << std::endl;
            return false;
        }
        out << az << " " << el << " " << scale << std::endl;
        out.close();
        return true;
    }
    glm::vec3 getDir() {
        glm::vec3 negXdir(-1.0f, 0.0f, 0.0f);
        return glm::rotateZ(glm::rotateY(negXdir, -el), az);
    }
};

// lights
const int NUMLIGHTS = 4 + 34;
struct Dirlight lights[NUMLIGHTS];
int selectedLight;
int selectedLightAttrib;


// worldpos of spotlight (light 3)
const glm::vec3 spotLightPosition(-28.2751, 253.996, 67.9616);


// right mouse controlling?
bool rightMouseControllLights;


// these must be set with GL_MODELVIEW set to identity
void setDirectionalLightAttribs() {
    // set directional and spot light attribs (GL_LIGHTx)
    for (int i=0; i<4; i++) {
        glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, glm::value_ptr(lights[i].scale * lights[i].color));
        glLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION, glm::value_ptr(lights[i].getDir()));
    }
    // set spotlight (light3) position
    {
        glm::vec4 position = glm::vec4(spotLightPosition, 1.0f);
        glLightfv(GL_LIGHT3, GL_POSITION, glm::value_ptr(position));
    }
}

void setPointLightAttribs(GLint pointLightPosUniformLocation) {
    // set point light attribs (uniform vec3 pointLightPos[] in frag shader)
    float pointLightPositions[(NUMLIGHTS - 4)*3];
    int length = 0;
    for (int i=4; i<NUMLIGHTS; i++) {
        pointLightPositions[length++] = lights[i].az;
        pointLightPositions[length++] = lights[i].el;
        pointLightPositions[length++] = lights[i].scale;
    }
    glUniform3fv(pointLightPosUniformLocation, (NUMLIGHTS - 4)*3, pointLightPositions);
}


void resetCamera(){
    camera.setPosition(defaultCameraPos);
    camera.setLook(defaultCameraLook);
}

bool readSceneFile(const std::string& filename) {
    std::ifstream in(filename.c_str(), std::ios::in);
    if(!in){
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        printf("%s\n", line.c_str());
        objFilePaths.push_back(line);
    }
    return true;
}

bool readCameraFile(const std::string& filename) {
    std::ifstream in(filename.c_str(), std::ios::in);
    if(!in){
        return false;
    }
    in >> defaultCameraPos.x >> defaultCameraPos.y >> defaultCameraPos.z;
    in >> defaultCameraLook.x >> defaultCameraLook.y >> defaultCameraLook.z;
    return true;
}

bool writeCameraFile(const std::string& filename) {
    std::ofstream out(filename.c_str(), std::ios::out);
    if(!out){
        return false;
    }
    glm::vec3 pos = camera.getPosition();
    glm::vec3 look = camera.getLook();
    out << pos.x << ' ' <<  pos.y << ' ' << pos.z << std::endl;
    out << look.x << ' ' << look.y << ' ' << look.z << std::endl;
    return true;
}


//
// Initialize the application, loading all of the settings that
// we will be accessing later in our fragment shaders.
//
void Setup() {
    shader = new STShaderProgram();
    shader->LoadVertexShader("kernels/default.vert");
    shader->LoadFragmentShader("kernels/phong.frag");

    shadowMapShader = new STShaderProgram();
    shadowMapShader->LoadVertexShader("kernels/shadow.vert");
    shadowMapShader->LoadFragmentShader("kernels/shadow.frag");

    environmentShader = new STShaderProgram();
    environmentShader->LoadVertexShader("kernels/environment.vert");
    environmentShader->LoadFragmentShader("kernels/environment.frag");

    textureShader = new STShaderProgram();
    textureShader->LoadVertexShader("kernels/texture.vert");
    textureShader->LoadFragmentShader("kernels/texture.frag");

    
    // read camera file
    if (!readCameraFile("camera.txt")) {
        defaultCameraPos = glm::vec3(15.0f, 0.0f, 0.0f);
        defaultCameraLook = glm::vec3(-1.0f, 0.0f, 0.0f);
    }
    resetCamera();
    camera.setLens(0.1f, 10000.0f, 45.0f);



    glClearColor(0.f, 15.0f / 255.0f, 66.0f / 255.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);


    glEnable(GL_LIGHTING);

    // read lights files
    for (int i=0; i<NUMLIGHTS; i++) {
        lights[i] = struct Dirlight();
        lights[i].readLightsFile(i);
    }
    
    selectedLight = 0;
    selectedLightAttrib = 0;


    // set non-manipulable attribs of the lights

    // set all necessary light attributes except GL_DIFFUSE and GL_SPOT_DIRECTION
    // (all hardcoded)

    // direct light 0
    {
        static float ambientLight[]  = {0.10, 0.10, 0.10, 1.0};
        lights[0].color = glm::vec3(1.0f, 1.0f, 1.0f);
        static float specularLight[] = {1.00, 1.00, 1.00, 1.0};
        
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_SPECULAR,  specularLight);
        glLightfv(GL_LIGHT0, GL_AMBIENT,   ambientLight);
        //glLightfv(GL_LIGHT0, GL_DIFFUSE,   diffuseLight);
    }
    // direct light 1
    {
        static float ambientLight[]  = {0.0, 0.0, 0.0, 1.0};
        lights[1].color = glm::vec3(1.0f, 1.0f, 1.0f);
        static float specularLight[] = {0.00, 0.00, 0.00, 1.0};
        
        glEnable(GL_LIGHT1);
        glLightfv(GL_LIGHT1, GL_SPECULAR,  specularLight);
        glLightfv(GL_LIGHT1, GL_AMBIENT,   ambientLight);
        //glLightfv(GL_LIGHT1, GL_DIFFUSE,   diffuseLight);
    }
    // direct light 2
    {
        static float ambientLight[]  = {0.0, 0.0, 0.0, 1.0};
        lights[2].color = glm::vec3(1.0f, 1.0f, 1.0f);
        static float specularLight[] = {0.00, 0.00, 0.00, 1.0};
        
        glEnable(GL_LIGHT2);
        glLightfv(GL_LIGHT2, GL_SPECULAR,  specularLight);
        glLightfv(GL_LIGHT2, GL_AMBIENT,   ambientLight);
        //glLightfv(GL_LIGHT2, GL_DIFFUSE,   diffuseLight);
    }
    // spotlight 3
    {
        static float ambientLight[]  = {0.00, 0.00, 0.00, 1.0};
        lights[3].color = glm::vec3(1.0f, 1.0f, 1.0f);
        static float specularLight[] = {1.00, 1.00, 1.00, 1.0};
        
        //static float spotCutoff = 5.0f;

        glEnable(GL_LIGHT3);
        glLightfv(GL_LIGHT3, GL_SPECULAR,  specularLight);
        glLightfv(GL_LIGHT3, GL_AMBIENT,   ambientLight);
        //glLightfv(GL_LIGHT3, GL_DIFFUSE,   diffuseLight);

        //glLightfv(GL_LIGHT3, GL_SPOT_CUTOFF, &spotCutoff);
    }

    // setup shadow stuff
    {
        // setup depthTex and fbo
        glGenFramebuffers(1, &shadowFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);

        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, SHADOWMAP_TEX_WIDTH, SHADOWMAP_TEX_HEIGHT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        float borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};     // full shadow outside depthtex edges
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex, 0);
    
        glDrawBuffer(GL_NONE);  // no color buffers are rendered to

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("shadowmap framebuffer setup failed\n");
            exit(1);
        }

        // setup spotlight texture
        STImage spotImg("textures/spot.png");
        int width = spotImg.GetWidth();
        int height = spotImg.GetHeight();

        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &spotTex);
        glBindTexture(GL_TEXTURE_2D, spotTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, width, height);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, spotImg.GetPixels());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }



    // set up cube map
    {
        glEnable(GL_TEXTURE_CUBE_MAP);
        glGenTextures(1, &cubeMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        STImage Xneg("cubemap/Xneg.png");
        STImage Xpos("cubemap/Xpos.png");
        STImage Yneg("cubemap/Yneg.png");
        STImage Ypos("cubemap/Ypos.png");
        STImage Zneg("cubemap/Zneg.png");
        STImage Zpos("cubemap/Zpos.png");
        int width = Xneg.GetWidth();
        int height = Xneg.GetHeight();

        /*glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, Xpos.GetPixels());
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, Xneg.GetPixels());
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, Yneg.GetPixels());
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, Ypos.GetPixels());
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, Zpos.GetPixels());
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, Zneg.GetPixels());*/
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA, width, height);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, Xpos.GetPixels());
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, Xneg.GetPixels());
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, Yneg.GetPixels());
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, Ypos.GetPixels());
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, Zpos.GetPixels());
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, Zneg.GetPixels());
    }

    // set up billboard textures
    {
        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &orbBillboardTex);
        glBindTexture(GL_TEXTURE_2D, orbBillboardTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        STImage orbImg("textures/orb.png");
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, orbImg.GetWidth(), orbImg.GetHeight());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, orbImg.GetWidth(), orbImg.GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, orbImg.GetPixels());


        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &beamBillboardTex);
        glBindTexture(GL_TEXTURE_2D, beamBillboardTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        STImage beamImg("textures/beam.png");
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, beamImg.GetWidth(), beamImg.GetHeight());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, beamImg.GetWidth(), beamImg.GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, beamImg.GetPixels());
    }

    // set up reflection tex fbo
    {
        glGenFramebuffers(1, &reflectionFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, reflectionFbo);

        glEnable(GL_TEXTURE_2D);

        // set up texture to render reflected scene to
        glGenTextures(1, &reflectionTex);
        glBindTexture(GL_TEXTURE_2D, reflectionTex);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, REFTEX_SIZE, REFTEX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, REFTEX_SIZE, REFTEX_SIZE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach to fbo
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, reflectionTex, 0);
    
        // depth buffer
        glGenRenderbuffers(1, &reflectionDepthTex);
        glBindRenderbuffer(GL_RENDERBUFFER, reflectionDepthTex);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, REFTEX_SIZE, REFTEX_SIZE);
        // attach to fbo
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflectionDepthTex);

        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("reflection fbo setup failed\n");
            exit(1);
        }
    }

    // initial scene manipulation settings
    rightMouseControllLights = false;
    axisOfRotation = glm::vec3(0.0f, 0.0f, 1.0f);
    axisOfTranslation = glm::vec3(0.0f, 0.0f, 1.0f);

    // load meshes
    objs.resize(objFilePaths.size());
    for (size_t i=0; i < objFilePaths.size(); i++) {
        /*objs.push_back(Obj());
        objs.back().read(objFilePaths[i]);*/    // may cause array to expand and copy, which causes errors freeing objs
        objs[i].read(objFilePaths[i]);
    }
    selectedObj = 0;
}

void CleanUp()
{
}


void DrawScene(bool drawAxes, bool clipped, float clipZ, bool drawWater,
    const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& proj,
    const glm::mat4& lightViewProj);

void DrawObjsToDepthTex(const glm::mat4 view, const glm::mat4 proj);


//
// Display the output image from our vertex and fragment shaders
//
void DisplayCallback()
{
    Camera lightCam;
    lightCam.setLens(0.1f, 10000.0f, 30.0f);
    lightCam.setAspect(1.0f);
    lightCam.setPosition(spotLightPosition);
    lightCam.setLook(lights[3].getDir());


    // render scene objs to depth tex for shadowmap #####################################################################################################################################################
    
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glViewport(0, 0, SHADOWMAP_TEX_WIDTH, SHADOWMAP_TEX_HEIGHT);

    glClear(GL_DEPTH_BUFFER_BIT);

    DrawObjsToDepthTex(lightCam.getView(), lightCam.getProj());
    

    // render mirrored scene to reflection tex ##########################################################################################################################################################
    
    glBindFramebuffer(GL_FRAMEBUFFER, reflectionFbo);
    glViewport(0, 0, REFTEX_SIZE, REFTEX_SIZE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    
    // find height of water
    glm::vec4 origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    float waterZ = (objs[0].worldMat * origin).z;



    
    // calculate mirrored camera position
    glm::vec3 cameraPosMirrored = camera.getPosition();
    cameraPosMirrored.z = 2*waterZ - cameraPosMirrored.z;

    // calculate mirrored view matrix
    glm::mat4 reflectZ = glm::mat4( 1.0, 0.0, 0.0, 0.0,
                                    0.0, 1.0, 0.0, 0.0,
                                    0.0, 0.0, -1.0, 0.0,
                                    0.0, 0.0, 0.0, 1.0 );
    glm::vec3 translateZ(0.0f, 0.0f, waterZ);
    glm::mat4 reflectWaterZ = glm::translate(translateZ) * reflectZ * glm::translate(-translateZ);
    glm::mat4 viewMirrored = camera.getView() * reflectWaterZ;

    // lightviewproj does not need to be mirrored since the worldpos of the fragments is unaffected (since we're just
    // mirroring the view matrix).

    DrawScene(false, true, waterZ, false, cameraPosMirrored, viewMirrored, camera.getProj(), lightCam.getViewProj());
    


    // render scene and XYZ axes ########################################################################################################################################################################

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gWindowSizeX, gWindowSizeY);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    

    DrawScene(axes, false, 0.0f, true, camera.getPosition(), camera.getView(), camera.getProj(), lightCam.getViewProj());

    glutSwapBuffers();
    glutPostRedisplay();
}


void DrawObjsToDepthTex(const glm::mat4 view, const glm::mat4 proj) {
    // shadow map: render objs from spotlight perspective to depth texture

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glViewport(0, 0, SHADOWMAP_TEX_WIDTH, SHADOWMAP_TEX_HEIGHT);

    glClear(GL_DEPTH_BUFFER_BIT);

    shadowMapShader->Bind();

    glPolygonOffset(-10.0f, -100.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));

    // draw objs
    glMatrixMode(GL_MODELVIEW);

    for (size_t i=0; i < objs.size(); i++) {
        glm::mat4 modelView = view * objs[i].worldMat;
        glLoadMatrixf(glm::value_ptr(modelView));
        
        std::vector<STTriangleMesh*>& stMeshes = objs[i].stMeshes;
        for (int j=0; j < stMeshes.size(); j++) {
            stMeshes[j]->Draw(smooth);
        }
    }

    glPolygonOffset(0.0f, 0.0f);

    shadowMapShader->UnBind();
}



void DrawScene(bool drawAxes, bool clipped, float clipZ, bool drawWater,
        const glm::vec3& cameraPos, const glm::mat4& view, const glm::mat4& proj,
        const glm::mat4& lightViewProj) {

    // render environment and XYZ axes ===============================================================================================================
    {
        environmentShader->Bind();

        // draw environment using cubemap
        // must do this first, or else it will occlude objects drawn before this
        {
            environmentShader->SetTexture("cubeMap", 3);
            environmentShader->SetUniform("eyePosWorld", STColor3f(cameraPos.x, cameraPos.y, cameraPos.z));

            environmentShader->SetUniform("overrideWithColor", -1.0f);

            // bind cubemap
            glActiveTexture(GL_TEXTURE3);
            glEnable(GL_TEXTURE_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);


            // set matrices
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(glm::value_ptr(proj));

            glMatrixMode(GL_MODELVIEW);
            glm::mat4 modelMat = glm::translate(cameraPos);
            glLoadMatrixf(glm::value_ptr(view * modelMat));    // cube will be drawn around camera

            GLint modelMatLocation = environmentShader->GetUniformLocation("modelMat");
            glUniformMatrix4fv(modelMatLocation, 1, false, glm::value_ptr(modelMat));

            const float s = 1.0f;

            glDepthMask(GL_FALSE);  // disable depth writes so cube will not occlude future pixels

            glBegin(GL_TRIANGLE_STRIP);
            glVertex3f(s, -s, -s);
            glVertex3f(-s, -s, -s);
            glVertex3f(s, -s, s);
            glVertex3f(-s, -s, s);
            glVertex3f(s, s, s);
            glVertex3f(-s, s, s);
            glVertex3f(s, s, -s);
            glVertex3f(-s, s, -s);
            glVertex3f(s, -s, -s);
            glVertex3f(-s, -s, -s);
            glEnd();

            glBegin(GL_TRIANGLE_STRIP);
            glVertex3f(s, s, -s);
            glVertex3f(s, -s, -s);
            glVertex3f(s, s, s);
            glVertex3f(s, -s, s);
            glEnd();

            glBegin(GL_TRIANGLE_STRIP);
            glVertex3f(-s, -s, -s);
            glVertex3f(-s, s, -s);
            glVertex3f(-s, -s, s);
            glVertex3f(-s, s, s);
            glEnd();

            glDepthMask(GL_TRUE);
        }
    
        // draw world XYZ reference axes
        if (drawAxes) {
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(glm::value_ptr(proj));

            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(glm::value_ptr(view));

            environmentShader->SetUniform("overrideWithColor", 1.0f);

            glLineWidth(1.0f);

            environmentShader->SetUniform("color", STColor3f(1.0f, 0.0f, 0.0f));
            glBegin(GL_LINES);
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(10000.0f, 0.0f, 0.0f);
            glEnd();

            environmentShader->SetUniform("color", STColor3f(0.0f, 1.0f, 0.0f));
            glBegin(GL_LINES);
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(0.0f, 10000.0f, 0.0f);
            glEnd();

            environmentShader->SetUniform("color", STColor3f(0.0f, 0.0f, 1.0f));
            glBegin(GL_LINES);
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(0.0f, 0.0f, 10000.0f);
            glEnd();

            glPopAttrib();
        }

        environmentShader->UnBind();

    }
    


    // render models  ========================================================================================================================================================================================

    {
        shader->Bind();
        
        // displacement mapping not supported
        shader->SetUniform("displacementMapping", -1.0);

        // set depthtex du and dv for PCF sampling
        shader->SetUniform("depthTexDu", 1.0f / SHADOWMAP_TEX_WIDTH);
        shader->SetUniform("depthTexDv", 1.0f / SHADOWMAP_TEX_HEIGHT);

        shader->SetTexture("normalTex", 0);
        shader->SetTexture("displacementTex", 1);
	    shader->SetTexture("colorTex", 2);
        shader->SetTexture("cubeMap", 3);
        //shader->SetTexture("reflTex", 4); // only needed if drawing water
        shader->SetTexture("depthTex", 5);
        shader->SetTexture("spotTex", 6);


        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(proj));
    
    

        // load light positions
        // since phong.frag needs these in world space (which they're in already) but OpenGL
        // will automatically transform these to view space, we want GL_MODELVIEW to be identity.

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();


        setDirectionalLightAttribs();


        // bind cubemap
        glActiveTexture(GL_TEXTURE3);
        glEnable(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

        // bind depthtex and spottex for shadowmapping
        glActiveTexture(GL_TEXTURE5);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, depthTex);
    
        glActiveTexture(GL_TEXTURE6);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, spotTex);
    
        // set world eye pos
        shader->SetUniform("eyePosWorld", STColor3f(cameraPos.x, cameraPos.y, cameraPos.z));
        // get location of modelmat uniforms
        GLint modelMatLocation = shader->GetUniformLocation("modelMat");
	    GLint modelMatInvTransLocation = shader->GetUniformLocation("modelMatInvTrans");

        // set viewproj matrix of shadow-casting light (spotlight)
        GLint lightViewProjMatUniformLocation = shader->GetUniformLocation("lightViewProjMat");
        glUniformMatrix4fv(lightViewProjMatUniformLocation, 1, false, glm::value_ptr(lightViewProj));


        // set light attribs
        GLint pointLightPosUniformLocation = shader->GetUniformLocation("pointLightPos");
        setPointLightAttribs(pointLightPosUniformLocation);


        // draw objs
        glMatrixMode(GL_MODELVIEW);

        // draw water
        if (drawWater) {
            shader->SetUniform("cubeMapping", -1.0f);   // use refl tex
            shader->SetUniform("clipping", -1.0f);      // disable clipping

            shader->SetTexture("reflTex", 4);

            // bind cubemap
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, reflectionTex);


            int i = 0;      // water should be first entry in scene.txt
            glm::mat4 modelView = view * objs[i].worldMat;
            glLoadMatrixf(glm::value_ptr(modelView));

            GLint viewMatUniformLocation = shader->GetUniformLocation("viewMat");
            glUniformMatrix4fv(viewMatUniformLocation, 1, false, glm::value_ptr(view));
        
            std::vector<STTriangleMesh*>& stMeshes = objs[i].stMeshes;
            for (int j=0; j < stMeshes.size(); j++) {
            
                glm::mat4 invTrans = glm::transpose(glm::inverse(objs[i].worldMat));
                glUniformMatrix4fv(modelMatLocation, 1, false, glm::value_ptr(objs[i].worldMat));
                glUniformMatrix4fv(modelMatInvTransLocation, 1, false, glm::value_ptr(invTrans));

                shader->SetUniform("normalMapping", stMeshes[j]->mHasNormalMap ? 1.0 : -1.0);
                shader->SetUniform("colorMapping", stMeshes[j]->mHasColorMap ? 1.0 : -1.0);
            
                stMeshes[j]->Draw(smooth);
            }
        }
    
        // draw other objs
    
        shader->SetUniform("cubeMapping", 1.0);                     // don't use refl tex
        if (clipped) {                                              // set clipping
            shader->SetUniform("clipping", 1.0f);                   
            shader->SetUniform("clipZ", clipZ);
        } else {
            shader->SetUniform("clipping", -1.0f);
        }

        for (size_t i=1; i < objs.size(); i++) {
            glm::mat4 modelView = view * objs[i].worldMat;
            glLoadMatrixf(glm::value_ptr(modelView));
        
            std::vector<STTriangleMesh*>& stMeshes = objs[i].stMeshes;
            for (int j=0; j < stMeshes.size(); j++) {
            
                glm::mat4 invTrans = glm::transpose(glm::inverse(objs[i].worldMat));
                glUniformMatrix4fv(modelMatLocation, 1, false, glm::value_ptr(objs[i].worldMat));
                glUniformMatrix4fv(modelMatInvTransLocation, 1, false, glm::value_ptr(invTrans));

                shader->SetUniform("normalMapping", stMeshes[j]->mHasNormalMap ? 1.0 : -1.0);
                shader->SetUniform("colorMapping", stMeshes[j]->mHasColorMap ? 1.0 : -1.0);
            
                stMeshes[j]->Draw(smooth);
            }
        }

        shader->UnBind();
    }


    // render billboards  ========================================================================================================================================================================================
    {
        textureShader->Bind();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDepthMask(GL_FALSE);  // disable depth writes so billboards after this will not be occluded

        textureShader->SetUniform("clipping", clipped ? 1.0f : -1.0f);
        textureShader->SetUniform("clipZ", clipZ);
        textureShader->SetUniform("alphaScale", 1.0f);

        textureShader->SetTexture("colorTex", 4);

        glActiveTexture(GL_TEXTURE4);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, orbBillboardTex);
    

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(proj));


        GLint modelMatLocation = textureShader->GetUniformLocation("modelMat");
        
        // render pointlight orbs
        for (int i = 4; i < NUMLIGHTS; i++) {
        
            // calculate phi, theta of direction to camera from light pos
            glm::vec3 lightPos(lights[i].az, lights[i].el, lights[i].scale);
            glm::vec3 toEye = glm::normalize(cameraPos - lightPos);
            float phi = glm::degrees(glm::acos(toEye.z));
            float theta = glm::degrees(glm::atan(toEye.y, toEye.x));

            // calculate world matrix to aim the Z-axis of billboard space at 
            glm::mat4 worldMat;
            worldMat = glm::translate(worldMat, lightPos);                 // translate to lightpos
            worldMat = glm::rotate(worldMat, theta, glm::vec3(0.0f, 0.0f, 1.0f));               // theta
            worldMat = glm::rotate(worldMat, phi, glm::vec3(0.0f, 1.0f, 0.0f));    // phi
        
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(glm::value_ptr(view * worldMat));

            glUniformMatrix4fv(modelMatLocation, 1, false, glm::value_ptr(worldMat));

            float s = 4.0f;
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(s, -s, 0.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(s, s, 0.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-s, -s, 0.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-s, s, 0.0f);
            glEnd();
        }

        
        // render spotlight beam
        {
            textureShader->SetUniform("alphaScale", 0.375f);

            // bind beam texture
            glActiveTexture(GL_TEXTURE4);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, beamBillboardTex);

            // construct worldmat for billboard
            glm::vec3 toEye = camera.getPosition() - spotLightPosition;
            glm::vec3 zDir = lights[3].getDir();
            glm::vec3 yDir = glm::normalize(glm::cross(zDir, toEye));
            glm::vec3 xDir = glm::cross(yDir, zDir);
            
            glm::mat4 worldMat(glm::vec4(xDir, 0.0f),
                                glm::vec4(yDir, 0.0f),
                                glm::vec4(zDir, 0.0f),
                                glm::vec4(spotLightPosition, 1.0f));


            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(glm::value_ptr(view * worldMat));


            float w_half = 60.0f;   // width at wide end
            float h = 150.0f;       // length of beam

            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(0.0f, -w_half, h);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(0.0f, w_half, h);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(0.0f, -w_half, 0.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(0.0f, w_half, 0.0f);
            glEnd();
        }


        glDepthMask(GL_TRUE);

        glDisable(GL_BLEND);

        textureShader->UnBind();
    }
}











//
// Reshape the window and record the size so
// that we can use it for screenshots.
//
void ReshapeCallback(int w, int h)
{
	gWindowSizeX = w;
    gWindowSizeY = h;

    float aspectRatio = (float) gWindowSizeX / (float) gWindowSizeY;
    camera.setAspect(aspectRatio);
}

void SpecialKeyCallback(int key, int x, int y)
{
    // handle sprint movement
    float moveSensitivity = 5.0f;
    switch(key) {
    case GLUT_KEY_UP:
        camera.moveForward(moveSensitivity);
        break;
    case GLUT_KEY_DOWN:
        camera.moveForward(-moveSensitivity);
        break;
    case GLUT_KEY_LEFT:
        camera.moveRight(-moveSensitivity);
        break;
    case GLUT_KEY_RIGHT:
        camera.moveRight(moveSensitivity);
        break;
    }
    glutPostRedisplay();
}



void KeyCallback(unsigned char key, int x, int y)
{
    switch(key) {
    case 'p': {
            //
            // Take a screenshot, and save as screenshot.jpg
            //
            STImage* screenshot = new STImage(gWindowSizeX, gWindowSizeY);
            screenshot->Read(0,0);
            screenshot->Save("screenshot.jpg");
            delete screenshot;
        }
        break;
    case 'r':
        resetCamera();
        break;
    case 'm':   // write out world matrix of current object to file
        //objs[selectedObj].writeWorldMatrix();
        printf("writing scene files...\n");
        for (int i=0; i<objs.size(); i++) {
            objs[i].writeWorldMatrix();
        }
        for (int i=0; i<NUMLIGHTS; i++) {
            lights[i].writeLightsFile(i);
        }
        writeCameraFile("camera.txt");
        break;
    case 'f': // switch between smooth shading and flat shading
        smooth = !smooth;
        break;
    case 'o':   // center object's center of mass at origin
        objs[selectedObj].centerAtOrigin();
        printf("model centered at origin\n");
        break;
    case 'x':   // toggle axes
        axes = !axes;
        break;
	case 'q':
		exit(0);
    default:
        break;
    }

    // handle WASD movement
    float moveSensitivity = 0.7f;
    switch(key) {
    case 'w':
        camera.moveForward(moveSensitivity);
        break;
    case 's':
        camera.moveForward(-moveSensitivity);
        break;
    case 'a':
        camera.moveRight(-moveSensitivity);
        break;
    case 'd':
        camera.moveRight(moveSensitivity);
        break;
    case 32:    // spacebar
        camera.moveUp(moveSensitivity);
        break;
    case 'c':
        camera.moveUp(-moveSensitivity);
        break;
    }

    // select which axis to rotate the currently-selected object
    // or rotate it around that axis
    glm::mat4 &mat = objs[selectedObj].worldMat;
    switch (key) {
    case '1':
        axisOfRotation = glm::vec3(1.0f, 0.0f, 0.0f);
        printf("axis of rotation is X\n");
        break;
    case '2':
        axisOfRotation = glm::vec3(0.0f, 1.0f, 0.0f);
        printf("axis of rotation is Y\n");
        break;
    case '3':
        axisOfRotation = glm::vec3(0.0f, 0.0f, 1.0f);
        printf("axis of rotation is Z\n");
        break;
    case '-':
        //objs[selectedObj].worldMat = glm::rotate(45.0f, axisOfRotation) * objs[selectedObj].worldMat;
        objs[selectedObj].rotateCenter(axisOfRotation, 45.0f);
        printf("rotated +45\n");
        break;
    case '=':
        //objs[selectedObj].worldMat = glm::rotate(-45.0f, axisOfRotation) * objs[selectedObj].worldMat;
        objs[selectedObj].rotateCenter(axisOfRotation, -45.0f);
        printf("rotated -45\n");
        break;
    case '[':
        //objs[selectedObj].worldMat = glm::rotate(1.0f, axisOfRotation) * objs[selectedObj].worldMat;
        objs[selectedObj].rotateCenter(axisOfRotation, 1.0f);
        printf("rotated +1\n");
        break;
    case ']':
        //objs[selectedObj].worldMat = glm::rotate(-1.0f, axisOfRotation) * objs[selectedObj].worldMat;
        objs[selectedObj].rotateCenter(axisOfRotation, -1.0f);
        printf("rotated -1\n");
        break;
    case '0': // reset world matrix
        objs[selectedObj].resetWorldMatrix();
        printf("orientation reset\n");
        break;
    }

    // select which axis to translate the currently-selected object
    switch (key) {
    case '4':
        axisOfTranslation = glm::vec3(1.0f, 0.0f, 0.0f);
        printf("axis of translation is X\n");
        break;
    case '5':
        axisOfTranslation = glm::vec3(0.0f, 1.0f, 0.0f);
        printf("axis of translation is Y\n");
        break;
    case '6':
        axisOfTranslation = glm::vec3(0.0f, 0.0f, 1.0f);
        printf("axis of translation is Z\n");
        break;
    }

    // scale object
    float downFactor = 0.99f;
    glm::vec3 downscaleVector(downFactor, downFactor, downFactor);
    glm::vec3 upscaleVector(1.0f/downFactor, 1.0f/downFactor, 1.0f/downFactor);
    switch (key) {
    case ';':
        objs[selectedObj].worldMat = glm::scale(downscaleVector) * objs[selectedObj].worldMat;
        break;
    case '\'':
        objs[selectedObj].worldMat = glm::scale(upscaleVector) * objs[selectedObj].worldMat;
        break;
    }

    // select object
    switch (key) {
    case ',':
        selectedObj = selectedObj == 0 ? objs.size() - 1 : selectedObj - 1;
        rightMouseControllLights = false;
        printf("%s selected --------\n", objs[selectedObj].name.c_str());
        break;
    case '.':
        selectedObj = selectedObj == objs.size()-1 ? 0 : selectedObj + 1;
        rightMouseControllLights = false;
        printf("%s selected --------\n", objs[selectedObj].name.c_str());
        break;
    }

    // select/manipulate light
    switch (key) {
    case 'y':
        rightMouseControllLights = !rightMouseControllLights;
        if (rightMouseControllLights) {
            printf("right mouse controlling lights\n");
        } else {
            printf("right mouse controlling objs\n");
        }
        break;
    case 'b':
        selectedLight = selectedLight == 0 ? NUMLIGHTS - 1 : selectedLight - 1;
        rightMouseControllLights = true;
        printf("light %d selected **************\n", selectedLight);
        break;
    case 'n':
        selectedLight = selectedLight == NUMLIGHTS-1 ? 0 : selectedLight + 1;
        rightMouseControllLights = true;
        printf("light %d selected **************\n", selectedLight);
        break;
    case '7':
        selectedLightAttrib = 0;
        printf("light attrib az selected ====\n");
        break;
    case '8':
        printf("light attrib el selected ====\n");
        selectedLightAttrib = 1;
        break;
    case '9':
        printf("light attrib scale selected ====\n");
        selectedLightAttrib = 2;
        break;
    }


    glutPostRedisplay();
}

/**
 * Mouse event handler
 */
void MouseCallback(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON || button == GLUT_RIGHT_BUTTON)
    {
        gMouseButton = button;
    } else
    {
        gMouseButton = -1;
    }
    
    if (state == GLUT_UP)
    {
        gPreviousMouseX = -1;
        gPreviousMouseY = -1;
    }
}

/**
 * Mouse active motion callback (when button is pressed)
 */
void MouseMotionCallback(int x, int y)
{
    if (gPreviousMouseX >= 0 && gPreviousMouseY >= 0)
    {
        //compute delta
        float deltaX = x-gPreviousMouseX;
        float deltaY = y-gPreviousMouseY;
        gPreviousMouseX = x;
        gPreviousMouseY = y;
        
        float rotateSensitivity = 0.5f;
        float translateSensitivity = 0.2f;

        if (gMouseButton == GLUT_LEFT_BUTTON)
        {
            // look around
            camera.rotateRight(deltaX*rotateSensitivity);
            camera.rotateUp(-deltaY*rotateSensitivity);
        } else if (gMouseButton == GLUT_RIGHT_BUTTON) {
            if (!rightMouseControllLights) {
                // translate selected object
                objs[selectedObj].worldMat = glm::translate(-deltaY * axisOfTranslation) * objs[selectedObj].worldMat;
            } else {
                // change selected light attribute
                switch (selectedLightAttrib) {
                case 0:
                    lights[selectedLight].az += (-deltaY * 0.25f);
                    if (selectedLight < 4) {    // do not clamp light attrib values if a point light
                        while (lights[selectedLight].az > 360.0f) {
                            lights[selectedLight].az -= 360.0f;
                        }
                        while (lights[selectedLight].az < 0.0f) {
                            lights[selectedLight].az += 360.0f;
                        }
                    }
                    printf("%f\n", lights[selectedLight].az);
                    break;
                case 1:
                    lights[selectedLight].el += (-deltaY * 0.25f);
                    if (selectedLight < 4) {    // do not clamp light attrib values if a point light
                        if (lights[selectedLight].el > 90.0f) lights[selectedLight].el = 90.0f;
                        else if (lights[selectedLight].el < -90.0f) lights[selectedLight].el = -90.0f;
                    }
                    printf("%f\n", lights[selectedLight].el);
                    break;
                case 2:
                    if (selectedLight < 4) {    // do not clamp light attrib values if a point light
                        lights[selectedLight].scale += (-deltaY * 0.002f);
                        if (lights[selectedLight].scale > 1.0f) lights[selectedLight].scale = 1.0f;
                        else if (lights[selectedLight].scale < 0.0f) lights[selectedLight].scale = 0.0f;
                    } else {
                        lights[selectedLight].scale += (-deltaY * 0.25f);
                    }
                    printf("%f\n", lights[selectedLight].scale);
                    break;
                }
            }
        }
    } else
    {
        gPreviousMouseX = x;
        gPreviousMouseY = y;
    }
    
}

void usage()
{
	printf("usage: assignment2 sceneFile\n");
	exit(0);
}

int main(int argc, char** argv)
{
	if (argc != 2)
		usage();

    if (!readSceneFile(std::string(argv[1]))) {
        printf("error reading scene file %s\n", std::string(argv[3]).c_str());
        exit(1);
    }

    //
    // Initialize GLUT.
    //
    glutInit(&argc, argv);
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(20, 20);
    glutInitWindowSize(640, 480);
    glutCreateWindow("CS148 Assignment 2");
    
    //
    // Initialize GLEW.
    //
#ifndef __APPLE__
    glewInit();
    if(!GLEW_VERSION_2_0) {
        printf("Your graphics card or graphics driver does\n"
			   "\tnot support OpenGL 2.0, trying ARB extensions\n");

        if(!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader) {
            printf("ARB extensions don't work either.\n");
            printf("\tYou can try updating your graphics drivers.\n"
				   "\tIf that does not work, you will have to find\n");
            printf("\ta machine with a newer graphics card.\n");
            exit(1);
        }
    }
#endif

    // Be sure to initialize GLUT (and GLEW for this assignment) before
    // initializing your application.

    Setup();

    glutDisplayFunc(DisplayCallback);
    glutReshapeFunc(ReshapeCallback);
    glutSpecialFunc(SpecialKeyCallback);
    glutKeyboardFunc(KeyCallback);
    glutMouseFunc(MouseCallback);
    glutMotionFunc(MouseMotionCallback);
    glutIdleFunc(DisplayCallback);

    glutMainLoop();

    // Cleanup code should be called here.
    CleanUp();

    return 0;
}
