#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
GLuint vao, vbo, ebo;
const unsigned int CIRCLE_VERTS_NUM = 362;
float circleVerts[CIRCLE_VERTS_NUM * 3];
unsigned int circleIndices[CIRCLE_VERTS_NUM];
float* initVertexData();
unsigned int* initIndicesData();
void initGLData();

// simuation
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
	Ball(): position(), velocity(), mass(1.0f), radius(0.5f) {}
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

// rendering
void drawCircle(Shader& shader, glm::vec3 position, float radius);
void render(Shader& shader, std::vector<Ball>& balls);

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
	Shader shader("billiard.vs", "billiard.fs");
	initGLData();

	std::vector<Ball> balls;
	for (int i = 0; i < NUM_OF_BALLS; i++) {
		float mass = Utils::RandFloat() * 4.0f + 1.0f;
		float radius = (mass / 5.0f) * 2.0f;
		glm::vec2 velocity(
			Utils::RandFloat() * 50.0f - 25.0f,
			Utils::RandFloat() * 50.0f - 25.0f
		);

		glm::vec2 position(
			Utils::RandFloat() * (2.0f * (WORLD_WIDTH * 0.9f)) - (WORLD_WIDTH * 0.9f),
			Utils::RandFloat() * (2.0f * (WORLD_HEIGHT * 0.9f)) - (WORLD_HEIGHT * 0.9f)
		);

		Ball ball;
		ball.mass = mass;
		ball.radius = radius;
		ball.velocity = velocity;
		//ball.velocity = glm::vec2();
		ball.position = position;
		balls.emplace_back(ball);
	}

	// main loop
	while (!glfwWindowShouldClose(window)) {
		processInput(window);
		update(shader, balls, FIX_DT);
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

	if (getKeyDown(window, GLFW_KEY_G)) {
		useGravity = !useGravity;
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

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * CIRCLE_VERTS_NUM * 3, circleVerts, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
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

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLE_FAN, CIRCLE_VERTS_NUM, GL_UNSIGNED_INT, 0);
}

void render(Shader& shader, std::vector<Ball>& balls) {
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	drawBalls(shader, balls);
	//drawCircle(shader, glm::vec3(0, 0, 0), 1.0f);
	//drawCircle(shader, glm::vec3(-2, 0, 0), RENDER_SCALE);
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

void updateBalls(std::vector<Ball>& balls, float dt) {
	int n = balls.size();
	for (int i = 0; i < n; i++) {
		Ball& b1 = balls[i];
		b1.update(dt);

		for (int j = i + 1; j < n; j++) {
			Ball& b2 = balls[j];
			handleBallCollision(b1, b2, RESTITUTION);
		}

		handleWorldBorder(b1, WORLD_WIDTH, WORLD_HEIGHT);
	}
}

void drawBalls(Shader& shader, std::vector<Ball>& balls) {
	for (const Ball& ball : balls) {
		drawCircle(shader, glm::vec3(ball.position, 0.0f), ball.radius);
	}
}

void update(Shader& shader, std::vector<Ball>& balls, float dt) {
	updateBalls(balls, dt);
	render(shader, balls);
	//drawBalls(shader, balls);
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