#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

extern "C" {
#include <SDL.h>
#include <gl/glew.h>
#include <SDL_opengl.h>
#include <gl/glu.h>
}

#include <CGAL/Surface_mesh_default_triangulation_3.h>
#include <CGAL/Complex_2_in_triangulation_3.h>
#include <CGAL/make_surface_mesh.h>
#include <CGAL/Implicit_surface_3.h>
#include <CGAL/IO/facets_in_complex_2_to_triangle_mesh.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>

using namespace std;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 800;

char WINDOW_NAME[] = "Hello, World! Now in 3D more!";
SDL_Window * gWindow = NULL;
SDL_GLContext gContext;

// default triangulation for Surface_mesher
typedef CGAL::Surface_mesh_default_triangulation_3 Tr;
// c2t3
typedef CGAL::Complex_2_in_triangulation_3<Tr> C2t3;
typedef Tr::Geom_traits GT;
typedef GT::Sphere_3 Sphere_3;
typedef GT::Point_3 Point_3;
typedef GT::FT FT;
typedef FT (*Function)(Point_3);
typedef CGAL::Implicit_surface_3<GT, Function> Surface_3;
// surface mesh
typedef CGAL::Simple_cartesian<float> K;
typedef CGAL::Surface_mesh<K::Point_3> Mesh;
typedef Mesh::Vertex_index vertex_descriptor;
typedef Mesh::Face_index face_descriptor;

void die(string message) {
    cout << message << endl;
    exit(1);
}

void init() {
    // init SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) die("SDL");
    if (! SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) die("texture");

    // init SDL GL
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );

    gWindow = SDL_CreateWindow(WINDOW_NAME, SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               SCREEN_WIDTH, SCREEN_HEIGHT,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (gWindow == NULL) die("window");

    gContext = SDL_GL_CreateContext(gWindow);
    if (! gContext) die("gl context");

    // init GLEW
    glewExperimental = GL_TRUE; 
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) die("glew");

    // GL viewport
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void close()
{
    //TODO close OpenGL

    SDL_DestroyWindow(gWindow);
    gWindow = NULL;

    SDL_Quit();
}

FT sphere_function(Point_3 p) {
    const FT x2 = p.x() * p.x();
    const FT y2 = p.y() * p.y();
    const FT z2 = p.z() * p.z();
    return x2 + y2 + z2 - 1;
}

GLuint VAO;
int nvertices;
GLuint shaderProgram;

void setup_sphere() {
    Tr tr;            // 3D-Delaunay triangulation
    C2t3 c2t3(tr);    // 2D-complex in 3D-Delaunay triangulation

    // defining the surface
    Surface_3 surface(sphere_function,              // pointer to function
                      Sphere_3(CGAL::ORIGIN, 2.0)); // bounding sphere
    // Note that "2.0" above is the *squared* radius of the bounding sphere!

    // defining meshing criteria
    CGAL::Surface_mesh_default_criteria_3<Tr> criteria(30.,  // angular bound
                                                       0.1,  // radius bound
                                                       0.1); // distance bound
    // meshing surface
    CGAL::make_surface_mesh(c2t3, surface, criteria, CGAL::Manifold_tag());

    Mesh mesh;
    CGAL::facets_in_complex_2_to_triangle_mesh(c2t3, mesh);

    vector<float> vertices = {};
    for (auto face : mesh.faces()) {
        for (auto vertex : vertices_around_face(mesh.halfedge(face), mesh)) {
            K::Point_3 point = mesh.point(vertex);
            vertices.push_back(point.x());
            vertices.push_back(point.y());
            vertices.push_back(point.z());
        }
    }

    glGenVertexArrays(1, & VAO);

    GLuint VBO;
    glGenBuffers(1, & VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    nvertices = vertices.size();
    glBufferData(GL_ARRAY_BUFFER, nvertices * sizeof(GLfloat), & vertices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // vertex shader
    const char * vertex_shader_code =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "//layout (location = 1) in vec3 aColor;\n"
        "out vec3 ourColor;\n"
        "//uniform mat4 transform;\n"
        "void main() {\n"
        "  //gl_Position = transform * vec4(aPos.x,aPos.y,aPos.z, 1.0);\n"
        "  gl_Position = vec4(aPos.x,aPos.y,aPos.z, 1.0);\n"
        "  //ourColor = aColor;\n"
        "  ourColor = vec3(1.0,1.0,1.0);\n"
        "}";
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, & vertex_shader_code, NULL);
    glCompileShader(vertexShader);
    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, & success);
    if (! success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << infoLog << endl;
        die("vertex shader");
    }

    // fragment shader
    const char * fragment_shader_code = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec3 ourColor;\n"
        "uniform bool outline;\n"
        "void main() {\n"
        "  if (outline) FragColor = vec4(0.2f,0.2f,0.2f,1.0f);\n"
        "  else FragColor = vec4(ourColor, 1.0f);\n"
        "}";
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, & fragment_shader_code, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, & success);
    if (! success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << infoLog << endl;
        die("fragment shader");
    }

    // shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, & success);
    if (! success) die("shader program");

    // delete shaders (unneeded after program link)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void draw_sphere() {
    unsigned int outlineLoc = glGetUniformLocation(shaderProgram, "outline");

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glUniform1ui(outlineLoc, false);
    glDrawArrays(GL_TRIANGLES, 0, nvertices);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUniform1ui(outlineLoc, true);
    glDrawArrays(GL_TRIANGLES, 0, nvertices);
}

int main(int nargs, char * args[])
{
    init();

    setup_sphere();

    // background color
    glClearColor(0.2, 0.3, 0.3, 1.0);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_sphere();
    SDL_GL_SwapWindow(gWindow);

    bool done = false;
    while (! done)
    {
        SDL_Event e;
        SDL_WaitEvent(& e); //TODO check for error

        if (e.type == SDL_QUIT) done = true;
    }

    close();

    return 0;
}
