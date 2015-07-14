#include "Obj.h"

#define NOMINMAX

//#include <iostream>
#include <fstream>
/*#include <map>
#include <math.h>
#include <string.h>
#include <algorithm>
*/
#include "st.h"

Obj::Obj() :
    name(),
    stMeshes(),
    centerOfMass(),
    defaultWorldMat(),
    worldMat()
{
}

Obj::Obj(const std::string& filename) :
    stMeshes(),
    centerOfMass(),
    defaultWorldMat(),
    worldMat()
{
    read(filename);
}

Obj::~Obj() {
    for (size_t i=0; i<stMeshes.size(); i++) {
        delete stMeshes.back();
        stMeshes.pop_back();
    }
}

bool Obj::read(const std::string& filename) {
    int pos = filename.find_last_of('.');
    name = filename.substr(0, pos);

    std::string err = STTriangleMesh::LoadObj(stMeshes, filename);
    if (err.length() > 0) {
        std::cout << "Load obj error: " << err;
        return false;
    }
    STPoint3 cmSt = STTriangleMesh::GetMassCenter(stMeshes);
    centerOfMass = glm::vec3(cmSt.x, cmSt.y, cmSt.z);
    
    if (!readWorldMatrix()) {
        // no default matrix file was found; use bbcenter-to-origin translation matrix as default
        std::cout << name << " has no mat file, using default" << std::endl;
        centerAtOrigin();
        defaultWorldMat = worldMat;
    } else {
        std::cout << name << " mat file found" << std::endl;
    }
    return true;
}

bool Obj::readWorldMatrix() {
    std::string matFileName = name;
    matFileName.append(".txt");
    std::ifstream in(matFileName.c_str(), std::ios::in);
    if( !in ){
        return false;
    }

    float values[16];
    for (int i=0; i<16; i+=4) {
        in >> values[i] >> values[i+1] >> values[i+2] >> values[i+3];
    }
    defaultWorldMat = glm::mat4(values[0], values[1], values[2], values[3],
    values[4], values[5], values[6], values[7],
    values[8], values[9], values[10], values[11],
    values[12], values[13], values[14], values[15]);
    
    in.close();

    worldMat = defaultWorldMat;

    return true;
}

bool Obj::writeWorldMatrix() {
    std::string matFileName = name;
    matFileName.append(".txt");
    std::ofstream out(matFileName.c_str(), std::ios::out);
    if (!out) {
        std::cout << "cannot open file" << matFileName << std::endl;
        return false;
    }
    float* values = glm::value_ptr(worldMat);
    for (int i=0; i<16; i+=4) {
        out << values[i] << " " << values[i+1] << " " << values[i+2] << " " << values[i+3] << std::endl;
    }
    out.close();

    std::cout << matFileName << " written" << std::endl;
    return true;
}

void Obj::resetWorldMatrix() {
    worldMat = defaultWorldMat;
}

void Obj::centerAtOrigin() {
    glm::vec4 worldCm = worldMat * glm::vec4(centerOfMass, 1.0f);
    printf("center was at %f, %f %f\n", worldCm.x, worldCm.y, worldCm.z);
    worldMat = glm::translate(-glm::vec3(worldCm)) * worldMat;
}

void Obj::rotateCenter(glm::vec3& axis, float deg) {
    glm::vec4 center = worldMat * glm::vec4(centerOfMass, 1.0f);
    glm::vec3 translateToOrigin = -glm::vec3(center);
    worldMat = glm::translate(-translateToOrigin) * glm::rotate(deg, axis) * glm::translate(translateToOrigin) * worldMat;
}