//
// Created by Orgest on 6/14/2025.
//

#pragma once
#include <cmath>

struct Vec4
{
	float x, y, z, w;

	Vec4() = default;
	Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	Vec4(float scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}

	Vec4 operator+(const Vec4& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
	Vec4 operator-(const Vec4& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
	Vec4 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
	Vec4 operator/(float scalar) const { return {x / scalar, y / scalar, z / scalar, w / scalar}; }

	Vec4& operator+=(const Vec4& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
	Vec4& operator-=(const Vec4& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; return *this; }
	Vec4& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }
	Vec4& operator/=(float scalar) { return *this *= 1.0f / scalar; }

	float& operator[](int index) {return *(&x + index);}
	const float& operator[](int index) const {return *(&x + index);}


	static Vec4 Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }

	[[nodiscard]] float Length() const { return std::sqrt(x * x + y * y + z * z + w * w); }
	[[nodiscard]] float LengthSquared() const { return x * x + y * y + z * z + w * w; }
	[[nodiscard]] Vec4 Normalized() const {
		float len = Length();
		return len > 0.0f ? Vec4{x / len, y / len, z / len, w / len} : Vec4{0.0f, 0.0f, 0.0f, 0.0f};
	}

	static float Dot(const Vec4& a, const Vec4& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	}

};



using Color = Vec4;
