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
#include <GL/glew.h>
#include <SDL_opengl.h>

#include "tesselator.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
}

using namespace std;

//TODO fix coordinate system mismatch (lefthand vs righthand, +z vs -z)

const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 1000;

char WINDOW_NAME[] = "Hello, World! Now in 3D more!";
SDL_Window * gWindow = NULL;
SDL_GLContext gContext;

void die(string message) {
    cout << message << endl;
    exit(1);
}

void GLAPIENTRY MessageCallback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar * message,
        const void * userParam) {
    cerr << "GL CALLBACK: " << (type==GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "") << " type=" << type << " severity=" << severity << " message=" << message << endl;
}

void init() {
    // init SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) die("SDL");
    if (! SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) die("texture");

    // init SDL GL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    gWindow = SDL_CreateWindow(WINDOW_NAME, SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               SCREEN_WIDTH, SCREEN_HEIGHT,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (gWindow == NULL) die("window");

    //memset(& gContext, 0, sizeof(gContext));
    gContext = SDL_GL_CreateContext(gWindow);
    if (! gContext) die("gl context");

    // init GLEW
    glewExperimental = GL_TRUE; 
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) die("glew");

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

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

int fact(int n) {
    int r = 1;
    for (int i=2 ; i<=n ; i+=1) r *= i;
    return r;
}

int binomial(int n, int k) {
    return fact(n) / (fact(k) * fact(n-k));
}

float bernstein_3(int ix, float uv) {
    return binomial(3, ix) * pow(uv, ix) * pow(1-uv, 3-ix);
}

glm::vec3 bezier_bicubic_quad(vector<glm::vec3> controls, float u, float v) {
    glm::vec3 p(0,0,0);

    for (int i=0 ; i<4 ; i+=1) {
        for (int j=0 ; j<4 ; j+=1) {
            p += bernstein_3(i, u) * bernstein_3(j, v) * controls[i*4 + j];
        }
    }

    return p;
}

void load_patches(string filename, vector<vector<glm::vec3>> & patches) {
    ifstream f(filename);
    string line;

    //TODO fix only accepts comments at the beginning of the file
    do getline(f, line); while (line[0] == '#');
    int npatches = stoi(line);

    for (int ix=0 ; ix<npatches ; ix+=1) {
        getline(f, line);
        int uorder, vorder;
        istringstream(line) >> uorder >> vorder;

        vector<glm::vec3> patch = {};
        for (int v=0 ; v<=vorder ; v+=1) {
            for (int u=0 ; u<=uorder ; u+=1) {
                getline(f, line);
                glm::vec3 p;
                istringstream(line) >> p.x >> p.y >> p.z;
                patch.push_back(p);
            }
        }
        patches.push_back(patch);
    }
    f.close();
}

void add_point(vector<float> & vertices, glm::vec3 point) {
    vertices.push_back(point.x);
    vertices.push_back(point.y);
    vertices.push_back(point.z);
}

GLuint teapot_VAO;
int teapot_ntris;

const int TEAPOT_FINENESS = 10;
void load_teapot() {
    // load teapot bezier patch control points
    vector<vector<glm::vec3>> patches = {};
    load_patches("teapotCGA.bpt", patches);

    // convert bezier patches to triangle meshes
    const int tf = TEAPOT_FINENESS;
    vector<float> coords = {};
    for (auto patch : patches) {
        //TODO fix assumption of bicubic patches
        //TODO make sure uv coordinates match (important?)
        glm::vec3 ps[tf][tf];
        for (int i=0 ; i<tf ; i+=1) {
            for (int j=0 ; j<tf ; j+=1) {
                ps[i][j] = bezier_bicubic_quad(patch, i/float(tf-1), j/float(tf-1));
            }
        }
        
        for (int ix=0 ; ix<tf-1 ; ix+=1) {
            for (int jx=0 ; jx<tf-1 ; jx+=1) {
                glm::vec3 normal = glm::triangleNormal(
                        ps[ix][jx],
                        ps[ix][jx+1],
                        ps[ix+1][jx]
                );

                add_point(coords, ps[ix][jx]);
                add_point(coords, normal);
                add_point(coords, ps[ix][jx+1]);
                add_point(coords, normal);
                add_point(coords, ps[ix+1][jx]);
                add_point(coords, normal);

                add_point(coords, ps[ix+1][jx+1]);
                add_point(coords, normal);
                add_point(coords, ps[ix+1][jx]);
                add_point(coords, normal);
                add_point(coords, ps[ix][jx+1]);
                add_point(coords, normal);
            }
        }
    }

    glGenVertexArrays(1, & teapot_VAO);

    GLuint VBO;
    glGenBuffers(1, & VBO);

    glBindVertexArray(teapot_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    teapot_ntris = coords.size() / 6;
    glBufferData(GL_ARRAY_BUFFER, coords.size() * sizeof(float), & coords[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void *) (3*sizeof(float)));
    glEnableVertexAttribArray(1);
}

GLuint shaderProgram;

void draw_teapot() {
    auto model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0, -2, -3));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(-15.0f), glm::vec3(0, 0, 1));

    glm::vec3 color = {1.0, 1.0, 1.0};

    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    glBindVertexArray(teapot_VAO);

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(objectColorLoc, 1, glm::value_ptr(color));

    glDrawArrays(GL_TRIANGLES, 0, teapot_ntris);
}

FT_Library ft;
FT_Face face;

// TODO move divide by font_size into pl_funcs
int pl_moveto(const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::vec3>> *) user;
    vector<glm::vec3> polyline = {{FT_to->x, FT_to->y, 0.0}};
    polylines->push_back(polyline);
    return 0;
}

int pl_lineto(const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::vec3>> *) user;
    vector<glm::vec3> & polyline = polylines->back();
    polyline.push_back({FT_to->x, FT_to->y, 0.0});
    return 0;
}

int pl_conicto(const FT_Vector * FT_ctl, const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::vec3>> *) user;
    vector<glm::vec3> & polyline = polylines->back();

    glm::vec3 & from = polyline.back();
    glm::vec3 ctl = {FT_ctl->x, FT_ctl->y, 0.0};
    glm::vec3 to = {FT_to->x, FT_to->y, 0.0};

    glm::vec3 mid = 0.25f * from + 0.5f * ctl + 0.25f * to;

    polyline.push_back(mid);
    polyline.push_back(to);
    return 0;
}

int pl_cubicto(const FT_Vector * FT_ctl1, const FT_Vector * FT_ctl2,
            const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::vec3>> *) user;
    vector<glm::vec3> & polyline = polylines->back();

    glm::vec3 & from = polyline.back();
    glm::vec3 ctl1 = {FT_ctl1->x, FT_ctl1->y, 0.0};
    glm::vec3 ctl2 = {FT_ctl2->x, FT_ctl2->y, 0.0};
    glm::vec3 to = {FT_to->x, FT_to->y, 0.0};

    glm::vec3 mid1 = 0.2963f * from + 0.4444f * ctl1 + 0.2222f * ctl2
                     + 0.0370f * to;
    glm::vec3 mid2 = 0.0370f * from + 0.2222f * ctl1 + 0.4444f * ctl2
                     + 0.2963f * to;

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

const float THICKNESS = 0.25;

struct Character {
    float advance_x;
    float top;
    float bot;
    GLuint VAO;
    int ntris;
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
        vector<vector<glm::vec3>> polylines;
        FT_Outline_Decompose(& outline, & pl_funcs, (void *) & polylines);

        // mesh polylines to triangles (both front and back face)
        TESStesselator * tobj = tessNewTess(nullptr);
        if (! tobj) die("tesselator");
        tessSetOption(tobj, TESS_CONSTRAINED_DELAUNAY_TRIANGULATION, 1);

        front_vertices = {};
        back_vertices = {};

        for (vector<glm::vec3> & polyline : polylines) {
            tessAddContour(tobj, 3, & polyline[0], 3*sizeof(float), polyline.size());
        }
        tessTesselate(tobj, TESS_WINDING_ODD, TESS_POLYGONS, 3, 3, nullptr);

        glm::vec3 z(0, 0, THICKNESS/2);
        glm::vec3 norm(0, 0, -1);
        glm::vec3 p;

        const float * verts = tessGetVertices(tobj);
        const int * elems = tessGetElements(tobj);
        //cout << c << " -> " << "verts:" << verts << " elems:" << elems << " nelems:" << tessGetElementCount(tobj) << endl;
        /*
        for (int ix=0 ; ix<tessGetElementCount(tobj) ; ix+=1) {
            for (int n=0 ; n<3 ; n+=1) {
                const float * vert = & verts[elems[ix*3 + n]];
                p = {vert[0]/font_size, vert[1]/font_size, vert[2]/font_size};

                add_point(front_vertices, p-z);
                add_point(front_vertices, norm);
                add_point(back_vertices, p+z);
                add_point(back_vertices, -norm);
            }
        }
        */
        for (int ix=0 ; ix<tessGetElementCount(tobj) ; ix+=1) {
            const int * p = & elems[ix * 3];
            for (int j=0 ; j<3 ; j+=1) {
                glm::vec3 point = {verts[p[j]*3]/font_size, verts[p[j]*3+1]/font_size, 0};
                add_point(front_vertices, point-z);
                add_point(front_vertices, norm);
                add_point(back_vertices, point+z);
                add_point(back_vertices, -norm);
            }
        }

        tessDeleteTess(tobj); // for some reason not deleting kills rp3d, shrug

        // add sides
        float font_ratio = 1/font_size;
        auto half_deep = glm::vec3(0,0,THICKNESS/2);
        vector<float> side_vertices = {};
        for (auto & polyline : polylines) {
            auto prev_point = polyline.back();
            for (glm::vec3 & point : polyline) {
                //TODO blend normals between adjacent faces
                glm::vec3 normal = glm::triangleNormal(
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

        ch.ntris = vertices.size() / 6;
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), & vertices[0], GL_STATIC_DRAW);

        // vertex positions
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
        glEnableVertexAttribArray(0);

        // vertex normals
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void *) (3*sizeof(float)));
        glEnableVertexAttribArray(1);
    }
}

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
        "  vec3 viewPos = vec3(0.0, 0.0, 10.0);\n" //TODO make this a uniform
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

rp3d::DynamicsWorld * world;

struct spring {
    rp3d::RigidBody * from_body;
    rp3d::Vector3 from_con;
    rp3d::RigidBody * to_body;
    rp3d::Vector3 to_con;
    float strength;
    float rest_length;

    void apply_force();
};

void spring::apply_force() {
    rp3d::Vector3 from_point = from_con;
    if (from_body) from_point = from_body->getTransform() * from_con;

    rp3d::Vector3 to_point = to_con;
    if (to_body) to_point = to_body->getTransform() * to_con;

    rp3d::Vector3 delta = from_point - to_point;
    rp3d::Vector3 delta_unit = delta.getUnit();
    float delta_length = delta.length();
    rp3d::Vector3 force = strength * (delta_length - rest_length) * delta_unit;

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

    glDrawArrays(GL_TRIANGLES, 0, ch.ntris);
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
    rp3d::RigidBody * body;
    glm::mat4 draw_transform = glm::translate(glm::mat4(1.0), glm::vec3(0,-0.5,0)); // TODO derive this from text geometry

    ext_text() {}
    ext_text(string text, float mass, glm::vec3 color, rp3d::Transform pose=rp3d::Transform());

    void draw(glm::mat4 base_model);
};

ext_text::ext_text(string newtext, float newmass, glm::vec3 newcolor, rp3d::Transform pose) {
    //cout << "creating ext_text" << endl;

    text = newtext;
    width = word_width(text);
    //height = word_height(text);
    height = 0.667; // TODO derive this from text geometry
    depth = THICKNESS;
    mass = newmass;
    color = newcolor;

    //cout << "creating rigidbody" << endl;

    body = world->createRigidBody(pose);
    body->setLinearDamping(0.01);
    body->setAngularDamping(0.01);

    //cout << "creating collisionshape" << endl;

    rp3d::CollisionShape * shape = new rp3d::BoxShape(rp3d::Vector3(width/2, height/2, depth/2));

    //cout << "adding collisionshape" << endl;

    body->addCollisionShape(shape, rp3d::Transform(), mass);

    //cout << "done creating ext_text" << endl;
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
    //cout << "setting up scene" << endl;

    rp3d::Vector3 gravity(0.0, -9.81, 0.0);
    world = new rp3d::DynamicsWorld(gravity);

    // read words from Lua file
    lua_State * L = luaL_newstate();

    //TODO use lua function to read file
    ifstream luargb("rgb.lua");
    string line;
    while (! luargb.eof()) {
        getline(luargb, line);
        int error = luaL_loadstring(L, line.c_str()) || lua_pcall(L, 0, 0, 0);
        if (error) die("lua");
    }
    luargb.close();

    //cout << "done reading colors" << endl;

    ifstream luaconf("text3d_conf.lua");
    while (! luaconf.eof()) {
        getline(luaconf, line);
        int error = luaL_loadstring(L, line.c_str()) || lua_pcall(L, 0, 0, 0);
        if (error) die("lua");
    }
    luaconf.close();

    //cout << "done reading conf.lua" << endl;

    rp3d::RigidBody * prevbody = nullptr;

    //cout << "setting up words" << endl;

    lua_getglobal(L, "words");
    int nwords = luaL_len(L, -1);
    lua_pop(L, 1);
    for (int n=1 ; n<=nwords ; n+=1) {
        //cout << "setting up a word" << endl;
        //cout << "stack:" << lua_gettop(L) << endl;

        lua_getglobal(L, "words");
        lua_geti(L, -1, n);
        string text(lua_tostring(L, -1));
        lua_pop(L, 2);

        //cout << "word is " << text << endl;

        glm::vec3 color;
        lua_getglobal(L, "colors");
        lua_geti(L, -1, n);

        lua_getfield(L, -1, "red");
        color[0] = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "green");
        color[1] = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "blue");
        color[2] = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_pop(L, 2);

        ext_text word = ext_text(text, 1, color, rp3d::Transform(rp3d::Vector3(n, -n, 0), rp3d::Quaternion::identity()));
        words.push_back(word);

        //cout << "done setting up a word" << endl;
        //cout << "stack:" << lua_gettop(L) << endl;

        spring s;
        float y = prevbody == nullptr ? 3.5 : -0.333;
        s = {prevbody, rp3d::Vector3(-1.5,y,0),
             word.body, rp3d::Vector3(-1.5,.333,0),
             200, 0.5};
        springs.push_back(s);
        s = {prevbody, rp3d::Vector3(1.5,y,0),
             word.body, rp3d::Vector3(1.5,.333,0),
             200, 0.5};
        springs.push_back(s);

        //cout << "done setting up its springs" << endl;

        prevbody = word.body;
    }

    lua_close(L);

    //cout << "done setting up scene" << endl;
}

float time_step = 1.0 / 1000.0;
void physics_step(float dt) {
    for (float n=0 ; n<dt ; n+=time_step) {
        for (auto spring : springs) spring.apply_force();

        world->update(time_step);
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
    view = glm::translate(view, glm::vec3(0.0, 0.0, -4.0));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    glUniform3f(lightPosLoc, 1.0, 1.0, -1.0);
    glUniform3f(lightColorLoc, 1.0, 1.0, 1.0);

    auto base_model = glm::mat4(1.0);
    for (auto word : words) word.draw(base_model);

    draw_teapot();
}

unsigned int FRAME_TICK;

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
    //cout << "loaded" << endl;

    load_teapot();

    setup_shaders();
    //cout << "shaders" << endl;

    setup_scene();
    //cout << "scene" << endl;

    // timer tick every 20msec
    FRAME_TICK = SDL_RegisterEvents(1);
    SDL_AddTimer(20, timer_callback, NULL);

    bool done = false;
    while (! done)
    {
        SDL_Event e;
        SDL_WaitEvent(& e); //TODO check for error

        if (e.type == SDL_QUIT) done = true;
        else if (e.type == FRAME_TICK) {
            //cout << "before physics" << endl;
            physics_step(20.0/1000.0); // step forward 20msec
            //cout << "after physics" << endl;

            // background color
            glClearColor(0.2, 0.3, 0.3, 1.0);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            //cout << "before draw" << endl;
            draw_scene();
            //cout << "after draw" << endl;
            SDL_GL_SwapWindow(gWindow);
            frame += 1;
        }
    }

    close();

    return 0;
}
