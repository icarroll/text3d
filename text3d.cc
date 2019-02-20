#include <chrono>
#include <cmath>
#include <iostream>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/normal.hpp>

#include "reactphysics3d.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

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
using namespace reactphysics3d;

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

// TODO move divide by font_size into pl_funcs
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

float font_size;
vector<float> front_vertices;
vector<float> back_vertices;

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

const double THICKNESS = 0.25;

void tess_vertex_cb(void * vertex) {
    //cout << "tess_vertex_cb " << vertex << endl;
    double * v_in = (double *) vertex;
    front_vertices.push_back(v_in[0] / font_size);
    front_vertices.push_back(v_in[1] / font_size);
    front_vertices.push_back(v_in[2] / font_size + THICKNESS/2);
    front_vertices.push_back(0);
    front_vertices.push_back(0);
    front_vertices.push_back(1);

    back_vertices.push_back(v_in[0] / font_size);
    back_vertices.push_back(v_in[1] / font_size);
    back_vertices.push_back(v_in[2] / font_size - THICKNESS/2);
    back_vertices.push_back(0);
    back_vertices.push_back(0);
    back_vertices.push_back(-1);
}

void tess_end_cb() {
    //cout << "tess_end_cb " << endl;
}

void tess_error_cb(GLenum errorCode) {
    die("tesselate");
}

void tess_nothing_cb() {
    // do nothing
}

void add_point(vector<float> & vertices, glm::dvec3 point) {
    vertices.push_back(point.x);
    vertices.push_back(point.y);
    vertices.push_back(point.z);
}

struct Character {
    float advance_x;
    float top;
    float bot;
    GLuint VAO;
    int nvertices;
};

vector<Character> Characters(128);

void load_glyphs() {
    if (FT_Init_FreeType(& ft)) die("freetype");
    if (FT_New_Face(ft, "georgiab.ttf", 0, & face)) die("font");
    font_size = face->units_per_EM;

    for (char c=0 ; c<=126 ; c+=1) { // char 127 hangs for some reason
        if (FT_Load_Char(face, c, FT_LOAD_NO_SCALE)) die("glyph");

        // decompose glyph to polyline
        FT_Outline outline = face->glyph->outline;
        vector<vector<glm::dvec3>> polylines;
        FT_Outline_Decompose(& outline, & pl_funcs, (void *) & polylines);

        // mesh polylines to triangles (both front and back face)
        // TODO use standalone tesselator instead of GLU's
        auto tobj = gluNewTess();
        gluTessCallback(tobj, GLU_TESS_BEGIN, (void (__stdcall *)(void)) tess_begin_cb);
        gluTessCallback(tobj, GLU_TESS_VERTEX, (void (__stdcall *)(void)) tess_vertex_cb);
        gluTessCallback(tobj, GLU_TESS_END, (void (__stdcall *)(void)) tess_end_cb);
        gluTessCallback(tobj, GLU_TESS_ERROR, (void (__stdcall *)(void)) tess_error_cb);
        // for now, make the tesselator not do strips and fans
        gluTessCallback(tobj, GLU_TESS_EDGE_FLAG, (void (__stdcall *)(void)) tess_nothing_cb);

        front_vertices = {};
        back_vertices = {};
        gluTessBeginPolygon(tobj, nullptr);
        for (auto & polyline : polylines) {
            gluTessBeginContour(tobj);
            for (auto & point : polyline) {
                gluTessVertex(tobj, glm::value_ptr(point),
                              glm::value_ptr(point));
            }
            gluTessEndContour(tobj);
        }
        gluTessEndPolygon(tobj);

        // add sides
        double font_ratio = 1/font_size;
        auto half_deep = glm::dvec3(0,0,THICKNESS/2);
        vector<float> side_vertices = {};
        for (auto & polyline : polylines) {
            auto prev_point = polyline.back();
            for (glm::dvec3 & point : polyline) {
                glm::dvec3 normal = glm::triangleNormal(
                        prev_point * font_ratio - half_deep,
                        point * font_ratio + half_deep,
                        point * font_ratio - half_deep
                );

                add_point(side_vertices, point * font_ratio + half_deep);
                add_point(side_vertices, normal);
                add_point(side_vertices, prev_point * font_ratio - half_deep);
                add_point(side_vertices, normal);
                add_point(side_vertices, point * font_ratio - half_deep);
                add_point(side_vertices, normal);

                add_point(side_vertices, point * font_ratio + half_deep);
                add_point(side_vertices, normal);
                add_point(side_vertices, prev_point * font_ratio + half_deep);
                add_point(side_vertices, normal);
                add_point(side_vertices, prev_point * font_ratio - half_deep);
                add_point(side_vertices, normal);

                prev_point = point;
            }
        }

        // send triangles to opengl
        Character & ch = Characters[c];
        ch.advance_x = face->glyph->advance.x / font_size;

        glGenVertexArrays(1, & ch.VAO);

        GLuint VBO;
        glGenBuffers(1, & VBO);

        glBindVertexArray(ch.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        vector<float> vertices;
        vertices.reserve(front_vertices.size() + back_vertices.size() + side_vertices.size());
        vertices.insert(vertices.end(), front_vertices.begin(), front_vertices.end());
        vertices.insert(vertices.end(), back_vertices.begin(), back_vertices.end());
        vertices.insert(vertices.end(), side_vertices.begin(), side_vertices.end());

        ch.nvertices = vertices.size();
        glBufferData(GL_ARRAY_BUFFER, ch.nvertices * sizeof(float), & vertices[0], GL_STATIC_DRAW);

        // vertex positions
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
        glEnableVertexAttribArray(0);

        // vertex normals
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void *) (3*sizeof(float)));
        glEnableVertexAttribArray(1);
    }
}

GLuint shaderProgram;

void setup_shaders() {
    // vertex shader
    const char * vertex_shader_code =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aNormal;\n"
        "out vec3 FragPos;\n"
        "out vec3 Normal;\n"
        "uniform mat4 projection;\n"
        "uniform mat4 view;\n"
        "uniform mat4 model;\n"
        "void main() {\n"
        "  FragPos = aPos;\n"
        "  Normal = mat3(transpose(inverse(model))) * aNormal;\n"
        "  gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
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
        "uniform vec3 lightPos;\n"
        "uniform vec3 lightColor;\n"
        "uniform vec3 objectColor;\n"
        "void main() {\n"
        "  float ambientStrength = 0.1;\n"
        "  vec3 ambient = ambientStrength * lightColor;\n"
        "  vec3 norm = -normalize(Normal);\n"
        "  vec3 lightDir = normalize(lightPos - FragPos);\n"
        "  float diff = max(dot(norm, lightDir), 0.0);\n"
        "  vec3 diffuse = diff * lightColor;\n"
        "  float specularStrength = 0.25;\n"
        "  vec3 viewPos = vec3(0.0, 0.0, 10.0);\n"
        "  vec3 viewDir = normalize(viewPos - FragPos);\n"
        "  vec3 reflectDir = reflect(-lightDir, norm);\n"
        "  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
        "  vec3 specular = specularStrength * spec * lightColor;\n"
        "  \n"
        "  vec3 result = (ambient + diffuse + specular) * objectColor;\n"
        "  FragColor = vec4(result, 1.0);\n"
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

Vector3 gravity(0.0, -9.81, 0.0);
DynamicsWorld world(gravity);

struct spring {
    RigidBody * from_body;
    Vector3 from_con;
    RigidBody * to_body;
    Vector3 to_con;
    float strength;
    float rest_length;

    void apply_force();
};

void spring::apply_force() {
    Vector3 from_point = from_con;
    if (from_body) from_point = from_body->getTransform() * from_con;

    Vector3 to_point = to_con;
    if (to_body) to_point = to_body->getTransform() * to_con;

    Vector3 delta = from_point - to_point;
    Vector3 delta_unit = delta.getUnit();
    float delta_length = delta.length();
    Vector3 force = strength * (delta_length - rest_length) * delta_unit;

    if (from_body) from_body->applyForce(-force, from_point);
    if (to_body) to_body->applyForce(force, to_point);
}

float word_width(string word) {
    float width = 0.0;
    for (char c : word) if (c != '\0') width += Characters[c].advance_x;
    return width;
}

float word_height(string word) {
    float top = -INFINITY;
    float bot = INFINITY;
    for (char c : word) {
        if (c == '\0') continue;
        Character ch = Characters[c];
        if (ch.top > top) top = ch.top;
        if (ch.bot < bot) bot = ch.bot;
    }
    return top - bot;
}

void draw_letter(Character & ch, glm::mat4 model, glm::vec3 color) {
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    glBindVertexArray(ch.VAO);

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(objectColorLoc, 1, glm::value_ptr(color));

    glPolygonMode(GL_FRONT, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, ch.nvertices);
}

void draw_word(string word, glm::mat4 base_model, glm::vec3 color) {
    float x = -word_width(word)/2;
    for (char c : word) {
        if (c == '\0') continue;

        Character & ch = Characters[c];
        auto model = glm::translate(base_model, glm::vec3(x, 0.0f, 0.0f));
        draw_letter(ch, model, color);

        x += ch.advance_x;
    }
}

struct ext_text {
    string text;
    float width;
    float height;
    float depth;
    float mass;
    glm::vec3 color;
    RigidBody * body;
    glm::mat4 draw_transform = glm::translate(glm::mat4(1.0), glm::vec3(0,-0.5,0)); // TODO derive this from text geometry

    ext_text() {}
    ext_text(string text, float mass, glm::vec3 color, Transform pose=Transform());

    void draw(glm::mat4 base_model);
};

ext_text::ext_text(string newtext, float newmass, glm::vec3 newcolor, Transform pose) {
    text = newtext;
    width = word_width(text);
    //height = word_height(text);
    height = 0.667; // TODO derive this from text geometry
    depth = THICKNESS;
    mass = newmass;
    color = newcolor;

    body = world.createRigidBody(pose);
    CollisionShape * shape = new BoxShape(Vector3(width/2, height/2, depth/2));
    body->addCollisionShape(shape, Transform(), mass);
}

void ext_text::draw(glm::mat4 base_model) {
    glm::mat4 model;
    body->getTransform().getOpenGLMatrix(glm::value_ptr(model));
    model = base_model * model * draw_transform;
    draw_word(text, model, color);
}

vector<ext_text> words;
vector<spring> springs;

void setup_scene() {
    // read words from Lua file
    lua_State * L = luaL_newstate();

    ifstream luargb("rgb.lua");
    string line;
    while (! luargb.eof()) {
        getline(luargb, line);
        int error = luaL_loadstring(L, line.c_str()) || lua_pcall(L, 0, 0, 0);
        if (error) die("lua");
    }
    luargb.close();

    ifstream luaconf("text3d_conf.lua");
    while (! luaconf.eof()) {
        getline(luaconf, line);
        int error = luaL_loadstring(L, line.c_str()) || lua_pcall(L, 0, 0, 0);
        if (error) die("lua");
    }
    luaconf.close();

    RigidBody * prevbody = nullptr;

    lua_getglobal(L, "words");
    int nwords = luaL_len(L, -1);
    for (int n=1 ; n<=nwords ; n+=1) {
        lua_getglobal(L, "words");
        lua_geti(L, -1, n);
        string text(lua_tostring(L, -1));

        glm::vec3 color;
        lua_getglobal(L, "colors");
        lua_geti(L, -1, n);
        lua_getfield(L, -1, "red");
        color[0] = lua_tonumber(L, -1);
        lua_getglobal(L, "colors");
        lua_geti(L, -1, n);
        lua_getfield(L, -1, "green");
        color[1] = lua_tonumber(L, -1);
        lua_getglobal(L, "colors");
        lua_geti(L, -1, n);
        lua_getfield(L, -1, "blue");
        color[2] = lua_tonumber(L, -1);

        ext_text word = ext_text(text, 1, color, Transform(Vector3(0, 0, 0), Quaternion::identity()));
        words.push_back(word);

        spring s;
        float y = prevbody == nullptr ? 2.5 : -0.333;
        s = {prevbody, Vector3(-1.5,y,0),
             word.body, Vector3(-1.5,.333,0),
             50, 1.0};
        springs.push_back(s);
        s = {prevbody, Vector3(1.5,y,0),
             word.body, Vector3(1.5,.333,0),
             50, 1.0};
        springs.push_back(s);

        prevbody = word.body;
    }
}

float time_step = 1.0 / 1000.0;
void physics_step(float dt) {
    for (float n=0 ; n<dt ; n+=time_step) {
        for (auto spring : springs) spring.apply_force();

        world.update(time_step);
    }
}

void draw_scene() {
    glUseProgram(shaderProgram);
    unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
    unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");

    auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    auto view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(0.0, 0.0, -3.0));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    glUniform3f(lightPosLoc, 1.0, 1.0, -1.0);
    glUniform3f(lightColorLoc, 1.0, 1.0, 1.0);

    auto base_model = glm::mat4(1.0);
    for (auto word : words) word.draw(base_model);
}

int FRAME_TICK;

uint32_t timer_callback(uint32_t interval, void * param) {
    SDL_Event e;
    e.type = FRAME_TICK;
    SDL_PushEvent(& e);

    return interval;
}

int frame = 0;

int main(int nargs, char * args[])
{
    init();

    load_glyphs();

    setup_shaders();

    setup_scene();

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
            physics_step(20.0/1000.0); // step forward 20msec

            // background color
            glClearColor(0.2, 0.3, 0.3, 1.0);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            draw_scene();
            SDL_GL_SwapWindow(gWindow);
            frame += 1;
        }
    }

    close();

    return 0;
}
