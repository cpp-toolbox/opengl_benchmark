// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
// clang-format on

int window_width = 1920;
int window_height = 1080;

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

void generate_model_matrices(glm::mat4 *model_matrices, int num_objects,
                             glm::vec3 origin) {
  // Calculate grid size for a perfect cube
  int grid_size = static_cast<int>(ceil(pow(num_objects, 1.0f / 3.0f)));

  // Ensure grid size is at least 2 to prevent holes
  grid_size = std::max(grid_size, 2);

  // Spacing for the grid
  float spacing = 2.0f / (grid_size - 1);

  // Generate matrices
  for (int i = 0; i < num_objects; ++i) {
    int layer = i / (grid_size * grid_size);             // z-axis
    int row = (i % (grid_size * grid_size)) / grid_size; // y-axis
    int col = i % grid_size;                             // x-axis

    // Map the grid positions to NDC space
    float x = origin.x + col * spacing;
    float y = origin.y + row * spacing;
    float z = origin.z + layer * spacing;

    // Apply the transformation for the current model
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, z));
    model = glm::scale(model, glm::vec3(0.3f)); // Scale the object

    // Store the model matrix
    model_matrices[i] = model;
  }
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

  int total_num_objects = num_objects * 4;

  std::cout << "Number of objects: " << num_objects << '\n';

  GLuint VAO, VBO, shader_program, UBO_0, UBO_1, UBO_2, UBO_3;
  GLfloat triangle_vertices[total_num_objects * 9]; // 3 vertices per triangle,
                                                    // 9 components per triangle

  // Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return -1;
  }

  // Create a windowed mode window and its OpenGL context
  GLFWwindow *window =
      glfwCreateWindow(window_width, window_height,
                       "transforms in uniform buffer", nullptr, nullptr);
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

  for (int i = 0; i < total_num_objects; ++i) {
    // Define a small triangle centered at (0, 0)
    float scale = 0.10;
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

      "uniform float camera_rotation_angle;\n"

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
      "layout(std140) uniform ModelMatrices2 {\n"
      "    mat4 modelMatrices2[" +
      std::to_string(num_objects) +
      "];\n"
      "};\n"
      "layout(std140) uniform ModelMatrices3 {\n"
      "    mat4 modelMatrices3[" +
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
      "    } else if (triangleIndex < 2 * " +
      std::to_string(num_objects) +
      ") {\n"
      "        model = modelMatrices1[triangleIndex - " +
      std::to_string(num_objects) +
      "];\n"
      "    } else if (triangleIndex < 3 * " +
      std::to_string(num_objects) +
      ") {\n"
      "        model = modelMatrices2[triangleIndex - 2 * " +
      std::to_string(num_objects) +
      "];\n"
      "    } else {\n"
      "        model = modelMatrices3[triangleIndex - 3 * " +
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
  glm::mat4 model_matrices2[num_objects];
  glm::mat4 model_matrices3[num_objects];

  // Define a margin to space out the cubes
  float margin = 0.5f; // Adjust this value as needed for the desired spacing

  // Generate matrices for the first cube, starting from the top-right (1, 1,
  // -1)
  glm::vec3 origin0(1.0f + margin, 1.0f + margin, -1.0f);
  generate_model_matrices(model_matrices0, num_objects, origin0);

  // Generate matrices for the second cube, starting from the top-left (-1, 1,
  // -1)
  glm::vec3 origin1(-1.0f - margin, 1.0f + margin, -1.0f);
  generate_model_matrices(model_matrices1, num_objects, origin1);

  // Generate matrices for the third cube, starting from the bottom-left (-1,
  // -1, -1)
  glm::vec3 origin2(-1.0f - margin, -1.0f - margin, -1.0f);
  generate_model_matrices(model_matrices2, num_objects, origin2);

  // Generate matrices for the fourth cube, starting from the bottom-right (1,
  // -1, -1)
  glm::vec3 origin3(1.0f + margin, -1.0f - margin, -1.0f);
  generate_model_matrices(model_matrices3, num_objects, origin3);

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

  glGenBuffers(2, &UBO_2);
  glBindBuffer(GL_UNIFORM_BUFFER, UBO_2);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(model_matrices2), model_matrices2,
               GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 2, UBO_2); // Bind to binding point 2

  glGenBuffers(3, &UBO_3);
  glBindBuffer(GL_UNIFORM_BUFFER, UBO_3);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(model_matrices3), model_matrices3,
               GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 3, UBO_3); // Bind to binding point 3

  // Use the shader program
  glUseProgram(shader_program);

  GLuint block_index_0 =
      glGetUniformBlockIndex(shader_program, "ModelMatrices0");
  GLuint block_index_1 =
      glGetUniformBlockIndex(shader_program, "ModelMatrices1");
  GLuint block_index_2 =
      glGetUniformBlockIndex(shader_program, "ModelMatrices2");
  GLuint block_index_3 =
      glGetUniformBlockIndex(shader_program, "ModelMatrices3");

  glUniformBlockBinding(shader_program, block_index_0, 0);
  glUniformBlockBinding(shader_program, block_index_1, 1);
  glUniformBlockBinding(shader_program, block_index_2, 2);
  glUniformBlockBinding(shader_program, block_index_3, 3);

  // Set the projection and view matrices
  glm::mat4 projection = glm::perspective(
      glm::radians(80.0f), (float)window_width / window_height, 0.1f, 10.0f);
  glm::mat4 view = glm::mat4(1.0f); // Identity view matrix

  // Set the uniform variables
  GLint projectionLoc = glGetUniformLocation(shader_program, "projection");
  GLint viewLoc = glGetUniformLocation(shader_program, "view");

  glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

  bool paused = false;

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Process input
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
      paused = !paused; // Toggle paused state
      while (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        glfwPollEvents(); // Prevent multiple toggles while holding spacebar
      }
    }

    float radius = 8.0f; // Distance from the origin
    float time = glfwGetTime();
    float cam_x = cos(time) * radius;
    float cam_z = sin(time) * radius;
    glm::vec3 camera_position = glm::vec3(cam_x, 1.0f, cam_z);
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f); // Looking at the origin
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);     // Up direction

    glm::mat4 view = glm::lookAt(camera_position, target, up);

    if (not paused)
      glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // Clear the screen
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw the triangles
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3 * total_num_objects);
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
