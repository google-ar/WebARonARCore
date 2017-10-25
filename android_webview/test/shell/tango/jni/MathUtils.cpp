#include "MathUtils.h"

#include "gtx/quaternion.hpp"
#include "gtc/type_ptr.hpp"

namespace tango_chromium {

void matrixFrustum(float const & left,
             float const & right,
             float const & bottom,
             float const & top,
             float const & near,
             float const & far,
             float* matrix) {

  float x = 2 * near / ( right - left );
  float y = 2 * near / ( top - bottom );

  float a = ( right + left ) / ( right - left );
  float b = ( top + bottom ) / ( top - bottom );
  float c = - ( far + near ) / ( far - near );
  float d = - 2 * far * near / ( far - near );

  matrix[ 0 ] = x;  matrix[ 4 ] = 0;  matrix[ 8 ] = a;  matrix[ 12 ] = 0;
  matrix[ 1 ] = 0;  matrix[ 5 ] = y;  matrix[ 9 ] = b;  matrix[ 13 ] = 0;
  matrix[ 2 ] = 0;  matrix[ 6 ] = 0;  matrix[ 10 ] = c; matrix[ 14 ] = d;
  matrix[ 3 ] = 0;  matrix[ 7 ] = 0;  matrix[ 11 ] = - 1; matrix[ 15 ] = 0;


    // matrix[0] = (float(2) * near) / (right - left);
    // matrix[5] = (float(2) * near) / (top - bottom);
    // matrix[2][0] = (right + left) / (right - left);
    // matrix[2][1] = (top + bottom) / (top - bottom);
    // matrix[10] = -(farVal + nearVal) / (farVal - nearVal);
    // matrix[2][3] = float(-1);
    // matrix[3][2] = -(float(2) * farVal * nearVal) / (farVal - nearVal);
}

void matrixProjection(float width, float height,
                      float fx, float fy,
                      float cx, float cy,
                      float near, float far,
                      float* matrix) {
  const float xscale = near / fx;
  const float yscale = near / fy;

  const float xoffset = (cx - (width / 2.0)) * xscale;
  // Color camera's coordinates has y pointing downwards so we negate this term.
  const float yoffset = -(cy - (height / 2.0)) * yscale;

  matrixFrustum(xscale * -width / 2.0f - xoffset,
          xscale * width / 2.0f - xoffset,
          yscale * -height / 2.0f - yoffset,
          yscale * height / 2.0f - yoffset, near, far,
          matrix);
}

glm::mat4 mat4FromTranslationOrientation(const double *translation, 
                                              const double *orientation) {
  glm::vec3 translationV3 = glm::vec3((float)translation[0],
  (float)translation[1], (float)translation[2]);
  glm::quat orientationQ = glm::quat((float)orientation[0], (float)orientation[1],
  (float)orientation[2], (float)orientation[3]);

  glm::mat4 m = glm::mat4();
  float* out;
  out = glm::value_ptr(m);
  float x = (float)orientation[0];
  float y = (float)orientation[1];
  float z = (float)orientation[2];
  float w = (float)orientation[3];

  float x2 = x + x;
  float y2 = y + y;
  float z2 = z + z;
  float xx = x * x2;
  float xy = x * y2;
  float xz = x * z2;
  float yy = y * y2;
  float yz = y * z2;
  float zz = z * z2;
  float wx = w * x2;
  float wy = w * y2;
  float wz = w * z2;

  out[0] = 1 - (yy + zz);
  out[1] = xy + wz;
  out[2] = xz - wy;
  out[3] = 0;
  out[4] = xy - wz;
  out[5] = 1 - (xx + zz);
  out[6] = yz + wx;
  out[7] = 0;
  out[8] = xz + wy;
  out[9] = yz - wx;
  out[10] = 1 - (xx + yy);
  out[11] = 0;
  out[12] = (float)translation[0];
  out[13] = (float)translation[1];
  out[14] = (float)translation[2];
  out[15] = 1;

  return m;
};

float rayIntersectsPlane(const glm::vec3 &planeNormal, const glm::vec3 &planePosition,
    const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) {
  float denom = glm::dot(planeNormal, rayDirection);
  glm::vec3 rayToPlane = planePosition - rayOrigin;
  return glm::dot(rayToPlane, planeNormal) / denom;
}

glm::vec3 transformVec3ByMat4(const glm::vec3 &v, const glm::mat4 &m4) {
  const float* m;
  m = glm::value_ptr(m4);

  float x = v[0];
  float y = v[1];
  float z = v[2];
  float w = m[3] * x + m[7] * y + m[11] * z + m[15];

  if (w == 0) {
    w = 1.0f;
  }

  return glm::vec3(
    (m[0] * x + m[4] * y + m[8] * z + m[12]) / w,
    (m[1] * x + m[5] * y + m[9] * z + m[13]) / w,
    (m[2] * x + m[6] * y + m[10] * z + m[14]) / w);
}

void setFloat16FromMat4(float *out, const glm::mat4 &m4) {
  const float* fM = glm::value_ptr(m4);
  for (int i = 0; i < 16; i++) {
    out[i] = fM[i];
  }
}

bool isPointInPolygon(const glm::vec3 &point, const glm::vec3* polygonPoints, int count) {
  if (count < 3)
  {
    return false;
  }

  glm::vec3 lastUp = glm::vec3(0, 0, 0);
  for (int i = 0; i < count; ++i)
  {
    glm::vec3 v0 = point - polygonPoints[i];
    glm::vec3 v1;
    if (i == count - 1)
    {
      v1 = polygonPoints[0] - polygonPoints[i];
    }
    else
    {
      v1 = polygonPoints[i + 1] - polygonPoints[i];
    }

    glm::vec3 up = glm::cross(v0, v1);
    if (i != 0)
    {
      float sign = glm::dot(up, lastUp);
      if (sign < 0)
      {
        return false;
      }
    }

    lastUp = up;
  }
  return true;
}

} // tango_chromium