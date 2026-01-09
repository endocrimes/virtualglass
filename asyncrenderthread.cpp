
#include "asyncrenderthread.h"
#include "asyncrenderinternal.h"
#include "asyncrenderwidget.h"
#include "geometry.h"
#include "peelrenderer.h"
#include "globaldepthpeelingsetting.h"
#include "globalbackgroundcolor.h"
#include "constants.h"
#include "glassopengl.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QSurfaceFormat>

#define glewGetContext() glewContext

using std::vector;

using namespace AsyncRenderInternal;

RenderThread::RenderThread(Controller *_controller) : controller(_controller), surface(NULL), context(NULL)
{
	// Create offscreen surface in main thread (required by Qt)
	surface = new QOffscreenSurface();
	surface->setFormat(QSurfaceFormat::defaultFormat());
	surface->create();

	// Note: context will be created in run() because Qt 6 requires
	// QOpenGLContext to be created in the thread where it will be used
}

RenderThread::~RenderThread()
{
	// Note: context is deleted in run() after thread completes
	// Surface can be deleted here since it was created in the main thread
	delete surface;
	surface = NULL;
}

void RenderThread::run()
{
	// Create context in worker thread (Qt 6 requirement)
	context = new QOpenGLContext();
	context->setFormat(surface->format());
	if (!context->create()) {
		std::cerr << "ERROR: Failed to create OpenGL context in render thread" << std::endl;
		delete context;
		context = NULL;
		return;
	}

	if (!context->makeCurrent(surface)) {
		std::cerr << "ERROR: Failed to make OpenGL context current in render thread" << std::endl;
		delete context;
		context = NULL;
		return;
	}

	//-----------------------------------------------
	//Init glew for this thread (needed for peeling):
	GLEWContext *glewContext = new GLEWContext;
	GLenum err = glewInit();
	PeelRenderer *peelRenderer = NULL;
	if (err != GLEW_OK) 
	{
		std::cerr << "WARNING: Failure initializing glew: " << glewGetErrorString(err) << std::endl;
		std::cerr << " ... we will continue, but code that uses extensions will cause a crash" << std::endl;
	} 
	else 
	{
		try 
		{
			peelRenderer = new PeelRenderer(glewContext);
		} 
		catch (...) 
		{
			std::cerr << "Caught exception constructing peelRenderer, will fall back to regular rendering." << std::endl;
			peelRenderer = NULL;
		}
	}

	GlassOpenGL::initialize();
	GlassOpenGL::errors("RenderThread::run()");

	controller->renderQueueLock.lock();
	while (!controller->quitThreads) 
	{
		if (controller->renderQueue.empty()) 
		{
			controller->renderQueueHasData.wait(&controller->renderQueueLock);
			continue;
		}

		//pull job off the queue:
		Job *job = controller->renderQueue.front();
		controller->renderQueue.pop_front();

		controller->renderQueueLock.unlock();

		//shouldn't change if it's a per-thread context, which I've been lead to suspect is true.
		assert(QOpenGLContext::currentContext() == context);

		QOpenGLFramebufferObject fb(job->camera.size.x, job->camera.size.y, QOpenGLFramebufferObject::Depth);
		fb.bind();
		glPushAttrib(GL_VIEWPORT_BIT);
		glViewport(0, 0, job->camera.size.x, job->camera.size.y);

		setupCamera(job->camera);
		if (peelRenderer && GlobalDepthPeelingSetting::enabled()) 
			peelRenderer->render(*job->geometry);
		else 
			GlassOpenGL::renderWithoutDepthPeeling(*job->geometry);

		glPopAttrib();
		fb.release();

		assert(!job->result);

		job->result = new QImage(fb.toImage());

		//pass job back to main thread:
		emit jobFinished(job);

		controller->renderQueueLock.lock();
	}
	controller->renderQueueLock.unlock();

	delete peelRenderer;
	peelRenderer = NULL;

	delete glewContext;
	glewContext = NULL;

	context->doneCurrent();

	// Delete context in the same thread where it was created
	delete context;
	context = NULL;
}

void RenderThread::setupCamera(Camera const &camera) 
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float w = camera.size.x;
	float h = camera.size.y;

	if (camera.isPerspective) 
	{
		gluPerspective(45.0, w / h, 0.1, 100.0);
	} 
	else 
	{
		float s = 2.2f / length(camera.eye - camera.lookAt);
		glScalef(h / w * s, s, -0.01f);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(camera.eye.x, camera.eye.y, camera.eye.z,
		camera.lookAt.x, camera.lookAt.y, camera.lookAt.z,
		camera.up.x, camera.up.y, camera.up.z);

	GlassOpenGL::errors("RenderThread::setupCamera");
}

