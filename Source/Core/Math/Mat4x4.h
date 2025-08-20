#pragma once
#include <cmath>
#include <Vec3.h>

// column-major matrix
struct Mat4x4
{
	union
	{
		float m[16]{};
		float m4[4][4];
	};

	Mat4x4()
	{
		for (float& i : m)
			i = 0.0f;
	}

	Mat4x4(float scalar)
	{
		for (float& i : m) i = 0.0f;
		m[0] = m[5] = m[10] = m[15] = scalar;
	}

	float* ptr() { return m; }
	const float* ptr() const { return m; }

	Mat4x4 operator*(const Mat4x4& rhs) const
	{
		Mat4x4 result{};
		for (u32 col = 0; col < 4; col++)
		{
			for (u32 row = 0; row < 4; row++)
			{
				float sum = 0.0f;
				for (u32 k = 0; k < 4; k++)
				{
					sum += m[k * 4 + row] * rhs.m[col * 4 + k];
				}
				result.m[col * 4 + row] = sum;
			}
		}
		return result;
	}

	static Mat4x4 Identity()
	{
		return {1.0f};
	}

	static Mat4x4 RotateY(float radians)
	{
		Mat4x4 r = Identity();
		float c = std::cos(radians);
		float s = std::sin(radians);
		r.m[0] = c;
		r.m[2] = s;
		r.m[8] = -s;
		r.m[10] = c;
		return r;
	}

	static Mat4x4 RotateX(float radians)
	{
		Mat4x4 r = Identity();
		float c = std::cos(radians);
		float s = std::sin(radians);
		r.m[5] = c;
		r.m[6] = s;
		r.m[9] = -s;
		r.m[10] = c;
		return r;
	}

	static Mat4x4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
	{
		Vec3 f = (center - eye).Normalized();
		Vec3 s = f.Cross(up).Normalized();
		Vec3 u = s.Cross(f);

		Mat4x4 r = Identity();
		r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
		r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
		r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
		r.m[12] = -s.Dot(eye);
		r.m[13] = -u.Dot(eye);
		r.m[14] =  f.Dot(eye);
		return r;
	}

	static Mat4x4 Perspective(float fovyRadians, float aspect, float zNear, float zFar)
	{
		Mat4x4 r{};
		float f = 1.0f / std::tan(fovyRadians * 0.5f);
		r.m[0] = f / aspect;
		r.m[5] = f;
		r.m[10] = zFar / (zNear - zFar);
		r.m[11] = -1.0f;
		r.m[14] = (zFar * zNear) / (zNear - zFar);
		return r;
	}

	static Mat4x4 Translation(const Vec3& t)
	{
		Mat4x4 result = Identity();
		result.m[12] = t.x;
		result.m[13] = t.y;
		result.m[14] = t.z;
		return result;
	}
};
