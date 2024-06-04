/**
 * @file		engine_pipeline_shadowmapping.cpp
 * @brief	A pipeline for generating planar shadow maps
 *
 * @author	Achille Peternier (achille.peternier@supsi.ch), (C) SUPSI
 */



 //////////////
 // #INCLUDE //
 //////////////

    // Main include:
#include "engine.h"

// OGL:      
#include <GL/glew.h>
#include <GLFW/glfw3.h>



/////////////
// SHADERS //
/////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Default pipeline vertex shader.
 */
static const std::string pipeline_vs = R"(
 
// Per-vertex data from VBOs:
layout(location = 0) in vec3 a_vertex;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;

// Uniforms:
uniform mat4 modelviewMat;
uniform mat4 lightInv;

void main()
{   
   gl_Position = lightInv * modelviewMat * vec4(a_vertex, 1.0f);
}
)";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Geometry shader that connects the vertex to the fragment shader.
 * It transforms the vertex from world space to 6 different light spaces.
 */
static const std::string pipeline_gs = R"(

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 shadowMatrices[6];

// FragPos from GS (output per emitvertex)
out vec4 FragPos;

void main()
{
    for(int face = 0; face < 6; ++face)
    {
        // built-in variable that specifies to which face we render.
        gl_Layer = face;

        // for each triangle vertex
        for(int i = 0; i < 3; ++i)
        {
            FragPos = gl_in[i].gl_Position;
            gl_Position = shadowMatrices[face] * FragPos;
            EmitVertex();
        }    
        EndPrimitive();
    }
}  
)";


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Default pipeline fragment shader.
 */
static const std::string pipeline_fs = R"(

in vec4 FragPos;

uniform vec3 lightPosition;
uniform float far_plane;
uniform mat4 lightInv;

void main()
{
    // get distance between fragment and light source
    float lightDistance = length(FragPos.xyz - (lightInv * vec4(lightPosition, 1.0f)).xyz);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / far_plane;
    
    // write this as modified depth
    gl_FragDepth = lightDistance;
}
)";



/////////////////////////
// RESERVED STRUCTURES //
/////////////////////////

/**
 * @brief PipelineShadowMapping reserved structure.
 */
struct Eng::PipelineShadowMapping::Reserved
{
    Eng::Shader vs;
    Eng::Shader gs; // add geometry shader to the pipeline
    Eng::Shader fs;
    Eng::Program program;
    Eng::Texture depthMap;
    Eng::Fbo fbo;


    /**
     * Constructor.
     */
    Reserved()
    {}
};



/////////////////////////////////////////
// BODY OF CLASS PipelineShadowMapping //
/////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor.
 */
ENG_API Eng::PipelineShadowMapping::PipelineShadowMapping() : reserved(std::make_unique<Eng::PipelineShadowMapping::Reserved>())
{
    ENG_LOG_DETAIL("[+]");
    this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor with name.
 * @param name node name
 */
ENG_API Eng::PipelineShadowMapping::PipelineShadowMapping(const std::string& name) : Eng::Pipeline(name), reserved(std::make_unique<Eng::PipelineShadowMapping::Reserved>())
{
    ENG_LOG_DETAIL("[+]");
    this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Move constructor.
 */
ENG_API Eng::PipelineShadowMapping::PipelineShadowMapping(PipelineShadowMapping&& other) : Eng::Pipeline(std::move(other)), reserved(std::move(other.reserved))
{
    ENG_LOG_DETAIL("[M]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Destructor.
 */
ENG_API Eng::PipelineShadowMapping::~PipelineShadowMapping()
{
    ENG_LOG_DETAIL("[-]");
    if (this->isInitialized())
        free();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets shadow map texture reference.
 * @return shadow map texture reference
 */
const Eng::Texture ENG_API& Eng::PipelineShadowMapping::getShadowMap() const
{
    return reserved->depthMap;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes this pipeline.
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::init()
{
    // Already initialized?
    if (this->Eng::Managed::init() == false)
        return false;
    if (!this->isDirty())
        return false;

    // Build:
    reserved->vs.load(Eng::Shader::Type::vertex, pipeline_vs);
    reserved->gs.load(Eng::Shader::Type::geometry, pipeline_gs);
    reserved->fs.load(Eng::Shader::Type::fragment, pipeline_fs);
    if (reserved->program.build({ reserved->vs, reserved->gs, reserved->fs }) == false)
    {
        ENG_LOG_ERROR("Unable to build shadow mapping program");
        return false;
    }
    this->setProgram(reserved->program);

    // Depth map:
    if (reserved->depthMap.create(depthTextureSize, depthTextureSize, Eng::Texture::Format::depth_cube) == false) // depth->depth_cube enum
    {
        ENG_LOG_ERROR("Unable to init depth map");
        return false;
    }


    // Depth FBO:
    reserved->fbo.attachTexture(reserved->depthMap);
    if (reserved->fbo.validate() == false)
    {
        ENG_LOG_ERROR("Unable to init depth FBO");
        return false;
    }

    // Done: 
    this->setDirty(false);
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases this pipeline.
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::free()
{
    if (this->Eng::Managed::free() == false)
        return false;

    // Done:   
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Main rendering method for the pipeline.
 * @param camera camera matrix
 * @param proj projection matrix
 * @param list list of renderables
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::render(const glm::mat4& camera, const glm::mat4& proj, const Eng::List& list)
{
    // Safety net:
    if (list == Eng::List::empty)
    {
        ENG_LOG_ERROR("Invalid params");
        return false;
    }

    // Lazy-loading:
    if (this->isDirty())
        if (!this->init())
        {
            ENG_LOG_ERROR("Unable to render (initialization failed)");
            return false;
        }

    // Just to update the cache
    this->Eng::Pipeline::render(glm::mat4(1.0f), glm::mat4(1.0f), list);

    // Create a projection matrix for the light source with a FOV of 90:
    Eng::Base& eng = Eng::Base::getInstance();
    //float nearPlane = 1.0f;
    float farPlane = 125.0f;
    //float aspectRatio = 1;
    //glm::mat4 lightProj = glm::perspective(glm::radians(90.0f), aspectRatio, nearPlane, farPlane);
    glm::mat4 lightProj = proj;

    // Get light position
    glm::vec3 lightPosition = glm::vec3(camera[3]);

    // Create transformation matrices used for generating the depth cubemap:
    std::vector<glm::mat4> shadowTransforms;
    shadowTransforms.push_back(lightProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
    shadowTransforms.push_back(lightProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
    shadowTransforms.push_back(lightProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
    shadowTransforms.push_back(lightProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
    shadowTransforms.push_back(lightProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
    shadowTransforms.push_back(lightProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

    // Apply program:
    Eng::Program& program = getProgram();
    if (program == Eng::Program::empty)
    {
        ENG_LOG_ERROR("Invalid program");
        return false;
    }

    program.render();
    program.setMat4("lightInv", camera);
    // Loads the 6 shadow matrices into the shader
    for (unsigned int i = 0; i < 6; ++i) {
        program.setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
    }
    program.setFloat("far_plane", farPlane);
    //program.setVec3("lightPos", lightPosition);

    // Bind FBO and change OpenGL settings:
    reserved->fbo.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    glColorMask(0, 0, 0, 0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // Render meshes:   
    list.render(camera, proj, Eng::List::Pass::meshes);

    // Redo OpenGL settings:
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glColorMask(1, 1, 1, 1);

    Eng::Fbo::reset(eng.getWindowSize().x, eng.getWindowSize().y);

    // Done:   
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Shortcut for using a camera instead of the explicit matrices.
 * @param camera camera to use
 * @param list list of renderables
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::render(const Eng::Camera& camera, const Eng::List& list)
{
    return this->render(glm::inverse(camera.getWorldMatrix()), camera.getProjMatrix(), list);
}