#include "MathUtils.h"

#include "gtx/quaternion.hpp"
#include "gtc/type_ptr.hpp"
#include "gtc/matrix_transform.hpp"

namespace tango_chromium {

void matrixFrustum(const float& left,
    const float& right,
    const float& bottom,
    const float& top,
    const float& near,
    const float& far,
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
  const float x_scale = near / fx;
  const float y_scale = near / fy;

  const float x_offset = (cx - (width / 2.0)) * x_scale;
  // Color camera's coordinates has y pointing downwards so we negate this term.
  const float y_offset = -(cy - (height / 2.0)) * y_scale;

  matrixFrustum(x_scale * -width / 2.0f - x_offset,
          x_scale * width / 2.0f - x_offset,
          y_scale * -height / 2.0f - y_offset,
          y_scale * height / 2.0f - y_offset, near, far,
          matrix);
}

glm::mat4 mat4FromTranslationOrientation(const double *translation, 
    const double *orientation) {
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

void translationOrientationAndScaleFromMat4(const glm::mat4& matrix, 
    glm::vec3& translation, glm::quat& orientation, glm::vec3& scale) {

  const float* p_matrix = glm::value_ptr(matrix);
  scale.x = glm::length(glm::vec3(p_matrix[0], p_matrix[1], p_matrix[ 2]));
  scale.y = glm::length(glm::vec3(p_matrix[4], p_matrix[5], p_matrix[ 6]));
  scale.z = glm::length(glm::vec3(p_matrix[8], p_matrix[9], p_matrix[10]));
  float determinant = glm::determinant(matrix);
  if (determinant < 0) {
    scale.x = -scale.x;
  }
  translation.x = p_matrix[12];
  translation.y = p_matrix[13];
  translation.z = p_matrix[14];

  glm::mat4 matrix_copy(matrix);
  float* p_matrix_copy = const_cast<float*>(glm::value_ptr(matrix_copy));
  glm::vec3 inverse_scale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);

  p_matrix_copy[ 0] *= inverse_scale.x;
  p_matrix_copy[ 1] *= inverse_scale.x;
  p_matrix_copy[ 2] *= inverse_scale.x;

  p_matrix_copy[ 4] *= inverse_scale.y;
  p_matrix_copy[ 5] *= inverse_scale.y;
  p_matrix_copy[ 6] *= inverse_scale.y;

  p_matrix_copy[ 8] *= inverse_scale.z;
  p_matrix_copy[ 9] *= inverse_scale.z;
  p_matrix_copy[10] *= inverse_scale.z;

  glm::quat orientation_copy(matrix_copy);
  orientation.x = orientation_copy.x;
  orientation.y = orientation_copy.y;
  orientation.z = orientation_copy.z;
  orientation.w = orientation_copy.w;

    // Original code from ThreeJS
    // ==========================
    // var te = this.elements;

    //   var sx = vector.set( te[ 0 ], te[ 1 ], te[ 2 ] ).length();
    //   var sy = vector.set( te[ 4 ], te[ 5 ], te[ 6 ] ).length();
    //   var sz = vector.set( te[ 8 ], te[ 9 ], te[ 10 ] ).length();

    //   // if determine is negative, we need to invert one scale
    //   var det = this.determinant();
    //   if ( det < 0 ) sx = - sx;

    //   position.x = te[ 12 ];
    //   position.y = te[ 13 ];
    //   position.z = te[ 14 ];

    //   // scale the rotation part
    //   matrix.copy( this );

    //   var invSX = 1 / sx;
    //   var invSY = 1 / sy;
    //   var invSZ = 1 / sz;

    //   matrix.elements[ 0 ] *= invSX;
    //   matrix.elements[ 1 ] *= invSX;
    //   matrix.elements[ 2 ] *= invSX;

    //   matrix.elements[ 4 ] *= invSY;
    //   matrix.elements[ 5 ] *= invSY;
    //   matrix.elements[ 6 ] *= invSY;

    //   matrix.elements[ 8 ] *= invSZ;
    //   matrix.elements[ 9 ] *= invSZ;
    //   matrix.elements[ 10 ] *= invSZ;

    //   quaternion.setFromRotationMatrix( matrix );

    //   scale.x = sx;
    //   scale.y = sy;
    //   scale.z = sz;
}

float rayIntersectsPlane(const glm::vec3 &plane_normal, 
    const glm::vec3& plane_position,
    const glm::vec3& ray_origin, const glm::vec3& ray_direction) {
  float denom = glm::dot(plane_normal, ray_direction);
  glm::vec3 ray_to_plane = plane_position - ray_origin;
  return glm::dot(ray_to_plane, plane_normal) / denom;
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

bool isPointInPolygon(const glm::vec3 &point, const glm::vec3* polygon_points,
    int count) {
  if (count < 3)
  {
    return false;
  }

  glm::vec3 lastUp = glm::vec3(0, 0, 0);
  for (int i = 0; i < count; ++i)
  {
    glm::vec3 v0 = point - polygon_points[i];
    glm::vec3 v1;
    if (i == count - 1)
    {
      v1 = polygon_points[0] - polygon_points[i];
    }
    else
    {
      v1 = polygon_points[i + 1] - polygon_points[i];
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

glm::vec3 getSafeReciprocalScale(const glm::vec3& scale, float tolerance) {
  glm::vec3 safe_reciprocal_scale;
  if (glm::abs(scale.x) <= tolerance) {
    safe_reciprocal_scale.x = 0.f;
  } else {
    safe_reciprocal_scale.x = 1 / scale.x;
  }

  if (glm::abs(scale.y) <= tolerance) {
    safe_reciprocal_scale.y = 0.f;
  } else {
    safe_reciprocal_scale.y = 1 / scale.y;
  }

  if (glm::abs(scale.z) <= tolerance) {
    safe_reciprocal_scale.z = 0.f;
  } else {
    safe_reciprocal_scale.z = 1 / scale.z;
  }

  return safe_reciprocal_scale;
}

// Code taken from how the Unreal Engine integration of anchors handles
// the involved math: https://daydream-internal.git.corp.google.com/ThirdParty/UnrealGithub/+/release-arcore-devpreview/Engine/Source/Runtime/Core/Private/Math/Transform.cpp
glm::mat4 getMatrixRelativeToMatrix(const glm::mat4& original_matrix, 
    const glm::mat4& relative_to_matrix, bool use_original_scale) {
  // Extract the components of the original matrix
  glm::vec3 original_scale;
  glm::quat original_rotation;
  glm::vec3 original_translation;
  translationOrientationAndScaleFromMat4(
      original_matrix,
      original_translation,
      original_rotation,
      original_scale);

  // Force the original scale to be 1 if specified not to use the original scale
  if (!use_original_scale) {
    original_scale.x = original_scale.y = original_scale.z = 1.0f;
  }

  // Extract the component of the relative to matrix
  glm::vec3 relative_to_scale;
  glm::quat relativeToRotation;
  glm::vec3 relative_to_translation;
  translationOrientationAndScaleFromMat4(
      relative_to_matrix, 
      relative_to_translation, 
      relativeToRotation, 
      relative_to_scale);

  // Calculate the translation, scale and rotation
  glm::vec3 safe_reciprocal_scale = getSafeReciprocalScale(relative_to_scale, 
      0.00001f);

  glm::vec3 final_scale(original_scale * safe_reciprocal_scale);

  glm::quat relative_to_rotation_inverse = glm::inverse(relativeToRotation);
  glm::quat final_rotation(relative_to_rotation_inverse * original_rotation);

  glm::vec3 final_translation(
      (relative_to_rotation_inverse * 
      (original_translation - relative_to_translation)) * safe_reciprocal_scale);

  // Calculate the matrix relative to matrix from the translation, scale 
  // and rotation.
  glm::mat4 matrix_relative_to_matrix;
  matrix_relative_to_matrix = glm::toMat4(final_rotation);
  matrix_relative_to_matrix = glm::scale(matrix_relative_to_matrix, final_scale);
  matrix_relative_to_matrix = glm::translate(matrix_relative_to_matrix, 
      final_translation);
  return matrix_relative_to_matrix;
}

} // tango_chromium