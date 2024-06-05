/**
 * @file		engine_pipeline.cpp
 * @brief	Simple forward-rendering pipeline
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
uniform mat4 projectionMat;
uniform mat3 normalMat;
uniform mat4 lightMatrix;
uniform mat4 worldMat;

// Varying:
out vec4 fragPosition;
out vec4 fragPositionLightSpace;
out vec3 normal;
out vec2 uv;
out vec3 _fragPos;

void main()
{
   normal = normalMat * a_normal.xyz;
   uv = a_uv;

   fragPosition = modelviewMat * vec4(a_vertex, 1.0f);
   fragPositionLightSpace = lightMatrix * fragPosition;
   _fragPos = (worldMat * vec4(a_vertex, 1.0f)).xyz;
   gl_Position = projectionMat * fragPosition;
})";


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Default pipeline fragment shader.
 */
static const std::string pipeline_fs = R"(

// Uniform:
#ifdef ENG_BINDLESS_SUPPORTED
   layout (bindless_sampler) uniform sampler2D texture0; // Albedo
   layout (bindless_sampler) uniform sampler2D texture1; // Normal
   layout (bindless_sampler) uniform sampler2D texture2; // Roughness
   layout (bindless_sampler) uniform sampler2D texture3; // Metalness
   layout (bindless_sampler) uniform samplerCube depthMap; // Shadow map
#else
   layout (binding = 0) uniform sampler2D texture0; // Albedo
   layout (binding = 1) uniform sampler2D texture1; // Normal
   layout (binding = 2) uniform sampler2D texture2; // Roughness
   layout (binding = 3) uniform sampler2D texture3; // Metalness
   layout (binding = 4) uniform samplerCube depthMap; // Shadow map
#endif

// Uniform (material):
uniform vec3 mtlEmission;
uniform vec3 mtlAlbedo;
uniform float mtlOpacity;
uniform float mtlRoughness;
uniform float mtlMetalness;
uniform float acne_bias;
uniform float pfc_radius_scale_factor;

// Uniform (light):
uniform uint totNrOfLights;
uniform vec3 lightColor;
uniform vec3 lightAmbient;
uniform vec3 lightPosition;

// Uniform (camera):
uniform float far_plane;
uniform vec3 viewPos;

// Uniform (flags):
uniform int depthBuffer;

// Varying:
in vec4 fragPosition;
in vec4 fragPositionLightSpace;
in vec3 normal;
in vec2 uv;
in vec3 _fragPos;

 
// Output to the framebuffer:
out vec4 outFragment;

float closestDepth;

vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

/**
 * Computes the amount of shadow for a given fragment.
 * @param fragPos frament coords in world coordinates
 * @return shadow intensity
 */
float shadowAmount(vec3 fragPos)
{
    vec3 lightPos = lightPosition.xyz;
    vec3 fragToLight = fragPos - lightPos;
    
    float currentDepth = length(fragToLight);
    float shadow = 0.0;
    int samples = 20;
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / pfc_radius_scale_factor;
    for(int i = 0; i < samples; ++i)
    {
        closestDepth = texture(depthMap, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= far_plane;   
        if(currentDepth - acne_bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
        
    // display closestDepth as debug (to visualize depth cubemap)
    // FragColor = vec4(vec3(closestDepth / far_plane), 1.0);    
       
    return shadow;
}  


//////////
// MAIN //
//////////
 
void main()
{
   // Texture lookup:
   vec4 albedo_texel = texture(texture0, uv);
   vec4 normal_texel = texture(texture1, uv);
   vec4 roughness_texel = mtlRoughness * texture(texture2, uv);
   vec4 metalness_texel = mtlMetalness * texture(texture3, uv);
   float justUseIt = albedo_texel.r + normal_texel.r + roughness_texel.r + metalness_texel.r;

   // Material props:
   justUseIt += mtlEmission.r + mtlAlbedo.r + mtlOpacity + mtlRoughness + mtlMetalness;

   vec3 fragColor = lightAmbient; 
   
   vec3 N = normalize(normal);   
   vec3 V = normalize(-fragPosition.xyz);   
   vec3 L = normalize(lightPosition - fragPosition.xyz);      

   // Light only front faces:
   if (dot(N, V) > 0.0f)
   {
      float shadow = 1.0f - shadowAmount(_fragPos);
      
      // Diffuse term:   
      float nDotL = max(0.0f, dot(N, L));      
      fragColor += roughness_texel.r * nDotL * lightColor * shadow;
      
      // Specular term:     
      vec3 H = normalize(L + V);                     
      float nDotH = max(0.0f, dot(N, H));         
      fragColor += (1.0f - roughness_texel.r) * pow(nDotH, 70.0f) * lightColor * shadow;         
   }
   
   outFragment = vec4((mtlEmission / float(totNrOfLights)) + fragColor * albedo_texel.xyz, justUseIt);
   if(depthBuffer == 1) {
      outFragment = vec4(vec3(closestDepth / far_plane), 1.0f); // Debugging shadow map
   }
})";



/////////////////////////
// RESERVED STRUCTURES //
/////////////////////////

/**
 * @brief PipelineDefault reserved structure.
 */
struct Eng::PipelineDefault::Reserved
{  
   Eng::Shader vs;
   Eng::Shader fs;
   Eng::Program program;
   
   bool wireframe;
   bool depthBuffer;
   float acne_bias;
   float pfc_radius_scale_factor;

   PipelineShadowMapping shadowMapping;


   /**
    * Constructor. 
    */
   Reserved() : wireframe{ false }, depthBuffer{ false }, acne_bias{ 0.05f }, pfc_radius_scale_factor{ 16.0f }
   {}
};



///////////////////////////////////
// BODY OF CLASS PipelineDefault //
///////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor.
 */
ENG_API Eng::PipelineDefault::PipelineDefault() : reserved(std::make_unique<Eng::PipelineDefault::Reserved>())
{	
   ENG_LOG_DETAIL("[+]");      
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor with name.
 * @param name node name
 */
ENG_API Eng::PipelineDefault::PipelineDefault(const std::string &name) : Eng::Pipeline(name), reserved(std::make_unique<Eng::PipelineDefault::Reserved>())
{	   
   ENG_LOG_DETAIL("[+]");   
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Move constructor. 
 */
ENG_API Eng::PipelineDefault::PipelineDefault(PipelineDefault &&other) : Eng::Pipeline(std::move(other)), reserved(std::move(other.reserved))
{  
   ENG_LOG_DETAIL("[M]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Destructor.
 */
ENG_API Eng::PipelineDefault::~PipelineDefault()
{	
   ENG_LOG_DETAIL("[-]");
   if (this->isInitialized())
      free();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes this pipeline. 
 * @return TF
 */
bool ENG_API Eng::PipelineDefault::init()
{
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
      ENG_LOG_ERROR("Unable to build default program");
      return false;
   }
   this->setProgram(reserved->program);

   reserved->program.setFloat("acne_bias", reserved->acne_bias);
   reserved->program.setFloat("pfc_radius_scale_factor", reserved->pfc_radius_scale_factor);
   // Done: 
   this->setDirty(false);
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases this pipeline.
 * @return TF
 */
bool ENG_API Eng::PipelineDefault::free()
{
   if (this->Eng::Managed::free() == false)
      return false;

   // Done:   
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets a reference to the shadow mapping pipeline.
 * @return shadow mapping pipeline reference
 */
const Eng::PipelineShadowMapping ENG_API &Eng::PipelineDefault::getShadowMappingPipeline() const
{
   return reserved->shadowMapping;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets the status of the wireframe status.
 * @return wireframe status
 */
bool ENG_API Eng::PipelineDefault::isWireframe() const
{
   return reserved->wireframe;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Sets the status of the wireframe flag.
 * @param flag wireframe flag
 */
void ENG_API Eng::PipelineDefault::setWireframe(bool flag)
{
   reserved->wireframe = flag;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Sets the status of the front face culling flag.
 * @param flag wireframe flag
 */
void ENG_API Eng::PipelineDefault::setFrontFaceCulling(bool flag)
{
    reserved->shadowMapping.setFrontFaceCulling(flag);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets the status of the front face culling flag.
 * @return wireframe status
 */
bool ENG_API Eng::PipelineDefault::isFrontFaceCulling() const
{
    return reserved->shadowMapping.isFrontFaceCulling();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * increments the bias to counteract acne
 * @param flag acne bias incr
 */
void ENG_API Eng::PipelineDefault::incr_bias(float val)
{
    reserved->acne_bias = (float)std::fmax(0, reserved->acne_bias+val);
    reserved->program.setFloat("acne_bias", reserved->acne_bias);
    std::cout << "Bias = " << reserved->acne_bias << std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * increments the pfc radius
 * @param flag scale factor of the pcf radius incr
 */

void ENG_API Eng::PipelineDefault::incr_pfc_radius(float val)
{
    reserved->pfc_radius_scale_factor = (float)std::fmax(1.0f, reserved->pfc_radius_scale_factor + val);
    reserved->program.setFloat("pfc_radius_scale_factor", reserved->pfc_radius_scale_factor);
   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets the status of the depth buffer status.
 * @return depth buffer status
 */
bool ENG_API Eng::PipelineDefault::isDepthBuffer() const
{
    return reserved->depthBuffer;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Sets the status of the depth buffer flag.
 * @param flag depth buffer flag
 */
void ENG_API Eng::PipelineDefault::setDepthBuffer(bool flag)
{
    reserved->depthBuffer = flag;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Main rendering method for the pipeline.
 * @param camera camera matrix
 * @param proj projection matrix
 * @param list list of renderables
 * @return TF
 */
bool ENG_API Eng::PipelineDefault::render(const glm::mat4& camera, const glm::mat4& proj, const Eng::List& list)
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

   // Just to update the cache:
   this->Eng::Pipeline::render(glm::mat4(1.0f), glm::mat4(1.0f), list);

   // Apply program:
   Eng::Program &program = getProgram();
   if (program == Eng::Program::empty)
   {
      ENG_LOG_ERROR("Invalid program");
      return false;
   }   
   program.render();   
   program.setMat4("projectionMat", proj);   
   program.setVec3("viewPos", camera[3]);
 
   
   // Wireframe is on?
   if (isWireframe())
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);      

   // Multipass rendering:
   uint32_t totNrOfLights = list.getNrOfLights();
   program.setUInt("totNrOfLights", totNrOfLights);

   for (uint32_t l = 0; l < totNrOfLights; l++)
   {
      // Enable addictive blending from light 1 on:
      if (l == 1)      
      {
         glEnable(GL_BLEND);
         glBlendFunc(GL_ONE, GL_ONE);         
      }
      
      // Render one light at time:
      const Eng::List::RenderableElem &lightRe = list.getRenderableElem(l);   
      const Eng::Light &light = dynamic_cast<const Eng::Light &>(lightRe.reference.get());

      // Render shadow map:
      reserved->shadowMapping.render(glm::inverse(lightRe.matrix), light.getProjMatrix(), list);

      // Re-enable this pipeline's program:
      program.render();   
      glm::mat4 lightFinalMatrix = camera * lightRe.matrix; // Light position in eye coords
      lightRe.reference.get().render(0, &lightFinalMatrix);

      lightFinalMatrix = light.getProjMatrix() * glm::inverse(lightRe.matrix) * glm::inverse(camera); // To convert from eye coords into light space
      program.setMat4("lightMatrix", lightFinalMatrix);
      program.setFloat("far_plane", 125.0f);
      int db = isDepthBuffer() ? 1 : 0;
      program.setInt("depthBuffer", db);
      reserved->shadowMapping.getShadowMap().render(4);      
      
      // Render meshes:
      list.render(camera, proj, Eng::List::Pass::meshes);      
   }

   // Disable blending, in case we used it:
   if (list.getNrOfLights() > 1)         
      glDisable(GL_BLEND);            

   // Wireframe is on?
   if (isWireframe())
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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
bool ENG_API Eng::PipelineDefault::render(const Eng::Camera &camera, const Eng::List &list)
{
   return this->render(glm::inverse(camera.getWorldMatrix()), camera.getProjMatrix(), list);
}