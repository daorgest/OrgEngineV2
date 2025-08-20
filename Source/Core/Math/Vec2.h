//
// Created by Orgest on 6/10/2025.
//

#pragma once
#include <cmath>

struct Vec2
{
	float x, y;

	Vec2() = default;
	Vec2(float x, float y) : x(x), y(y) {}

	Vec2 operator+(const Vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
	Vec2 operator-(const Vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
	Vec2 operator*(const float scalar) const { return {x * scalar, y * scalar}; }
	Vec2 operator/(const float scalar) const { return {x / scalar, y / scalar}; }

	Vec2& operator+=(const Vec2& rhs) {return *this = *this + rhs;}
	Vec2& operator-=(const Vec2& rhs) {return *this = *this - rhs;}
	Vec2& operator*=(const float scalar) {return *this = *this * scalar;}
	Vec2& operator/=(const float scalar) {return *this = *this / scalar;}

	float& operator[](int i) {return *(&x + i);}
	const float& operator[](int i) const {return *(&x + i);}

	[[nodiscard]] float Length() const { return sqrtf(x * x + y * y); }
	[[nodiscard]] float LengthSq() const { return x * x + y * y; }
	[[nodiscard]] Vec2 Normalized() const {
		float len = Length();
		return len > 0.0f ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
	}
};

using Point = Vec2;
