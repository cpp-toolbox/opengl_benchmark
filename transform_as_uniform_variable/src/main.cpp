#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

const int numTriangles = 100;

GLuint VAO, VBO, shaderProgram;
GLfloat triangleVertices[numTriangles * 9]; // 3 vertices per triangle, 9 components per triangle

// Function to compile shaders
GLuint CompileShader(const char *source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

// Function to create the shader program
GLuint CreateShaderProgram(const char *vertexSource, const char *fragmentSource) {
    GLuint vertexShader = CompileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = CompileShader(fragmentSource, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create a windowed mode window and its OpenGL context
    GLFWwindow *window = glfwCreateWindow(800, 600, "transform as uniform variable", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); 

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Set up vertex data and buffers for 100 triangles
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    // Generate the vertex data for 100 triangles
    for (int i = 0; i < numTriangles; ++i) {
        // Define a small triangle centered at (0, 0)
        GLfloat triangle[9] = {
            0.0f,  0.1f,  0.0f, // Vertex 1
            -0.1f, -0.1f, 0.0f, // Vertex 2
            0.1f,  -0.1f, 0.0f  // Vertex 3
        };

        for (int j = 0; j < 3; ++j) {
            triangleVertices[i * 9 + j * 3] = triangle[j * 3];
            triangleVertices[i * 9 + j * 3 + 1] = triangle[j * 3 + 1];
            triangleVertices[i * 9 + j * 3 + 2] = triangle[j * 3 + 2];
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangleVertices), triangleVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Define shaders
    const char *vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 position;
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 modelMatrices[100];  // Model matrices for each triangle

        void main() {
            int triangleIndex = gl_VertexID / 3;  // Calculate the index of the current triangle
            gl_Position = projection * view * modelMatrices[triangleIndex] * vec4(position, 1.0);
        }
    )";

    const char *fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f); // Green color for the triangles
        }
    )";

    // Create shader program
    shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Set up model matrices for each triangle
    glm::mat4 modelMatrices[numTriangles];
    for (int i = 0; i < numTriangles; ++i) {
        float angle = 2.0f * M_PI * i / numTriangles;
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(cos(angle), sin(angle), 0.0f)); // Position around the circle
        model = glm::scale(model, glm::vec3(0.3f));                             // Scale down the triangle
        modelMatrices[i] = model;
    }

    // Use the shader program
    glUseProgram(shaderProgram);

    // Set the projection and view matrices
    glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Orthogonal projection
    glm::mat4 view = glm::mat4(1.0f);                                         // Identity view matrix

    // Set the uniform variables
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");

    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Process input
        glfwPollEvents();

        // Clear the screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        GLint modelMatricesLoc = glGetUniformLocation(shaderProgram, "modelMatrices");

        glUniformMatrix4fv(modelMatricesLoc, numTriangles, GL_FALSE, glm::value_ptr(modelMatrices[0]));

        // Draw the triangles
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3 * numTriangles); // Draw 100 triangles
        glBindVertexArray(0);

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
