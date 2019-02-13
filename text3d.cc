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

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
}

using namespace std;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 800;

char WINDOW_NAME[] = "Hello, World! Now in 3D more!";
SDL_Window * gWindow = NULL;
SDL_GLContext gContext;

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

FT_Library ft;
FT_Face face;

int pl_moveto(const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::dvec3>> *) user;
    vector<glm::dvec3> polyline = {{FT_to->x, FT_to->y, 0.0}};
    polylines->push_back(polyline);
    return 0;
}

int pl_lineto(const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::dvec3>> *) user;
    vector<glm::dvec3> & polyline = polylines->back();
    polyline.push_back({FT_to->x, FT_to->y, 0.0});
    return 0;
}

int pl_conicto(const FT_Vector * FT_ctl, const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::dvec3>> *) user;
    vector<glm::dvec3> & polyline = polylines->back();

    glm::dvec3 & from = polyline.back();
    glm::dvec3 ctl = {FT_ctl->x, FT_ctl->y, 0.0};
    glm::dvec3 to = {FT_to->x, FT_to->y, 0.0};

    glm::dvec3 mid = 0.25 * from + 0.5 * ctl + 0.25 * to;

    polyline.push_back(mid);
    polyline.push_back(to);
    return 0;
}

int pl_cubicto(const FT_Vector * FT_ctl1, const FT_Vector * FT_ctl2,
            const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::dvec3>> *) user;
    vector<glm::dvec3> & polyline = polylines->back();

    glm::dvec3 & from = polyline.back();
    glm::dvec3 ctl1 = {FT_ctl1->x, FT_ctl1->y, 0.0};
    glm::dvec3 ctl2 = {FT_ctl2->x, FT_ctl2->y, 0.0};
    glm::dvec3 to = {FT_to->x, FT_to->y, 0.0};

    glm::dvec3 mid1 = 0.2963 * from + 0.4444 * ctl1 + 0.2222 * ctl2
                      + 0.0370 * to;
    glm::dvec3 mid2 = 0.0370 * from + 0.2222 * ctl1 + 0.4444 * ctl2
                      + 0.2963 * to;

    polyline.push_back(mid1);
    polyline.push_back(mid2);
    polyline.push_back(to);
    return 0;
}

FT_Outline_Funcs pl_funcs = {& pl_moveto, & pl_lineto,
                             & pl_conicto, & pl_cubicto,
                             0, 0};

float size;
vector<float> vertices;

void tess_begin_cb(GLenum which) {
    //cout << "tess_begin_cb " << which << endl;
    switch (which) {
        case GL_TRIANGLES:
            break;
        case GL_TRIANGLE_STRIP:
            // fallthrough
        case GL_TRIANGLE_FAN:
            // fallthrough
        default:
            die("unimplemented which");
    }
}

void tess_vertex_cb(void * vertex) {
    //cout << "tess_vertex_cb " << vertex << endl;
    double * v_in = (double *) vertex;
    vertices.push_back(v_in[0] / size);
    vertices.push_back(v_in[1] / size);
    vertices.push_back(v_in[2] / size);
}

void tess_end_cb() {
    //cout << "tess_end_cb " << endl;
    //TODO
}

void tess_error_cb(GLenum errorCode) {
    die("tesselate");
}

void tess_nothing_cb() {
}

struct Character {
    GLfloat advance_x;
    GLuint VAO;
    int nvertices;
};

vector<Character> Characters(128);

void load_glyphs() {
    if (FT_Init_FreeType(& ft)) die("freetype");
    if (FT_New_Face(ft, "arial.ttf", 0, & face)) die("font");
    size = face->units_per_EM;

    for (char c=0 ; c<=126 ; c+=1) { // char 127 hangs for some reason
        if (FT_Load_Char(face, c, FT_LOAD_NO_SCALE)) die("glyph");

        // decompose glyph to polyline
        FT_Outline outline = face->glyph->outline;
        vector<vector<glm::dvec3>> polylines;
        FT_Outline_Decompose(& outline, & pl_funcs, (void *) & polylines);

        // mesh polylines to triangles
        auto tobj = gluNewTess();
        gluTessCallback(tobj, GLU_TESS_BEGIN, (void (__stdcall *)(void)) tess_begin_cb);
        gluTessCallback(tobj, GLU_TESS_VERTEX, (void (__stdcall *)(void)) tess_vertex_cb);
        gluTessCallback(tobj, GLU_TESS_END, (void (__stdcall *)(void)) tess_end_cb);
        gluTessCallback(tobj, GLU_TESS_ERROR, (void (__stdcall *)(void)) tess_error_cb);
        // for now, make the tesselator not do strips and fans
        gluTessCallback(tobj, GLU_TESS_EDGE_FLAG, (void (__stdcall *)(void)) tess_nothing_cb);

        vertices = {};
        gluTessBeginPolygon(tobj, nullptr);
        for (auto & polyline : polylines) {
            gluTessBeginContour(tobj);
            for (glm::dvec3 & point : polyline) {
                gluTessVertex(tobj, glm::value_ptr(point),
                              glm::value_ptr(point));
            }
            gluTessEndContour(tobj);
        }
        gluTessEndPolygon(tobj);

        // send triangles to opengl
        Character & ch = Characters[c];
        ch.advance_x = face->glyph->advance.x / size;

        glGenVertexArrays(1, & ch.VAO);

        GLuint VBO;
        glGenBuffers(1, & VBO);

        glBindVertexArray(ch.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        ch.nvertices = vertices.size();
        glBufferData(GL_ARRAY_BUFFER, ch.nvertices * sizeof(GLfloat), & vertices[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
    }
}

GLuint shaderProgram;

void setup_shaders() {
    // vertex shader
    const char * vertex_shader_code =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "out vec3 FragPos;\n"
        "out vec3 Normal;\n"
        "uniform mat4 model;\n"
        "void main() {\n"
        "  FragPos = aPos;\n"
        "  Normal = aPos;\n" // still cheating even though no longer sphere
        "  gl_Position = model * vec4(aPos, 1.0);\n"
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
        "in vec3 FragPos;\n"
        "in vec3 Normal;\n"
        "uniform bool outline;\n"
        "uniform vec3 lightPos;\n"
        "uniform vec3 lightColor;\n"
        "uniform vec3 objectColor;\n"
        "void main() {\n"
        "  if (outline) FragColor = vec4(0.2f,0.2f,0.2f,1.0f);\n"
        "  else {\n"
        "    float ambientStrength = 0.1;\n"
        "    vec3 ambient = ambientStrength * lightColor;\n"
        "    vec3 norm = -normalize(Normal);\n"
        "    vec3 lightDir = normalize(lightPos - FragPos);\n"
        "    float diff = max(dot(norm, lightDir), 0.0);\n"
        "    vec3 diffuse = diff * lightColor;\n"
        "    float specularStrength = 0.25;\n"
        "    vec3 viewPos = vec3(0.0, 0.0, 10.0);\n"
        "    vec3 viewDir = normalize(viewPos - FragPos);\n"
        "    vec3 reflectDir = reflect(-lightDir, norm);\n"
        "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
        "    vec3 specular = specularStrength * spec * lightColor;\n"
        "    \n"
        "    vec3 result = (ambient + diffuse + specular) * objectColor;\n"
        "    FragColor = vec4(result, 1.0);\n"
        "  }\n"
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

int frame = 0;

void draw() {
    float x = -3.0;
    for (char c : "Hello, World!") {
        if (c == '\0') continue;
        Character & ch = Characters[c];

        glUseProgram(shaderProgram);
        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int outlineLoc = glGetUniformLocation(shaderProgram, "outline");
        unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
        unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
        unsigned int objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");

        glBindVertexArray(ch.VAO);

        auto model = glm::mat4(1.0f);
        float scale = 1.0/3.0;
        model = glm::scale(model, glm::vec3(scale, scale, scale));
        model = glm::translate(model, glm::vec3(x, -0.5f, 0.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        x += ch.advance_x;

        float angle = frame * M_PI / 360.0;
        glUniform3f(lightPosLoc, 10*cos(angle), 10*sin(angle), 0.0);
        glUniform3f(lightColorLoc, 0.9, 0.9, 1.0);
        glUniform3f(objectColorLoc, 0.1, 1.0, 0.1);

        glPolygonMode(GL_FRONT, GL_FILL);
        glUniform1ui(outlineLoc, false);
        glDrawArrays(GL_TRIANGLES, 0, ch.nvertices);

        /*
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUniform1ui(outlineLoc, true);
        glDrawArrays(GL_TRIANGLES, 0, nvertices);
        */
    }
}

int FRAME_TICK;

uint32_t timer_callback(uint32_t interval, void * param) {
    SDL_Event e;
    e.type = FRAME_TICK;
    SDL_PushEvent(& e);

    return interval;
}

int main(int nargs, char * args[])
{
    init();

    load_glyphs();

    setup_shaders();

    // timer tick every 20msec
    FRAME_TICK = SDL_RegisterEvents(1);
    SDL_TimerID draw_timer_id = SDL_AddTimer(20, timer_callback, NULL);

    bool done = false;
    while (! done)
    {
        SDL_Event e;
        SDL_WaitEvent(& e); //TODO check for error

        if (e.type == SDL_QUIT) done = true;
        else if (e.type == FRAME_TICK) {
            // background color
            glClearColor(0.2, 0.3, 0.3, 1.0);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            draw();
            SDL_GL_SwapWindow(gWindow);
            frame += 1;
        }
    }

    close();

    return 0;
}
