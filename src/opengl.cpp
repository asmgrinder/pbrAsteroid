/*
 * Physically Based Rendering
 * Forked from Michał Siejak PBR project
 *
 * OpenGL 4.5 renderer.
 */

#include <stdexcept>
#include <memory>

#include "opengl.hpp"

#include <GLFW/glfw3.h>


namespace OpenGL
{


GLFWwindow* Renderer::initialize(int width, int height, int maxSamples)
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	glfwWindowHint(GLFW_SAMPLES, 0);

	GLFWwindow* window = glfwCreateWindow(width, height, "PBR asteroid (OpenGL 4.5)", nullptr, nullptr);
	if (!window)
	{
		throw std::runtime_error("Failed to create OpenGL context");
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(-1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		throw std::runtime_error("Failed to initialize OpenGL extensions loader");
	}

#ifdef _DEBUG
	glDebugMessageCallback(Renderer::logMessage, nullptr);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

	glViewport(0, 0, width, height);

	GLint maxSupportedSamples;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSupportedSamples);

	const int samples = glm::min(maxSamples, maxSupportedSamples);
	mFramebuffer = std::make_shared<Framebuffer>();
	{
		mFramebuffer->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, GL_RGBA16F, width, height, samples);
		mFramebuffer->AttachRenderbuffer(GL_COLOR_ATTACHMENT1, GL_RGBA16F, width, height, samples);
		mFramebuffer->AttachRenderbuffer(GL_COLOR_ATTACHMENT2, GL_R16F,    width, height, samples);
		mFramebuffer->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT, width, height, samples);
		mFramebuffer->SetDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });
		auto status = mFramebuffer->CheckStatus();
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			throw std::runtime_error("Framebuffer is not complete: " + std::to_string(status));
		}
	}

	mResolveFramebuffer = std::make_shared<Framebuffer>();
	{
		mResolveFramebuffer->AttachTexture(GL_COLOR_ATTACHMENT0, GL_RGBA16F, width, height);
		mResolveFramebuffer->AttachTexture(GL_COLOR_ATTACHMENT1, GL_RGBA16F, width, height);
		mResolveFramebuffer->AttachTexture(GL_COLOR_ATTACHMENT2, GL_R16F,    width, height);
		mResolveFramebuffer->SetDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });
		auto status = mResolveFramebuffer->CheckStatus();
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			throw std::runtime_error("Framebuffer is not complete: " + std::to_string(status));
		}
	}

	mCameraPtr = std::make_shared<Camera>(glm::radians(60.0f), glm::vec2{ width, height }, 0.25f, 5000.0f);
	mCameraPtr->SetPosition(glm::vec3{ 0, 0, 1000 });

	std::cout << "OpenGL 4.5 renderer [" << glGetString(GL_RENDERER) << "]" << std::endl;
	return window;
}

void Renderer::shutdown()
{
	mCameraPtr = nullptr;

	mResolveFramebuffer->Release();
	mFramebuffer->Release();

	mEmptyVao.Release();

	mSkyboxUB.Release();

	mSkybox.Release();
	mPbrAsteroid.Release();
	mFullScreenQuad.Release();

	mTonemapProgram.Release();
	mSkyboxProgram.Release();

	mEnvPtr->Release();
}

std::function<void (int w, int h)> Renderer::setup()
{
	// Set global OpenGL state.
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glFrontFace(GL_CCW);

	// Create empty VAO for rendering full screen triangle
	mEmptyVao = MeshGeometry(nullptr, true);

	// Create uniform buffers.
	mSkyboxUB.Create();

	// Load assets & compile/link rendering programs
	mTonemapProgram =
		ShaderProgram{{ std::make_tuple(GL_VERTEX_SHADER,   Shader::GetFileContents("data/shaders/tonemap_vs.glsl")),
						std::make_tuple(GL_FRAGMENT_SHADER, Shader::GetFileContents("data/shaders/tonemap_fs.glsl")) }};

	mSkyboxProgram =
		ShaderProgram{{ std::make_tuple(GL_VERTEX_SHADER,   Shader::GetFileContents("data/shaders/skybox_vs.glsl")),
						std::make_tuple(GL_FRAGMENT_SHADER, Shader::GetFileContents("data/shaders/skybox_fs.glsl")) }};

	// cube2sphere skybox_front.png skybox_back.png skybox_left.png skybox_right.png skybox_top.png skybox_bottom.png -r 4096 2048 -fHDR -oskybox_equirectangular
	mEnvPtr = std::make_shared<Environment>(Image::fromFile("data/textures/skybox.hdr", 3));

	mSkybox = MeshGeometry{ Mesh::fromFile("data/meshes/skybox.obj") };
	mPbrAsteroid = PbrAsteroid{ Mesh::fromFile("data/meshes/asteroid7.fbx"), mEnvPtr };

	return [&](int w, int h) { glViewport(0, 0, w, h); };
}

void Renderer::renderScene(bool OpaquePass)
{
	mPbrAsteroid.Render(OpaquePass);
}

void Renderer::render(GLFWwindow* window, const ViewSettings& view, const SceneSettings& scene)
{
	// process rotation
	float roll = 0;
	if (view.m_keyMapping.count(GLFW_KEY_Z) > 0)
	{
		roll += 1;
	}
	if (view.m_keyMapping.count(GLFW_KEY_C) > 0)
	{
		roll -= 1;
	}
	mCameraPtr->Rotate(roll * glm::radians(2.0f),
						view.m_diffCursorY * glm::radians(0.25f),
						view.m_diffCursorX * glm::radians(0.25f));
	// process movement
	const std::vector<std::pair<int, glm::vec3>> movements =
	{
		{ GLFW_KEY_W, Camera::Forward },
		{ GLFW_KEY_S, Camera::Backward },
		{ GLFW_KEY_A, Camera::Left },
		{ GLFW_KEY_D, Camera::Right },
		{ GLFW_KEY_Q, Camera::Down },
		{ GLFW_KEY_E, Camera::Up }
	};
	glm::vec3 move { 0, 0, 0 };
	for (auto &movement : movements)
	{
		if (view.m_keyMapping.count(movement.first) > 0)
		{
			move += movement.second;
		}
	}
	float speedMult = ((view.m_keyMapping.count(GLFW_KEY_LEFT_SHIFT) > 0) ? 5.0f : 0.5f);
	mCameraPtr->Move(move * speedMult);

	int fbWidth, fbHeight;
	glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

	mFramebuffer->ResizeAll(fbWidth, fbHeight);
	mResolveFramebuffer->ResizeAll(fbWidth, fbHeight);

	const auto &colorRb = mFramebuffer->GetRenderTarget(GL_COLOR_ATTACHMENT0);
	assert(colorRb->GetWidth() == fbWidth);
	assert(colorRb->GetHeight() == fbHeight);

	const glm::mat4 projectionMatrix = mCameraPtr->GetProjection();
	const glm::mat4 viewMatrix = mCameraPtr->GetView();
	const glm::mat4 viewRotationMatrix = glm::mat4(glm::mat3(viewMatrix));

	// Prepare framebuffer for rendering
	mFramebuffer->Bind();
	// opaque pass
	glDepthMask(GL_TRUE);								// enable write to depth buffer to clear it
	glClearColor(0.f, 0.f, 0.f, 0.f);					// zero color buffer, necessary only for transparency targets,
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	//  since every pixel will be overwritten by skybox

	// Draw skybox
	glDisable(GL_BLEND);								// disable blending
	glDepthMask(GL_FALSE);								// disable write to depth buffer for skybox
	glDisable(GL_DEPTH_TEST);

	// Update skybox uniform buffer
	{
		auto &skyboxUniforms = mSkyboxUB.GetReference();
		skyboxUniforms.skyViewProjectionMatrix = projectionMatrix * viewRotationMatrix;
		mSkyboxUB.Bind(0);
	}

	mSkyboxProgram.Use();
	mEnvPtr->BindTextureUnit(0);
	mSkybox.Render();

	glDepthMask(GL_TRUE);					// enable write to depth buffer to clear it
	glEnable(GL_DEPTH_TEST);

	// Draw scene
	std::array<SceneSettings::Light, SceneSettings::NumLights> lightsArr;
	for (int i = 0; i < lightsArr.size(); i++)
	{
		lightsArr[i] = scene.lights[i];
	}
	glm::mat4 pbrModelMat =
								glm::scale(glm::mat4{ 1.0f }, 2.5f * glm::vec3{ 1.0f, 1.0f, 1.0f }) *
								glm::eulerAngleXY(glm::radians(scene.pitch), glm::radians(scene.yaw));
	const glm::vec4 viewport = glm::vec4{ 0, 0, fbWidth, fbHeight };
	mPbrAsteroid.SetShadingUniforms(lightsArr, viewport, projectionMatrix, viewMatrix, pbrModelMat);

	renderScene(true);

	// transparency pass (Order Independent Transparency (OIT), see https://developer.download.nvidia.com/SDK/10/opengl/src/dual_depth_peeling/doc/DualDepthPeeling.pdf)
	glDepthMask(GL_FALSE);					// do not write new data to depth buffer
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);						// enable blending so that transparent geometry sum up
	glBlendFunc(GL_ONE, GL_ONE);			// weights for data in FB and new data

	// Draw scene
	renderScene(false);	//, view, scene

	mFramebuffer->Unbind();

	glDisable(GL_BLEND);					// disable blending

	// Resolve multisample framebuffer (copy renderbuffers to textures)
	mFramebuffer->Resolve(*mResolveFramebuffer,
		{
			{ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0, GL_COLOR_BUFFER_BIT, GL_NEAREST },
			{ GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT1, GL_COLOR_BUFFER_BIT, GL_NEAREST },
			{ GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT2, GL_COLOR_BUFFER_BIT, GL_NEAREST },
		});
	mFramebuffer->InvalidateAttachments({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });

	// Draw a full screen triangle for postprocessing/tone mapping and transparency processing
	mTonemapProgram.Use();
	try
	{
		const auto attachment0 = mResolveFramebuffer->GetRenderTarget(GL_COLOR_ATTACHMENT0);
		const auto attachment1 = mResolveFramebuffer->GetRenderTarget(GL_COLOR_ATTACHMENT1);
		const auto attachment2 = mResolveFramebuffer->GetRenderTarget(GL_COLOR_ATTACHMENT2);
		std::dynamic_pointer_cast<const Texture>(attachment0)->BindTextureUnit(0);
		std::dynamic_pointer_cast<const Texture>(attachment1)->BindTextureUnit(1);
		std::dynamic_pointer_cast<const Texture>(attachment2)->BindTextureUnit(2);
		mEmptyVao.Render();
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << std::endl;
	}

	glfwSwapBuffers(window);
}

#ifdef _DEBUG
void OpenGL::Renderer::logMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	const std::unordered_map<GLenum, std::string> errorType =
	{
		{ GL_DEBUG_TYPE_ERROR, "** GL ERROR **" },
		{ GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "** DEPRECATED BEHAVIOUR **" },
		{ GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "** UNDEFINED BEHAVIOUR **" },
		{ GL_DEBUG_TYPE_PORTABILITY, "** PORTABILITY **" },
		{ GL_DEBUG_TYPE_PERFORMANCE, "** PERFORMANCE **" },
		{ GL_DEBUG_TYPE_OTHER, "** OTHER **" },
	};
	const std::unordered_map<GLenum, std::string> errorSeverity =
	{
		{ GL_DEBUG_SEVERITY_HIGH, "high severity"},
		{ GL_DEBUG_SEVERITY_MEDIUM, "medium severity"},
		{ GL_DEBUG_SEVERITY_LOW, "low severity"},
		{ GL_DEBUG_SEVERITY_NOTIFICATION, "notification"},
	};
	std::cerr << "GL CALLBACK: " << errorType.at(type) << " (" << errorSeverity.at(severity) << "), message = " << message << std::endl;
}
#endif

} // OpenGL

