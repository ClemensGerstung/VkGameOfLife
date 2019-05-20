#pragma once

#include <functional>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

constexpr float zoom = 1.0f;

enum class CameraPerspective
{
  Orthogonal,
  Perspective
};

enum class CameraMode
{
  Ego,
  Orbit
};

class Camera
{
private:
  VkExtent2D extent;
  float fov;
  float near;
  float far;

  glm::vec3 position;
  glm::vec3 lookAt;

  glm::mat4 mat;
  glm::mat4 inverse;
  bool dirty;

  CameraPerspective perspective;
  CameraMode mode;

  //std::function<void(float)> zoom;
  std::function<glm::mat4()> projection;
public:
  Camera() = default;
  ~Camera() = default;

  Camera(VkExtent2D extent, glm::vec3 pos, glm::vec3 lookat = { 0, 0, 0 }, float fov = 22.5f, float near = -zoom, float far = zoom);
  Camera(uint32_t width, uint32_t height, glm::vec3 pos, glm::vec3 lookat = { 0, 0, 0 }, float fov = 22.5f, float near = -zoom, float far = zoom);
  Camera(const Camera& camera);
  Camera(Camera&& camera) = delete;

  Camera& operator=(const Camera& camera);
  Camera& operator=(Camera&& camera) = delete;

  void Move(const glm::vec3& delta);
  void Rotate(float theta, const glm::vec3& axis);

  glm::vec4 ScreenToWorld(uint32_t screenX, uint32_t screenY, float z = 0.5f);
  glm::mat4 WorldToScreenMatrix();

  void Zoom(float delta)
  {
    //zoom(delta);
  }

  //static Camera& CreateOrthoOrbit()
  //{
  //  Camera camera;

  //  camera.projection = [&pos = camera.position]() -> glm::mat4
  //  {
  //    return glm::ortho(-pos.z, pos.z, -pos.z, pos.z, -pos.z, pos.z);
  //  };

  //  return camera;
  //}
  //static Camera& CreateOrthoEgo();
  //static Camera& CreateOrbit();
  //static Camera& CreateEgo();
};