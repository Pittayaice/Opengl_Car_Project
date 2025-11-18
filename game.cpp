#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <stb_image.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <iostream>
#include <vector>
#include <deque>
#include <cstdlib>
#include <ctime>
#include <string>
#include <map>


const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 5.0f, -5.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Camera view mode
bool isFirstPersonView = false;

glm::vec3 lanes[3] = { {-3.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f}, {3.0f,0.0f,0.0f} };
int playerLane = 1;

// Lane transition variables
bool isChangingLane = false;
int targetLane = 1;
float laneChangeProgress = 0.0f;
float laneChangeSpeed = 2.0f; // Time to complete lane change
float currentCarX = 0.0f;
float carRotationY = 0.0f;

Model* playerCar;
Model* stopSignModel;
Model* coneModel;
Model* barrelModel;

Model* buildingModels[4]; // b2,b3,b4,b5

unsigned int roadVAO, roadTexture;
unsigned int grassVAO, grassTexture;
unsigned int footpathVAO, footpathTexture;
unsigned int curbVAO, curbTexture;


struct RoadSegment { float zStart; };
std::deque<RoadSegment> roadSegments;
float segmentSize = 20.0f;

float carZ = 0.0f;
float speed = 20.0f;
float baseSpeed = 20.0f;  // Store the original base speed
int lastSpeedIncreaseScore = 0;  // Track when we last increased speed

struct Obstacle {
    glm::vec3 pos;
    int type; // 0=StopSign, 1=Cone, 2=Barrel
};
std::vector<Obstacle> obstacles;

struct Building {
    glm::vec3 pos;
    int type; // 0=b2,1=b3,2=b4,3=b5
    bool leftSide;
};
std::vector<Building> buildings;

float distanceTraveled = 0.0f;
int totalScore = 0;
bool gameOver = false;
bool gameStarted = false;  // Track if game has started

// Text rendering structures
struct Character {
    unsigned int TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    unsigned int Advance;
};

std::map<GLchar, Character> Characters;
unsigned int textVAO, textVBO;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void renderObjects(Shader& shader);
void spawnObstacles();
void spawnBuildings();
void generateRoadIfNeeded();
void resetGame();
unsigned int loadTexture(const std::string& path);
void RenderText(Shader& shader, std::string text, float x, float y, float scale, glm::vec3 color);
float GetTextWidth(std::string text, float scale);

int main()
{
    srand((unsigned int)time(0));

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3-Lane Car Avoidance", NULL, NULL);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cout << "Failed to initialize GLAD\n"; return -1; }

    glEnable(GL_DEPTH_TEST);

    Shader shader("1.2.depth_testing.vs", "1.2.depth_testing.fs");

    // Text rendering shader
    Shader textShader(
        FileSystem::getPath("src/7.in_practice/2.text_rendering/text.vs").c_str(),
        FileSystem::getPath("src/7.in_practice/2.text_rendering/text.fs").c_str()
    );

    // Load models
    playerCar = new Model(FileSystem::getPath("resources/project/car/Jeep_Renegade_2016.obj"));
    stopSignModel = new Model(FileSystem::getPath("resources/project/StopSign/StopSign.obj"));
    coneModel = new Model(FileSystem::getPath("resources/project/cone/TrafficCone.obj"));
    barrelModel = new Model(FileSystem::getPath("resources/project/barrel/barrel.obj"));
    buildingModels[0] = new Model(FileSystem::getPath("resources/project/building/b2/b2.obj"));
    buildingModels[1] = new Model(FileSystem::getPath("resources/project/building/b3/b3.obj"));
    buildingModels[2] = new Model(FileSystem::getPath("resources/project/building/b4/b4.obj"));
    buildingModels[3] = new Model(FileSystem::getPath("resources/project/building/b5/b5.obj"));

    // ----- ROAD -----
    float roadVertices[] = {
        -5.0f, 0.01f, 0.0f,    0.0f,1.0f,0.0f,  1.0f, 0.0f,
         5.0f, 0.01f, 0.0f,    0.0f,1.0f,0.0f,  1.0f, 1.0f,
         5.0f, 0.01f, segmentSize, 0.0f,1.0f,0.0f, 0.0f, 1.0f,
        -5.0f, 0.01f, segmentSize, 0.0f,1.0f,0.0f, 0.0f, 0.0f
    };
    unsigned int roadIndices[] = { 0,1,2, 2,3,0 };
    unsigned int roadVBO, roadEBO;

    glGenVertexArrays(1, &roadVAO);
    glGenBuffers(1, &roadVBO);
    glGenBuffers(1, &roadEBO);

    glBindVertexArray(roadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, roadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(roadVertices), roadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, roadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(roadIndices), roadIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // ----- GRASS GROUND -----
    float grassVertices[] = {
        -200.0f, 0.0f, -200.0f,   0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
         200.0f, 0.0f, -200.0f,   0.0f, 1.0f, 0.0f,  50.0f, 0.0f,
         200.0f, 0.0f,  200.0f,   0.0f, 1.0f, 0.0f,  50.0f, 50.0f,
        -200.0f, 0.0f,  200.0f,   0.0f, 1.0f, 0.0f,  0.0f, 50.0f
    };
    unsigned int grassIndices[] = { 0, 1, 2, 2, 3, 0 };
    unsigned int grassVBO, grassEBO;

    glGenVertexArrays(1, &grassVAO);
    glGenBuffers(1, &grassVBO);
    glGenBuffers(1, &grassEBO);

    glBindVertexArray(grassVAO);
    glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(grassVertices), grassVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grassEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(grassIndices), grassIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // ----- FOOTPATH -----
    float footpathVertices[] = {
        // Left side
        -11.0f + 0.15f, 0.2f, 0.0f,   0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        -5.0f + 0.15f, 0.2f, 0.0f,   0.0f, 1.0f, 0.0f,  3.0f, 0.0f,
        -5.0f + 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f,  3.0f, 8.0f,
        -11.0f + 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f,  0.0f, 8.0f,

        // Right side
         11.0f - 0.15f, 0.2f, 0.0f,   0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
          5.0f - 0.15f, 0.2f, 0.0f,   0.0f, 1.0f, 0.0f,  3.0f, 0.0f,
          5.0f - 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f,  3.0f, 8.0f,
         11.0f - 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f,  0.0f, 8.0f,
    };
    unsigned int footpathIndices[] = {
        0, 1, 2, 2, 3, 0,   // Left
        4, 5, 6, 6, 7, 4    // Right
    };
    unsigned int footpathVBO, footpathEBO;

    glGenVertexArrays(1, &footpathVAO);
    glGenBuffers(1, &footpathVBO);
    glGenBuffers(1, &footpathEBO);

    glBindVertexArray(footpathVAO);
    glBindBuffer(GL_ARRAY_BUFFER, footpathVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(footpathVertices), footpathVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, footpathEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(footpathIndices), footpathIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // ----- CURB (red write) -----
    float curbVertices[] = {
        // Left curb
        -5.0f + 0.15f, 0.2f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        -4.8f + 0.15f, 0.2f, 0.0f,  0.0f, 1.0f, 0.0f,  5.0f, 0.0f,
        -4.8f + 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f, 5.0f, 2.0f,
        -5.0f + 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f,

        // Right curb
         4.8f - 0.15f, 0.2f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
         5.0f - 0.15f, 0.2f, 0.0f,  0.0f, 1.0f, 0.0f,  5.0f, 0.0f,
         5.0f - 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f, 5.0f, 2.0f,
         4.8f - 0.15f, 0.2f, segmentSize, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f
    };
    unsigned int curbIndices[] = {
        0, 1, 2, 2, 3, 0,   // Left curb
        4, 5, 6, 6, 7, 4    // Right curb
    };

    unsigned int curbVBO, curbEBO;
    glGenVertexArrays(1, &curbVAO);
    glGenBuffers(1, &curbVBO);
    glGenBuffers(1, &curbEBO);

    glBindVertexArray(curbVAO);
    glBindBuffer(GL_ARRAY_BUFFER, curbVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(curbVertices), curbVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, curbEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(curbIndices), curbIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Load texture
    roadTexture = loadTexture(FileSystem::getPath("resources/project/road/3lane.jpg"));
    grassTexture = loadTexture(FileSystem::getPath("resources/project/grass/grass.jpg"));
    footpathTexture = loadTexture(FileSystem::getPath("resources/project/grass/brick1.jpg"));
    curbTexture = loadTexture(FileSystem::getPath("resources/project/grass/redwhite1.jpg"));

    // Initialize FreeType for text rendering
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return -1;
    }

    std::string font_name = FileSystem::getPath("resources/fonts/Antonio-Bold.ttf");
    if (font_name.empty()) {
        std::cout << "ERROR::FREETYPE: Failed to load font_name" << std::endl;
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, font_name.c_str(), 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load first 128 ASCII characters
    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Configure VAO/VBO for text rendering
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Setup orthographic projection for text rendering
    glm::mat4 textProjection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
    textShader.use();
    textShader.setMat4("projection", textProjection);

    // Setup road segments
    float groundLength = 1000.0f;
    int numSegments = int(groundLength / segmentSize) + 2;
    float startZ = -groundLength / 2;
    for (int i = 0; i < numSegments; i++)
        roadSegments.push_back({ startZ + i * segmentSize });

    resetGame();

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        if (!gameStarted) {
            // Main menu rendering
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Title
            std::string titleText = "CAR AVOIDANCE";
            float titleWidth = GetTextWidth(titleText, 2.0f);
            float titleX = (SCR_WIDTH - titleWidth) / 2.0f;
            float titleY = SCR_HEIGHT - 120.0f;  // Slightly lower
            RenderText(textShader, titleText, titleX, titleY, 2.0f, glm::vec3(1.0f, 1.0f, 0.0f));

            // Instructions
            std::string instrText = "Avoid obstacles and survive as long as possible!";
            float instrWidth = GetTextWidth(instrText, 0.8f);
            float instrX = (SCR_WIDTH - instrWidth) / 2.0f;
            RenderText(textShader, instrText, instrX, titleY - 150.0f, 0.8f, glm::vec3(0.8f, 0.8f, 0.8f));

            // Controls
            std::string controlText1 = "A/D - Change Lane  |  C - Toggle Camera";
            float ctrl1Width = GetTextWidth(controlText1, 0.7f);
            float ctrl1X = (SCR_WIDTH - ctrl1Width) / 2.0f;
            RenderText(textShader, controlText1, ctrl1X, titleY - 210.0f, 0.7f, glm::vec3(0.7f, 0.9f, 1.0f));

            std::string controlText2 = "ESC - Menu (during game) / Quit (on menu)";
            float ctrl2Width = GetTextWidth(controlText2, 0.7f);
            float ctrl2X = (SCR_WIDTH - ctrl2Width) / 2.0f;
            RenderText(textShader, controlText2, ctrl2X, titleY - 260.0f, 0.7f, glm::vec3(0.7f, 0.9f, 1.0f));

            // Start button
            std::string startText = "Press SPACE to Start";
            float startWidth = GetTextWidth(startText, 1.2f);
            float startX = (SCR_WIDTH - startWidth) / 2.0f;
            RenderText(textShader, startText, startX, titleY - 400.0f, 1.2f, glm::vec3(0.0f, 1.0f, 0.5f));

            glDisable(GL_BLEND);

            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        if (!gameOver) {
            carZ += speed * deltaTime;
            distanceTraveled = carZ / 10.0f;

            // Update score based on distance (10 points per meter)
            totalScore = (int)(distanceTraveled * 10);

            // Increase speed by 10% every 500 points
            int currentThreshold = (totalScore / 500) * 500;
            if (currentThreshold > lastSpeedIncreaseScore && currentThreshold > 0) {
                lastSpeedIncreaseScore = currentThreshold;
                speed = baseSpeed * (1.0f + (currentThreshold / 500) * 0.1f);
                std::cout << "Speed increased! Score: " << totalScore
                    << " | New speed: " << speed << std::endl;
            }

            generateRoadIfNeeded();
            spawnObstacles();
            spawnBuildings();

            // Handle lane transition
            if (isChangingLane) {
                laneChangeProgress += deltaTime * laneChangeSpeed;

                if (laneChangeProgress >= 1.0f) {
                    laneChangeProgress = 1.0f;
                    isChangingLane = false;
                    playerLane = targetLane;
                    carRotationY = 0.0f;
                }

                // Smooth interpolation for position
                float startX = lanes[playerLane].x;
                float endX = lanes[targetLane].x;
                currentCarX = startX + (endX - startX) * laneChangeProgress;

                // Rotation: 0 -> 15 -> 0 degrees (reduced from 30 for slower rotation)
                float maxRotationAngle = 15.0f;
                if (laneChangeProgress < 0.5f) {
                    // First half: rotate from 0 to 15 degrees
                    carRotationY = (laneChangeProgress * 2.0f) * maxRotationAngle;
                }
                else {
                    // Second half: rotate from 15 back to 0 degrees
                    carRotationY = (2.0f - laneChangeProgress * 2.0f) * maxRotationAngle;
                }

                // Apply direction (left is negative, right is positive)
                if (targetLane < playerLane) {
                    carRotationY = -carRotationY;
                }
            }
            else {
                currentCarX = lanes[playerLane].x;
                carRotationY = 0.0f;
            }
        }

        glm::vec3 carPos = glm::vec3(currentCarX, 0.0f, carZ);

        if (isFirstPersonView) {
            // First-person view: camera inside/in front of the car, rotating with it
            // Apply a reduced rotation for first-person (80% of car rotation for gentler feel)
            float cameraRotationMultiplier = 0.8f;  // Adjust this value: lower = slower rotation
            float rotationRad = glm::radians(-carRotationY * cameraRotationMultiplier);

            // Offset position relative to car (before rotation)
            glm::vec3 cameraOffset(0.0f, 1.5f, 0.65f);

            // Rotate the offset around Y-axis to match car rotation
            glm::vec3 rotatedOffset;
            rotatedOffset.x = cameraOffset.x * cos(rotationRad) - cameraOffset.z * sin(rotationRad);
            rotatedOffset.y = cameraOffset.y;
            rotatedOffset.z = cameraOffset.x * sin(rotationRad) + cameraOffset.z * cos(rotationRad);

            camera.Position = carPos + rotatedOffset;

            // Camera looks forward in the direction the car is facing
            glm::vec3 forwardDir(0.0f, -0.1f, 1.0f);
            glm::vec3 rotatedForward;
            rotatedForward.x = forwardDir.x * cos(rotationRad) - forwardDir.z * sin(rotationRad);
            rotatedForward.y = forwardDir.y;
            rotatedForward.z = forwardDir.x * sin(rotationRad) + forwardDir.z * cos(rotationRad);

            camera.Front = glm::normalize(rotatedForward);
        }
        else {
            // Third-person view: camera behind the car
            camera.Position = carPos + glm::vec3(0.0f, 5.0f, -13.0f);
            camera.Front = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));
        }

        // Dark foggy atmosphere
        glClearColor(0.25f, 0.25f, 0.27f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setVec3("cameraPos", camera.Position);
        shader.setInt("texture_diffuse1", 0);

        renderObjects(shader);

        // Render score text in top right corner
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::string scoreText = "Score: " + std::to_string(totalScore);
        float scoreWidth = GetTextWidth(scoreText, 1.0f);
        float textX = SCR_WIDTH - scoreWidth - 20.0f;  // Anchor to right edge, grow left
        float textY = SCR_HEIGHT - 60.0f;   // 60 pixels from top
        RenderText(textShader, scoreText, textX, textY, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));

        std::string distText = std::to_string((int)distanceTraveled) + "m";
        float distWidth = GetTextWidth(distText, 0.8f);
        float distX = SCR_WIDTH - distWidth - 20.0f;  // Anchor to right edge, grow left
        RenderText(textShader, distText, distX, textY - 50.0f, 0.8f, glm::vec3(0.8f, 0.8f, 0.8f));

        // Render speed to bottom left
        std::string speedText = "Speed: " + std::to_string((int)speed);
        float speedX = 20.0f;  // 20 pixels from left edge
        float speedY = 40.0f;  // 40 pixels from bottom
        RenderText(textShader, speedText, speedX, speedY, 0.9f, glm::vec3(0.8f, 1.0f, 0.8f));

        if (gameOver) {
            std::string gameOverText = "GAME OVER!";
            float gameOverWidth = GetTextWidth(gameOverText, 1.5f);
            float gameOverX = (SCR_WIDTH - gameOverWidth) / 2.0f;
            float gameOverY = SCR_HEIGHT / 2.0f;
            RenderText(textShader, gameOverText, gameOverX, gameOverY, 1.5f, glm::vec3(1.0f, 0.0f, 0.0f));

            std::string finalScoreText = "Final Score: " + std::to_string(totalScore);
            float finalScoreWidth = GetTextWidth(finalScoreText, 1.0f);
            float finalScoreX = (SCR_WIDTH - finalScoreWidth) / 2.0f;
            RenderText(textShader, finalScoreText, finalScoreX, gameOverY - 70.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));

            std::string restartText = "Press R to Restart";
            float restartWidth = GetTextWidth(restartText, 0.8f);
            float restartX = (SCR_WIDTH - restartWidth) / 2.0f;
            RenderText(textShader, restartText, restartX, gameOverY - 130.0f, 0.8f, glm::vec3(0.8f, 0.8f, 0.8f));

            std::string menuText = "Press M for Main Menu";
            float menuWidth = GetTextWidth(menuText, 0.8f);
            float menuX = (SCR_WIDTH - menuWidth) / 2.0f;
            RenderText(textShader, menuText, menuX, gameOverY - 180.0f, 0.8f, glm::vec3(1.0f, 0.8f, 0.0f));
        }

        glDisable(GL_BLEND);

        std::string title = "Car Avoidance - Distance: " + std::to_string((int)distanceTraveled) + "m";
        title += isFirstPersonView ? " [First-Person]" : " [Third-Person]";
        title += " | Score: " + std::to_string(totalScore);
        if (gameOver) title += " - GAME OVER! Final Score: " + std::to_string(totalScore) + " - Press R to Restart";
        glfwSetWindowTitle(window, title.c_str());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

//--------------------------------------------

unsigned int loadTexture(const std::string& path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else std::cout << "Failed to load texture: " << path << "\n";
    stbi_image_free(data);
    return textureID;
}

void resetGame() {
    playerLane = 1;
    targetLane = 1;
    isChangingLane = false;
    laneChangeProgress = 0.0f;
    currentCarX = 0.0f;
    carRotationY = 0.0f;
    carZ = 0.0f;
    obstacles.clear();
    buildings.clear();
    distanceTraveled = 0.0f;
    totalScore = 0;
    speed = baseSpeed;  // Reset to base speed
    lastSpeedIncreaseScore = 0;  // Reset speed increase tracker
    gameOver = false;
    isFirstPersonView = false;

    roadSegments.clear();
    float groundLength = 1000.0f;
    float startZ = -groundLength / 2;
    int numSegments = int(groundLength / segmentSize) + 2;
    for (int i = 0; i < numSegments; i++)
        roadSegments.push_back({ startZ + i * segmentSize });
}

void spawnObstacles() {
    static float timer = 0.0f;
    timer += deltaTime;
    if (timer > 1.0f) {
        int lane = rand() % 3;
        int type = rand() % 3;
        float zPos = carZ + 80.0f + rand() % 50;
        obstacles.push_back({ lanes[lane] + glm::vec3(0.0f, 0.0f, zPos), type });
        timer = 0.0f;
    }
}

void spawnBuildings() {
    static float timer = 0.0f;
    timer += deltaTime;

    if (timer > 3.0f) {
        float zPos = carZ + 80.0f + rand() % 50;

        buildings.push_back({ glm::vec3(-16.0f, 0.0f, zPos), rand() % 4, true });

        buildings.push_back({ glm::vec3(16.0f, 0.0f, zPos), rand() % 4, false });

        while (!buildings.empty() && buildings.front().pos.z < carZ - 100.0f)
            buildings.erase(buildings.begin());

        timer = 0.0f;
    }
}

void generateRoadIfNeeded() {
    if (!roadSegments.empty() && carZ + segmentSize * 3 > roadSegments.back().zStart) {
        float zNew = roadSegments.back().zStart + segmentSize;
        roadSegments.push_back({ zNew });
        while (!roadSegments.empty() && roadSegments.front().zStart + segmentSize < carZ - 50.0f)
            roadSegments.pop_front();
    }
}

void processInput(GLFWwindow* window) {
    static bool escapePressed = false;

    // ESC key behavior: exit to menu during gameplay, or quit if on menu
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escapePressed) {
        if (gameStarted && !gameOver) {
            // In gameplay: return to menu
            gameStarted = false;
            resetGame();
        }
        else if (!gameStarted) {
            // On main menu: quit application
            glfwSetWindowShouldClose(window, true);
        }
        escapePressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) escapePressed = false;

    static bool leftPressed = false, rightPressed = false, restartPressed = false, cameraPressed = false;

    // Start game from menu
    static bool startPressed = false;
    if (!gameStarted && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !startPressed) {
        gameStarted = true;
        startPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) startPressed = false;

    // Return to menu from game over
    static bool menuPressed = false;
    if (gameOver && glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !menuPressed) {
        gameStarted = false;
        resetGame();
        menuPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE) menuPressed = false;

    // Camera toggle
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cameraPressed) {
        isFirstPersonView = !isFirstPersonView;
        cameraPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) cameraPressed = false;

    // Only allow lane change if not currently changing lanes
    if (!isChangingLane && !gameOver) {
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && !leftPressed) {
            if (playerLane > 0) {
                targetLane = playerLane - 1;
                isChangingLane = true;
                laneChangeProgress = 0.0f;
            }
            leftPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) leftPressed = false;

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && !rightPressed) {
            if (playerLane < 2) {
                targetLane = playerLane + 1;
                isChangingLane = true;
                laneChangeProgress = 0.0f;
            }
            rightPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) rightPressed = false;
    }
    else {
        // Reset key states
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) leftPressed = false;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) rightPressed = false;
    }

    if (gameOver && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !restartPressed) {
        resetGame();
        gameStarted = true;
        restartPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE) restartPressed = false;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }

void renderObjects(Shader& shader) {
    glm::mat4 model;

    // Car with rotation
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(currentCarX, 0.0f, carZ));
    model = glm::rotate(model, glm::radians(carRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
    shader.setMat4("model", model);
    playerCar->Draw(shader);

    // Grass Ground
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, grassTexture);
    shader.setInt("texture_diffuse1", 0);
    glm::mat4 grassModel = glm::mat4(1.0f);
    grassModel = glm::translate(grassModel, glm::vec3(0.0f, -0.01f, carZ));
    shader.setMat4("model", grassModel);
    glBindVertexArray(grassVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Footpath (both sides)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, footpathTexture);
    shader.setInt("texture_diffuse1", 0);
    for (auto& seg : roadSegments) {
        glm::mat4 fmodel = glm::mat4(1.0f);
        fmodel = glm::translate(fmodel, glm::vec3(0.0f, 0.0f, seg.zStart));
        shader.setMat4("model", fmodel);

        glBindVertexArray(footpathVAO);
        glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
    }

    // ----- CURB (red write) -----
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, curbTexture);
    shader.setInt("texture_diffuse1", 0);
    for (auto& seg : roadSegments) {
        glm::mat4 cmodel = glm::mat4(1.0f);
        cmodel = glm::translate(cmodel, glm::vec3(0.0f, 0.0f, seg.zStart));
        shader.setMat4("model", cmodel);
        glBindVertexArray(curbVAO);
        glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
    }

    // Road
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, roadTexture);
    shader.setInt("texture_diffuse1", 0);
    for (auto& seg : roadSegments) {
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, glm::vec3(0.0f, 0.01f, seg.zStart));
        shader.setMat4("model", m);
        glBindVertexArray(roadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    // Obstacles
    for (auto& obs : obstacles) {
        glm::vec3 carPos = glm::vec3(currentCarX, 0.0f, carZ);
        if (glm::distance(obs.pos, carPos) < 2.0f) gameOver = true;

        model = glm::mat4(1.0f);
        model = glm::translate(model, obs.pos);

        if (obs.type == 0) { // StopSign
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(1.0f));
        }
        else if (obs.type == 1) { // Cone
            model = glm::scale(model, glm::vec3(0.6f));
        }
        else if (obs.type == 2) { // Barrel
            model = glm::scale(model, glm::vec3(2.0f));
        }

        shader.setMat4("model", model);

        if (obs.type == 0) stopSignModel->Draw(shader);
        else if (obs.type == 1) coneModel->Draw(shader);
        else barrelModel->Draw(shader);
    }

    // Buildings
    for (auto& b : buildings) {
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 pos = b.pos;

        if (b.leftSide) {
            if (b.type == 0) pos.x -= 2.5f;
            else if (b.type == 1) pos.x -= 3.0f;
            else if (b.type == 2) pos.x -= 3.8f;
            else if (b.type == 3) pos.x -= 5.0f;
        }
        else {
            if (b.type == 0) pos.x += 2.5f;
            else if (b.type == 1) pos.x += 3.0f;
            else if (b.type == 2) pos.x += 3.8f;
            else if (b.type == 3) pos.x += 5.0f;
        }

        if (b.type == 2) pos.y += 3.1f;

        model = glm::translate(model, pos);

        float baseAngle = 0.0f;
        if (b.type == 0) baseAngle = -90.0f;
        else if (b.type == 1) baseAngle = 90.0f;
        else if (b.type == 2) baseAngle = 90.0f;
        else if (b.type == 3) baseAngle = 90.0f;

        float finalAngle = b.leftSide ? baseAngle : -baseAngle;
        model = glm::rotate(model, glm::radians(finalAngle), glm::vec3(0.0f, 1.0f, 0.0f));

        if (b.type == 0) model = glm::scale(model, glm::vec3(1.00f));
        else if (b.type == 1) model = glm::scale(model, glm::vec3(1.0f));
        else if (b.type == 2) model = glm::scale(model, glm::vec3(0.8f));
        else if (b.type == 3) model = glm::scale(model, glm::vec3(1.0f));

        shader.setMat4("model", model);
        buildingModels[b.type]->Draw(shader);
    }

    glBindVertexArray(0);
}

// Text rendering function
void RenderText(Shader& shader, std::string text, float x, float y, float scale, glm::vec3 color) {
    shader.use();
    shader.setVec3("textColor", color);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (auto c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Helper function to calculate text width
float GetTextWidth(std::string text, float scale) {
    float width = 0.0f;
    for (auto c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];
        width += (ch.Advance >> 6) * scale;
    }
    return width;
}
