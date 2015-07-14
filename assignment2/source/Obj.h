#pragma once

#include "STTriangleMesh.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>

class Obj {

public:
    Obj();
    Obj(const std::string& filename);
    ~Obj();

    bool read(const std::string& filename);

    bool readWorldMatrix();
    bool writeWorldMatrix();
    void resetWorldMatrix();

    void centerAtOrigin();

    void rotateCenter(glm::vec3& axis, float deg);

    std::string name;
    std::vector<STTriangleMesh*> stMeshes;
    glm::vec3 centerOfMass;

    glm::mat4 defaultWorldMat;
    glm::mat4 worldMat;
};
