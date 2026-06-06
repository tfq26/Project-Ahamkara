#pragma once

#include <algorithm>
#include <cmath>

namespace ae {

constexpr float pi = 3.14159265358979323846F;
constexpr float epsilon = 1.0e-6F;

inline float to_radians(float degrees) {
    return degrees * (pi / 180.0F);
}

inline float to_degrees(float radians) {
    return radians * (180.0F / pi);
}

inline float clamp(float value, float min_value, float max_value) {
    return std::clamp(value, min_value, max_value);
}

inline float wrap_degrees(float degrees) {
    float wrapped = std::fmod(degrees, 360.0F);
    if (wrapped >= 180.0F) {
        wrapped -= 360.0F;
    }
    if (wrapped < -180.0F) {
        wrapped += 360.0F;
    }
    return wrapped;
}

struct Vec2 {
    float x {0.0F};
    float y {0.0F};

    [[nodiscard]] float length_squared() const {
        return x * x + y * y;
    }

    [[nodiscard]] float length() const {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] Vec2 normalized() const {
        const float len = length();
        if (len <= epsilon) {
            return {};
        }

        return {x / len, y / len};
    }

    Vec2& operator+=(const Vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vec2& operator-=(const Vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vec2& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Vec2& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }
};

inline Vec2 operator+(Vec2 lhs, const Vec2& rhs) {
    lhs += rhs;
    return lhs;
}

inline Vec2 operator-(Vec2 lhs, const Vec2& rhs) {
    lhs -= rhs;
    return lhs;
}

inline Vec2 operator*(Vec2 value, float scalar) {
    value *= scalar;
    return value;
}

inline Vec2 operator*(float scalar, Vec2 value) {
    value *= scalar;
    return value;
}

inline Vec2 operator/(Vec2 value, float scalar) {
    value /= scalar;
    return value;
}

inline bool operator==(const Vec2& lhs, const Vec2& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator!=(const Vec2& lhs, const Vec2& rhs) {
    return !(lhs == rhs);
}

inline float dot(const Vec2& lhs, const Vec2& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

struct Vec3 {
    float x {0.0F};
    float y {0.0F};
    float z {0.0F};

    [[nodiscard]] float length_squared() const {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] Vec3 normalized() const {
        const float len = length();
        if (len <= epsilon) {
            return {};
        }

        return {x / len, y / len, z / len};
    }

    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vec3& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    Vec3& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }
};

inline Vec3 operator+(Vec3 lhs, const Vec3& rhs) {
    lhs += rhs;
    return lhs;
}

inline Vec3 operator-(Vec3 lhs, const Vec3& rhs) {
    lhs -= rhs;
    return lhs;
}

inline Vec3 operator-(Vec3 value) {
    return {-value.x, -value.y, -value.z};
}

inline Vec3 operator*(Vec3 value, float scalar) {
    value *= scalar;
    return value;
}

inline Vec3 operator*(float scalar, Vec3 value) {
    value *= scalar;
    return value;
}

inline Vec3 operator/(Vec3 value, float scalar) {
    value /= scalar;
    return value;
}

inline bool operator==(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

inline bool operator!=(const Vec3& lhs, const Vec3& rhs) {
    return !(lhs == rhs);
}

inline float dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

struct Mat4 {
    float m[4][4] {};

    [[nodiscard]] static Mat4 identity() {
        Mat4 result {};
        result.m[0][0] = 1.0F;
        result.m[1][1] = 1.0F;
        result.m[2][2] = 1.0F;
        result.m[3][3] = 1.0F;
        return result;
    }

    [[nodiscard]] static Mat4 translation(const Vec3& translation) {
        Mat4 result = identity();
        result.m[0][3] = translation.x;
        result.m[1][3] = translation.y;
        result.m[2][3] = translation.z;
        return result;
    }

    [[nodiscard]] static Mat4 perspective(float vertical_fov_degrees, float aspect_ratio, float near_plane, float far_plane) {
        Mat4 result {};
        const float tan_half_fov = std::tan(to_radians(vertical_fov_degrees) * 0.5F);
        result.m[0][0] = 1.0F / (aspect_ratio * tan_half_fov);
        result.m[1][1] = 1.0F / tan_half_fov;
        result.m[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
        result.m[2][3] = -(2.0F * far_plane * near_plane) / (far_plane - near_plane);
        result.m[3][2] = -1.0F;
        return result;
    }

    [[nodiscard]] static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
        const Vec3 forward = (target - eye).normalized();
        const Vec3 right = cross(up, forward).normalized();
        const Vec3 camera_up = cross(forward, right);

        Mat4 result = identity();
        result.m[0][0] = right.x;
        result.m[0][1] = right.y;
        result.m[0][2] = right.z;
        result.m[0][3] = -dot(right, eye);

        result.m[1][0] = camera_up.x;
        result.m[1][1] = camera_up.y;
        result.m[1][2] = camera_up.z;
        result.m[1][3] = -dot(camera_up, eye);

        result.m[2][0] = -forward.x;
        result.m[2][1] = -forward.y;
        result.m[2][2] = -forward.z;
        result.m[2][3] = dot(forward, eye);

        return result;
    }

    [[nodiscard]] Vec3 transform_point(const Vec3& point) const {
        const float x = m[0][0] * point.x + m[0][1] * point.y + m[0][2] * point.z + m[0][3];
        const float y = m[1][0] * point.x + m[1][1] * point.y + m[1][2] * point.z + m[1][3];
        const float z = m[2][0] * point.x + m[2][1] * point.y + m[2][2] * point.z + m[2][3];
        const float w = m[3][0] * point.x + m[3][1] * point.y + m[3][2] * point.z + m[3][3];

        if (std::fabs(w) <= epsilon) {
            return {x, y, z};
        }

        return {x / w, y / w, z / w};
    }
};

inline Mat4 operator*(const Mat4& lhs, const Mat4& rhs) {
    Mat4 result {};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            for (int index = 0; index < 4; ++index) {
                result.m[row][column] += lhs.m[row][index] * rhs.m[index][column];
            }
        }
    }
    return result;
}

}  // namespace ae
