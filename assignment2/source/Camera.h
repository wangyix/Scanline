#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
private:
	glm::vec3 position;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 lookNeg;

	float nearPlane;
	float farPlane;
	float fovy;
	float aspect;

	glm::mat4 proj;

public:
	Camera();

	void setPosition(const glm::vec3 &position);
    void setLook(const glm::vec3 &direction);

	void setLens(float nearplane, float farplane, float fovy);
	void setAspect(float aspect);

	glm::vec3 moveForward(float Dist);
	glm::vec3 moveRight(float Dist);
	glm::vec3 moveUp(float Dist);

	void rotateRight(float angle);
	void rotateUp(float angle);
	void lookAt(const glm::vec3 &target);

    glm::vec3 getPosition() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    glm::vec3 getLook() const;

	glm::mat4 getView() const;
    glm::mat4 getProj() const;
	glm::mat4 getViewProj() const;

private:
	void updateProj();
	void orthonormalize();
};