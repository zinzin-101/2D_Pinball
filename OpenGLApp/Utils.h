#pragma once
#include <cmath>
#include <cstdlib>

namespace Utils {
	constexpr float PI = 3.14159f;

	inline float deg2Rad(float deg) {
		return (deg * PI) / 180.0f;
	}

	inline float RandFloat() {
		return (float)rand() / (float)RAND_MAX;
	}
}
