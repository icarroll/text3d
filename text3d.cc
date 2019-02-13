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

struct Character {
    GLuint advance_x;
    int nvertices;
    GLuint VAO;
};

map<GLchar, Character> Characters;

int moveto(const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::fvec2>> *) user;
    vector<glm::fvec2> polyline = {{FT_to->x, FT_to->y}};
    polylines->push_back(polyline);
    return 0;
}

int lineto(const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::fvec2>> *) user;
    vector<glm::fvec2> & polyline = polylines->back();
    polyline.push_back({FT_to->x, FT_to->y});
    return 0;
}

int conicto(const FT_Vector * FT_ctl, const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::fvec2>> *) user;
    vector<glm::fvec2> & polyline = polylines->back();

    glm::fvec2 & from = polyline.back();
    glm::fvec2 ctl = {FT_ctl->x, FT_ctl->y};
    glm::fvec2 to = {FT_to->x, FT_to->y};

    glm::fvec2 mid = 0.25f * from + 0.5f * ctl + 0.25f * to;

    polyline.push_back(mid);
    polyline.push_back(to);
    return 0;
}

int cubicto(const FT_Vector * FT_ctl1, const FT_Vector * FT_ctl2,
            const FT_Vector * FT_to, void * user) {
    auto * polylines = (vector<vector<glm::fvec2>> *) user;
    vector<glm::fvec2> & polyline = polylines->back();

    glm::fvec2 & from = polyline.back();
    glm::fvec2 ctl1 = {FT_ctl1->x, FT_ctl1->y};
    glm::fvec2 ctl2 = {FT_ctl2->x, FT_ctl2->y};
    glm::fvec2 to = {FT_to->x, FT_to->y};

    glm::fvec2 mid1 = 0.2963f * from + 0.4444f * ctl1 + 0.2222f * ctl2
                      + 0.0370f * to;
    glm::fvec2 mid2 = 0.0370f * from + 0.2222f * ctl1 + 0.4444f * ctl2
                      + 0.2963f * to;

    polyline.push_back(mid1);
    polyline.push_back(mid2);
    polyline.push_back(to);
    return 0;
}

FT_Outline_Funcs pl_funcs = {& moveto, & lineto, & conicto, & cubicto, 0, 0};

vector<GLuint> VAOs;
vector<int> nverticeses;
void load_glyphs() {
    if (FT_Init_FreeType(& ft)) die("freetype");
    if (FT_New_Face(ft, "arial.ttf", 0, & face)) die("font");
    //FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (GLubyte c=0 ; c<128 ; c+=1) {
        if (FT_Load_Char(face, c, FT_LOAD_NO_SCALE)) die("glyph");
        //Characters.insert(pair<GLchar, Character>(c, character));
    }

    // decompose glyph to polyline
    vector<vector<glm::fvec2>> polylines;
    for (auto c : "Hello, World!") {
        if (FT_Load_Char(face, c, FT_LOAD_NO_SCALE)) die("glyph");
        FT_Outline outline = face->glyph->outline;
        FT_Outline_Decompose(& outline, & pl_funcs, (void *) & polylines);

        // mesh polyline to triangles
        // TODO

        // send triangles to opengl
        float size = face->units_per_EM;
        vector<float> vertices = {};
        for (auto & polyline : polylines) {
            for (auto & point : polyline) {
                vertices.push_back(point.x / size);
                vertices.push_back(point.y / size);
                vertices.push_back(0.0);
            }
        }

        VAOs.push_back(0);
        glGenVertexArrays(1, & VAOs.back());

        GLuint VBO;
        glGenBuffers(1, & VBO);

        glBindVertexArray(VAOs.back());
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        nverticeses.push_back(vertices.size());
        glBufferData(GL_ARRAY_BUFFER, nverticeses.back() * sizeof(GLfloat), & vertices[0], GL_STATIC_DRAW);
    }
}

GLuint shaderProgram;

void setup_shaders() {
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // vertex shader
    const char * vertex_shader_code =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "out vec3 FragPos;\n"
        "out vec3 Normal;\n"
        "void main() {\n"
        "  FragPos = aPos;\n"
        "  Normal = aPos;\n" // cheating because sphere
        "  gl_Position = vec4(aPos.x,aPos.y,aPos.z, 1.0);\n"
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
    for (int ix=0 ; ix<VAOs.size() ; ix+=1) {
        unsigned int outlineLoc = glGetUniformLocation(shaderProgram, "outline");
        unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
        unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
        unsigned int objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");

        glUseProgram(shaderProgram);
        glBindVertexArray(VAOs[ix]);

        float angle = frame * M_PI / 360.0;
        glUniform3f(lightPosLoc, 10*cos(angle), 10*sin(angle), 0.0);
        glUniform3f(lightColorLoc, 0.9, 0.9, 1.0);
        glUniform3f(objectColorLoc, 0.1, 1.0, 0.1);

        glPolygonMode(GL_FRONT, GL_FILL);
        glUniform1ui(outlineLoc, false);
        glDrawArrays(GL_TRIANGLES, 0, nverticeses[ix]);

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
