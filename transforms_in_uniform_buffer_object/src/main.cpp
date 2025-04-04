// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
// clang-format on


// TODO: next we have to make the number of ubos variable to do further testing

// Function to compile shaders
GLuint compile_shader(const char *source, GLenum type) {
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
GLuint create_shader_program(const char *vertex_source,
                             const char *fragment_source) {

  GLuint vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
  GLuint fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(program, 512, nullptr, infoLog);
    std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <num_objects>\n";
    return 1;
  }

  int num_objects = std::atoi(argv[1]);
  if (num_objects <= 0) {
    std::cerr << "Error: num_objects must be a positive integer.\n";
    return 1;
  }

  std::cout << "Number of objects: " << num_objects << '\n';

  GLuint VAO, VBO, shader_program, UBO_0, UBO_1;
  GLfloat triangle_vertices[num_objects * 2 * 9]; // 3 vertices per triangle, 9
                                              // components per triangle

  // Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return -1;
  }

  // Create a windowed mode window and its OpenGL context
  GLFWwindow *window = glfwCreateWindow(
      800, 600, "transforms in uniform buffer", nullptr, nullptr);
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

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);

  for (int i = 0; i < num_objects * 2; ++i) {
    // Define a small triangle centered at (0, 0)
    float scale = 0.02;
    GLfloat triangle[9] = {
        0.0f,          1.0f * scale,  0.0f, // Vertex 1
        -1.0f * scale, -1.0f * scale, 0.0f, // Vertex 2
        1.0f * scale,  -1.0f * scale, 0.0f  // Vertex 3
    };

    for (int j = 0; j < 3; ++j) {
      triangle_vertices[i * 9 + j * 3] = triangle[j * 3];
      triangle_vertices[i * 9 + j * 3 + 1] = triangle[j * 3 + 1];
      triangle_vertices[i * 9 + j * 3 + 2] = triangle[j * 3 + 2];
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_vertices), triangle_vertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                        (GLvoid *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  std::string shader_code =
      "#version 330 core\n"
      "layout (location = 0) in vec3 position;\n"
      "uniform mat4 projection;\n"
      "uniform mat4 view;\n"

      "layout(std140) uniform ModelMatrices0 {\n"
      "    mat4 modelMatrices0[" +
      std::to_string(num_objects) +
      "];\n"
      "};\n"
      "layout(std140) uniform ModelMatrices1 {\n"
      "    mat4 modelMatrices1[" +
      std::to_string(num_objects) +
      "];\n"
      "};\n"

      "void main() {\n"
      "    int triangleIndex = gl_VertexID / 3;\n"
      "    mat4 model;\n"
      "    if (triangleIndex < " +
      std::to_string(num_objects) +
      ") {\n"
      "        model = modelMatrices0[triangleIndex];\n"
      "    } else {\n"
      "        model = modelMatrices1[triangleIndex - " +
      std::to_string(num_objects) +
      "];\n"
      "    }\n"
      "    gl_Position = projection * view * model * vec4(position, 1.0);\n"
      "}";

  const char *vertex_shader_source = shader_code.c_str();

  const char *fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f); // Green color for the triangles
        }
    )";

  // Create shader program
  shader_program =
      create_shader_program(vertex_shader_source, fragmentShaderSource);

  glm::mat4 model_matrices0[num_objects];
  glm::mat4 model_matrices1[num_objects];

  auto total_num_objects = num_objects * 2;

  int grid_size_x =
      static_cast<int>(ceil(sqrt(num_objects))); // Number of columns
  int grid_size_y = static_cast<int>(
      ceil(float(num_objects) / grid_size_x)); // Number of rows

  // The max grid size in NDC space is [-1, 1] for both axes
  float x_spacing =
      2.0f / (grid_size_x - 1); // Space between objects along the x-axis
  float y_spacing =
      2.0f / (grid_size_y - 1); // Space between objects along the y-axis

  for (int i = 0; i < num_objects; ++i) {
    // Calculate row and column positions based on the index
    int row = i / grid_size_x;
    int col = i % grid_size_x;

    // Map the grid positions to NDC space
    float x = -1.0f + col * x_spacing;
    float y = -1.0f + row * y_spacing;

    // Original triangle positions
    glm::mat4 model0 = glm::mat4(1.0f);
    model0 = glm::translate(model0, glm::vec3(x, y, 0.0f));
    model0 = glm::scale(model0, glm::vec3(0.3f));
    model_matrices0[i] = model0;

    // Offset triangle positions — shifted by half a cell
    float x_offset = x + x_spacing * 0.5f;
    float y_offset = y + y_spacing * 0.5f;

    glm::mat4 model1 = glm::mat4(1.0f);
    model1 = glm::translate(model1, glm::vec3(x_offset, y_offset, 0.0f));
    model1 = glm::scale(model1, glm::vec3(0.8f));
    model_matrices1[i] = model1;
  }

  // Create and bind the UBO
  glGenBuffers(1, &UBO_0);
  glBindBuffer(GL_UNIFORM_BUFFER, UBO_0);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(model_matrices0), model_matrices0,
               GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO_0); // Bind to binding point 0

  glGenBuffers(1, &UBO_1);
  glBindBuffer(GL_UNIFORM_BUFFER, UBO_1);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(model_matrices1), model_matrices1,
               GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, UBO_1); // Bind to binding point 1

  // Use the shader program
  glUseProgram(shader_program);

  GLuint block_index_0 =
      glGetUniformBlockIndex(shader_program, "ModelMatrices0");
  GLuint block_index_1 =
      glGetUniformBlockIndex(shader_program, "ModelMatrices1");

  glUniformBlockBinding(shader_program, block_index_0, 0);
  glUniformBlockBinding(shader_program, block_index_1, 1);

  // Set the projection and view matrices
  glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f,
                                    1.0f); // Orthogonal projection
  glm::mat4 view = glm::mat4(1.0f);        // Identity view matrix

  // Set the uniform variables
  GLint projectionLoc = glGetUniformLocation(shader_program, "projection");
  GLint viewLoc = glGetUniformLocation(shader_program, "view");

  glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Process input
    glfwPollEvents();

    // Clear the screen
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw the triangles
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3 * num_objects * 2); 
    glBindVertexArray(0);

    // Swap buffers
    glfwSwapBuffers(window);
  }

  // Clean up
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &UBO_0);
  glDeleteProgram(shader_program);

  glfwTerminate();
  return 0;
}
