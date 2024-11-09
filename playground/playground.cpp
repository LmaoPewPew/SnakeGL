#include "playground.h"

// Include standard headers
#include <stdio.h>
#include <stdlib.h>

// Include GLFW
#include <glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
using namespace glm;
#include <glm/gtc/matrix_transform.hpp>


#include <common/shader.hpp>

#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>
#include <random>

// Constants
// ----------------------------------------------------------
int score = 0;
int lastMultipleOfFive = 0;
constexpr auto WIDTH = 20;
constexpr auto HEIGHT = 20;
constexpr auto WINDOW_WIDTH = 800;
constexpr auto WINDOW_HEIGHT = 800;

static constexpr int CELL_WIDTH = WINDOW_WIDTH / WIDTH;
static constexpr int CELL_HEIGHT = WINDOW_HEIGHT / HEIGHT;
// ----------------------------------------------------------

// Forward declaration
// ----------------------------------------------------------
class Entity;
class Empty;
class SnakeTail;
class SnakeHead;
class SnakeGL;

enum INPUT_TYPE;

void inline setColor(float r, float g, float b);
void drawCell(float x, float y, const glm::vec3& color);
void updateAnimationLoop(SnakeGL& snake); // Changed to non-const reference
bool initializeWindow();
bool initializeVertexbuffer();
bool cleanupVertexbuffer();
bool closeWindow();
int gameSpeed(int speedValue);


// ----------------------------------------------------------

// Class definition
// ----------------------------------------------------------
enum INPUT_TYPE
{
    UP,
    DOWN,
    LEFT,
    RIGHT
};

class Entity
{
public:
    int x, y;

    Entity() : x(0), y(0) {}
    Entity(int _x, int _y) : x(_x), y(_y) {}
    virtual ~Entity() = default;

    void setX(int _x) { x = _x; }
    void setY(int _y) { y = _y; }
    int getX() const { return x; }
    int getY() const { return y; }
};

class SnakeTail : public Entity {
public:
    SnakeTail() = default;
    SnakeTail(int _x, int _y) : Entity(_x, _y) {}
};

class SnakeHead : public Entity
{
private:
    std::vector<SnakeTail> tail;

public:
    SnakeHead() = default;
    SnakeHead(int _x, int _y) : Entity(_x, _y) {}

    // Method to return a non-const reference to tail
    inline std::vector<SnakeTail>& getTail() { return tail; }

    // Optionally, keep const getter for read-only access if needed elsewhere
    inline const std::vector<SnakeTail>& getTail() const { return tail; }
};

class Empty : public Entity
{
public:
    Empty() = default;
    Empty(int _x, int _y) : Entity(_x, _y) {}
};

class Food : public Entity
{
public:
    Food() = default;
    Food(int _x, int _y) : Entity(_x, _y) {}
};

class SnakeGL
{
private:
    SnakeHead head;
    std::array<Entity, WIDTH* HEIGHT> grid;  // Using Empty instead of Entity
    INPUT_TYPE currentDirection = UP;
    Food food;

public:
    SnakeGL();
    void updateSnake();
    void handleInput(INPUT_TYPE inputType);

    const inline SnakeHead& getHead() const { return head; }
    const inline std::vector<SnakeTail>& getTail() const { return head.getTail(); }
    const inline std::array<Entity, WIDTH* HEIGHT>& getGrid() const { return grid; }
    const INPUT_TYPE getDir() { return currentDirection; }
    const inline Food& getFood() const { return food; }
};
// ----------------------------------------------------------

// Function definition
// ----------------------------------------------------------
int main(void)
{
    SnakeGL snake{};

    // Initialize window
    bool windowInitialized = initializeWindow();
    if (!windowInitialized) return -1;

    // Initialize vertex buffer
    bool vertexbufferInitialized = initializeVertexbuffer();
    if (!vertexbufferInitialized) return -1;

    // Create and compile our GLSL program from the shaders
    programID = LoadShaders("SimpleVertexShader.vertexshader", "SimpleFragmentShader.fragmentshader");

    auto lastUpdateTime = std::chrono::steady_clock::now();
    int timeoutDuration = 150; // Timeout duration in milliseconds

    // Start animation loop until escape key is pressed
    do
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime).count();

        timeoutDuration = gameSpeed(timeoutDuration);

        if (elapsedTime >= timeoutDuration) {
            INPUT_TYPE dir = snake.getDir();
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dir = UP;
            else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dir = DOWN;
            else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dir = LEFT;
            else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dir = RIGHT;

            snake.handleInput(dir);
            snake.updateSnake();
            updateAnimationLoop(snake);

            // Reset the last update time
            lastUpdateTime = currentTime; // Reset last update time
        }
        else {
            // Sleep for a short duration to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } // Check if the ESC key was pressed or the window was closed
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
        glfwWindowShouldClose(window) == 0);

    // Cleanup and close window
    cleanupVertexbuffer();
    glDeleteProgram(programID);
    closeWindow();

    return 0;
}

void updateAnimationLoop(SnakeGL& snake) {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the shader program
    glUseProgram(programID);

    // Calculate the normalized dimensions for each cell
    float cellWidth = 2.0f / WIDTH;  // Normalized width of each cell
    float cellHeight = 2.0f / HEIGHT; // Normalized height of each cell

    // Calculate offset to ensure the whole map is visible
    float offsetX = -1.0f + (cellWidth / 2);
    float offsetY = 1.0f - (cellHeight / 2);

    // Process input only when a valid key is pressed (check input)
    INPUT_TYPE dir = snake.getDir(); // Get the current direction

    // Check for key presses in the current frame to avoid "random" movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dir = UP;
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dir = DOWN;
    else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dir = LEFT;
    else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dir = RIGHT;

    snake.handleInput(dir); // Handle the updated input

    snake.updateSnake(); // Update the snake's state (movement)

    // Draw all the cells on the grid
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            glm::vec3 cellColor(0.0f, 0.0f, 0.0f); // Default cell color (black)

            // Check if the current cell is the snake's head
            if (snake.getHead().getX() == x && snake.getHead().getY() == y) {
                cellColor = glm::vec3(0.0f, 0.8f, 0.4f); // Green for the snake's head
            }
            // Check if the current cell is part of the snake's body
            else if (std::any_of(snake.getHead().getTail().begin(), snake.getHead().getTail().end(),
                [x, y](const SnakeTail& tailSeg) {
                    return tailSeg.getX() == x && tailSeg.getY() == y;
                })) {
                cellColor = glm::vec3(0.0f, 1.0f, 0.4f); // Green for the snake's body
            }
            else if (snake.getFood().x == x && snake.getFood().y == y)
            {
                cellColor = glm::vec3(1.0f, 0.0f, 0.0f); // Red for the food
            }

            // Calculate the position of the cell in normalized coordinates
            float xPos = (x * cellWidth) + offsetX;
            float yPos = offsetY - (y * cellHeight); // Flip Y to match OpenGL coordinates

            drawCell(xPos, yPos, cellColor); // Draw the cell at the computed position
        }
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // Set the window size back to the fixed size if resized
    glfwSetWindowSize(window, WINDOW_WIDTH, WINDOW_HEIGHT);
}

bool initializeWindow()
{
    // Initialise GLFW
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        getchar();
        return false;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Open a window and create its OpenGL context
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "SnakeGL", NULL, NULL);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
        getchar();
        glfwTerminate();
        return false;
    }

    // Set the callback to prevent resizing
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwMakeContextCurrent(window);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return false;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    // Dark blue background
    glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

    return true;
    }

bool initializeVertexbuffer()
{
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    GLfloat cellSize = 1.0f;

    // Rectangle composed of two triangles
    static const GLfloat g_vertex_buffer_data[] = {
        -0.5f, -0.5f, 0.0f,  // Bottom left
         0.5f, -0.5f, 0.0f,  // Bottom right
         0.5f,  0.5f, 0.0f,  // Top right
        -0.5f, -0.5f, 0.0f,  // Bottom left
         0.5f,  0.5f, 0.0f,  // Top right
        -0.5f,  0.5f, 0.0f   // Top left
    };

    glGenBuffers(1, &vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

    return true;
}

bool cleanupVertexbuffer()
{
    // Cleanup VBO
    glDeleteBuffers(1, &vertexbuffer);
    glDeleteVertexArrays(1, &VertexArrayID);
    return true;
}

bool closeWindow()
{
    glfwTerminate();
    return true;
}

void drawCell(float x, float y, const glm::vec3& color) {
    // Set the color uniform
    glUniform3f(glGetUniformLocation(programID, "inputColor"), color.r, color.g, color.b);

    // Set the position uniform
    glUniform2f(glGetUniformLocation(programID, "cellPosition"), x, y);

    // Enable the vertex attribute array and bind the buffer
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Draw the cell as a rectangle
    glDrawArrays(GL_TRIANGLES, 0, 6); // Draw two triangles to form a rectangle

    glDisableVertexAttribArray(0);
}

void inline setColor(float r, float g, float b)
{
    glUniform3f(glGetUniformLocation(programID, "ourColor"), r, g, b);
}
// ----------------------------------------------------------

// Class definitions 
// ----------------------------------------------------------
SnakeGL::SnakeGL() : head(WIDTH / 2, HEIGHT / 2)
{
    std::for_each(grid.begin(), grid.end(), [](auto& item)
        {
            item = Empty();
        });

    grid[head.x, head.y] = head;

    std::random_device rd;
    std::mt19937 gen(rd()); // Mersenne Twister engine
    std::uniform_int_distribution<int> distX(4, WIDTH - 2);
    std::uniform_int_distribution<int> distY(5, HEIGHT - 2);
    int rndXPos = distX(gen);
    int rndYPos = distY(gen);

    food = Food(rndXPos, rndYPos);
    //std::cout << "Food_X: " << food.x << " Food_Y: " << food.y << std::endl; //to check the Pos
}

void SnakeGL::updateSnake()
{
    int newX = head.getX();
    int newY = head.getY();

    switch (currentDirection)
    {
    case UP:
        newY = (newY - 1 + HEIGHT) % HEIGHT;
        break;
    case DOWN:
        newY = (newY + 1) % HEIGHT;
        break;
    case LEFT:
        newX = (newX - 1 + WIDTH) % WIDTH;
        break;
    case RIGHT:
        newX = (newX + 1) % WIDTH;
        break;
    }



    // Collision detection with tail
    if (std::any_of(head.getTail().begin(), head.getTail().end(),
        [newX, newY](const SnakeTail& segment) {
            return segment.getX() == newX && segment.getY() == newY;
        })) {
        std::cout << "Game Over!! -- Your Score: " << score << std::endl; ////////////////////////////////////////////////////
        exit(0); // Exit on collision
    }

    // Update tail positions
    for (size_t i = head.getTail().size(); i > 1; --i) {
        head.getTail()[i - 1].setX(head.getTail()[i - 2].getX());
        head.getTail()[i - 1].setY(head.getTail()[i - 2].getY());
    }
    if (!head.getTail().empty()) {
        head.getTail()[0].setX(head.getX());
        head.getTail()[0].setY(head.getY());
    }

    // Update head position
    head.setX(newX);
    head.setY(newY);
    grid[newY * WIDTH + newX] = head;

    // Check if the snake has eaten the food ////////////////////////////////////////////////////////////// HEREE SCORE GETS UPDATED
    if (newY == food.getY() && newX == food.getX())
    {
        std::cout << "Score: " << ++score << std::endl;

        // Spawn new food
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> distX(4, WIDTH - 2);
        std::uniform_int_distribution<int> distY(5, HEIGHT - 2);
        food.setX(distX(gen));
        food.setY(distY(gen));

        //std::cout << "Food_X: " << food.x << " Food_Y: " << food.y << std::endl; //to check the Pos

        // Extend tail by adding a new segment at the end
        head.getTail().push_back(SnakeTail(newX, newY));
    }
}

void SnakeGL::handleInput(INPUT_TYPE inputType)
{
    // Prevent reversing direction: only update the direction if the new input isn't opposite of the current direction
    if ((inputType == UP && currentDirection != DOWN) ||
        (inputType == DOWN && currentDirection != UP) ||
        (inputType == LEFT && currentDirection != RIGHT) ||
        (inputType == RIGHT && currentDirection != LEFT)) {
        currentDirection = inputType;
    }
}

int gameSpeed(int speedValue) {
    std::cout << speedValue << std::endl;
    int decreaseAmount = 0;

    if (score % 5 == 0 && score != lastMultipleOfFive) {
        decreaseAmount = 5;
        lastMultipleOfFive = score;
    }

    return std::max(90, speedValue - decreaseAmount);
}