#ifndef _MATH_UTILS_H_
#define _MATH_UTILS_H_

#include "glm.hpp"

namespace tango_chromium {

void matrixFrustum(const float& left, const float& right,
    const float& bottom, const float& top,
    const float& near, const float & far,
    float* matrix);

void matrixProjection(float width, float height,
    float fx, float fy,
    float cx, float cy,
    float near, float far,
    float* matrix);

glm::mat4 mat4FromTranslationOrientation(const double *translation, 
    const double *orientation);

void translationOrientationAndScaleFromMat4(const glm::mat4& matrix, 
    glm::vec3& translation, glm::quat& orientation, glm::vec3& scale);

float rayIntersectsPlane(const glm::vec3 &plane_normal, 
    const glm::vec3& plane_position,
    const glm::vec3& ray_origin, 
    const glm::vec3& ray_direction);

glm::vec3 transformVec3ByMat4(const glm::vec3 &v, const glm::mat4 &m4);

void setFloat16FromMat4(float *out, const glm::mat4 &m4);

bool isPointInPolygon(const glm::vec3 &point, const glm::vec3* polygon_points, 
    int count);

glm::vec3 getSafeReciprocalScale(const glm::vec3& scale, float tolerance);

glm::mat4 getMatrixRelativeToMatrix(const glm::mat4& original_matrix, 
    const glm::mat4& relative_to_matrix, bool use_original_scale);

} // tango_chromium

#endif // _MATH_UTILS_H_