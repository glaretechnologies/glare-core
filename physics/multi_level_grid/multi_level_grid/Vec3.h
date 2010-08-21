#pragma once


class Vec3
{
public:
	Vec3()
	{
	}


	Vec3(float x_, float y_, float z_)
	:	x(x_), y(y_), z(z_)
	{}


	~Vec3()
	{

	}

	const Vec3 operator - (const Vec3& v) const
	{
		return Vec3(x - v.x, y - v.y, z - v.z);
	}

	const Vec3 operator + (const Vec3& v) const
	{
		return Vec3(x + v.x, y + v.y, z + v.z);
	}

	const Vec3 operator * (float f) const
	{
		return Vec3(x*f, y*f, z*f);
	}

	float x, y, z;
};
