// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <array>
// clang-format on

// int window_width = 1920;
// int window_height = 1080;

int window_width = 800;
int window_height = 800;

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

std::string generate_shader_code(int num_ubos,
                                 int size_of_model_matrices_per_ubo) {
  std::string shader_code = "#version 330 core\n"
                            "layout (location = 0) in vec3 position;\n"
                            "uniform mat4 projection;\n"
                            "uniform mat4 view;\n";

  // Declare the UBOs
  for (int i = 0; i < num_ubos; ++i) {
    shader_code += "layout(std140) uniform ModelMatrices" + std::to_string(i) +
                   " {\n"
                   "    mat4 modelMatrices" +
                   std::to_string(i) + "[" +
                   std::to_string(size_of_model_matrices_per_ubo) +
                   "];\n"
                   "};\n";
  }

  // Begin main function
  shader_code += "void main() {\n"
                 "    int triangleIndex = gl_VertexID / 3;\n"
                 "    mat4 model;\n";

  // Generate a generalized if block
  shader_code += "    int block = triangleIndex / " +
                 std::to_string(size_of_model_matrices_per_ubo) + ";\n";
  shader_code += "    int index = triangleIndex % " +
                 std::to_string(size_of_model_matrices_per_ubo) + ";\n";

  // Generate a switch statement
  shader_code += "    switch (block) {\n";
  for (int i = 0; i < num_ubos; ++i) {
    shader_code += "        case " + std::to_string(i) +
                   ": model = modelMatrices" + std::to_string(i) +
                   "[index]; break;\n";
  }
  shader_code += "        default: model = mat4(1.0); break;\n";
  shader_code += "    }\n";

  // Finish the shader
  shader_code +=
      "    gl_Position = projection * view * model * vec4(position, 1.0);\n"
      "}\n";

  return shader_code;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <num_matrices_per_ubo> <num_ubos>\n";
    return 1;
  }

  int num_matrices_per_ubo = std::atoi(argv[1]);
  if (num_matrices_per_ubo <= 0) {
    std::cerr << "Error: num_matrices_per_ubo must be a positive integer.\n";
    return 1;
  }

  int num_ubos = std::atoi(argv[2]);
  if (num_ubos <= 0) {
    std::cerr << "Error: num_ubos must be a positive integer.\n";
    return 1;
  }

  int total_num_objects = num_matrices_per_ubo * num_ubos;

  std::cout << "Number of objects: " << num_matrices_per_ubo << '\n';

  // this needs to be generalized
  GLuint VAO, VBO, shader_program;
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

  // create a triangle for each object
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

  // consider the following code, create a function that, generalize the number
  // of uniform buffer objects also generalize the if statement block to a
  // single computation based on the number of ubos and size of each one
  std::string shader_code =
      generate_shader_code(num_ubos, num_matrices_per_ubo);

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

  // TODO: instead we do a class that has a bounded id generator and use that
  // to add stuff to it internally so we don't have to worry about creating four
  // of these instead they are already internally there and say ok, push this
  // one on and it is just autmatically correct?

  // Use a radius to scale the unit sphere positions
  float radius = 2.f; // Example radius, adjust as needed

  // Generate uniformly distributed origins on a unit sphere
  std::vector<glm::vec3> origins;
  origins.reserve(num_ubos);

  for (int i = 0; i < num_ubos; ++i) {
    float offset = 2.0f / num_ubos;
    float y = i * offset - 1.0f + offset / 2.0f;
    float r = std::sqrt(1.0f - y * y);

    float phi = i * 2.39996323f; // ~Golden angle in radians
    float x = std::cos(phi) * r;
    float z = std::sin(phi) * r;

    // Scale the unit sphere position by the radius
    origins.emplace_back(x * radius, y * radius, z * radius);
  }

  // Allocate space for model matrices on CPU
  std::vector<std::vector<glm::mat4>> ltw_matrices_on_cpu(num_ubos);

  // Generate matrices for each group
  for (int i = 0; i < num_ubos; ++i) {
    ltw_matrices_on_cpu[i].resize(num_matrices_per_ubo);
    generate_model_matrices(ltw_matrices_on_cpu[i].data(), num_matrices_per_ubo,
                            origins[i]);
  }

  // Create and bind UBOs
  std::vector<GLuint> ubos(num_ubos);
  glGenBuffers(num_ubos, ubos.data());

  for (int i = 0; i < num_ubos; ++i) {
    glBindBuffer(GL_UNIFORM_BUFFER, ubos[i]);
    glBufferData(GL_UNIFORM_BUFFER,
                 ltw_matrices_on_cpu[i].size() * sizeof(glm::mat4),
                 ltw_matrices_on_cpu[i].data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, i, ubos[i]);
  }

  // Use the shader program
  glUseProgram(shader_program);

  // Get uniform block indices and bind them
  for (int i = 0; i < num_ubos; ++i) {
    std::string block_name = "ModelMatrices" + std::to_string(i);
    GLuint block_index =
        glGetUniformBlockIndex(shader_program, block_name.c_str());
    glUniformBlockBinding(shader_program, block_index, i);
  }

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

    // Draw the triangles, multiplying by 3 because 3 indices per triangle
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3 * total_num_objects);
    glBindVertexArray(0);

    // Swap buffers
    glfwSwapBuffers(window);
  }

  // Clean up
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shader_program);

  glfwTerminate();
  return 0;
}
