#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <shader.h>

#include "Utils.h"

#include <iostream>
#include <thread>
#include <map>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
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
float* initCircleVertexData();
unsigned int* initCircleIndicesData();
void initCircleData();

GLuint squareVAO, squareVBO, squareEBO;
float squareVerts[4 * 3];
unsigned int squareIndices[6];
float* initSquareVertexData();
unsigned int* initSquareIndicesData();
void initSquareData();

void initGLData();

// simulation
const float WORLD_WIDTH = 100.0f;
const float WORLD_HEIGHT = 100.0f;
const float FIX_DT = 1.0f / 60.0f;
const glm::vec2 GRAVITY = glm::vec2(0.0f, -9.81);
const float RESTITUTION = 0.2f;
const float FLIPPER_HEIGHT = 1.7f;
const float BORDER_SIZE = 0.5f;

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

	Flipper(glm::vec2 position, float radius, float length, float restAngle, float maxRotation, float angularVelocity, float restitution, bool positiveSign = true) :
		id(-1),
		position(position), radius(radius), length(length), restAngle(restAngle), maxRotation(maxRotation), isSignPositive(positiveSign),
		angularVelocity(angularVelocity), restitution(restitution),
		currentRotation(0.0f), currentAngularVelocity(0.0f), isFlipped(false) {}

	void update(float dt) {
		float prevRotation = currentRotation;
		if (isFlipped) currentRotation = glm::min(currentRotation + angularVelocity * dt, maxRotation);
		else currentRotation = glm::max(currentRotation - angularVelocity * dt, 0.0f);
		currentAngularVelocity = (isSignPositive ? 1.0f : -1.0f) * (currentRotation - prevRotation) / dt;
	}

	glm::vec2 getFlipperEnd() const {
		float angle = restAngle + (isSignPositive ? 1.0f : -1.0f) * currentRotation;
		glm::vec2 dir = glm::vec2(glm::cos(angle), glm::sin(angle));
		return position + dir * length;
	}
};

std::vector<glm::vec2> borderPoints;
std::vector<Ball> balls;
std::vector<Obstacle> obstacles;
std::vector<Flipper> flippers;

void resetScene();
void handleBallCollision(Ball& b1, Ball& b2, float restitution);
void handleBallObstacleCollision(Ball& ball, Obstacle& obstacle);
void handleBallFlipperCollision(Ball& ball, Flipper& flipper);
void handleBallBorderCollision(Ball& ball, std::vector<glm::vec2>& borderPoints);
void updateSimulation(std::vector<Ball>& balls, std::vector<Obstacle>& obstacles, std::vector<Flipper>& flippers, std::vector<glm::vec2>& borderPoints, float dt);

// rendering
void drawCircle(Shader& shader, glm::vec3 position, float radius, glm::vec3 color);
void drawSquareLine(Shader& shader, glm::vec3 startPos, glm::vec3 endPos, float radius, glm::vec3 color);
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
	glfwSetMouseButtonCallback(window, mouse_button_callback);

	srand(time(NULL));

	// init gl
	Shader circleShader("circle.vs", "circle.fs");
	Shader squareShader("square.vs", "square.fs");
	initGLData();

	resetScene();

	while (!glfwWindowShouldClose(window)) {
		processInput(window);

		updateSimulation(balls, obstacles, flippers, borderPoints, FIX_DT);

		// render
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		renderBalls(circleShader, balls);
		renderObstacles(circleShader, obstacles);
		renderFlippers(squareShader, flippers);
		renderBorder(squareShader, borderPoints);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	return 0; 
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {

}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (flippers.empty()) return;

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) flippers[0].isFlipped = true;
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) flippers[0].isFlipped = false;

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) flippers[1].isFlipped = true;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) flippers[1].isFlipped = false;
}

void processInput(GLFWwindow* window) {
	if (getKeyDown(window, GLFW_KEY_ESCAPE)) {
		glfwSetWindowShouldClose(window, true);
	}

	if (getKeyDown(window, GLFW_KEY_R)) {
		resetScene();
	}
}

float* initCircleVertexData() {
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

unsigned int* initCircleIndicesData() {
	for (int i = 0; i < CIRCLE_VERTS_NUM; i++) {
		circleIndices[i] = i;
	}

	return circleIndices;
}

float* initSquareVertexData(){
	squareVerts[0] = 0.0f;
	squareVerts[1] = -0.5f;
	squareVerts[2] = 0.0f;

	squareVerts[3] = 1.0f;
	squareVerts[4] = -0.5f;
	squareVerts[5] = 0.0f;

	squareVerts[6] = 1.0f;
	squareVerts[7] = 0.5f;
	squareVerts[8] = 0.0f;

	squareVerts[9] = 0.0f;
	squareVerts[10] = 0.5;
	squareVerts[11] = 0.0f;
	
	return squareVerts;
}

unsigned int* initSquareIndicesData() {
	squareIndices[0] = 0;
	squareIndices[1] = 1;
	squareIndices[2] = 2;
	squareIndices[3] = 0;
	squareIndices[4] = 2;
	squareIndices[5] = 3;

	return squareIndices;
}

void initCircleData() {
	initCircleVertexData();
	initCircleIndicesData();

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

void initSquareData() {
	initSquareVertexData();
	initSquareIndicesData();

	glGenVertexArrays(1, &squareVAO);
	glBindVertexArray(squareVAO);

	glGenBuffers(1, &squareVBO);
	glBindBuffer(GL_ARRAY_BUFFER, squareVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 3, squareVerts, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &squareEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, squareEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 6, squareIndices, GL_STATIC_DRAW);
}

void initGLData() {
	initCircleData();
	initSquareData();
}

void drawCircle(Shader& shader, glm::vec3 position, float radius, glm::vec3 color) {
	shader.use();
	glm::mat4 projection = glm::ortho(
		-(WORLD_WIDTH / 2.0f), (WORLD_WIDTH / 2.0f),
		-(WORLD_HEIGHT / 2.0f), (WORLD_HEIGHT / 2.0f),
		-1.0f, 1.0f
	);
	shader.setMat4("projection", projection);
	shader.setFloat("scale", radius);
	shader.setVec3("position", position);
	shader.setVec3("color", color);

	glBindVertexArray(circleVAO);
	glDrawElements(GL_TRIANGLE_FAN, CIRCLE_VERTS_NUM, GL_UNSIGNED_INT, 0);
}

void drawSquareLine(Shader& shader, glm::vec3 startPos, glm::vec3 endPos, float radius, glm::vec3 color) {
	shader.use();
	glm::mat4 projection = glm::ortho(
		-(WORLD_WIDTH / 2.0f), (WORLD_WIDTH / 2.0f),
		-(WORLD_HEIGHT / 2.0f), (WORLD_HEIGHT / 2.0f),
		-1.0f, 1.0f
	);

	glm::mat4 view(1.0f);
	glm::vec2 startToEnd = endPos - startPos;
	float length = glm::length(startToEnd);
	glm::vec2 dir = glm::normalize(startToEnd);
	float angle = glm::atan(dir.y, dir.x);
	glm::mat4 model = glm::translate(glm::mat4(1.0f), startPos)	* 
					  glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f)) *
					  glm::scale(glm::mat4(1.0f), glm::vec3(length, radius, 0.0f)) * glm::mat4(1.0f);
	shader.setMat4("projection", projection);
	shader.setMat4("view", view);
	shader.setMat4("model", model);
	shader.setVec3("color", color);

	glBindVertexArray(squareVAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void renderBalls(Shader& shader, std::vector<Ball>& balls) {
	for (const Ball& ball : balls) {
		drawCircle(shader, glm::vec3(ball.position, 0.0f), ball.radius, glm::vec3(1.0f));
	}
}

void renderObstacles(Shader& shader, std::vector<Obstacle>& obstacles) {
	for (const Obstacle& obstacle : obstacles) {
		drawCircle(shader, glm::vec3(obstacle.position, 0.0f), obstacle.radius, glm::vec3(1.0f, 1.0f, 0.0f));
	}
}
void renderFlippers(Shader& shader, std::vector<Flipper>& flippers) {
	for (const Flipper& flipper : flippers) {
		glm::vec3 startPos = glm::vec3(flipper.position, 0.0f);
		glm::vec3 endPos = glm::vec3(flipper.getFlipperEnd(), 0.0f);
		drawSquareLine(shader, startPos, endPos, flipper.radius, glm::vec3(1.0f, 0.0f, 0.0f));
	}
}
void renderBorder(Shader& shader, std::vector<glm::vec2>& borderPoints) {
	int n = borderPoints.size();
	for (int i = 0; i < n; i++) {
		glm::vec3 startPos = glm::vec3(borderPoints[i], 0.0f);
		glm::vec3 endPos = glm::vec3(borderPoints[(i + 1) % n], 0.0f);
		drawSquareLine(shader, startPos, endPos, BORDER_SIZE, glm::vec3(1.0f, 1.0f, 1.0f));
	}
}

void resetScene() {
	borderPoints.clear();
	balls.clear();
	flippers.clear();
	obstacles.clear();

	// init simulation
	borderPoints.push_back(glm::vec2(-25.0f, 25.0f));
	borderPoints.push_back(glm::vec2(-25.0f, -25.0f));
	borderPoints.push_back(glm::vec2(-15.0f, -30.0f));
	borderPoints.push_back(glm::vec2(-15.0f, -40.0f));
	borderPoints.push_back(glm::vec2(15.0f, -40.0f));
	borderPoints.push_back(glm::vec2(15.0f, -30.0f));
	borderPoints.push_back(glm::vec2(25.0f, -25.0f));
	borderPoints.push_back(glm::vec2(25.0f, 25.0f));

	Ball ball;
	ball.radius = 1.0f;
	ball.mass = Utils::PI * ball.radius * ball.radius;
	ball.position = glm::vec2(15.0f, 10.0f);
	ball.velocity = glm::vec2(0.0f, 0.0f);
	balls.push_back(ball);
	ball.position = glm::vec2(-15.0f, 10.0);
	ball.velocity = glm::vec2(0.2f, 0.0f);
	balls.push_back(ball);
	balls.push_back(ball);
	balls.push_back(ball);

	obstacles.push_back(Obstacle(glm::vec2(10.0f, 10.0f), 3.0f));
	obstacles.push_back(Obstacle(glm::vec2(-12.0f, -8.0f), 5.0f));
	obstacles.push_back(Obstacle(glm::vec2(4.0f, -6.0f), 6.2f));
	obstacles.push_back(Obstacle(glm::vec2(-5.0f, 4.2f), 4.0f));

	float radius = 1.0f;
	float length = 12.5f;
	float maxRotation = Utils::deg2Rad(45.0f);
	float restAngle = Utils::deg2Rad(0.0f);
	float angularVelocity = 10.0f;
	float restitution = 0.0f;
	glm::vec2 pos1 = glm::vec2(-15.0f, -32.0f);
	glm::vec2 pos2 = glm::vec2(15.0f, -32.0f);
	flippers.push_back(Flipper(pos1, radius, length, -restAngle, maxRotation, angularVelocity, restitution));
	flippers.push_back(Flipper(pos2, radius, length, Utils::PI + restAngle, maxRotation, angularVelocity, restitution, false));
	flippers[0].id = 0;
	flippers[1].id = 1;
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
		glm::vec2 a = borderPoints[i];
		glm::vec2 b = borderPoints[(i + 1) % n];
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
		if (distance > ball.radius ) return;

		ball.position += d * (ball.radius - distance);
	}
	else {
		ball.position += d * -(distance + ball.radius);
	}

	float v = glm::dot(ball.velocity, d);
	float newV = glm::abs(v) * RESTITUTION;

	ball.velocity += d * (newV - v);
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