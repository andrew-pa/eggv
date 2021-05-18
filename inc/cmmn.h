#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL 
#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <string>
#include <sstream>
#include <vector>
#include <stack>
#include <queue>
#include <map>
#include <iostream>
#include <fstream>
#include <functional>
#include <exception>
#include <algorithm>
#include <chrono>
#include <thread>
#include <memory>
#include <optional>
#include <tuple>

#include <vulkan/vulkan.hpp>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#undef min
#undef max

template<typename T, typename E>
class result {
	uint8_t type;
	union { T t; E e; };
public:
	result(T t) : t(t), type(0) {}
	result(E e) : e(e), type(1) {}

	operator T() {
		if (type) throw std::runtime_error("tried to unwrap error result");
		return t;
	}

	inline T unwrap() {
		if (type) throw std::runtime_error("tried to unwrap error result");
		return t;
	}

	inline bool ok() { return !type; }
	inline E err() { return e; }

	template<typename U>
	inline result<U, E> map(std::function<U(T)> f) {
		if (type) return result<U, E>(e);
		else return result<U, E>(f(t));
	}

	template<typename G>
	inline result<T, G> map_err(std::function<G(E)> f) {
		if (!type) return result<T, G>(t);
		else return result<T, G>(f(e));
	}
};

// a 3D ray, origin at e, direction d
struct ray {
public:
	vec3 e, d;
	ray(vec3 _e = vec3(0.f), vec3 _d = vec3(0.f))
		: e(_e), d(_d) {}

	// calculate the position along the ray at t
	inline vec3 operator ()(float t) const {
		return e + d * t;
	}

	// transform this ray by a 4x4 matrix transform
	inline void transform(const mat4& m) {
		e = (m * vec4(e, 1.f)).xyz();
		d = (m * vec4(d, 0.f)).xyz();
	}
};

// an Axis Aligned Bounding Box
struct aabb {
public:
	// min point contained by the box
	vec3 _min;
	// max point contained by box
	vec3 _max;

	aabb()
		: _min(FLT_MAX), _max(FLT_MIN)
	{}

	aabb(vec3 m, vec3 x)
		: _min(m), _max(x)
	{}

	// create an AABB that is the union of AABBs a and b
	aabb(const aabb& a, const aabb& b)
		: _min(), _max()
	{
		add_point(a._min);
		add_point(a._max);
		add_point(b._min);
		add_point(b._max);
	}

	// extend the AABB to include point p
	inline void add_point(vec3 p) {
		if (p.x > _max.x) _max.x = p.x;
		if (p.y > _max.y) _max.y = p.y;
		if (p.z > _max.z) _max.z = p.z;

		if (p.x < _min.x) _min.x = p.x;
		if (p.y < _min.y) _min.y = p.y;
		if (p.z < _min.z) _min.z = p.z;
	}

	// check to see if the AABB contains p, including on the edges
	inline bool contains(vec3 p) const {
		if (p.x >= _min.x && p.x <= _max.x &&
			p.y >= _min.y && p.y <= _max.y &&
			p.z >= _min.z && p.z <= _max.z)
			return true;
		return false;
	}

	// check to see if the AABB contains any corner of AABB b
	// this may find intersections as well as containment
	inline bool inside_of(const aabb& b) const {
		return b.contains(vec3(_min.x, _min.y, _min.z)) ||

			b.contains(vec3(_max.x, _min.y, _min.z)) ||
			b.contains(vec3(_min.x, _max.y, _min.z)) ||
			b.contains(vec3(_min.x, _min.y, _max.z)) ||

			b.contains(vec3(_min.x, _max.y, _max.z)) ||
			b.contains(vec3(_max.x, _min.y, _max.z)) ||
			b.contains(vec3(_max.x, _max.y, _min.z)) ||

			b.contains(vec3(_max.x, _max.y, _max.z));
	}

	// transform AABB by matrix to obtain new AABB
	inline aabb transform(const mat4& m) const {
		vec3 min, max;
		min = vec3(m[3][0], m[3][1], m[3][2]);
		max = min;

		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
			{
				if (m[i][j] > 0)
				{
					min[i] += m[i][j] * _min[j];
					max[i] += m[i][j] * _max[j];
				}
				else
				{
					min[i] += m[i][j] * _max[j];
					max[i] += m[i][j] * _min[j];
				}
			}
		return aabb(min, max);
	}

	// heavily optimized ray intersection function 
	inline bool hit(const ray& r) const {
		//if (contains(r.e)) return true;

		vec3 rrd = 1.f / r.d;

		vec3 t1 = (_min - r.e) * rrd;
		vec3 t2 = (_max - r.e) * rrd;

		vec3 m12 = glm::min(t1, t2);
		vec3 x12 = glm::max(t1, t2);

		float tmin = m12.x;
		tmin = glm::max(tmin, m12.y);
		tmin = glm::max(tmin, m12.z);

		float tmax = x12.x;
		tmax = glm::min(tmax, x12.y);
		tmax = glm::min(tmax, x12.z);


		return tmax >= tmin;
	}

	inline bool hit(vec3 re, vec3 rrd) const {
		vec3 t1 = (_min - re) * rrd;
		vec3 t2 = (_max - re) * rrd;

		vec3 m12 = glm::min(t1, t2);
		vec3 x12 = glm::max(t1, t2);

		float tmin = m12.x;
		tmin = glm::max(tmin, m12.y);
		tmin = glm::max(tmin, m12.z);

		float tmax = x12.x;
		tmax = glm::min(tmax, x12.y);
		tmax = glm::min(tmax, x12.z);


		return tmax >= tmin;
	}


	// ray intersection function that returns the interval on the ray that is inside the AABB
	inline std::tuple<float, float> hit_retint(const ray& r) const {
		vec3 rrd = 1.f / r.d;

		vec3 t1 = (_min - r.e) * rrd;
		vec3 t2 = (_max - r.e) * rrd;

		vec3 m12 = glm::min(t1, t2);
		vec3 x12 = glm::max(t1, t2);

		float tmin = m12.x;
		tmin = glm::max(tmin, m12.y);
		tmin = glm::max(tmin, m12.z);

		float tmax = x12.x;
		tmax = glm::min(tmax, x12.y);
		tmax = glm::min(tmax, x12.z);


		return {tmin, tmax};
	}

	inline vec3 center() const {
		return (_min + _max) * 1.f / 2.f;
	}
	inline vec3 extents() const {
		return _max - _min;
	}

	inline float surface_area() const {
		vec3 d = _max - _min;
		return 2.f * (d.x * d.y + d.x * d.z + d.y * d.z);
	}

	inline void add_aabb(const aabb& b)
	{
		add_point(b._min);
		add_point(b._max);
	}
};

inline float hit_sphere(const ray& r, vec3 center, float radius2) {
	vec3 v = r.e - center;
	float b = -dot(v, r.d);
	float det = (b * b) - dot(v, v) + radius2;
	if (det < 0.0f) return std::numeric_limits<float>::max();
	det = sqrt(det);
	return min(b - det, b + det);
}

inline json serialize(vec3 v) {
	return { v.x, v.y, v.z };
}

inline json serialize(vec4 v) {
	return { v.x, v.y, v.z };
}

inline json serialize(mat4 v) {
	auto r = json::array();
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++i)
			r[i * 4 + j] = v[i][j];
	return r;
}

inline vec3 deserialize_v3(const json& j) {
	return vec3(j.at(0), j.at(1), j.at(2));
}
inline vec4 deserialize_v4(const json& j) {
	return vec4(j.at(0), j.at(1), j.at(2), j.at(4));
}

enum class viewport_shape_type {
    box, axis, line, arrow
};

struct viewport_shape {
        viewport_shape_type type;
	vec3 pos[2];
	vec3 color;
        mat4 T;

        viewport_shape(viewport_shape_type ty = viewport_shape_type::arrow, vec3 a = vec3(0.), vec3 b = vec3(0.), vec3 col = vec3(1.), mat4 T = mat4(0))
            : type(ty), color(col), T(T)
        {
            pos[0] = a;
            pos[1] = b;
        }
};

/*template<typename T>
T generate_collect(size_t count, std::function<T::value_type(size_t)> f) {
	auto c = T();
	generate_n(c.begin(), count, f);
	return c;
}*/
