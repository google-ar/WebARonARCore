#ifndef _MATH_UTILS_H_
#define _MATH_UTILS_H_

#include "glm.hpp"

namespace tango_chromium {

void matrixFrustum(float const & left, float const & right,
                   float const & bottom, float const & top,
                   float const & near, float const & far,
                   float* matrix);

void matrixProjection(float width, float height,
                      float fx, float fy,
                      float cx, float cy,
                      float near, float far,
                      float* matrix);

glm::mat4 mat4FromTranslationOrientation(const double *translation, 
                                         const double *orientation);

float rayIntersectsPlane(const glm::vec3 &planeNormal, 
                         const glm::vec3 &planePosition,
                         const glm::vec3 &rayOrigin, 
                         const glm::vec3 &rayDirection);

glm::vec3 transformVec3ByMat4(const glm::vec3 &v, const glm::mat4 &m4);

void setFloat16FromMat4(float *out, const glm::mat4 &m4);

bool isPointInPolygon(const glm::vec3 &point, const glm::vec3* polygonPoints, int count);

} // tango_chromium

#endif // _MATH_UTILS_H_