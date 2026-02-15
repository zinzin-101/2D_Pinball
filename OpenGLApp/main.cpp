#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <shader.h>
#include <filesystem.h>

#include "Utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <thread>
#include <map>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window);

// settings
const unsigned int WIDTH = 1600;
const unsigned int HEIGHT = 900;

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
GLuint squareOutlineVAO, squareOutlineVBO, squareOutlineEBO;
float squareVerts[4 * 3];
unsigned int squareIndices[6];
unsigned int outlineIndices[5];
float* initSquareVertexData();
unsigned int* initSquareIndicesData();
void initSquareData();

void initGLData();

// simulation
const float WORLD_WIDTH = 151.1f;
const float WORLD_HEIGHT = 85.0f;
const float FIX_DT = 1.0f / 60.0f;
float deltaTime = 0.0f;
float lastTime  = 0.0f;
const glm::vec2 GRAVITY = glm::vec2(0.0f, -9.81) * 10.0f;
const float RESTITUTION = 0.2f;
const float FLIPPER_HEIGHT = 1.7f;
const float BORDER_SIZE = 2.5f;

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
void updateSimulation(float dt);

// rendering
void drawCircle(Shader& shader, glm::vec3 position, float radius, glm::vec3 color);
void drawSquareLine(Shader& shader, glm::vec3 startPos, glm::vec3 endPos, float radius, glm::vec3 color);
void renderBalls(Shader& shader);
void renderObstacles(Shader& shader);
void renderFlippers(Shader& shader);
void renderBorder(Shader& shader);

// debugging
#define DRAW_DEBUG
void drawSquareOutline(Shader& shader, glm::vec3 startPos, glm::vec3 endPos, float radius);
void drawCircleOutline(Shader& shader, glm::vec3 position, float radius);

// visual
struct Texture {
	GLuint id;
	unsigned int width, height;
	unsigned int internalFormat;
	unsigned int imageFormat;
	unsigned int wrapS;
	unsigned int wrapT;
	unsigned int filterMin;
	unsigned int filterMax;
	Texture() : width(0), height(0), internalFormat(GL_RGB), imageFormat(GL_RGB), wrapS(GL_REPEAT), wrapT(GL_REPEAT), filterMin(GL_NEAREST), filterMax(GL_NEAREST) {
		glGenTextures(1, &this->id);
	}

	void generate(unsigned int width, unsigned int height, unsigned char* data) {
		this->width = width;
		this->height = height;
		// create Texture
		glBindTexture(GL_TEXTURE_2D, id);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, imageFormat, GL_UNSIGNED_BYTE, data);
		// set Texture wrap and filter modes
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterMin);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterMax);

		// unbind texture
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void bind() {
		glBindTexture(GL_TEXTURE_2D, id);
	}
};

struct Sprite {
	Shader* shader;
	Texture texture;
	GLuint quadVAO;
	bool isFlipped;
	glm::vec3 offset;
	Sprite(Shader& shader, Texture texture): shader(&shader), texture(texture), isFlipped(false), offset(0.0f) {
		initRenderData();
	}

	virtual ~Sprite() {
		glDeleteVertexArrays(1, &this->quadVAO);
	}

	virtual void drawSprite(glm::vec3 position, glm::vec3 size, float rotation, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), bool isRadian = false) {
		shader->use();
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(position));
		model = glm::translate(model, glm::vec3(offset));

		model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));
		float angle = isRadian ? rotation : glm::radians(rotation);
		model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		//model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));

		model = glm::scale(model, glm::vec3(isFlipped ? -1.0f : 1.0f, 1.0f, 1.0f));
		model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));

		glm::mat4 projection = glm::ortho(
			-(WORLD_WIDTH / 2.0f), (WORLD_WIDTH / 2.0f),
			-(WORLD_HEIGHT / 2.0f), (WORLD_HEIGHT / 2.0f),
			-1.0f, 1.0f
		);


		shader->setMat4("model", model);
		shader->setMat4("projection", projection);
		shader->setBool("enableTiling", false);
		shader->setVec3("color", color);

		glActiveTexture(GL_TEXTURE0);
		texture.bind();

		glBindVertexArray(this->quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	void initRenderData() {
		unsigned int vbo;
		float vertices[] = {
			// pos      // tex
			0.0f, 1.0f, 0.0f, 1.0f,
			1.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,

			0.0f, 1.0f, 0.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 0.0f, 1.0f, 0.0f
		};

		glGenVertexArrays(1, &this->quadVAO);
		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindVertexArray(this->quadVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
};

struct SquareLineSprite : Sprite {
	bool useTiling;
	float spriteScale;
	SquareLineSprite(Shader& shader, Texture texture): Sprite(shader, texture), useTiling(false), spriteScale(1.0f) {}
	virtual void drawSprite(glm::vec3 position, glm::vec3 size, float rotation, glm::vec3 color, bool isRadian = false) override {
		this->shader->use();
		glm::mat4 model = glm::mat4(1.0f);

		model = glm::translate(model, glm::vec3(position));
		model = glm::translate(model, glm::vec3(offset));
		float angle = isRadian ? rotation : glm::radians(rotation);
		model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::translate(model, glm::vec3(0.0f, -0.5f * size.y, 0.0f));
		model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));
		model = glm::translate(model, glm::vec3(isFlipped ? 1.0f: 0.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(isFlipped ? -1.0f : 1.0f, 1.0f, 1.0f));


		glm::mat4 projection = glm::ortho(
			-(WORLD_WIDTH / 2.0f), (WORLD_WIDTH / 2.0f),
			-(WORLD_HEIGHT / 2.0f), (WORLD_HEIGHT / 2.0f),
			-1.0f, 1.0f
		);


		shader->setMat4("model", model);
		shader->setMat4("projection", projection);
		shader->setBool("enableTiling", useTiling);
		shader->setVec2("tiling", size.x * spriteScale, size.y * spriteScale);
		shader->setVec3("color", color);

		glActiveTexture(GL_TEXTURE0);
		texture.bind();

		glBindVertexArray(this->quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}
};

struct AnimatedSprite {
	Sprite* sprite;
	Shader* shader;
	unsigned int frameCount;
	unsigned int currentFrame;
	float timePerFrame;
	float timer;
	glm::vec2 animationOffset;
	AnimatedSprite(Shader& shader, Sprite& sprite) : shader(&shader), sprite(&sprite), frameCount(0), currentFrame(0), timePerFrame(0.0f), timer(0.0f), animationOffset(0.0f) {}
	AnimatedSprite():shader(nullptr), sprite(nullptr), frameCount(0), currentFrame(0), timePerFrame(0.0f), timer(0.0f), animationOffset(0.0f) {}

	void drawSprite(glm::vec3 position, glm::vec3 size, float rotation, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), bool isRadian = false) {
		shader->use();
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(position));
		model = glm::translate(model, glm::vec3(sprite->offset));

		model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));
		float angle = isRadian ? rotation : glm::radians(rotation);
		model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		//model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));

		model = glm::scale(model, glm::vec3(sprite->isFlipped ? -1.0f : 1.0f, 1.0f, 1.0f));
		model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));

		glm::mat4 projection = glm::ortho(
			-(WORLD_WIDTH / 2.0f), (WORLD_WIDTH / 2.0f),
			-(WORLD_HEIGHT / 2.0f), (WORLD_HEIGHT / 2.0f),
			-1.0f, 1.0f
		);

		shader->setMat4("model", model);
		shader->setMat4("projection", projection);
		shader->setVec3("color", color);

		shader->setVec2("offset", animationOffset);
		shader->setVec2("frameScale", glm::vec2(1.0f / (float)frameCount, 1.0f));

		glActiveTexture(GL_TEXTURE0);
		sprite->texture.bind();

		glBindVertexArray(sprite->quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	void setFrame(unsigned int frameIndex) {
		animationOffset.x = (1.0f / (float)frameCount) * (float)frameIndex;
	}

	void update(float dt) {
		timer += dt;
		if (timer > timePerFrame) {
			timer = 0.0f;
			currentFrame = ++currentFrame % frameCount;
			setFrame(currentFrame);
		}
	}
};

Texture loadTextureFromFile(const char* filename, bool hasAlpha);
void drawTexturedSquareLine(Sprite* sprite, glm::vec3 startPos, glm::vec3 endPos, float radius);

// game data
enum GameState {
	RUNNING,
	GAME_OVER
};

struct Enemy : Circle {
	AnimatedSprite flyingSprite;
	AnimatedSprite dyingSprite;
	bool isDead;
	bool isFacingRight;
	glm::vec2 velocity;
	AnimatedSprite* currentSprite;
	bool canRemove;

	Enemy(glm::vec2 position, float radius, AnimatedSprite& flyingSprite, AnimatedSprite& dyingSprite, bool isFacingRight, glm::vec2 velocity) :
		Circle(position, radius),
		flyingSprite(flyingSprite), dyingSprite(dyingSprite),
		isDead(false), isFacingRight(isFacingRight), velocity(velocity), currentSprite(nullptr), canRemove(false) {
		currentSprite = &this->flyingSprite;
	}

	Enemy(const Enemy& other): Circle(other.position, other.radius) {
		this->flyingSprite = other.flyingSprite;
		this->dyingSprite = other.dyingSprite;
		this->isDead = other.isDead;
		this->isFacingRight = other.isFacingRight;
		this->velocity = other.velocity;
		this->currentSprite = &this->flyingSprite;
		this->canRemove = other.canRemove;
	}

	void update(float dt) {
		currentSprite->update(dt);

		if (isDead) {
			if (currentSprite->currentFrame >= currentSprite->frameCount - 1) {
				canRemove = true;
			}
			return;
		}

		position += velocity * dt;
	}

	void setToDead() {
		isDead = true;
		currentSprite = &dyingSprite;
	}
};

bool checkCircleCollision(Circle& c1, Circle& c2);
void updateGame(float dt);
void renderEnemy(Enemy& enemy, Shader* debugShader);
void renderEnemies(Shader* debugShader);

std::vector<Enemy> enemies;
GameState gameState = RUNNING;


enum ObjectType {
	BALL,
	OBSTACLE,
	BORDER,
	FLIPPER
};

enum AnimatedObject {
	FLYING_ENEMY,
	DYING_ENEMY
};

Sprite* objectToSprite[4];
AnimatedSprite* objectToAnimatedSprite[2];
const float BORDER_SPRITE_SCALE = 0.1f;

// controls
std::map<unsigned, bool> keyDownMap;
bool getKeyDown(GLFWwindow* window, unsigned int key);

int main() {

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "2D_Pinball", NULL, NULL);

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
	Shader textureShader("texture.vs", "texture.fs");
	Shader animationShader("animation.vs", "animation.fs");
	initGLData();


	stbi_set_flip_vertically_on_load(true);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Sprite testSprite = SquareLineSprite(textureShader);
	//SquareLineSprite testSprite2 = SquareLineSprite(textureShader);
	//Texture testTexture = loadTextureFromFile((FileSystem::getPath("resources/enemyTexture.png").c_str()), true);
	SquareLineSprite borderSprite = SquareLineSprite(textureShader, loadTextureFromFile((FileSystem::getPath("resources/stone.png").c_str()), true));
	borderSprite.useTiling = true;
	borderSprite.spriteScale = BORDER_SPRITE_SCALE;
	objectToSprite[BORDER] = &borderSprite;

	Sprite pinballSprite = Sprite(textureShader, loadTextureFromFile((FileSystem::getPath("resources/pinball.png").c_str()), true));
	objectToSprite[BALL] = &pinballSprite;

	SquareLineSprite flipperSprite = SquareLineSprite(textureShader, loadTextureFromFile((FileSystem::getPath("resources/flipper.png").c_str()), true));
	objectToSprite[FLIPPER] = &flipperSprite;

	Sprite obstacleSprite = Sprite(textureShader, loadTextureFromFile((FileSystem::getPath("resources/sand.png").c_str()), true));
	objectToSprite[OBSTACLE] = &obstacleSprite;

	Sprite enemyFlyingSprite = Sprite(textureShader, loadTextureFromFile((FileSystem::getPath("resources/enemy_flying.png").c_str()), true));
	AnimatedSprite enemyFlying = AnimatedSprite(animationShader, enemyFlyingSprite);
	enemyFlying.frameCount = 4;
	enemyFlying.timePerFrame = 0.1f;
	objectToAnimatedSprite[FLYING_ENEMY] = &enemyFlying;
	
	Sprite enemyDyingSprite = Sprite(textureShader, loadTextureFromFile((FileSystem::getPath("resources/enemy_dying.png").c_str()), true));
	AnimatedSprite enemyDying = AnimatedSprite(animationShader, enemyDyingSprite);
	enemyDying.frameCount = 7;
	enemyDying.timePerFrame = 0.05f;
	objectToAnimatedSprite[DYING_ENEMY] = &enemyDying;

	resetScene();

	while (!glfwWindowShouldClose(window)) {
		processInput(window);

		float currentTime = (float)glfwGetTime();
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;

		updateSimulation(deltaTime);
		updateGame(deltaTime);

		// render
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		renderBalls(circleShader);
		renderObstacles(circleShader);
		renderFlippers(squareShader);
		renderBorder(squareShader);
		renderEnemies(&circleShader);

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

	outlineIndices[0] = 0;
	outlineIndices[1] = 1;
	outlineIndices[2] = 2;
	outlineIndices[3] = 3;
	outlineIndices[4] = 0;

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

	// square
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

	// square outline
	glGenVertexArrays(1, &squareOutlineVAO);
	glBindVertexArray(squareOutlineVAO);

	glGenBuffers(1, &squareOutlineVBO);
	glBindBuffer(GL_ARRAY_BUFFER, squareOutlineVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 3, squareVerts, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &squareOutlineEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, squareOutlineEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 5, outlineIndices, GL_STATIC_DRAW);
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

void renderBalls(Shader& shader) {
	for (const Ball& ball : balls) {
		//drawCircle(shader, glm::vec3(ball.position, 0.0f), ball.radius, glm::vec3(1.0f));
		objectToSprite[BALL]->drawSprite(glm::vec3(ball.position, 0.0f), glm::vec3(2.0f * ball.radius), 0.0f, glm::vec3(1.0f), true);
	}

	#ifdef DRAW_DEBUG
	for (const Ball& ball : balls) {
		drawCircleOutline(shader, glm::vec3(ball.position, 0.0f), ball.radius);
	}
	#endif
}

void renderObstacles(Shader& shader) {
	for (const Obstacle& obstacle : obstacles) {
		//drawCircle(shader, glm::vec3(obstacle.position, 0.0f), obstacle.radius, glm::vec3(1.0f, 1.0f, 0.0f));
		objectToSprite[OBSTACLE]->drawSprite(glm::vec3(obstacle.position, 0.0f), glm::vec3(2.0f * obstacle.radius), 0.0f, glm::vec3(1.0f));
	}

	#ifdef DRAW_DEBUG
	for (const Obstacle& obstacle : obstacles) {
		drawCircleOutline(shader, glm::vec3(obstacle.position, 0.0f), obstacle.radius);
	}
	#endif
}
void renderFlippers(Shader& shader) {
	for (const Flipper& flipper : flippers) {
		glm::vec3 startPos = glm::vec3(flipper.position, 0.0f);
		glm::vec3 endPos = glm::vec3(flipper.getFlipperEnd(), 0.0f);
		//drawSquareLine(shader, startPos, endPos, flipper.radius, glm::vec3(1.0f, 0.0f, 0.0f));

		if (startPos.x < endPos.x) {
			std::swap(startPos, endPos);
			objectToSprite[FLIPPER]->isFlipped = true;
		}
		else {
			objectToSprite[FLIPPER]->isFlipped = false;
		}

		objectToSprite[FLIPPER]->offset = glm::vec3(0.0f, -0.5f, 0.0f);
		drawTexturedSquareLine(objectToSprite[FLIPPER], startPos, endPos, flipper.radius * 2.0f);
	}

	#ifdef DRAW_DEBUG
	for (const Flipper& flipper : flippers) {
		glm::vec3 startPos = glm::vec3(flipper.position, 0.0f);
		glm::vec3 endPos = glm::vec3(flipper.getFlipperEnd(), 0.0f);
		drawSquareOutline(shader, startPos, endPos, flipper.radius);
	}
	#endif
}
void renderBorder(Shader& shader) {
	int n = borderPoints.size();
	for (int i = 0; i < n; i++) {
		glm::vec3 startPos = glm::vec3(borderPoints[i], 0.0f);
		glm::vec3 endPos = glm::vec3(borderPoints[(i + 1) % n], 0.0f);
		//drawSquareLine(shader, startPos, endPos, BORDER_SIZE, glm::vec3(1.0f, 1.0f, 1.0f));
		drawTexturedSquareLine(objectToSprite[BORDER], startPos, endPos, BORDER_SIZE);
	}

	#ifdef DRAW_DEBUG
	for (int i = 0; i < n; i++) {
		glm::vec3 startPos = glm::vec3(borderPoints[i], 0.0f);
		glm::vec3 endPos = glm::vec3(borderPoints[(i + 1) % n], 0.0f);
		drawSquareOutline(shader, startPos, endPos, BORDER_SIZE);
	}
	#endif
}

void drawSquareOutline(Shader& shader, glm::vec3 startPos, glm::vec3 endPos, float radius) {
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
	glm::mat4 model = glm::translate(glm::mat4(1.0f), startPos) *
		glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f)) *
		glm::scale(glm::mat4(1.0f), glm::vec3(length, radius, 0.0f)) * glm::mat4(1.0f);
	shader.setMat4("projection", projection);
	shader.setMat4("view", view);
	shader.setMat4("model", model);
	shader.setVec3("color", glm::vec3(0.0f, 1.0f, 0.0f));

	glBindVertexArray(squareOutlineVAO);
	glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_INT, 0);
}

void drawCircleOutline(Shader& shader, glm::vec3 position, float radius) {
	shader.use();
	glm::mat4 projection = glm::ortho(
		-(WORLD_WIDTH / 2.0f), (WORLD_WIDTH / 2.0f),
		-(WORLD_HEIGHT / 2.0f), (WORLD_HEIGHT / 2.0f),
		-1.0f, 1.0f
	);
	shader.setMat4("projection", projection);
	shader.setFloat("scale", radius);
	shader.setVec3("position", position);
	shader.setVec3("color", glm::vec3(0.0f, 1.0f, 0.0f));

	glBindVertexArray(circleVAO);
	glDrawElements(GL_LINES, CIRCLE_VERTS_NUM, GL_UNSIGNED_INT, 0);
}

void resetScene() {
	borderPoints.clear();
	balls.clear();
	flippers.clear();
	obstacles.clear();
	enemies.clear();

	// init simulation
	//borderPoints.push_back(glm::vec2(-25.0f, 35.0f));
	//borderPoints.push_back(glm::vec2(-25.0f, -15.0f));
	//borderPoints.push_back(glm::vec2(-15.0f, -20.0f));
	//borderPoints.push_back(glm::vec2(-15.0f, -30.0f));
	//borderPoints.push_back(glm::vec2(15.0f, -30.0f));
	//borderPoints.push_back(glm::vec2(15.0f, -20.0f));
	//borderPoints.push_back(glm::vec2(25.0f, -15.0f));
	//borderPoints.push_back(glm::vec2(25.0f, 35.0f));

	//Ball ball;
	//ball.radius = 2.0f;
	//ball.mass = Utils::PI * ball.radius * ball.radius;
	//ball.position = glm::vec2(15.0f, 10.0f);
	//ball.velocity = glm::vec2(0.0f, 0.0f);
	//balls.push_back(ball);
	//ball.position = glm::vec2(-15.0f, 10.0);
	//ball.velocity = glm::vec2(0.2f, 0.0f);
	//balls.push_back(ball);
	//balls.push_back(ball);
	//balls.push_back(ball);

	//obstacles.push_back(Obstacle(glm::vec2(10.0f, 30.0f), 3.0f));
	//obstacles.push_back(Obstacle(glm::vec2(-12.0f, 8.0f), 5.0f));
	//obstacles.push_back(Obstacle(glm::vec2(4.0f, 4.0f), 6.2f));
	//obstacles.push_back(Obstacle(glm::vec2(-5.0f, 24.2f), 4.0f));

	//float radius = 1.0f;
	//float length = 12.5f;
	//float maxRotation = Utils::deg2Rad(45.0f);
	//float restAngle = Utils::deg2Rad(0.0f);
	//float angularVelocity = 10.0f;
	//float restitution = 0.0f;
	//glm::vec2 pos1 = glm::vec2(-15.0f, -22.0f);
	//glm::vec2 pos2 = glm::vec2(15.0f, -22.0f);
	//flippers.push_back(Flipper(pos1, radius, length, -restAngle, maxRotation, angularVelocity, restitution));
	//flippers.push_back(Flipper(pos2, radius, length, Utils::PI + restAngle, maxRotation, angularVelocity, restitution, false));
	//flippers[0].id = 0;
	//flippers[1].id = 1;
   // ---- TABLE SHAPE (fits your original scale) ----
	borderPoints.push_back(glm::vec2(-25.0f, 35.0f)); // top left
	borderPoints.push_back(glm::vec2(-25.0f, -10.0f)); // left wall
	borderPoints.push_back(glm::vec2(-15.0f, -18.0f)); // left slope
	borderPoints.push_back(glm::vec2(-15.0f, -30.0f)); // bottom left
	borderPoints.push_back(glm::vec2(15.0f, -30.0f)); // bottom right
	borderPoints.push_back(glm::vec2(15.0f, -18.0f)); // right slope
	borderPoints.push_back(glm::vec2(25.0f, -10.0f)); // right wall
	borderPoints.push_back(glm::vec2(25.0f, 35.0f)); // top right

	// ---- BALLS (spawn safely inside) ----
	Ball ball;
	ball.radius = 2.0f;
	ball.mass = Utils::PI * ball.radius * ball.radius;

	ball.position = glm::vec2(0.0f, 20.0f);
	ball.velocity = glm::vec2(0.0f, 0.0f);
	balls.push_back(ball);

	ball.position = glm::vec2(-8.0f, 15.0f);
	ball.velocity = glm::vec2(0.2f, 0.0f);
	balls.push_back(ball);

	ball.position = glm::vec2(8.0f, 15.0f);
	ball.velocity = glm::vec2(-0.2f, 0.0f);
	balls.push_back(ball);

	// ---- OBSTACLES (all inside borders) ----

	//// Top bumpers
	//obstacles.push_back(Obstacle(glm::vec2(0.0f, 28.0f), 4.0f));
	//obstacles.push_back(Obstacle(glm::vec2(-10.0f, 24.0f), 3.0f));
	//obstacles.push_back(Obstacle(glm::vec2(10.0f, 24.0f), 3.0f));

	//// Midfield chaos
	//obstacles.push_back(Obstacle(glm::vec2(-12.0f, 10.0f), 2.5f));
	//obstacles.push_back(Obstacle(glm::vec2(0.0f, 8.0f), 4.5f));
	//obstacles.push_back(Obstacle(glm::vec2(12.0f, 10.0f), 2.5f));

	//// Side nudgers (guide balls toward flippers, not block drain)
	//obstacles.push_back(Obstacle(glm::vec2(-12.0f, -4.0f), 2.0f));
	//obstacles.push_back(Obstacle(glm::vec2(12.0f, -4.0f), 2.0f));

	// ---- FLIPPERS (attached to lower slopes) ----
	float radius = 1.0f;
	float length = 12.0f;
	float maxRotation = Utils::deg2Rad(45.0f);
	float restAngle = Utils::deg2Rad(10.0f);
	float angularVelocity = 10.0f;
	float restitution = 0.0f;

	glm::vec2 pos1 = glm::vec2(-15.0f, -18.0f);
	glm::vec2 pos2 = glm::vec2(15.0f, -18.0f);

	flippers.push_back(Flipper(pos1, radius, length, -restAngle, maxRotation, angularVelocity, restitution));
	flippers.push_back(Flipper(pos2, radius, length, Utils::PI + restAngle, maxRotation, angularVelocity, restitution, false));

	flippers[0].id = 0;
	flippers[1].id = 1;

	// enemies
	AnimatedSprite& enemyFlying = *objectToAnimatedSprite[FLYING_ENEMY];
	AnimatedSprite& enemyDying = *objectToAnimatedSprite[DYING_ENEMY];
	Enemy testEnemy1(glm::vec2(-20.0f, 0.0f), 5.0f, enemyFlying, enemyDying, true, glm::vec2(0.0f));
	Enemy testEnemy2(glm::vec2(0.0f, 0.0f), 5.0f, enemyFlying, enemyDying, true, glm::vec2(0.0f));
	Enemy testEnemy3(glm::vec2(20.0f, 0.0f), 5.0f, enemyFlying, enemyDying, true, glm::vec2(0.0f));
	enemies.push_back(testEnemy1);
	enemies.push_back(testEnemy2);
	enemies.push_back(testEnemy3);
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
	if (distance == 0.0f || distance > ball.radius + flipper.radius * 0.5f) return;

	dir = glm::normalize(dir);

	float correction = ball.radius + flipper.radius * 0.5f - distance;
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

	glm::vec2 d, closest, ab;
	glm::vec2 normal = glm::vec2();
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
		if (distance > ball.radius + BORDER_SIZE * 0.5f) return;

		ball.position += d * (ball.radius - distance + BORDER_SIZE * 0.5f);
	}
	else {
		ball.position += d * -(distance + ball.radius - BORDER_SIZE * 0.5f);
	}

	float v = glm::dot(ball.velocity, d);
	float newV = glm::abs(v) * RESTITUTION;

	ball.velocity += d * (newV - v);
}

void updateSimulation(float dt) {
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

Texture loadTextureFromFile(const char* filename, bool hasAlpha) {
	Texture texture;
	if (hasAlpha) {
		texture.internalFormat = GL_RGBA;
		texture.imageFormat = GL_RGBA;
	}
	int width, height, nrChannels;
	unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
	texture.generate(width, height, data);
	stbi_image_free(data);
	return texture;
}

void drawTexturedSquareLine(Sprite* sprite, glm::vec3 startPos, glm::vec3 endPos, float radius) {
	glm::vec2 startToEnd = endPos - startPos;
	float length = glm::length(startToEnd);
	glm::vec2 dir = glm::normalize(startToEnd);
	float angle = glm::atan(dir.y, dir.x);

	sprite->drawSprite(startPos, glm::vec3(length, radius, 0.0f), angle, glm::vec3(1.0f), true);
}

bool checkCircleCollision(Circle& c1, Circle& c2) {
	float distance = glm::length(c1.position - c2.position);
	return distance < (c1.radius + c2.radius);
}

void updateGame(float dt) {
	for (Enemy& enemy : enemies) {
		enemy.update(dt);

		float lowestY = FLT_MAX;
		for (Flipper& flipper : flippers) {
			lowestY = glm::min(flipper.position.y, lowestY);
		}

		if (enemy.position.y + enemy.radius < lowestY) {
			gameState = GAME_OVER;
			break;
		}

		for (Ball& ball : balls) {
			if (checkCircleCollision(enemy, ball)) {
				enemy.setToDead();
				break;
			}
		}
	}

	for (int i = enemies.size() - 1; i >= 0; i--) {
		if (enemies.at(i).canRemove) {
			enemies.erase(enemies.begin() + i);
		}
	}
}

void renderEnemy(Enemy& enemy, Shader* debugShader = nullptr) {
	enemy.currentSprite->drawSprite(glm::vec3(enemy.position, 0.0f), glm::vec3(enemy.radius * 2.0f), 0.0f, glm::vec3(1.0f));
	#ifdef DRAW_DEBUG
	if (debugShader != nullptr)
		drawCircleOutline(*debugShader, glm::vec3(enemy.position, 0.0f), enemy.radius);
	#endif
}

void renderEnemies(Shader* debugShader = nullptr) {
	for (Enemy& enemy : enemies) {
		renderEnemy(enemy, debugShader);
	}
}