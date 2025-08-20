//
// Created by Orgest on 6/10/2025.
//

#pragma once
#include <cmath>

struct Vec3
{
	float x, y, z;

	Vec3() = default;
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
	explicit Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}

	Vec3 operator+(const Vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
	Vec3 operator-(const Vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
	Vec3 operator*(const float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
	Vec3 operator/(float scalar) const { return {x / scalar, y / scalar, z / scalar}; }

	Vec3& operator+=(const Vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
	Vec3& operator-=(const Vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
	Vec3& operator*=(const float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
	Vec3& operator/=(const float scalar) { return *this *= 1 / scalar; }

	float& operator[](int i) { return *(&x + i); }
	const float& operator[](int i) const { return *(&x + i); }

	static Vec3 Zero() { return {0, 0, 0}; }
	[[nodiscard]] float Length() const { return sqrt(x * x + y * y + z * z); }
	[[nodiscard]] float LengthSquared() const { return x * x + y * y + z * z; }
	[[nodiscard]] Vec3 Normalized() const {
		const float len = Length();
		return len > 0.0f ? Vec3{x / len, y / len, z / len} : Vec3{0.0f, 0.0f, 0.0f};
	}

	[[nodiscard]] float Dot(const Vec3& rhs) const
	{
		return (x * rhs.x) + (y * rhs.y) + (z * rhs.z);
	}

	[[nodiscard]] Vec3 Cross(const Vec3& rhs) const {
		return {
			y * rhs.z - z * rhs.y,
			z * rhs.x - x * rhs.z,
			x * rhs.y - y * rhs.x
		};
	}
};