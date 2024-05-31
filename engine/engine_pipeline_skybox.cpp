#include "engine.h";

#include <GL/glew.h>
#include <GLFW/glfw3.h>

static const std::string pipeline_vs = R"(
    layout (location = 0) in vec3 aPos;

    out vec3 TexCoords;

    uniform mat4 projection;
    uniform mat4 view;

    void main()
    {
        TexCoords = aPos;  
        vec4 pos = projection * view * vec4(aPos, 1.0);
        gl_Position = pos.xyww;
    }
)";

static const std::string pipeline_fs = R"(
    out vec4 FragColor;

    in vec3 TexCoords;

    uniform samplerCube skybox;

    void main()
    {    
        FragColor = texture(skybox, TexCoords);
    }
)";

struct Eng::PipelineSkybox::Reserved
{
    Eng::Shader vs;
    Eng::Shader fs;
    Eng::Program program;
    Eng::Vao vao;
    Eng::Vbo vbo;
    Eng::Ebo ebo;


    /**
     * Constructor.
     */
    Reserved()
    {}
};

ENG_API Eng::PipelineSkybox::PipelineSkybox(): reserved(std::make_unique<Eng::PipelineSkybox::Reserved>())
{
    ENG_LOG_DETAIL("[+]");
    this->setProgram(reserved->program);
}

ENG_API Eng::PipelineSkybox::PipelineSkybox(const std::string& name) : Eng::Pipeline(name), reserved(std::make_unique<Eng::PipelineSkybox::Reserved>())
{
    ENG_LOG_DETAIL("[+]");
    this->setProgram(reserved->program);
}

ENG_API Eng::PipelineSkybox::PipelineSkybox(PipelineSkybox&& other) : Eng::Pipeline(std::move(other)), reserved(std::move(other.reserved))
{
    ENG_LOG_DETAIL("[M]");
}

ENG_API Eng::PipelineSkybox::~PipelineSkybox()
{
    ENG_LOG_DETAIL("[-]");
    if (this->isInitialized())
        free();
}

bool ENG_API Eng::PipelineSkybox::init() {
    // Already initialized?
    if (this->Eng::Managed::init() == false)
        return false;
    if (!this->isDirty())
        return false;

    // Build:
    reserved->vs.load(Eng::Shader::Type::vertex, pipeline_vs);
    reserved->fs.load(Eng::Shader::Type::fragment, pipeline_fs);
    if (reserved->program.build({ reserved->vs, reserved->fs }) == false)
    {
        ENG_LOG_ERROR("Unable to build skybox program");
        return false;
    }
    this->setProgram(reserved->program);

    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxIndices[] = {
        // Back face
        0, 1, 2,
        2, 3, 0,
        // Left face
        4, 5, 6,
        6, 7, 4,
        // Right face
        8, 9, 10,
        10, 11, 8,
        // Front face
        12, 13, 14,
        14, 15, 12,
        // Top face
        16, 17, 18,
        18, 19, 16,
        // Bottom face
        20, 21, 22,
        22, 23, 20
    };

    reserved->vao.init();
    reserved->vao.render();

    reserved->vbo.create(36, skyboxVertices);
    reserved->ebo.create(12, skyboxIndices);

    this->setDirty(false);
    return true;
}

bool ENG_API Eng::PipelineSkybox::free() {
    if (this->Eng::Managed::free() == false)
        return false;

    // Done:   
    return true;
}

bool ENG_API Eng::PipelineSkybox::render(const Eng::Texture& texture, const Eng::List& list) {
    // Safety net:
    if (texture == Eng::Texture::empty || list == Eng::List::empty)
    {
        ENG_LOG_ERROR("Invalid params");
        return false;
    }

    // Just to update the cache
    this->Eng::Pipeline::render(glm::mat4(1.0f), glm::mat4(1.0f), list);

    // Lazy-loading:
    if (this->isDirty())
        if (!this->init())
        {
            ENG_LOG_ERROR("Unable to render (initialization failed)");
            return false;
        }

    // Apply program:
    Eng::Program& program = getProgram();
    if (program == Eng::Program::empty)
    {
        ENG_LOG_ERROR("Invalid program");
        return false;
    }
    program.render();
    texture.render(0);

    Eng::Base& eng = Eng::Base::getInstance();
    Eng::Fbo::reset(eng.getWindowSize().x, eng.getWindowSize().y);
 
    reserved->vao.render();
    glDrawElements(GL_TRIANGLES, reserved->ebo.getNrOfFaces() * 3, GL_UNSIGNED_INT, nullptr);

    return true;
}