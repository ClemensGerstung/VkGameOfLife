#include "Camera.h"

#include <glm/gtc/quaternion.hpp>

Camera::Camera(VkExtent2D extent, glm::vec3 pos, glm::vec3 lookat, float fov, float near, float far)
  : extent(extent), fov(fov), near(near), far(far), position(pos), lookAt(lookat)
{
  mat = glm::mat4(1.0f);
  dirty = true;
}

Camera::Camera(uint32_t width, uint32_t height, glm::vec3 pos, glm::vec3 lookat, float fov, float near, float far)
  : Camera({ width, height }, pos, lookat, fov, near, far)
{
}

Camera::Camera(const Camera & camera)
  : Camera(camera.extent, camera.position, camera.lookAt, camera.fov, camera.near, camera.far)
{
  position = camera.position;
  lookAt = camera.lookAt;
  mat = camera.mat;
  inverse = camera.inverse;
}


Camera & Camera::operator=(const Camera & camera)
{
  extent = camera.extent;
  fov = camera.fov;
  near = camera.near;
  far = camera.far;
  position = camera.position;
  lookAt = camera.lookAt;
  mat = camera.mat;
  inverse = camera.inverse;
  dirty = true;

  return *this;
}

void Camera::Move(const glm::vec3 & delta)
{
  position += delta;
  lookAt += delta;

  dirty = true;
}

void Camera::Rotate(float theta, const glm::vec3& axis)
{
  // rotate 'position' around 'lookat' with the given angle and axis
  // needs quaternion, or sth like that
  // I don't know, someone smarter than me will tell me probably
}

glm::vec4 Camera::ScreenToWorld(uint32_t screenX, uint32_t screenY, float z)
{
  glm::vec4 in;
   in.x = (2.0f * screenX / float(extent.width)) - 1.0f;
   in.y = (2.0f * screenY / float(extent.height)) - 1.0f;
  //in.x = screenX / float(extent.width);
  //in.y = screenY / float(extent.height);
  in.z = 0;
  in.w = 1.0f;

  glm::vec4 out = in * inverse;
  // out.w = 1.0f / out.w;
  // out.x *= out.w;
  // out.y *= out.w;
  // out.z *= out.w;

  return out;
}

glm::mat4 Camera::WorldToScreenMatrix()
{
  if (dirty)
  {
    auto view = glm::lookAt(position, lookAt, glm::vec3(0.0f, 1.0f, 0.0f));
    //auto proj = projection();
    auto proj = glm::ortho(position.z, -position.z, position.z, -position.z, position.z, -position.z);

    //auto proj = glm::perspective(glm::radians(fov), extent.width / (float)extent.height, near, far);
    proj[1][1] *= -1;
    
    mat = proj * view;
    inverse = glm::inverse(mat);

    dirty = false;
  }

  return mat;
}
