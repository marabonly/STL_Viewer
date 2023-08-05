#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "textures/stb_image.h"

#define ASSERT(x) if (!(x)) __debugbreak();

float* modelPositions = new float[0];
int modelPositionsLength = 0;
int modelTrianglesNumber{ 0 };

float rotCentreX{ 0 };
float rotCentreY{ 0 };
float rotCentreZ{ 0 };

float rotAngleX{ 0 };
float rotAngleY{ 0 };

float moveDeltaX{ 0 };
float moveDeltaY{ 0 };

double currentMouseXpos{ -1.0 };
double currentMouseYpos{ -1.0 };

double mouseStartXpos{ -1.0 };
double mouseStartYpos{ -1.0 };
double mouseScroll{ 1.0 };

bool rightMouseButtonPressed{ false };
bool middleMouseButtonPressed{ false };
bool toDoOptimiseView{ true };

int glContextWidth{ 1024 };
int glContextHeight{ 768 };

float glContextScaleX{ 1.0f };
float glContextScaleY{ 1.0f };

unsigned int modelVertexArray{ 0 };
unsigned int modelVertexBuffer{ 0 };
unsigned int modelTransformFeedback{ 0 };

unsigned int textVertexArray{ 0 };
unsigned int textVertexBuffer{ 0 };

static void GLClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

static void GLCheckError()
{
    while (GLenum error = glGetError())
    {
        std::cout << "[OpenGL Error] (" << error << ")" << std::endl;
    }
}

struct ShaderProgramSource
{
    std::string VertexSource;
    std::string FragmentSource;
};

static ShaderProgramSource ParseShader(const std::string& filepath)
{
    std::ifstream stream(filepath);

    enum class ShaderType
    {
        NONE = -1, VERTEX = 0, FRAGMENT = 1
    };
    ShaderType type = ShaderType::NONE;

    std::string line;

    std::stringstream ss[2];

    while (getline(stream, line))
    {
        if (line.find("#shader") != std::string::npos)
        {
            if (line.find("vertex") != std::string::npos)
                type = ShaderType::VERTEX;
            else if (line.find("fragment") != std::string::npos)
                type = ShaderType::FRAGMENT;
        }
        else
        {
            if (type != ShaderType::NONE) ss[(int)type] << line << '\n';
        }
    }

    return { ss[0].str(), ss[1].str() };
}

static unsigned int CompileShader(unsigned int type, const std::string& source)
{
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    // Error handling
    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* message = (char*)alloca(length * sizeof(char));
        glGetShaderInfoLog(id, length, &length, message);
        std::cout << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader!" << std::endl;
        std::cout << message << std::endl;
        glDeleteShader(id);
        return 0;
    }

    return id;
}

static unsigned int CreateShader(const std::string& vertexShader)
{
    unsigned int program = glCreateProgram();
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);

    glAttachShader(program, vs);

    const char* feedbackValues[1] = { "posXYZ" };
    glTransformFeedbackVaryings(program, 1, feedbackValues, GL_INTERLEAVED_ATTRIBS);

    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vs);

    return program;
}

static unsigned int CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
{
    unsigned int program = glCreateProgram();
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    currentMouseXpos = xpos;
    currentMouseYpos = ypos;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
        toDoOptimiseView = true;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (action == GLFW_PRESS)
        {
            middleMouseButtonPressed = true;
            mouseStartXpos = -1.0;
            mouseStartYpos = -1.0;
        }
        else
        {
            middleMouseButtonPressed = false;
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            rightMouseButtonPressed = true;
            mouseStartXpos = -1.0;
            mouseStartYpos = -1.0;
        }
        else
        {
            rightMouseButtonPressed = false;
        }
    }
}

void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    float sensitivity{ 0.1f };
    mouseScroll += yoffset * sensitivity;
}

void Rotate3DModel(GLFWwindow* window)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if ((mouseStartXpos < 0) or (mouseStartYpos < 0))
    {
        mouseStartXpos = xpos;
        mouseStartYpos = ypos;
        return;
    }

    float sensitivity{ 0.01f };

    rotAngleX = (ypos - mouseStartYpos) * sensitivity;
    rotAngleY = (xpos - mouseStartXpos) * sensitivity;

    mouseStartXpos = xpos;
    mouseStartYpos = ypos;
}

void Move3DModel(GLFWwindow* window)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if ((mouseStartXpos < 0) or (mouseStartYpos < 0))
    {
        mouseStartXpos = xpos;
        mouseStartYpos = ypos;
        return;
    }

    moveDeltaX = xpos - mouseStartXpos;
    moveDeltaY = mouseStartYpos - ypos;

    mouseStartXpos = xpos;
    mouseStartYpos = ypos;
}

void OptimiseView(glm::mat4& view, glm::mat4* proj)
{
    float minX, maxX, minY, maxY, minZ, maxZ;
    float largestDimension;
    float centreX, centreY;

    int triangleCount{ 0 };

    glm::vec3 vertexPosition;

    if (modelTrianglesNumber < 1)
    {
        minX = maxX = minY = maxY = minZ = maxZ = 0;
    }
    else
    {
        vertexPosition = { modelPositions[0], modelPositions[1], modelPositions[2] };

        minX = maxX = vertexPosition.x;
        minY = maxY = vertexPosition.y;
        minZ = maxZ = vertexPosition.z;
    }

    while (triangleCount < modelTrianglesNumber)
    {
        for (int initialPosition : {0, 3, 6})
        {
            vertexPosition = {
                modelPositions[triangleCount * 3 * 3 + initialPosition],
                modelPositions[triangleCount * 3 * 3 + initialPosition + 1],
                modelPositions[triangleCount * 3 * 3 + initialPosition + 2]
            };

            if (vertexPosition.x < minX) minX = vertexPosition.x;
            if (vertexPosition.x > maxX) maxX = vertexPosition.x;

            if (vertexPosition.y < minY) minY = vertexPosition.y;
            if (vertexPosition.y > maxY) maxY = vertexPosition.y;

            if (vertexPosition.z < minZ) minZ = vertexPosition.z;
            if (vertexPosition.z > maxZ) maxZ = vertexPosition.z;
        }
        triangleCount++;
    }

    centreX = minX + (maxX - minX) / 2.0f;
    centreY = minY + (maxY - minY) / 2.0f;

    if ((maxX - minX) >= (maxY - minY))
        largestDimension = maxX - minX;
    else
        largestDimension = maxY - minY;

    minX = centreX - largestDimension / 2.0f;
    maxX = centreX + largestDimension / 2.0f;

    minY = centreY - largestDimension / 2.0f;
    maxY = centreY + largestDimension / 2.0f;

    *proj = glm::ortho(minX, maxX, minY, maxY, -minZ - 10.0f * largestDimension, -maxZ + 10.0f * largestDimension);
    
    glContextScaleX = (maxX - minX) / glContextWidth;
    glContextScaleY = (maxY - minY) / glContextHeight;
}

void log(const std::string& string)
{
    std::cout << string << std::endl;
}

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    std::ifstream stream(paths[0], std::ios::binary);

    char header[80];
    stream.read(header, 80);

    int tempTrianglesNumber;
    stream.read((char*)&tempTrianglesNumber, 4);

    if ((tempTrianglesNumber < 1) || (tempTrianglesNumber > 1E8))
    {
        return;
    }

    modelTrianglesNumber = tempTrianglesNumber;

    struct triangle
    {
        float normalVector[3];
        float vertex1[3];
        float vertex2[3];
        float vertex3[3];

        short int attributeByteCount;
    };

    modelPositionsLength = modelTrianglesNumber * 3 * 3;

    delete[] modelPositions;

    modelPositions = new float[modelPositionsLength];

    float minX, maxX, minY, maxY, minZ, maxZ;

    minX = maxX = minY = maxY = minZ = maxZ = 0;

    int triangleCount{ 0 };
    triangle triangleBuf;
    int positionsArrayIndex{ 0 };
    float coordinate;

    while ((triangleCount < modelTrianglesNumber) && (stream.read((char*)&triangleBuf, 50)))
    {
        if (triangleCount == 0)
        {
            minX = maxX = triangleBuf.vertex1[0];
            minY = maxY = triangleBuf.vertex1[1];
            minZ = maxZ = triangleBuf.vertex1[2];
        }

        for (int i = 0; i < 9; i++)
        {
            coordinate = *(triangleBuf.vertex1 + i);

            if ((i == 0) || (i == 3) || (i == 6))
            {
                if (coordinate < minX) minX = coordinate;
                if (coordinate > maxX) maxX = coordinate;
            }
            else if ((i == 1) || (i == 4) || (i == 7))
            {
                if (coordinate < minY) minY = coordinate;
                if (coordinate > maxY) maxY = coordinate;
            }
            else
            {
                if (coordinate < minZ) minZ = coordinate;
                if (coordinate > maxZ) maxZ = coordinate;
            }

            modelPositions[positionsArrayIndex++] = coordinate;
        }

        triangleCount++;
    }

    rotCentreX = minX + (maxX - minX) / 2.0f;
    rotCentreY = minY + (maxY - minY) / 2.0f;
    rotCentreZ = minZ + (maxZ - minZ) / 2.0f;

    glDeleteBuffers(1, &modelVertexBuffer);
    glDeleteBuffers(1, &modelTransformFeedback);
    glDeleteVertexArrays(1, &modelVertexArray);

    glFlush();
    glFinish();

    glGenVertexArrays(1, &modelVertexArray);
    glGenBuffers(1, &modelVertexBuffer);
    glGenBuffers(1, &modelTransformFeedback);

    glBindVertexArray(modelVertexArray);

    glBindBuffer(GL_ARRAY_BUFFER, modelVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, modelPositionsLength * sizeof(float), modelPositions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, modelTransformFeedback);
    glBufferData(GL_ARRAY_BUFFER, modelPositionsLength * sizeof(float), nullptr, GL_STATIC_READ);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, modelTransformFeedback);

    toDoOptimiseView = true;
}

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int count;
    int windowWidth, windowHeight;
    int monitorX, monitorY;

    GLFWmonitor** monitors = glfwGetMonitors(&count);
    const GLFWvidmode* videoMode = glfwGetVideoMode(monitors[0]);

    glfwGetMonitorPos(monitors[0], &monitorX, &monitorY);

    // (1).  Set the visibility window hint to false for subsequent window creation
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(glContextWidth, glContextHeight, "STL Viewer", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // (2).  Reset the window hints to default
    glfwDefaultWindowHints();

    glfwSetWindowPos(window,
        monitorX + (videoMode->width - glContextWidth) / 2,
        monitorY + (videoMode->height - glContextHeight) / 2);

    // (3).  Show the window
    glfwShowWindow(window);

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK)
        return -1;

    glfwSwapInterval(1);

    glfwSetDropCallback(window, drop_callback);
    
    ShaderProgramSource sourceModelDraw = ParseShader("res/shaders/ModelDraw.shader");
    unsigned int shaderModelDraw = CreateShader(sourceModelDraw.VertexSource, sourceModelDraw.FragmentSource);
    glUseProgram(shaderModelDraw);

    int locationProjAtModelDraw = glGetUniformLocation(shaderModelDraw, "proj");
    ASSERT(locationProjAtModelDraw != -1);

    int locationViewAtModelDraw = glGetUniformLocation(shaderModelDraw, "view");
    ASSERT(locationViewAtModelDraw != -1);

    int locationColor = glGetUniformLocation(shaderModelDraw, "inColor");
    ASSERT(locationColor != -1);

    float modelColor[4] = { 0.2f, 0.3f, 0.8f, 1.0f };
    float edgesColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    ShaderProgramSource sourceTransformFeedback = ParseShader("res/shaders/TransformFeedback.shader");
    unsigned int shaderTransformFeedback = CreateShader(sourceTransformFeedback.VertexSource);
    glUseProgram(shaderTransformFeedback);

    int locationViewAtTransformFeedback = glGetUniformLocation(shaderTransformFeedback, "view");
    ASSERT(locationViewAtTransformFeedback != -1);
    
    ShaderProgramSource sourceTextDraw = ParseShader("res/shaders/TextDraw.shader");
    unsigned int shaderTextDraw = CreateShader(sourceTextDraw.VertexSource, sourceTextDraw.FragmentSource);
    glUseProgram(shaderTextDraw);

    glUniform1i(glGetUniformLocation(shaderTextDraw, "textureImage"), 0);
    
    float dimension{ 300.0f };

    glm::mat4 proj = glm::ortho(-dimension, dimension, -dimension, dimension, -20.0f * dimension, 20.0f * dimension);
    glm::mat4 view = glm::mat4(1.0f);

    glContextScaleX = 2 * dimension / glContextWidth;
    glContextScaleY = 2 * dimension / glContextHeight;

    // Checking input (keyboard, mouse)
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    glEnable(GL_DEPTH_TEST);
    
    // Background vertices

    const int textPositionsLength{ 2 * 3 * 2 * 2 };
    float textPositions[textPositionsLength] =
    {
        // positions    // texture coordinates
         1.0f,  1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f
    };
    int trianglesNumberText{ 2 };

    glGenVertexArrays(1, &textVertexArray);
    glGenBuffers(1, &textVertexBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, textVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, textPositionsLength * sizeof(float), textPositions, GL_STATIC_DRAW);

    glBindVertexArray(textVertexArray);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Background texture

    unsigned int texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // load and generate the texture
    int width, height, nrChannels;
    unsigned char* data = stbi_load("res/background.jpg", &width, &height, &nrChannels, 0);

    /*
    int i{ 0 };
    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
            data[i++] = 125;
    */

    if (data)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        if (moveDeltaX != 0)
        {
            proj = glm::translate(proj, glm::vec3(1.0f, 0, 0) * moveDeltaX * glContextScaleX);
        }
        if (moveDeltaY != 0)
        {
            proj = glm::translate(proj, glm::vec3(0, 1.0f, 0) * moveDeltaY * glContextScaleY);
        }
        if (rotAngleX != 0)
        {
            view = glm::translate(view, glm::vec3(rotCentreX, rotCentreY, rotCentreZ));
            view = glm::rotate(view, rotAngleX, glm::vec3(view[0].x, view[1].x, view[2].x));
            view = glm::translate(view, glm::vec3(-rotCentreX, -rotCentreY, -rotCentreZ));
        }
        if (rotAngleY != 0)
        {
            view = glm::translate(view, glm::vec3(rotCentreX, rotCentreY, rotCentreZ));
            view = glm::rotate(view, rotAngleY, glm::vec3(view[0].y, view[1].y, view[2].y));
            view = glm::translate(view, glm::vec3(-rotCentreX, -rotCentreY, -rotCentreZ));
        }
        if (mouseScroll != 1.0)
        {
            glm::vec4 originOnScreen = proj * glm::vec4(0, 0, 0, 1.0f);

            float scaleFactor = (float)mouseScroll - 1.0f;

            float mouseOnScreenX = currentMouseXpos / glContextWidth * 2.0f - 1.0f;
            float mouseOnScreenY = -1 * (currentMouseYpos / glContextHeight * 2.0f - 1.0f);

            float offsetToScreenCentreX = originOnScreen.x * (glContextWidth / 2.0f) * scaleFactor;
            float offsetToScreenCentreY = originOnScreen.y * (glContextHeight / 2.0f) * scaleFactor;

            float factorTowardsCentre = 1.5f;

            float offsetMouseX = mouseOnScreenX * (glContextWidth / 2.0f) * scaleFactor * factorTowardsCentre;
            float offsetMouseY = mouseOnScreenY * (glContextHeight / 2.0f) * scaleFactor * factorTowardsCentre;

            // Scaling
            
            proj = glm::scale(proj, glm::vec3(mouseScroll));

            glContextScaleX /= mouseScroll;
            glContextScaleY /= mouseScroll;

            // Translation

            proj = glm::translate(proj, glm::vec3(offsetToScreenCentreX * glContextScaleX, offsetToScreenCentreY * glContextScaleY, 0.0));
            proj = glm::translate(proj, glm::vec3(-offsetMouseX * glContextScaleX, -offsetMouseY * glContextScaleY, 0.0));
        }
        
        // Background

        glUseProgram(shaderTextDraw);

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(textVertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Model
        
        glBindVertexArray(modelVertexArray);
        glEnableVertexAttribArray(0);

        if (toDoOptimiseView)
        {
            glUseProgram(shaderTransformFeedback);

            glUniformMatrix4fv(locationViewAtTransformFeedback, 1, GL_FALSE, &view[0][0]);

            glBeginTransformFeedback(GL_TRIANGLES);
            glDrawArrays(GL_TRIANGLES, 0, modelTrianglesNumber * 3);
            glEndTransformFeedback();

            glFlush();

            glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, modelPositionsLength * sizeof(float), modelPositions);

            OptimiseView(view, &proj);
            toDoOptimiseView = false;
        }
        
        glUseProgram(shaderModelDraw);
        
        glClear(GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(locationProjAtModelDraw, 1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(locationViewAtModelDraw, 1, GL_FALSE, &view[0][0]);
        
        glUniform4fv(locationColor, 1, &modelColor[0]);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLES, 0, modelTrianglesNumber * 3);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glUniform4fv(locationColor, 1, &edgesColor[0]);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawArrays(GL_TRIANGLES, 0, modelTrianglesNumber * 3);
        
        glDisableVertexAttribArray(0);
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        moveDeltaX = 0;
        moveDeltaY = 0;
        rotAngleX = 0;
        rotAngleY = 0;
        mouseScroll = 1.0;

        /* Poll for and process events */
        glfwPollEvents();

        if (middleMouseButtonPressed) Rotate3DModel(window);
        if (rightMouseButtonPressed) Move3DModel(window);
    }

    glDeleteProgram(shaderModelDraw);

    delete[] modelPositions;

    glfwTerminate();
    return 0;
}
