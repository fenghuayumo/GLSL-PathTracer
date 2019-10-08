#include "Config.h"
#include "TiledRenderer.h"
#include "Camera.h"
#include "Scene.h"

namespace GLSLPT
{
	TiledRenderer::TiledRenderer(Scene *scene, const std::string& shadersDirectory) : Renderer(scene, shadersDirectory)
		, numTilesX(scene->renderOptions.numTilesX)
		, numTilesY(scene->renderOptions.numTilesY)
		, maxDepth(scene->renderOptions.maxDepth)
	{
	}

    TiledRenderer::~TiledRenderer() 
    {
    }

    void TiledRenderer::init()
    {
		if (initialized)
			return;

		Renderer::init();

		sampleCounter = 1;
		currentBuffer = 0;
		totalTime = 0;

		tileWidth = (int)screenSize.x / numTilesX;
		tileHeight = (int)screenSize.y / numTilesY;
		tileX = 0;
		tileY = numTilesY - 1;

		//Generate buffers for Meshes
		for (int i = 0; i < scene->meshes.size(); i++)
			scene->meshes[i]->generateBuffers();

        //----------------------------------------------------------
        // Shaders
        //----------------------------------------------------------

		//Todo : Avoid code duplication for Tiled and Progressive shaders 
		gBufferShader = loadShaders(shadersDirectory + "gVert.glsl", shadersDirectory + "gFrag.glsl");
		pathTraceShader = loadShaders(shadersDirectory + "PathTraceTiledVert.glsl", shadersDirectory + "PathTraceTiledFrag.glsl");
		outputShader = loadShaders(shadersDirectory + "OutputVert.glsl", shadersDirectory + "OutputFrag.glsl");
		accumShader = loadShaders(shadersDirectory + "AccumVert.glsl", shadersDirectory + "AccumFrag.glsl");
		tileOutputShader = loadShaders(shadersDirectory + "TileOutputVert.glsl", shadersDirectory + "TileOutputFrag.glsl");
		outputShader = loadShaders(shadersDirectory + "OutputVert.glsl", shadersDirectory + "OutputFrag.glsl");

		printf("Debug sizes : %d %d - %d %d\n", tileWidth, tileHeight, screenSize.x, screenSize.y);

        //----------------------------------------------------------
        // FBO Setup
        //----------------------------------------------------------
		//Create FBOs for path trace shader (Tiled)
		printf("Buffer pathTraceFBO\n");
		glGenFramebuffers(1, &pathTraceFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, pathTraceFBO);

		//Create Texture for FBO
		glGenTextures(1, &pathTraceTexture);
		glBindTexture(GL_TEXTURE_2D, pathTraceTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tileWidth, tileHeight, 0, GL_RGBA, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pathTraceTexture, 0);

	    //Create FBOs for gBuffer
		printf("Buffer gBufferFBO\n");
		glGenFramebuffers(1, &gBufferFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

		//Create texture for albedo
		glGenTextures(1, &gBufferTexture);
		glBindTexture(GL_TEXTURE_2D, gBufferTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenSize.x, screenSize.y, 0, GL_RGBA, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gBufferTexture, 0);

		unsigned int rboDepth;
		glGenRenderbuffers(1, &rboDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenSize.x, screenSize.y);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		//Create FBOs for accum buffer
		printf("Buffer accumFBO\n");
		glGenFramebuffers(1, &accumFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);

		//Create Texture for FBO
		glGenTextures(1, &accumTexture);
		glBindTexture(GL_TEXTURE_2D, accumTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, GLsizei(screenSize.x), GLsizei(screenSize.y), 0, GL_RGBA, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTexture, 0);

		//Create FBOs for tile output shader
		printf("Buffer outputFBO\n");
		glGenFramebuffers(1, &outputFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);

		//Create Texture for FBO
		glGenTextures(1, &tileOutputTexture[0]);
		glBindTexture(GL_TEXTURE_2D, tileOutputTexture[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenSize.x, screenSize.y, 0, GL_RGBA, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenTextures(1, &tileOutputTexture[1]);
		glBindTexture(GL_TEXTURE_2D, tileOutputTexture[1]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenSize.x, screenSize.y, 0, GL_RGBA, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileOutputTexture[currentBuffer], 0);

		GLuint shaderObject;

		pathTraceShader->use();
		shaderObject = pathTraceShader->object();

		glUniform1f(glGetUniformLocation(shaderObject, "hdrResolution"), scene->hdrData == nullptr ? 0 : float(scene->hdrData->width * scene->hdrData->height));
		glUniform1i(glGetUniformLocation(shaderObject, "topBVHIndex"), scene->bvhTranslator.topLevelIndexPackedXY);
		glUniform1i(glGetUniformLocation(shaderObject, "vertIndicesSize"), scene->indicesTexWidth);
		glUniform2f(glGetUniformLocation(shaderObject, "screenResolution"), float(screenSize.x), float(screenSize.y));
		glUniform1i(glGetUniformLocation(shaderObject, "numOfLights"), numOfLights);
		glUniform1f(glGetUniformLocation(shaderObject, "invTileWidth"), 1.0f / numTilesX);
		glUniform1f(glGetUniformLocation(shaderObject, "invTileHeight"), 1.0f / numTilesY);
		glUniform1i(glGetUniformLocation(shaderObject, "accumTexture"), 0);
		glUniform1i(glGetUniformLocation(shaderObject, "BVH"), 1);
		glUniform1i(glGetUniformLocation(shaderObject, "BBoxMin"), 2);
		glUniform1i(glGetUniformLocation(shaderObject, "BBoxMax"), 3);
		glUniform1i(glGetUniformLocation(shaderObject, "vertexIndicesTex"), 4);
		glUniform1i(glGetUniformLocation(shaderObject, "verticesTex"), 5);
		glUniform1i(glGetUniformLocation(shaderObject, "normalsTex"), 6);
		glUniform1i(glGetUniformLocation(shaderObject, "materialsTex"), 7);
		glUniform1i(glGetUniformLocation(shaderObject, "transformsTex"), 8);
		glUniform1i(glGetUniformLocation(shaderObject, "lightsTex"), 9);
		glUniform1i(glGetUniformLocation(shaderObject, "textureMapsArrayTex"), 10);
		glUniform1i(glGetUniformLocation(shaderObject, "hdrTex"), 11);
		glUniform1i(glGetUniformLocation(shaderObject, "hdrMarginalDistTex"), 12);
		glUniform1i(glGetUniformLocation(shaderObject, "hdrCondDistTex"), 13);

		pathTraceShader->stopUsing();

		gBufferShader->use();
		shaderObject = gBufferShader->object();
		glUniform1i(glGetUniformLocation(shaderObject, "materialsTex"), 7);
		glUniform1i(glGetUniformLocation(shaderObject, "lightsTex"), 9);
		glUniform1i(glGetUniformLocation(shaderObject, "textureMapsArrayTex"), 10);
		glUniform1i(glGetUniformLocation(shaderObject, "hdrTex"), 11);
		glUniform1i(glGetUniformLocation(shaderObject, "numOfLights"), numOfLights);
		gBufferShader->stopUsing();

    }

    void TiledRenderer::finish()
    {
        if (!initialized)
            return;

		glDeleteTextures(1, &pathTraceTexture);
		glDeleteTextures(1, &gBufferTexture);
		glDeleteTextures(1, &accumTexture);
		glDeleteTextures(1, &tileOutputTexture[0]);
		glDeleteTextures(1, &tileOutputTexture[1]);

		glDeleteFramebuffers(1, &pathTraceFBO);
		glDeleteFramebuffers(1, &gBufferFBO);
		glDeleteFramebuffers(1, &accumFBO);
		glDeleteFramebuffers(1, &outputFBO);

		delete pathTraceShader;
		delete gBufferShader;
		delete accumShader;
		delete tileOutputShader;
		delete outputShader;

		Renderer::finish();
    }

    void TiledRenderer::render()
    {
        if (!initialized)
        {
            printf("Tiled Renderer is not initialized\n");
            return;
        }

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, accumTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, BVHTex);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, BBoxminTex);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, BBoxmaxTex);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, vertexIndicesTex);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, verticesTex);
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, normalsTex);
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, materialsTex);
		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, transformsTex);
		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, lightsTex);
		glActiveTexture(GL_TEXTURE10);
		glBindTexture(GL_TEXTURE_2D_ARRAY, textureMapsArrayTex);
		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, hdrTex);
		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D, hdrMarginalDistTex);
		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D, hdrConditionalDistTex);

		if (!scene->camera->isMoving)
		{
			// Render to low res buffer once to avoid ghosting from previous frame
			if (scene->instancesModified)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
				glClearColor(0.0, 0.0, 0.0, 1.0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glViewport(0, 0, screenSize.x, screenSize.y);

				scene->rasterizeMeshes(gBufferShader);

				glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				glBlitFramebuffer(0, 0, screenSize.x, screenSize.y, 0, 0, screenSize.x, screenSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				scene->instancesModified = false;
			}

			GLuint shaderObject;
			pathTraceShader->use();

			shaderObject = pathTraceShader->object();
			glUniform1i(glGetUniformLocation(shaderObject, "tileX"), tileX);
			glUniform1i(glGetUniformLocation(shaderObject, "tileY"), tileY);
			pathTraceShader->stopUsing();

			glBindFramebuffer(GL_FRAMEBUFFER, pathTraceFBO);
			glViewport(0, 0, tileWidth, tileHeight);
			quad->Draw(pathTraceShader);

			glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);
			glViewport(tileWidth * tileX, tileHeight * tileY, tileWidth, tileHeight);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, pathTraceTexture);
			quad->Draw(accumShader);

			glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileOutputTexture[currentBuffer], 0);
			glViewport(0, 0, screenSize.x, screenSize.y);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, accumTexture);
			quad->Draw(tileOutputShader);

			tileX++;
			if (tileX >= numTilesX)
			{
				tileX = 0;
				tileY--;
				if (tileY < 0)
				{
					tileX = 0;
					tileY = numTilesY - 1;
					sampleCounter++;
					currentBuffer = 1 - currentBuffer;
				}
			}
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
			glClearColor(0.0, 0.0, 0.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, screenSize.x, screenSize.y);

			scene->rasterizeMeshes(gBufferShader);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, 0, screenSize.x, screenSize.y, 0, 0, screenSize.x, screenSize.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
    }

    void TiledRenderer::present() const
    {
        if (!initialized)
            return;

		if (!scene->camera->isMoving)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tileOutputTexture[1 - currentBuffer]);
			quad->Draw(outputShader);
		}
		else
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, gBufferTexture);
			quad->Draw(outputShader);
		}
    }

    float TiledRenderer::getProgress() const
    {
		return float((numTilesY - tileY - 1) * numTilesX + tileX) / float(numTilesX * numTilesY);
    }

    void TiledRenderer::update(float secondsElapsed)
    {
		Renderer::update(secondsElapsed);

		float r1, r2, r3;

		if (scene->camera->isMoving || scene->instancesModified)
		{
			r1 = r2 = r3 = 0;
			tileX = -1;
			tileY = numTilesY - 1;
			sampleCounter = 1;

			glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);
			glViewport(0, 0, screenSize.x, screenSize.y);
			glClear(GL_COLOR_BUFFER_BIT);

			glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileOutputTexture[1 - currentBuffer], 0);
			glViewport(0, 0, screenSize.x, screenSize.y);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, gBufferTexture);
			quad->Draw(accumShader);

			/*glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);
			glViewport(0, 0, screenSize.x, screenSize.y);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, gBufferTexture);
			quad->Draw(accumShader);*/

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		else
		{
			r1 = ((float)rand() / (RAND_MAX));
			r2 = ((float)rand() / (RAND_MAX));
			r3 = ((float)rand() / (RAND_MAX));
		}

		GLuint shaderObject;

		pathTraceShader->use();
		shaderObject = pathTraceShader->object();
		glUniform3fv(glGetUniformLocation(shaderObject, "camera.position"), 1, glm::value_ptr(scene->camera->position));
		glUniform3fv(glGetUniformLocation(shaderObject, "camera.right"), 1, glm::value_ptr(scene->camera->right));
		glUniform3fv(glGetUniformLocation(shaderObject, "camera.up"), 1, glm::value_ptr(scene->camera->up));
		glUniform3fv(glGetUniformLocation(shaderObject, "camera.forward"), 1, glm::value_ptr(scene->camera->forward));
		glUniform1f(glGetUniformLocation(shaderObject, "camera.fov"), scene->camera->fov);
		glUniform1f(glGetUniformLocation(shaderObject, "camera.focalDist"), scene->camera->focalDist);
		glUniform1f(glGetUniformLocation(shaderObject, "camera.aperture"), scene->camera->aperture);
		glUniform3fv(glGetUniformLocation(shaderObject, "randomVector"), 1, glm::value_ptr(glm::vec3(r1, r2, r3)));
		glUniform1i(glGetUniformLocation(shaderObject, "useEnvMap"), scene->hdrData == nullptr ? false : scene->renderOptions.useEnvMap);
		glUniform1f(glGetUniformLocation(shaderObject, "hdrMultiplier"), scene->renderOptions.hdrMultiplier);
		glUniform1i(glGetUniformLocation(shaderObject, "maxDepth"), maxDepth);
		glUniform1i(glGetUniformLocation(shaderObject, "tileX"), tileX);
		glUniform1i(glGetUniformLocation(shaderObject, "tileY"), tileY);
		pathTraceShader->stopUsing();

		gBufferShader->use();
		shaderObject = gBufferShader->object();
		glUniform3fv(glGetUniformLocation(shaderObject, "cameraPos"), 1, glm::value_ptr(scene->camera->position));
		glUniform1f(glGetUniformLocation(shaderObject, "hdrMultiplier"), scene->renderOptions.hdrMultiplier);
		glUniform3fv(glGetUniformLocation(shaderObject, "randomVector"), 1, glm::value_ptr(glm::vec3(r1, r2, r3)));
		glUniform1i(glGetUniformLocation(shaderObject, "useEnvMap"), scene->hdrData == nullptr ? false : scene->renderOptions.useEnvMap);
		gBufferShader->stopUsing();

		outputShader->use();
		shaderObject = outputShader->object();
		glUniform1f(glGetUniformLocation(shaderObject, "invSampleCounter"), scene->camera->isMoving ? 1.0f : 1.0f / sampleCounter);
		outputShader->stopUsing();
    }
}