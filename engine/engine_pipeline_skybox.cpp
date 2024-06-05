#include "engine.h";

#include <GL/glew.h>
#include <GLFW/glfw3.h>

static const std::string pipeline_vs = R"(
    layout (location = 0) in vec3 aPos;

    out vec3 TexCoords; 

    uniform mat4 projection;
    uniform mat4 model;
    uniform mat4 modelview; 

    void main()
    {
        TexCoords = aPos;  
        vec4 pos = projection * modelview * model * vec4(aPos, 1.0);
        gl_Position = pos.xyww;
    }
)";

static const std::string pipeline_fs = R"(
    out vec4 FragColor;

    in vec3 TexCoords;
    
    uniform samplerCube skybox;
    uniform float pfc_radius_scale_factor;

    vec3 gridSamplingDisk[20] = vec3[]
    (
    vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
    vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
    vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
    vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
    vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
    );

    int samples = 20;
    
    
    float diskRadius = 0.01;

    
    void main()
    {       
        
        for(int i = 0; i < samples; ++i)
        {
                FragColor += texture(skybox, TexCoords + gridSamplingDisk[i] * diskRadius);
                
        }
        FragColor /= samples;
    }
)";

struct Eng::PipelineSkybox::Reserved
{
    Eng::Shader vs;
    Eng::Shader fs;
    Eng::Program program;

    unsigned int skyboxVAO;
    unsigned int skyboxVBO;
    float pfc_radius_scale_factor;

    /**
     * Constructor.
     */
    //Reserved() :{}
    
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

void ENG_API Eng::PipelineSkybox::incr_pfc_radius(float val)
{
    reserved->pfc_radius_scale_factor = (float)std::fmax(1.0f, reserved->pfc_radius_scale_factor + val);
    reserved->program.setFloat("pfc_radius_scale_factor", reserved->pfc_radius_scale_factor);
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
    reserved->program.setMat4("model", glm::scale(glm::mat4(1.0f), glm::vec3(100.0f)));

    reserved->pfc_radius_scale_factor = 20.0f;
    reserved->program.setFloat("pfc_radius_scale_factor", reserved->pfc_radius_scale_factor);
    

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

    glGenVertexArrays(1, &reserved->skyboxVAO);
    glGenBuffers(1, &reserved->skyboxVBO);
    glBindVertexArray(reserved->skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, reserved->skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);


    this->setDirty(false);
    return true;
}

bool ENG_API Eng::PipelineSkybox::free() {
    if (this->Eng::Managed::free() == false)
        return false;

    // Done:   
    return true;
}

void ENG_API Eng::PipelineSkybox::renderCube() {
    glBindVertexArray(reserved->skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

bool ENG_API Eng::PipelineSkybox::render(const Eng::Texture& texture, const Eng::List& list, const Eng::Camera& camera) {
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

    //std::cout << "Proj: " << glm::to_string(camera.getProjMatrix()) << std::endl;
    //std::cout << "Model: " << glm::to_string(camera.getWorldMatrix()) << std::endl;

    program.setMat4("projection", camera.getProjMatrix());
    program.setMat4("modelview", glm::inverse(camera.getWorldMatrix()));

    

    //std::cout << glm::to_string(g) << std::endl;

    renderCube();

    return true;
}