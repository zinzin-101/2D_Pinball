#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <shader.h>

#include "Utils.h"

#include <iostream>
#include <thread>
#include <map>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 800;

// glfw
GLFWwindow* window = nullptr;

// vertex data
GLuint circleVAO, circleVBO, circleEBO;
const unsigned int CIRCLE_VERTS_NUM = 362;
float circleVerts[CIRCLE_VERTS_NUM * 3];
unsigned int circleIndices[CIRCLE_VERTS_NUM];
float* initVertexData();
unsigned int* initIndicesData();
void initGLData();

// simulation
const float WORLD_WIDTH = 50.0f;
const float WORLD_HEIGHT = 50.0f;
const float FIX_DT = 1.0f / 60.0f;
const glm::vec2 GRAVITY = glm::vec2(0.0f, -9.81);
const float RESTITUTION = 1.0f;
const float FLIPPER_HEIGHT = 1.7f;

struct Circle {
	glm::vec2 position;
	float radius;
	Circle(glm::vec2 position, float radius): position(position), radius(radius) {}
};

struct Ball : Circle {
	glm::vec2 velocity;
	float mass;
	Ball(): Circle(glm::vec2(), 0.5f), velocity(), mass(1.0f) {}
	void update(float dt) {
		velocity += GRAVITY * dt;
		position += velocity * dt;
	}
};

struct Obstacle : Circle {
	float pushAmount;
	Obstacle(): Circle(glm::vec2(), 0.5f), pushAmount(2.0f) {}
	Obstacle(glm::vec2 position, float radius, float pushAmount = 2.0f): Circle(position, radius), pushAmount(pushAmount) {}
};

struct Flipper {
	int id;

	glm::vec2 position;
	float radius;
	float length;
	float restAngle;
	float maxRotation;
	bool isSignPositive;
	float angularVelocity;
	float restitution;

	float currentRotation;
	float currentAngularVelocity;
	bool isFlipped;

	Flipper(glm::vec2 position, float radius, float length, float restAngle, float maxRotation, float angularVelocity, float restitution) :
		id(-1),
		position(position), radius(radius), length(length), restAngle(restAngle), maxRotation(maxRotation), isSignPositive(maxRotation >= 0.0f),
		angularVelocity(angularVelocity), restitution(restitution),
		currentRotation(0.0f), currentAngularVelocity(0.0f), isFlipped(false) {}

	void update(float dt) {
		float prevRotation = currentRotation;
		if (isFlipped) currentRotation = glm::min(currentRotation + angularVelocity * dt, maxRotation);
		else currentRotation = glm::max(currentRotation - angularVelocity * dt, 0.0f);
	}

	glm::vec2 getFlipperEnd() {
		float angle = restAngle + (isSignPositive ? 1.0f : -1.0f) * currentRotation;
		glm::vec2 dir = glm::vec2(glm::cos(angle), glm::sin(angle));
		return position + dir * length;
	}
};

void handleBallCollision(Ball& b1, Ball& b2, float restitution);
void handleBallObstacleCollision(Ball& ball, Obstacle& obstacle);
void handleBallFlipperCollision(Ball& ball, Flipper& flipper);
void handleBallBorderCollision(Ball& ball, std::vector<glm::vec2>& borderPoints);
void updateSimulation(std::vector<Ball>& balls, std::vector<Obstacle>& obstacles, std::vector<Flipper>& flippers, std::vector<glm::vec2>& borderPoints, float dt);

// rendering
void drawCircle(Shader& shader, glm::vec3 position, float radius);
void renderBalls(Shader& shader, std::vector<Ball>& balls);
void renderObstacles(Shader& shader, std::vector<Obstacle>& obstacles);
void renderFlippers(Shader& shader, std::vector<Flipper>& flippers);
void renderBorder(Shader& shader, std::vector<glm::vec2>& borderPoints);

// controls
std::map<unsigned, bool> keyDownMap;
bool getKeyDown(GLFWwindow* window, unsigned int key);

int main() {

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "2D_Billiard", NULL, NULL);

	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	srand(time(NULL));

	// init gl
	Shader shader("circle.vs", "circle.fs");
	initGLData();

	// init simulation
	float offset = 0.02f;

	std::vector<glm::vec2> borderPoints;
	borderPoints.push_back(glm::vec2(0.74f, 0.25f));
	borderPoints.push_back(glm::vec2(1.0f - offset, 0.4f));
	borderPoints.push_back(glm::vec2(1.0f - offset, FLIPPER_HEIGHT - offset));
	borderPoints.push_back(glm::vec2(offset, FLIPPER_HEIGHT - offset));
	borderPoints.push_back(glm::vec2(offset, 0.4f));
	borderPoints.push_back(glm::vec2(0.26f, 0.25f));
	borderPoints.push_back(glm::vec2(0.26f, 0.0f));
	borderPoints.push_back(glm::vec2(0.74f, 0.0f));

	std::vector<Ball> balls;


	while (!glfwWindowShouldClose(window)) {
		processInput(window);

		updateSimulation();

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		renderBalls();
		renderFlippers()
		renderObstacles();
		renderBorder();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	return 0; 
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {

}

void processInput(GLFWwindow* window) {
	if (getKeyDown(window, GLFW_KEY_ESCAPE)) {
		glfwSetWindowShouldClose(window, true);
	}
}

float* initVertexData() {
	circleVerts[0] = 0.0f;
	circleVerts[1] = 0.0f;
	circleVerts[2] = 0.0f;
	for (int i = 1; i < CIRCLE_VERTS_NUM; i++) {
		circleVerts[i * 3] = cos(Utils::deg2Rad(i));
		circleVerts[i * 3 + 1] = sin(Utils::deg2Rad(i));;
		circleVerts[i * 3 + 2] = 0.0f;
	}

	return circleVerts;
}

unsigned int* initIndicesData() {
	for (int i = 0; i < CIRCLE_VERTS_NUM; i++) {
		circleIndices[i] = i;
	}

	return circleIndices;
}

void initGLData() {
	initVertexData();
	initIndicesData();

	glGenVertexArrays(1, &circleVAO);
	glBindVertexArray(circleVAO);

	glGenBuffers(1, &circleVBO);
	glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * CIRCLE_VERTS_NUM * 3, circleVerts, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &circleEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, circleEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * CIRCLE_VERTS_NUM, circleIndices, GL_STATIC_DRAW);
}

void drawCircle(Shader& shader, glm::vec3 position, float radius) {
	shader.use();
	glm::mat4 projection = glm::ortho(
		-(WORLD_WIDTH / 2.0f), (WORLD_WIDTH / 2.0f),
		-(WORLD_HEIGHT / 2.0f), (WORLD_HEIGHT / 2.0f),
		-1.0f, 1.0f
	);
	shader.setMat4("projection", projection);
	shader.setFloat("scale", radius);
	shader.setVec3("position", position);
	shader.setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));

	glBindVertexArray(circleVAO);
	glDrawElements(GL_TRIANGLE_FAN, CIRCLE_VERTS_NUM, GL_UNSIGNED_INT, 0);
}

void renderBalls(Shader& shader, std::vector<Ball>& balls) {
	for (const Ball& ball : balls) {
		drawCircle(shader, glm::vec3(ball.position, 0.0f), ball.radius);
	}
}

void renderObstacles(Shader& shader, std::vector<Obstacle>& obstacles) {
	for (const Obstacle& obstacle : obstacles) {
		drawCircle(shader, glm::vec3(obstacle.position, 0.0f), obstacle.radius);
	}
}
void renderFlippers(Shader& shader, std::vector<Flipper>& flippers) {

}
void renderBorder(Shader& shader, std::vector<glm::vec2>& borderPoints) {

}

void handleBallCollision(Ball& b1, Ball& b2, float restitution) {
	glm::vec2 dir = b2.position - b1.position;
	float distance = glm::length(dir);
	if (distance <= 0.0001f || distance > b1.radius + b2.radius) return;

	dir = glm::normalize(dir);

	float correction = (b1.radius + b2.radius - distance) / 2.0f;
	b1.position += dir * -correction;
	b2.position += dir * correction;

	float v1 = glm::dot(b1.velocity, dir);
	float v2 = glm::dot(b2.velocity, dir);

	float m1 = b1.mass;
	float m2 = b2.mass;

	float newV1 = (m1 * v1 + m2 * v2 - m2 * (v1 - v2) * restitution) / (m1 + m2);
	float newV2 = (m1 * v1 + m2 * v2 - m1 * (v2 - v1) * restitution) / (m1 + m2);

	b1.velocity += dir * (newV1 - v1);
	b2.velocity += dir * (newV2 - v2);
}

void handleBallObstacleCollision(Ball& ball, Obstacle& obstacle) {
	glm::vec2 dir = ball.position - obstacle.position;
	float distance = glm::length(dir);
	if (distance == 0.0f || distance > ball.radius + obstacle.radius) return;

	dir = glm::normalize(dir);

	float correction = ball.radius + obstacle.radius - distance;
	ball.position += dir * correction;

	float v = glm::dot(ball.velocity, dir);
	ball.velocity += dir * (obstacle.pushAmount - v);
}

void handleBallFlipperCollision(Ball& ball, Flipper& flipper) {
	glm::vec2 closest = Utils::getClosestPointOnSegment(ball.position, flipper.position, flipper.getFlipperEnd());
	glm::vec2 dir = ball.position - closest;
	float distance = glm::length(dir);
	if (distance == 0.0f || distance > ball.radius + flipper.radius) return;

	dir = glm::normalize(dir);

	float correction = ball.radius + flipper.radius - distance;
	ball.position += dir * correction;

	glm::vec2 r = closest;
	r += dir * flipper.radius;
	r -= flipper.position;
	glm::vec2 surfaceVelocity = Utils::getPerpendicular(r);
	surfaceVelocity *= flipper.currentAngularVelocity;

	float v = glm::dot(ball.velocity, dir);
	float newV = glm::dot(surfaceVelocity, dir);

	ball.velocity += dir * (newV - v);
}

void handleBallBorderCollision(Ball& ball, std::vector<glm::vec2>& borderPoints) {
	if (borderPoints.size() < 3) return;

	glm::vec2 d, closest, ab, normal;
	float minDist = 0.0f;
	int n = borderPoints.size();
	for (int i = 0; i < n; i++) {
		glm::vec2 a = borderPoints.at(i);
		glm::vec2 b = borderPoints.at((i + 1) % n);
		glm::vec2 c = Utils::getClosestPointOnSegment(ball.position, a, b);
		d = ball.position - c;
		float distance = glm::length(d);
		if (i == 0 || distance < minDist) {
			minDist = distance;
			closest = c;
			ab = b - a;
			normal = Utils::getPerpendicular(ab);
		}
	}

	d = ball.position - closest;
	float distance = glm::length(d);
	if (distance == 0.0f) {
		d = normal;
		distance = glm::length(normal);
	}
	d = glm::normalize(d);

	if (glm::dot(d, normal) >= 0.0f) {
		if (distance > ball.radius) return;

		ball.position += d * (ball.radius - distance);
	}
	else {
		ball.position += d * (distance + ball.radius);
	}

	float v = glm::dot(ball.velocity, d);
	float newV = glm::abs(v) * RESTITUTION;

	ball.velocity += d * (newV - v);
}

void handleWorldBorder(Ball& ball, float worldWidth, float worldHeight) {
	if (ball.position.x < -(worldWidth / 2.0f) + ball.radius) {
		ball.position.x = -(worldWidth / 2.0f) + ball.radius;
		ball.velocity.x = -ball.velocity.x;
	}
	if (ball.position.x > (worldWidth / 2.0f) - ball.radius) {
		ball.position.x = (worldWidth / 2.0f) - ball.radius;
		ball.velocity.x = -ball.velocity.x;
	}
	if (ball.position.y < -(worldHeight / 2.0f) + ball.radius) {
		ball.position.y = -(worldHeight / 2.0f) + ball.radius;
		ball.velocity.y = -ball.velocity.y;
	}

	if (ball.position.y > (worldHeight / 2.0f) - ball.radius) {
		ball.position.y = (worldHeight / 2.0f) - ball.radius;
		ball.velocity.y = -ball.velocity.y;
	}
}

void updateSimulation(std::vector<Ball>& balls, std::vector<Obstacle>& obstacles, std::vector<Flipper>& flippers, std::vector<glm::vec2>& borderPoints, float dt) {
	for (Flipper& flipper : flippers) {
		flipper.update(dt);
	}

	int n = balls.size();
	for (int i = 0; i < n; i++) {
		Ball& ball = balls[i];
		ball.update(dt);

		for (int j = i + 1; j < n; j++) {
			Ball& otherBall = balls[j];
			handleBallCollision(ball, otherBall, RESTITUTION);
		}

		for (Obstacle& obstacle : obstacles)
			handleBallObstacleCollision(ball, obstacle);

		for (Flipper& flipper : flippers)
			handleBallFlipperCollision(ball, flipper);

		handleBallBorderCollision(ball, borderPoints);
	}
}

bool getKeyDown(GLFWwindow* window, unsigned int key) {
	// init
	if (keyDownMap.count(key) == 0) {
		keyDownMap[key] = false;
		return false;
	}

	if (glfwGetKey(window, key) == GLFW_PRESS && keyDownMap.at(key)) {
		return false;
	}

	if (glfwGetKey(window, key) == GLFW_RELEASE && keyDownMap.at(key)) {
		keyDownMap[key] = false;
		return false;
	}

	if (glfwGetKey(window, key) == GLFW_PRESS && !keyDownMap.at(key)) {
		keyDownMap[key] = true;
		return true;
	}
}