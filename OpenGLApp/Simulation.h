#pragma once

#include <glm/glm.hpp>
#include <vector>

const int NUM_OF_BALLS = 25;
const float WORLD_WIDTH = 50.0f;
const float WORLD_HEIGHT = 50.0f;
const float FIX_DT = 1.0f / 60.0f;
const glm::vec2 GRAVITY = glm::vec2(0.0f, -9.81);
bool useGravity = false;
const float RESTITUTION = 1.0f;

struct Ball {
	glm::vec2 position;
	glm::vec2 velocity;
	float mass;
	float radius;
	Ball() : position(), velocity(), mass(1.0f), radius(0.5f) {}
	void update(float dt) {
		if (useGravity) velocity += GRAVITY * dt;
		position += velocity * dt;
	}
};

void handleBallCollision(Ball& b1, Ball& b2, float restitution);
void handleWorldBorder(Ball& ball, float worldWidth, float worldHeight);
void updateBalls(std::vector<Ball>& balls, float dt);
void drawBalls(Shader& shader, std::vector<Ball>& balls);
void update(Shader& shader, std::vector<Ball>& balls, float dt);