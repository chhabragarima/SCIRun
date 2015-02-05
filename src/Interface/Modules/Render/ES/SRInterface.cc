/*
For more information, please see: http://software.sci.utah.edu

The MIT License

Copyright (c) 2013 Scientific Computing and Imaging Institute,
University of Utah.


Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

// Needed for OpenGL include files on Travis:
#include <gl-platform/GLPlatform.hpp>
#include <Interface/Modules/Render/UndefiningX11Cruft.h>
#include <QtOpenGL/QGLWidget>

#include <Interface/Modules/Render/namespaces.h>
#include <Interface/Modules/Render/ES/SRInterface.h>
#include <Interface/Modules/Render/ES/SRCamera.h>

#include <Core/Application/Application.h>

// CPM modules.

#include <gl-state/GLState.hpp>
#include <es-general/comp/StaticScreenDims.hpp>
#include <es-general/comp/StaticCamera.hpp>
#include <es-general/comp/StaticOrthoCamera.hpp>
#include <es-general/comp/StaticObjRefID.hpp>
#include <es-general/comp/StaticGlobalTime.hpp>
#include <es-general/comp/Transform.hpp>
#include <es-render/comp/StaticGeomMan.hpp>
#include <es-render/comp/StaticIBOMan.hpp>
#include <es-render/comp/StaticVBOMan.hpp>
#include <es-render/comp/StaticShaderMan.hpp>
#include <es-render/comp/Texture.hpp>
#include <es-render/util/Uniform.hpp>
#include <es-render/comp/VBO.hpp>
#include <es-render/comp/IBO.hpp>
#include <es-render/comp/Shader.hpp>
#include <es-fs/fscomp/StaticFS.hpp>
#include <es-fs/Filesystem.hpp>
#include <es-fs/FilesystemSync.hpp>

#include "CoreBootstrap.h"
#include "comp/StaticSRInterface.h"
#include "comp/RenderBasicGeom.h"
#include "comp/RenderColorMapGeom.h"
#include "comp/SRRenderState.h"
#include "comp/RenderList.h"
#include "comp/StaticWorldLight.h"
#include "comp/LightingUniforms.h"
#include "systems/RenderBasicSys.h"
#include "systems/RenderColorMapSys.h"
#include "systems/RenderTransBasicSys.h"
#include "systems/RenderTransColorMapSys.h"
#include <Core/Datatypes/ColorMap.h>

using namespace SCIRun::Core::Datatypes;

using namespace std::placeholders;

namespace fs = CPM_ES_FS_NS;

namespace SCIRun {
	namespace Render {

		//------------------------------------------------------------------------------
		SRInterface::SRInterface(std::shared_ptr<Gui::GLContext> context,
			const std::vector<std::string>& shaderDirs) :
			mMouseMode(MOUSE_OLDSCIRUN),
			mScreenWidth(640),
			mScreenHeight(480),
			mContext(context),
			mCamera(new SRCamera(*this))  // Should come after all vars have been initialized.
		{
			// Create default colormaps.
			generateColormaps();

      showOrientation_ = true;
      autoRotate_ = false;

			// Construct ESCore. We will need to bootstrap the core. We should also
			// probably add utility static classes.
			setupCore();
		}

		//------------------------------------------------------------------------------
		SRInterface::~SRInterface()
		{
			glDeleteTextures(1, &mRainbowCMap);
			glDeleteTextures(1, &mGrayscaleCMap);
			glDeleteTextures(1, &mBlackBodyCMap);
		}

		//------------------------------------------------------------------------------
		void SRInterface::setupCore()
		{
			mCore.addUserSystem(getSystemName_CoreBootstrap());

			// Add screen height / width static component.
			{
				gen::StaticScreenDims dims;
				dims.width = static_cast<uint32_t>(mScreenWidth);
				dims.height = static_cast<uint32_t>(mScreenHeight);
				mCore.addStaticComponent(dims);
			}

			// Be exceptionally careful with non-serializable components. They must be
			// created outside of the normal bootstrap. They cannot depend on anything
			// being serialized correctly. In this circumstance, the filesystem component
			// is system dependent and cannot be reliably serialized, so we add it and
			// mark it as non-serializable.
  {
	  // Generate synchronous filesystem, manually add its static component,
	  // then mark it as non-serializable.
    std::string filesystemRoot = SCIRun::Core::Application::Instance().executablePath().string();
    filesystemRoot += boost::filesystem::path::preferred_separator;
	  fs::StaticFS fileSystem(
		  std::shared_ptr<fs::FilesystemSync>(new fs::FilesystemSync(filesystemRoot)));
	  mCore.addStaticComponent(fileSystem);
	  mCore.disableComponentSerialization<fs::StaticFS>();
  }

			// Add StaticSRInterface
  {
	  StaticSRInterface iface(this);
	  mCore.addStaticComponent(iface);
  }
		}

		//------------------------------------------------------------------------------
		void SRInterface::setMouseMode(MouseMode mode)
		{
			mMouseMode = mode;
		}

		//------------------------------------------------------------------------------
		SRInterface::MouseMode SRInterface::getMouseMode()
		{
			return mMouseMode;
		}

		//------------------------------------------------------------------------------
		void SRInterface::eventResize(size_t width, size_t height)
		{
			mScreenWidth = width;
			mScreenHeight = height;

			mContext->makeCurrent();
			GL(glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height)));

			// Obtain StaticScreenDims component and populate.
			gen::StaticScreenDims* dims = mCore.getStaticComponent<gen::StaticScreenDims>();
			if (dims)
			{
				dims->width = static_cast<size_t>(width);
				dims->height = static_cast<size_t>(height);
			}

			// Setup default camera projection.
			gen::StaticCamera* cam = mCore.getStaticComponent<gen::StaticCamera>();
			gen::StaticOrthoCamera* orthoCam = mCore.getStaticComponent<gen::StaticOrthoCamera>();

			if (cam == nullptr || orthoCam == nullptr) return;

			float aspect = static_cast<float>(width) / static_cast<float>(height);

			float perspFOVY = 0.59f;
			float perspZNear = 0.01f;
			float perspZFar = 20000.0f;
			glm::mat4 proj = glm::perspective(perspFOVY, aspect, perspZNear, perspZFar);
			cam->data.setProjection(proj, perspFOVY, aspect, perspZNear, perspZFar);
            cam->data.winWidth = static_cast<float>(width);

			// Setup default ortho camera projection
			float orthoZNear = -1000.0f;
			float orthoZFar = 1000.0f;
			glm::mat4 orthoProj =
				glm::ortho(/*left*/   -1.0f,      /*right*/ 1.0f,
				/*bottom*/ -1.0f,      /*top*/   1.0f,
				/*znear*/  orthoZNear, /*zfar*/  orthoZFar);
			orthoCam->data.setOrthoProjection(orthoProj, aspect, 2.0f, 2.0f, orthoZNear, orthoZFar);
            orthoCam->data.winWidth = static_cast<float>(width);
		}

		//------------------------------------------------------------------------------
		void SRInterface::inputMouseDown(const glm::ivec2& pos, MouseButton btn)
		{
			mCamera->mouseDownEvent(pos, btn);
		}

		//------------------------------------------------------------------------------
		void SRInterface::inputMouseMove(const glm::ivec2& pos, MouseButton btn)
		{
			mCamera->mouseMoveEvent(pos, btn);
		}

		//------------------------------------------------------------------------------
		void SRInterface::inputMouseWheel(int32_t delta)
		{
			mCamera->mouseWheelEvent(delta);
		}

		//------------------------------------------------------------------------------
		void SRInterface::doAutoView()
		{
			if (mSceneBBox.valid())
			{
				mCamera->doAutoView(mSceneBBox);
			}
		}

		//------------------------------------------------------------------------------
		void SRInterface::setView(const glm::vec3& view, const glm::vec3& up)
		{
			mCamera->setView(view, up);
		}

		//------------------------------------------------------------------------------
		void SRInterface::showOrientation(bool value)
		{
			showOrientation_ = value;
		}

    //------------------------------------------------------------------------------
    void SRInterface::setBackgroundColor(QColor color)
    {
      mCore.setBackgroundColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }

		//------------------------------------------------------------------------------
		void SRInterface::inputMouseUp(const glm::ivec2& /*pos*/, MouseButton /*btn*/)
		{
		}

		//------------------------------------------------------------------------------
		uint64_t SRInterface::getEntityIDForName(const std::string& name, int port)
		{
			return (static_cast<uint64_t>(std::hash<std::string>()(name)) >> 8) + (static_cast<uint64_t>(port) << 56);
		}

		//------------------------------------------------------------------------------
		void SRInterface::handleGeomObject(boost::shared_ptr<Core::Datatypes::GeometryObject> obj, int port)
		{
			// Ensure our rendering context is current on our thread.
			mContext->makeCurrent();

			std::string objectName = obj->objectName;
			Core::Geometry::BBox bbox; // Bounding box containing all vertex buffer objects.

			// Check to see if the object already exists in our list. If so, then
			// remove the object. We will re-add it.
			auto foundObject = std::find_if(
				mSRObjects.begin(), mSRObjects.end(),
				[&objectName, this](const SRObject& obj) -> bool
			{
				if (obj.mName == objectName)
					return true;
				else
					return false;
			});

			ren::VBOMan& vboMan = *mCore.getStaticComponent<ren::StaticVBOMan>()->instance;
			ren::IBOMan& iboMan = *mCore.getStaticComponent<ren::StaticIBOMan>()->instance;
			if (foundObject != mSRObjects.end())
			{
				// Iterate through each of the passes and remove their associated
				// entity ID.
				for (const auto& pass : foundObject->mPasses)
				{
					uint64_t entityID = getEntityIDForName(pass.passName, port);
					mCore.removeEntity(entityID);
				}

				// We need to renormalize the core after removing entities. We don't need
				// to run a new pass however. Renormalization is enough to remove
				// old entities from the system.
				mCore.renormalize(true);

				// Run a garbage collection cycle for the VBOs and IBOs. We will likely
				// be using similar VBO and IBO names.
				vboMan.runGCCycle(mCore);
				iboMan.runGCCycle(mCore);

				// Remove the object from the entity system.
				mSRObjects.erase(foundObject);

			}

			// Add vertex buffer objects.
			int nameIndex = 0;
			for (auto it = obj->mVBOs.cbegin(); it != obj->mVBOs.cend(); ++it)
			{
				const Core::Datatypes::GeometryObject::SpireVBO& vbo = *it;

				if (vbo.onGPU)
				{
					// Generate vector of attributes to pass into the entity system.
					std::vector<std::tuple<std::string, size_t, bool>> attributeData;
					for (const auto& attribData : vbo.attributes)
					{
						attributeData.push_back(std::make_tuple(attribData.name, attribData.sizeInBytes, attribData.normalize));
					}

					GLuint vboID = vboMan.addInMemoryVBO(vbo.data->getBuffer(), vbo.data->getBufferSize(),
						attributeData, vbo.name);
				}

				bbox.extend(vbo.boundingBox);
			}

			// Add index buffer objects.
			nameIndex = 0;
			for (auto it = obj->mIBOs.cbegin(); it != obj->mIBOs.cend(); ++it)
			{
				const Core::Datatypes::GeometryObject::SpireIBO& ibo = *it;
				GLenum primType = GL_UNSIGNED_SHORT;
				switch (ibo.indexSize)
				{
				case 1: // 8-bit
					primType = GL_UNSIGNED_BYTE;
					break;

				case 2: // 16-bit
					primType = GL_UNSIGNED_SHORT;
					break;

				case 4: // 32-bit
					primType = GL_UNSIGNED_INT;
					break;

				default:
					primType = GL_UNSIGNED_INT;
					throw std::invalid_argument("Unable to determine index buffer depth.");
					break;
				}

				GLenum primitive = GL_TRIANGLES;
				switch (ibo.prim)
				{
				case Core::Datatypes::GeometryObject::SpireIBO::POINTS:
					primitive = GL_POINTS;
					break;

				case Core::Datatypes::GeometryObject::SpireIBO::LINES:
					primitive = GL_LINES;
					break;

				case Core::Datatypes::GeometryObject::SpireIBO::TRIANGLES:
				default:
					primitive = GL_TRIANGLES;
					break;
				}

				int numPrimitives = ibo.data->getBufferSize() / ibo.indexSize;

				iboMan.addInMemoryIBO(ibo.data->getBuffer(), ibo.data->getBufferSize(), primitive, primType,
					numPrimitives, ibo.name);
			}

			// Add default identity transform to the object globally (instead of per-pass)
			glm::mat4 xform;
			mSRObjects.push_back(SRObject(objectName, xform, bbox, obj->mColorMap, port));
			SRObject& elem = mSRObjects.back();

			ren::ShaderMan& shaderMan = *mCore.getStaticComponent<ren::StaticShaderMan>()->instance;

			// Add passes
			for (auto it = obj->mPasses.begin(); it != obj->mPasses.end(); ++it)
			{
				Core::Datatypes::GeometryObject::SpireSubPass& pass = *it;

				uint64_t entityID = getEntityIDForName(pass.passName, port);

				if (pass.renderType == Core::Datatypes::GeometryObject::RENDER_VBO_IBO)
				{
					addVBOToEntity(entityID, pass.vboName);
					addIBOToEntity(entityID, pass.iboName);
				}
				else
				{
					// We will be constructing a render list from the VBO and IBO.
					RenderList list;

					for (auto it = obj->mVBOs.cbegin(); it != obj->mVBOs.cend(); ++it)
					{
						const Core::Datatypes::GeometryObject::SpireVBO& vbo = *it;
						if (vbo.name == pass.vboName)
						{
							list.data = vbo.data;
							list.attributes = vbo.attributes;
							list.renderType = pass.renderType;
							list.numElements = vbo.numElements;
							mCore.addComponent(entityID, list);
							break;
						}
					}

          // Lookup the VBOs and IBOs associated with this particular draw list
          // and add them to our entity in question.
          std::string assetName = "Assets/sphere.geom";

          if (pass.renderType == Core::Datatypes::GeometryObject::RENDER_RLIST_SPHERE)
          {
            assetName = "Assets/sphere.geom";
          }

          if (pass.renderType == Core::Datatypes::GeometryObject::RENDER_RLIST_CYLINDER)
          {
            assetName = "Assests/arrow.geom";
          }

          addVBOToEntity(entityID, assetName);
          addIBOToEntity(entityID, assetName);
        }

        // Load vertex and fragment shader will use an already loaded program.
        //addShaderToEntity(entityID, pass.programName);
        shaderMan.loadVertexAndFragmentShader(mCore, entityID, pass.programName);

        // Add transformation
        gen::Transform trafo;

        if (pass.renderType == Core::Datatypes::GeometryObject::RENDER_RLIST_SPHERE)
        {
          double scale = pass.scalar;
          trafo.transform[0].x = scale;
          trafo.transform[1].y = scale;
          trafo.transform[2].z = scale;
        }
        mCore.addComponent(entityID, trafo);

        // Add lighting uniform checks
        LightingUniforms lightUniforms;
        mCore.addComponent(entityID, lightUniforms);

        // Add SCIRun render state.
        SRRenderState state;
        state.state = pass.renderState;
        mCore.addComponent(entityID, state);

        // Add appropriate renderer based on the color scheme to use.
        if (pass.mColorScheme == Core::Datatypes::GeometryObject::COLOR_UNIFORM
          || pass.mColorScheme == Core::Datatypes::GeometryObject::COLOR_IN_SITU)
        {
          RenderBasicGeom geom;
          mCore.addComponent(entityID, geom);
        }
        else if (pass.mColorScheme == Core::Datatypes::GeometryObject::COLOR_MAP
          && obj->mColorMap)
        {
          RenderColorMapGeom geom;
          mCore.addComponent(entityID, geom);

          // Construct texture component and add it to our entity for rendering.
          ren::Texture component;
          component.textureUnit = 0;
          component.setUniformName("uTX0");
          component.textureType = GL_TEXTURE_1D;

          // Setup appropriate texture to render the color map.
          if (*obj->mColorMap == "Rainbow") {
            component.glid = mRainbowCMap;
          } else if (*obj->mColorMap == "Blackbody") {
            component.glid = mBlackBodyCMap;
          } else {
            component.glid = mGrayscaleCMap;
          }
          mCore.addComponent(entityID, component);

          // Compare entity and system requirements.
          //mCore.displayEntityVersusSystemInfo(entityID, getSystemName_RenderColorMap());
        }
        else
        {
          std::cerr << "Renderer: Unknown color scheme!" << std::endl;
          RenderBasicGeom geom;
          mCore.addComponent(entityID, geom);
        }

        // Ensure common uniforms are covered.
        ren::CommonUniforms commonUniforms;
        mCore.addComponent(entityID, commonUniforms);

        for (const auto& uniform : pass.mUniforms)
        {
          applyUniform(entityID, uniform);
        }

        // Add components associated with entity. We just need a base class which
        // we can pass in an entity ID, then a derived class which bundles
        // all associated components (including types) together. We can use
        // a variadic template for this. This will allow us to place any components
        // we want on the objects in question in show field. This could lead to
        // much simpler customization.

        // Add a pass to our local object.
        elem.mPasses.emplace_back(pass.passName, pass.renderType);
        mCore.addComponent(entityID, pass);

      }

      // Recalculate scene bounding box. Should only be done when an object is added.
      mSceneBBox.reset();
      for (auto it = mSRObjects.begin(); it != mSRObjects.end(); ++it)
      {
        if (it->mBBox.valid())
        {
          mSceneBBox.extend(it->mBBox);
        }
      }
    }

		//------------------------------------------------------------------------------
		void SRInterface::addVBOToEntity(uint64_t entityID, const std::string& vboName)
		{
			ren::VBOMan& vboMan = *mCore.getStaticComponent<ren::StaticVBOMan>()->instance;
			ren::VBO vbo;

			vbo.glid = vboMan.hasVBO(vboName);

			mCore.addComponent(entityID, vbo);
		}

		//------------------------------------------------------------------------------
		void SRInterface::addIBOToEntity(uint64_t entityID, const std::string& iboName)
		{
			ren::IBOMan& iboMan = *mCore.getStaticComponent<ren::StaticIBOMan>()->instance;
			ren::IBO ibo;

			auto iboData = iboMan.getIBOData(iboName);

			ibo.glid = iboMan.hasIBO(iboName);
			ibo.primType = iboData.primType;
			ibo.primMode = iboData.primMode;
			ibo.numPrims = iboData.numPrims;

			mCore.addComponent(entityID, ibo);
		}

		//------------------------------------------------------------------------------
		void SRInterface::addShaderToEntity(uint64_t entityID, const std::string& shaderName)
		{
			ren::ShaderMan& shaderMan = *mCore.getStaticComponent<ren::StaticShaderMan>()->instance;
			ren::Shader shader;

			shader.glid = shaderMan.getIDForAsset(shaderName.c_str());

			mCore.addComponent(entityID, shader);
		}

		//------------------------------------------------------------------------------
		void SRInterface::applyUniform(uint64_t entityID, const Core::Datatypes::GeometryObject::SpireSubPass::Uniform& uniform)
		{
			switch (uniform.type)
			{
			case Core::Datatypes::GeometryObject::SpireSubPass::Uniform::UNIFORM_SCALAR:
				ren::addGLUniform(mCore, entityID, uniform.name.c_str(), static_cast<float>(uniform.data.x));
				break;

			case Core::Datatypes::GeometryObject::SpireSubPass::Uniform::UNIFORM_VEC4:
				ren::addGLUniform(mCore, entityID, uniform.name.c_str(), uniform.data);
				break;
			}
		}

		//------------------------------------------------------------------------------
		void SRInterface::removeAllGeomObjects()
		{
			mContext->makeCurrent();
			for (auto it = mSRObjects.begin(); it != mSRObjects.end(); ++it)
			{
				// Iterate through each of the passes and remove their associated
				// entity ID.
				for (const auto& pass : it->mPasses)
				{
					uint64_t entityID = getEntityIDForName(pass.passName, it->mPort);
					mCore.removeEntity(entityID);
				}
			}

			mCore.renormalize(true);

			mSRObjects.clear();
		}

		//------------------------------------------------------------------------------
		void SRInterface::gcInvalidObjects(const std::vector<std::string>& validObjects)
		{
			for (auto it = mSRObjects.begin(); it != mSRObjects.end();)
			{
				if (std::find(validObjects.begin(), validObjects.end(), it->mName) == validObjects.end())
				{
					for (const auto& pass : it->mPasses)
					{
						uint64_t entityID = getEntityIDForName(pass.passName, it->mPort);
						mCore.removeEntity(entityID);
					}
					it = mSRObjects.erase(it);
				}
				else
				{
					++it;
				}
			}

			mCore.renormalize(true);
		}

		//------------------------------------------------------------------------------
		void SRInterface::doFrame(double currentTime, double constantDeltaTime)
		{
			/// \todo Only render a frame if something has changed (new or deleted
			///       objects, or the view point has changed).

			mContext->makeCurrent();

			updateCamera();
			updateWorldLight();

			mCore.execute(currentTime, constantDeltaTime);

			if (showOrientation_)
			{
				// Do not even attempt to render if the framebuffer is not complete.
				// This can happen when the rendering window is hidden (in SCIRun5 for
				// example);
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
				{
					renderCoordinateAxes();
				}
			}


			// Set directional light source (in world space).
			// Need to set a static light source component which will be used when
			// uLightDirWorld is requested from a shader.
			// glm::vec3 viewDir = viewToWorld[2].xyz();
			// viewDir = -viewDir; // Cameras look down -Z.
			//mSpire->addGlobalUniform("uLightDirWorld", viewDir);
		}

		//------------------------------------------------------------------------------
		void SRInterface::updateCamera()
		{
			// Update the static camera with the appropriate world to view transform.
			mCamera->applyTransform();
			glm::mat4 viewToWorld = mCamera->getViewToWorld();

			gen::StaticCamera* camera = mCore.getStaticComponent<gen::StaticCamera>();
			if (camera)
			{
				camera->data.setView(viewToWorld);
			}
		}

		//------------------------------------------------------------------------------
		void SRInterface::updateWorldLight()
		{
			glm::mat4 viewToWorld = mCamera->getViewToWorld();

			// Set directional light source (in world space).
			StaticWorldLight* light = mCore.getStaticComponent<StaticWorldLight>();
			if (light)
			{
				glm::vec3 viewDir = viewToWorld[2].xyz();
				viewDir = -viewDir; // Cameras look down -Z.
				light->lightDir = viewDir;
			}
		}

		//------------------------------------------------------------------------------
		void SRInterface::renderCoordinateAxes()
		{
			// Only execute if static rendering resources are available. All of these
			// resource checks wouldn't be necessary if we were operating in the perview
			// of the entity system.
			if (mCore.getStaticComponent<ren::StaticVBOMan>() == nullptr) return;

			// This rendering algorithm is fairly inefficient. Use the entity component
			// system to optimize the rendering of a large amount of objects.
			ren::VBOMan& vboMan = *mCore.getStaticComponent<ren::StaticVBOMan>()->instance;
			ren::IBOMan& iboMan = *mCore.getStaticComponent<ren::StaticIBOMan>()->instance;
			ren::ShaderMan& shaderMan = *mCore.getStaticComponent<ren::StaticShaderMan>()->instance;

			GLuint arrowVBO = vboMan.hasVBO("Assets/arrow.geom");
			GLuint arrowIBO = iboMan.hasIBO("Assets/arrow.geom");
			GLuint shader = shaderMan.getIDForAsset("Shaders/DirPhong");

			// Bail if assets have not been loaded yet (asynchronous loading may take a
			// few frames).
			if (arrowVBO == 0 || arrowIBO == 0 || shader == 0) { return; }

			const ren::IBOMan::IBOData* iboData;
			try
			{
				iboData = &iboMan.getIBOData("Assets/arrow.geom");
			}
			catch (...)
			{
				// Return if IBO data not available.
				return;
			}

			// Ensure shader attributes are setup appropriately.
			mArrowAttribs.setup(arrowVBO, shader, vboMan);

			glm::mat4 trafo;

			GL(glUseProgram(shader));

			GL(glBindBuffer(GL_ARRAY_BUFFER, arrowVBO));
			GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, arrowIBO));

      bool depthMask = glIsEnabled(GL_DEPTH_WRITEMASK);
      bool cullFace = glIsEnabled(GL_CULL_FACE);
      bool blend = glIsEnabled(GL_BLEND);

      GL(glDepthMask(GL_TRUE));
      GL(glDisable(GL_CULL_FACE));
      GL(glDisable(GL_BLEND));

			// Note that we can pull aspect ratio from the screen dimensions static
			// variable.
			gen::StaticScreenDims* dims = mCore.getStaticComponent<gen::StaticScreenDims>();
			float aspect = static_cast<float>(dims->width) / static_cast<float>(dims->height);
			glm::mat4 projection = glm::perspective(0.59f, aspect, 1.0f, 2000.0f);

			// Build world transform for all axes. Rotates about uninverted camera's
			// view, then translates to a specified corner on the screen.
			glm::mat4 axesRot = mCamera->getWorldToView();
			axesRot[3][0] = 0.0f;
			axesRot[3][1] = 0.0f;
			axesRot[3][2] = 0.0f;
			glm::mat4 invCamTrans = glm::translate(glm::mat4(1.0f), glm::vec3(0.375f * aspect, 0.37f, -1.5f));
			glm::mat4 axesScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.8f));
			glm::mat4 axesTransform = axesScale * axesRot;

			GLint locCamViewVec = glGetUniformLocation(shader, "uCamViewVec");
			GLint locLightDirWorld = glGetUniformLocation(shader, "uLightDirWorld");

			GLint locAmbientColor = glGetUniformLocation(shader, "uAmbientColor");
			GLint locDiffuseColor = glGetUniformLocation(shader, "uDiffuseColor");
			GLint locSpecularColor = glGetUniformLocation(shader, "uSpecularColor");
			GLint locSpecularPower = glGetUniformLocation(shader, "uSpecularPower");

			GLint locProjIVObject = glGetUniformLocation(shader, "uProjIVObject");
			GLint locObject = glGetUniformLocation(shader, "uObject");

			GL(glUniform3f(locCamViewVec, 0.0f, 0.0f, -1.0f));
			GL(glUniform3f(locLightDirWorld, 0.0f, 0.0f, -1.0f));

			// Build projection for the axes to use on the screen. The arrors will not
			// use the camera, but will use the camera's transformation matrix.

			mArrowAttribs.bind();

			// X Axis
			{
				glm::mat4 xform = glm::rotate(glm::mat4(1.0f), glm::pi<float>() / 2.0f, glm::vec3(0.0, 1.0, 0.0));
				glm::mat4 finalTrafo = axesTransform * xform;

				GL(glUniform4f(locAmbientColor, 0.5f, 0.01f, 0.01f, 1.0f));
				GL(glUniform4f(locDiffuseColor, 1.0f, 0.0f, 0.0f, 1.0f));
				GL(glUniform4f(locSpecularColor, 0.5f, 0.5f, 0.5f, 1.0f));
				GL(glUniform1f(locSpecularPower, 16.0f));

				glm::mat4 worldToProj = projection * invCamTrans * finalTrafo;
				const GLfloat* ptr = glm::value_ptr(worldToProj);
				GL(glUniformMatrix4fv(locProjIVObject, 1, false, ptr));

				glm::mat4 objectSpace = finalTrafo;
				ptr = glm::value_ptr(objectSpace);
				GL(glUniformMatrix4fv(locObject, 1, false, ptr));

				GL(glDrawElements(iboData->primMode, iboData->numPrims, iboData->primType, 0));
			}

			// X Axis (dark)
  {
	  glm::mat4 xform = glm::rotate(glm::mat4(1.0f), -glm::pi<float>() / 2.0f, glm::vec3(0.0, 1.0, 0.0));
	  glm::mat4 finalTrafo = axesTransform * xform;

	  GL(glUniform4f(locAmbientColor, 0.1f, 0.01f, 0.01f, 1.0f));
	  GL(glUniform4f(locDiffuseColor, 0.25f, 0.0f, 0.0f, 1.0f));
	  GL(glUniform4f(locSpecularColor, 0.0f, 0.0f, 0.0f, 1.0f));
	  GL(glUniform1f(locSpecularPower, 16.0f));

	  glm::mat4 worldToProj = projection * invCamTrans * finalTrafo;
	  const GLfloat* ptr = glm::value_ptr(worldToProj);
	  GL(glUniformMatrix4fv(locProjIVObject, 1, false, ptr));

	  glm::mat4 objectSpace = finalTrafo;
	  ptr = glm::value_ptr(objectSpace);
	  GL(glUniformMatrix4fv(locObject, 1, false, ptr));

	  GL(glDrawElements(iboData->primMode, iboData->numPrims, iboData->primType, 0));
  }

			// Y Axis
  {
	  glm::mat4 xform = glm::rotate(glm::mat4(1.0f), -glm::pi<float>() / 2.0f, glm::vec3(1.0, 0.0, 0.0));
	  glm::mat4 finalTrafo = axesTransform * xform;

	  GL(glUniform4f(locAmbientColor, 0.01f, 0.5f, 0.01f, 1.0f));
	  GL(glUniform4f(locDiffuseColor, 0.0f, 1.0f, 0.0f, 1.0f));
	  GL(glUniform4f(locSpecularColor, 0.5f, 0.5f, 0.5f, 1.0f));
	  GL(glUniform1f(locSpecularPower, 16.0f));

	  glm::mat4 worldToProj = projection * invCamTrans * finalTrafo;
	  const GLfloat* ptr = glm::value_ptr(worldToProj);
	  GL(glUniformMatrix4fv(locProjIVObject, 1, false, ptr));

	  glm::mat4 objectSpace = finalTrafo;
	  ptr = glm::value_ptr(objectSpace);
	  GL(glUniformMatrix4fv(locObject, 1, false, ptr));

	  GL(glDrawElements(iboData->primMode, iboData->numPrims, iboData->primType, 0));
  }

			// Y Axis (dark)
  {
	  glm::mat4 xform = glm::rotate(glm::mat4(1.0f), glm::pi<float>() / 2.0f, glm::vec3(1.0, 0.0, 0.0));
	  glm::mat4 finalTrafo = axesTransform * xform;

	  GL(glUniform4f(locAmbientColor, 0.01f, 0.1f, 0.01f, 1.0f));
	  GL(glUniform4f(locDiffuseColor, 0.0f, 0.25f, 0.0f, 1.0f));
	  GL(glUniform4f(locSpecularColor, 0.0f, 0.0f, 0.0f, 1.0f));
	  GL(glUniform1f(locSpecularPower, 16.0f));

	  glm::mat4 worldToProj = projection * invCamTrans * finalTrafo;
	  const GLfloat* ptr = glm::value_ptr(worldToProj);
	  GL(glUniformMatrix4fv(locProjIVObject, 1, false, ptr));

	  glm::mat4 objectSpace = finalTrafo;
	  ptr = glm::value_ptr(objectSpace);
	  GL(glUniformMatrix4fv(locObject, 1, false, ptr));

	  GL(glDrawElements(iboData->primMode, iboData->numPrims, iboData->primType, 0));
  }

			// Z Axis
  {
	  // No rotation at all
	  glm::mat4 finalTrafo = axesTransform;

	  GL(glUniform4f(locAmbientColor, 0.01f, 0.01f, 0.5f, 1.0f));
	  GL(glUniform4f(locDiffuseColor, 0.0f, 0.0f, 1.0f, 1.0f));
	  GL(glUniform4f(locSpecularColor, 0.5f, 0.5f, 0.5f, 1.0f));
	  GL(glUniform1f(locSpecularPower, 16.0f));

	  glm::mat4 worldToProj = projection * invCamTrans * finalTrafo;
	  const GLfloat* ptr = glm::value_ptr(worldToProj);
	  GL(glUniformMatrix4fv(locProjIVObject, 1, false, ptr));

	  glm::mat4 objectSpace = finalTrafo;
	  ptr = glm::value_ptr(objectSpace);
	  GL(glUniformMatrix4fv(locObject, 1, false, ptr));

	  GL(glDrawElements(iboData->primMode, iboData->numPrims, iboData->primType, 0));
  }

			// Z Axis (dark)
  {
	  // No rotation at all
	  glm::mat4 xform = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(1.0, 0.0, 0.0));
	  glm::mat4 finalTrafo = axesTransform * xform;

	  GL(glUniform4f(locAmbientColor, 0.01f, 0.01f, 0.1f, 1.0f));
	  GL(glUniform4f(locDiffuseColor, 0.0f, 0.0f, 0.25f, 1.0f));
	  GL(glUniform4f(locSpecularColor, 0.0f, 0.0f, 0.0f, 1.0f));
	  GL(glUniform1f(locSpecularPower, 16.0f));

	  glm::mat4 worldToProj = projection * invCamTrans * finalTrafo;
	  const GLfloat* ptr = glm::value_ptr(worldToProj);
	  GL(glUniformMatrix4fv(locProjIVObject, 1, false, ptr));

	  glm::mat4 objectSpace = finalTrafo;
	  ptr = glm::value_ptr(objectSpace);
	  GL(glUniformMatrix4fv(locObject, 1, false, ptr));

	  GL(glDrawElements(iboData->primMode, iboData->numPrims, iboData->primType, 0));
  }

			mArrowAttribs.unbind();

      if (!depthMask)
      {
        GL(glDepthMask(GL_FALSE));
      }
      if (cullFace)
      {
        GL(glEnable(GL_CULL_FACE));
      }
      if (blend)
      {
        GL(glEnable(GL_BLEND));
      }
		}

		// Create default colormaps.
		void SRInterface::generateColormaps()
		{
			size_t resolution = 1000;
            float step = 1.f / static_cast<float>(resolution);
            ColorMap cm("Rainbow");
			std::vector<uint8_t> rainbow;
			rainbow.reserve(resolution * 3);
			for (float i = 0.f; i < 1.f; i+=step) {
                ColorRGB col = cm.getColorMapVal(i);
				rainbow.push_back(static_cast<uint8_t>(col.r() * 255.0f));
				rainbow.push_back(static_cast<uint8_t>(col.g() * 255.0f));
				rainbow.push_back(static_cast<uint8_t>(col.b() * 255.0f));
				rainbow.push_back(static_cast<uint8_t>(255.0f));
			}

			// Build rainbow texture (eyetracking version -- will need to change).
			GL(glGenTextures(1, &mRainbowCMap));
			GL(glBindTexture(GL_TEXTURE_1D, mRainbowCMap));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
			GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
			GL(glTexImage1D(GL_TEXTURE_1D, 0,
				GL_RGBA8,
				static_cast<GLsizei>(rainbow.size() / 4), 0,
				GL_RGBA,
				GL_UNSIGNED_BYTE, &rainbow[0]));

            cm = ColorMap("Grayscale");
			// build grayscale texture.
			std::vector<uint8_t> grayscale;
			grayscale.reserve(resolution * 3);
			for (float i = 0.f; i < 1.f; i+=step) {
                ColorRGB col = cm.getColorMapVal(i);
				grayscale.push_back(static_cast<uint8_t>(col.r() * 255.0f));
				grayscale.push_back(static_cast<uint8_t>(col.g() * 255.0f));
				grayscale.push_back(static_cast<uint8_t>(col.b() * 255.0f));
				grayscale.push_back(static_cast<uint8_t>(255.0f));
			}

			// Grayscale texture.
			GL(glGenTextures(1, &mGrayscaleCMap));
			GL(glBindTexture(GL_TEXTURE_1D, mGrayscaleCMap));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
			GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
			GL(glTexImage1D(GL_TEXTURE_1D, 0,
				GL_RGBA8,
				static_cast<GLsizei>(grayscale.size() / 4), 0,
				GL_RGBA,
				GL_UNSIGNED_BYTE, &grayscale[0]));

            cm = ColorMap("Blackbody");
            //blackbody texture
			std::vector<uint8_t> blackbody;
			blackbody.reserve(resolution * 3);
			for (float i = 0.f; i < 1.f; i+=step) {
                ColorRGB col = cm.getColorMapVal(i);
				blackbody.push_back(static_cast<uint8_t>(col.r() * 255.0f));
				blackbody.push_back(static_cast<uint8_t>(col.g() * 255.0f));
				blackbody.push_back(static_cast<uint8_t>(col.b() * 255.0f));
				blackbody.push_back(static_cast<uint8_t>(255.0f));
			}

			// Build rainbow texture (eyetracking version -- will need to change).
			GL(glGenTextures(1, &mBlackBodyCMap));
			GL(glBindTexture(GL_TEXTURE_1D, mBlackBodyCMap));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GL(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
			GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
			GL(glTexImage1D(GL_TEXTURE_1D, 0,
				GL_RGBA8,
				static_cast<GLsizei>(blackbody.size() / 4), 0,
				GL_RGBA,
				GL_UNSIGNED_BYTE, &blackbody[0]));
		}


	} // namespace Render
} // namespace SCIRun 

