/**
 * @file		main.cpp
 * @brief	Engine usage example
 * 
 * @author	Achille Peternier (achille.peternier@supsi.ch), (C) SUPSI
 */



//////////////
// #INCLUDE //
//////////////

   // Main engine header:
   #include "engine.h"

   // C/C++:
   #include <iostream>
   #include <chrono>



//////////   
// VARS //
//////////

   // Mouse status:
   double oldMouseX, oldMouseY;   
   bool mouseBR, mouseBL;

   // Camera:
   Eng::Camera camera;   

   // Light (loaded from OVO file later):
   std::reference_wrapper<Eng::Light> light = Eng::Light::empty;

   // Pipelines:
   Eng::PipelineDefault dfltPipe;
   Eng::PipelineFullscreen2D full2dPipe;
   Eng::PipelineSkybox skyboxPipe;

   // Flags:
   bool showShadowMap = false;
   bool perspectiveProj = false;



///////////////
// CALLBACKS //
///////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Mouse cursor callback.
 * @param mouseX updated mouse X coordinate
 * @param mouseY updated mouse Y coordinate
 */
void mouseCursorCallback(double mouseX, double mouseY)
{
   // ENG_LOG_DEBUG("x: %.1f, y: %.1f", mouseX, mouseY);
   float deltaX = (float)(mouseY - oldMouseY);
   float deltaY = (float)(mouseX - oldMouseX);
   oldMouseX = mouseX;
   oldMouseY = mouseY;

   // Rotate camera around:
   if (mouseBL)
   {
      camera.rotateAzimuth(deltaY);
      camera.rotatePolar(deltaX);
   }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Mouse button callback.
 * @param button mouse button id
 * @param action action
 * @param mods modifiers
 */
void mouseButtonCallback(int button, int action, int mods)
{
   // ENG_LOG_DEBUG("button: %d, action: %d, mods: %d", button, action, mods);
   switch (button)
   {
      case 0: mouseBL = (bool) action; break;
      case 1: mouseBR = (bool) action; break;
   }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Mouse scroll callback.
 * @param scrollX updated mouse scroll X coordinate
 * @param scrollY updated mouse scroll Y coordinate
 */
void mouseScrollCallback(double scrollX, double scrollY)
{
   // ENG_LOG_DEBUG("x: %.1f, y: %.1f", scrollX, scrollY);   
   camera.zoom((float) scrollY);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Keyboard callback.
 * @param key key code
 * @param scancode key scan code
 * @param action action
 * @param mods modifiers
 */
void keyboardCallback(int key, int scancode, int action, int mods)
{
   // ENG_LOG_DEBUG("key: %d, scancode: %d, action: %d, mods: %d", key, scancode, action, mods);
   if ( action == 0 ) {
       switch (key)
       {
       case 'W': dfltPipe.setWireframe(!dfltPipe.isWireframe()); break;
       case 'S': showShadowMap = !showShadowMap; break;
       case 'D': dfltPipe.setDepthBuffer(!dfltPipe.isDepthBuffer()); break;
       case 'I': light.get().setMatrix(glm::translate(light.get().getMatrix(), glm::vec3(0.0f, 1.0f, 0.0f))); break;
       case 'K': light.get().setMatrix(glm::translate(light.get().getMatrix(), glm::vec3(0.0f, -1.0f, 0.0f))); break;
       case 'J': light.get().setMatrix(glm::translate(light.get().getMatrix(), glm::vec3(1.0f, 0.0f, 0.0f))); break;
       case 'L': light.get().setMatrix(glm::translate(light.get().getMatrix(), glm::vec3(-1.0f, 0.0f, 0.0f))); break;
       case 'C': dfltPipe.incr_bias(-0.05); break;
       case 'V': dfltPipe.incr_bias(0.05); break;
       case 'Y': dfltPipe.incr_pfc_radius(2.0f); skyboxPipe.incr_pfc_radius(2.0f); break;
       case 'X': dfltPipe.incr_pfc_radius(-2.0f); skyboxPipe.incr_pfc_radius(-2.0f);  break;
       case ' ': dfltPipe.setFrontFaceCulling(!dfltPipe.isFrontFaceCulling()); std::cout << dfltPipe.isFrontFaceCulling() << std::endl; break;
       }
   }
}



//////////
// MAIN //
//////////

/**
 * Application entry point.
 * @param argc number of command-line arguments passed
 * @param argv array containing up to argc passed arguments
 * @return error code (0 on success, error code otherwise)
 */
int main(int argc, char *argv[])
{
   // Credits:
   std::cout << "Engine demo, A. Peternier (C) SUPSI" << std::endl;
   std::cout << std::endl;

   // Init engine:
   Eng::Base &eng = Eng::Base::getInstance();
   eng.init();

   // Register callbacks:
   eng.setMouseCursorCallback(mouseCursorCallback);
   eng.setMouseButtonCallback(mouseButtonCallback);
   eng.setMouseScrollCallback(mouseScrollCallback);
   eng.setKeyboardCallback(keyboardCallback);
     

   /////////////////
   // Loading scene:   
   Eng::Ovo ovo; 
   Eng::Node &root = ovo.load("Simple3dScene.ovo");
   //Eng::Node &root = ovo.load("new.ovo");
   std::cout << "Scene graph:\n" << root.getTreeAsString() << std::endl;
   
   // Get light ref:
   light = dynamic_cast<Eng::Light &>(Eng::Container::getInstance().find("Omni001"));      
   //light.get().setAmbient({ 0.3f, 0.3f, 0.3f });
   //light.get().setColor({ 1.5f, 1.5f, 1.5f });
   //light.get().setMatrix(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f)));
   light.get().setMatrix(glm::translate(glm::mat4(1.0f), glm::vec3(light.get().getMatrix()[3])));

   float nearPlane = 1.0f;
   float farPlane = 200.0f;
   eng.setFarPlane(farPlane);

   glm::mat4 lightProj = glm::perspective(glm::radians(90.0f), eng.getWindowSize().x / (float)eng.getWindowSize().y, nearPlane, farPlane);
   light.get().setProjMatrix(lightProj);

   //std::reference_wrapper<Eng::Light> light2 = dynamic_cast<Eng::Light&>(Eng::Container::getInstance().find("Omni002"));
   //light2.get().setMatrix(glm::translate(glm::mat4(1.0f), glm::vec3(light2.get().getMatrix()[3])));
   //light2.get().setProjMatrix(lightProj);

   //Eng::Mesh &hose = dynamic_cast<Eng::Mesh &>(Eng::Container::getInstance().find("Hose001"));
   //Eng::Mesh &hedra = dynamic_cast<Eng::Mesh &>(Eng::Container::getInstance().find("Hedra001"));
   //Eng::Mesh &chamfer = dynamic_cast<Eng::Mesh &>(Eng::Container::getInstance().find("ChamferCyl001"));

   // Rendering elements:
   Eng::List list;      
   
   // Init camera:
   camera.setProjMatrix(glm::perspective(glm::radians(45.0f), eng.getWindowSize().x / (float) eng.getWindowSize().y, nearPlane, farPlane));
   Eng::Node cameraLookAt = Eng::Node();
   cameraLookAt.setName("CameraLookAt");
   cameraLookAt.setMatrix(glm::translate(glm::mat4(1.0f), glm::vec3(-25.0f, 12.5f, 0.0f)));
   camera.lookAt(root);

  
   /////////////
   // Main loop:
   std::cout << "Entering main loop..." << std::endl;      
   std::chrono::high_resolution_clock timer;
   float fpsFactor = 0.0f;



   while (eng.processEvents())
   {      
      auto start = timer.now();

      // Update viewpoint:
      camera.update();   

      //hose.setMatrix(glm::rotate(hose.getMatrix(), glm::radians(15.0f * fpsFactor), glm::vec3(0.0f, 1.0f, 0.0f)));
      //hedra.setMatrix(glm::rotate(hedra.getMatrix(), glm::radians(30.0f * fpsFactor), glm::vec3(0.0f, 1.0f, 0.0f)));
      //chamfer.setMatrix(glm::rotate(chamfer.getMatrix(), glm::radians(10.0f * fpsFactor), glm::vec3(10.0f, 0.0f, 1.0f)));
      
      // Update list:
      list.reset();
      list.process(root);
      
      // Main rendering:
      eng.clear();
         dfltPipe.render(camera, list);

         if (showShadowMap)
         {            
            eng.clear();      
            skyboxPipe.render(dfltPipe.getShadowMappingPipeline().getShadowMap(), list, camera);
         }
      eng.swap();    

      auto stop = timer.now();
      auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() / 1000.0f;
      float fps = (1.0f / deltaTime) * 1000.0f;
      fpsFactor = 1.0f / fps;
      std::cout << "fps: " << fps << std::endl;
   }
   std::cout << "Leaving main loop..." << std::endl;

   // Release engine:
   eng.free();

   // Done:
   std::cout << "[application terminated]" << std::endl;
   return 0;
}
