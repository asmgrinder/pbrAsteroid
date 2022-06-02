/*
 * Physically Based Rendering
 * Forked from Michał Siejak PBR project
 *
 * OpenGL 4.5 renderer.
 */

#pragma once

#include <glad/glad.h>

#include "common/image.hpp"
#include "common/utils.hpp"
#include "common/renderer.hpp"
#include "common/mesh.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include <unordered_map>

namespace OpenGL {

class Camera
{
public:
	static constexpr glm::vec3 Forward { 0, 0, -1 };
	static constexpr glm::vec3 Backward { 0, 0, 1 };
	static constexpr glm::vec3 Up { 0, 1, 0 };
	static constexpr glm::vec3 Down { 0, -1, 0 };
	static constexpr glm::vec3 Left { -1, 0, 0 };
	static constexpr glm::vec3 Right { 1, 0, 0 };

	Camera(float FieldOfView, glm::vec2 ViewportSize, float NearClip, float FarClip)
		: mPosition(0, 0, 0)
		, mRotation(glm::vec3(0.f, 0.f, 0.f))
	{
		mProjection = glm::perspectiveFov(FieldOfView, ViewportSize.x, ViewportSize.y, NearClip, FarClip);
	}

	glm::mat4 GetProjection() { return mProjection; }

	glm::mat4 GetView()
	{
		return glm::lookAt(mPosition, mPosition + Forward * mRotation, Up * mRotation);//glm::translate(glm::toMat4(mRotation), mPosition);//
	}

	void Move(glm::vec3 Direction)
	{
		mPosition += Direction * mRotation;
	}

	void Rotate(float Roll, float Pitch, float Yaw)
	{
		// крен (roll), тангаж (pitch), рысканье (Yaw)
		mRotation *= glm::angleAxis(Yaw, Up * mRotation)
					* glm::angleAxis(Pitch, Right * mRotation)
					* glm::angleAxis(Roll, Forward * mRotation);
	}

	void SetPosition(glm::vec3 Position)
	{
		mPosition = Position;
	}

protected:
	glm::vec3 mPosition;
	glm::quat mRotation;
	glm::mat4 mProjection;
};

class NonCopyable
{
public:
	NonCopyable() {}
	virtual ~NonCopyable() { Release(); }

	virtual void Release() {}

protected:
	// forbid copy constructor and copy by assign operator
	NonCopyable(const NonCopyable &) = delete;
	void operator = (const NonCopyable &) = delete;
};

class Shader : public NonCopyable
{
public:
	Shader(GLenum ShaderType, const std::string ShaderSourceStr)
	{
		// compile shader from file
		mShader = glCreateShader(ShaderType);
		const GLchar *const p = &ShaderSourceStr[0];
		glShaderSource(mShader, 1, &p, NULL);
		glCompileShader(mShader);

		int success;
		glGetShaderiv(mShader, GL_COMPILE_STATUS, &success);

		if (!success)
		{
			std::vector<char> infoLog(512);
			glGetShaderInfoLog(mShader, infoLog.size() - 1, NULL, &infoLog[0]);
			const std::unordered_map<GLenum, std::string> typeNameStr =
			{
				{ GL_VERTEX_SHADER, "vertex shader" },
				{ GL_FRAGMENT_SHADER, "fragment shader" },
				{ GL_GEOMETRY_SHADER, "geometry shader" },
				{ GL_COMPUTE_SHADER, "compute shader" },
				{ GL_TESS_CONTROL_SHADER, "tesselation control shader" },
				{ GL_TESS_EVALUATION_SHADER, "tesselation evaluation shader" }
			};

			std::string tn = "unknown shader";
			try
			{
				tn = typeNameStr.at(ShaderType);
			}
			catch (const std::exception &e)
			{
				std::cout << e.what() << std::endl;
			}
			std::cout << "ERROR:shader compilation failed: " << std::endl << tn << std::endl << &infoLog[0] << std::endl;
		}
	}

	Shader(Shader &&Other)
		: mShader(std::move(Other.mShader))
	{
		Other.mShader = 0;
	}

	Shader &operator = (Shader &&Other)
	{
		if (&Other != this)
		{
			Release();

			std::swap(mShader, Other.mShader);
		}

		return *this;
	}

	bool IsUsable() const { return 0 != mShader; }

	void AttachTo(GLuint Program) const
	{
		glAttachShader(Program, mShader);
	}

	static std::string GetFileContents(std::string PathStr)
	{
		const std::string includeStr = "#include";
		std::string fileContents = getFileContents(PathStr);
		std::size_t found = 0;
		while (std::string::npos != (found = fileContents.find(includeStr, found)))
		{
			bool commented1 = found >= 2 && fileContents.substr(found - 2, 2) == "//";
			bool commented2 = found >= 3 && fileContents.substr(found - 3, 2) == "//";
			if (!commented1 && !commented2)
			{
				std::size_t fnIndex1 = fileContents.find_first_not_of(" \t", found + includeStr.length());
				const char ch = fileContents[fnIndex1];
				if (ch != '\"' && ch != '\'')
				{
					std::cout << "ERROR:: expecting file name after #include" << std::endl;
					break;
				}
				std::size_t fnIndex2 = fileContents.find_first_of("'\"", fnIndex1 + 1);
				std::string filename = fileContents.substr(fnIndex1 + 1, fnIndex2 - fnIndex1 - 1);
				std::size_t fnIndex = PathStr.find_last_of("/\\");
				if (std::string::npos != fnIndex)
				{
					filename = PathStr.substr(0, fnIndex + 1) + filename;
				}

				std::string includeStr = getFileContents(filename);
				fileContents = fileContents.substr(0, found)
								+ includeStr
								+ fileContents.substr(fnIndex2 + 1);
				found += includeStr.length();
			}
			else
			{
				found ++;
			}
		}
		return fileContents;
	}

	void Release() override
	{
		glDeleteShader(mShader);
		mShader = 0;
	}

protected:
	static std::string getFileContents(const std::string PathStr)
	{
		std::ifstream file;
		file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try
		{
			file.open(PathStr);
			std::stringstream stream;
			stream << file.rdbuf();
			file.close();
			return stream.str();
		}
		catch (std::ifstream::failure &e)
		{
			std::cout << "ERROR: Shader::GetFileContents: read failed (" << PathStr << ": " << e.what() << ")" << std::endl;
		}
		return "";
	}

	GLuint mShader;
};

class ShaderProgram : public NonCopyable
{
public:
	using List = std::initializer_list<std::tuple<GLenum, std::string>>;

	ShaderProgram()
		: mProgram(0)
	{
	}

	ShaderProgram(ShaderProgram &&Other)
		: mProgram(Other.mProgram)
	{
		Other.mProgram = 0;
	}

	ShaderProgram &operator = (ShaderProgram &&Other)
	{
		if (&Other != this)
		{
			Release();

			std::swap(mProgram, Other.mProgram);
		}

		return *this;
	}

	ShaderProgram(const List &ShaderList)
	{
		mProgram = glCreateProgram();
		std::vector<Shader> shaderList;
		for (const auto &shader : ShaderList)
		{
			shaderList.emplace_back(std::get<0>(shader), std::get<1>(shader));
		}
		for (const auto &shader : shaderList)
		{
			shader.AttachTo(mProgram);
		}

		glLinkProgram(mProgram);

		int success;
		glGetProgramiv(mProgram, GL_LINK_STATUS, &success);
		if (!success)
		{
			std::vector<char> infoLog(512);
			glGetProgramInfoLog(mProgram, infoLog.size(), NULL, &infoLog[0]);
			std::cout << "ERROR: shader program compilation failed" << std::endl << &infoLog[0] << std::endl;
		}
	}

	bool IsUsable() const { return 0 != mProgram; }

	void Use() const
	{
		if (IsUsable())
		{
			glUseProgram(mProgram);
		}
	}

	static void DispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
	{
		glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
	}

	void Release() override
	{
		glDeleteProgram(mProgram);
		mProgram = 0;
	}

	void SetFloat(GLint location, GLfloat v0)
	{
		glProgramUniform1f(mProgram, location, v0);
	}

	void SetVector(GLint location, glm::vec2 v0)
	{
		glProgramUniform2f(mProgram, location, v0.x, v0.y);
	}

	void SetVector(GLint location, glm::vec3 v0)
	{
		glProgramUniform3f(mProgram, location, v0.x, v0.y, v0.z);
	}

	void SetVector(GLint location, glm::vec4 v0)
	{
		glProgramUniform4f(mProgram, location, v0.x, v0.y, v0.z, v0.w);
	}

protected:
	GLuint mProgram;
};

class RenderTarget : public NonCopyable
{
public:
	RenderTarget() : NonCopyable(), mId(0), mWidth(0), mHeight(0) {}

	~RenderTarget() override { Release(); }

	RenderTarget(RenderTarget &&Other)
		: mId(Other.mId), mWidth(Other.mWidth), mHeight(Other.mHeight)
	{
		Other.mId = 0;
		Other.mWidth = Other.mHeight = 0;
	}

	RenderTarget &operator = (RenderTarget &&Other)
	{
		if (&Other != this)
		{
			Release();

			std::swap(mId, Other.mId);
			std::swap(mWidth, Other.mWidth);
			std::swap(mHeight, Other.mHeight);
		}
		return *this;
	}

	void Release() override
	{
		mId = 0;
		mWidth = 0;
		mHeight = 0;
	}

	virtual bool IsUsable() { return 0 != mId; }
	virtual GLint GetWidth() const { return mWidth; }
	virtual GLint GetHeight() const { return mHeight; }
	virtual void AttachTo(GLuint /*Fb*/, GLenum /*Attachment*/) const = 0;

protected:
	GLuint mId;
	GLint mWidth, mHeight;
};

class Texture : public RenderTarget
{
public:
	Texture()
		: RenderTarget()
	{
		mLevels = 0;
	}

	~Texture() override { Release(); }

	Texture(Texture &&Other)
		: RenderTarget(std::move(Other)), mLevels(Other.mLevels)
	{
		Other.mLevels = 0;
	}

	Texture &operator = (Texture &&Other)
	{
		if (&Other != this)
		{
			Release();

			RenderTarget::operator = (std::move(Other));
			std::swap(mLevels, Other.mLevels);
		}
		return *this;
	}

	Texture(GLenum Target)
		: RenderTarget()
	{
		mLevels = 0;
		glCreateTextures(Target, 1, &mId);
	}

	Texture(GLenum Target, int Width, int Height, GLenum InternalFormat, int Levels = 0)
	{
		createTexture(Target, Width, Height, InternalFormat, Levels);
	}

	Texture(const std::shared_ptr<class Image>& Img, GLenum Format, GLenum InternalFormat, int Levels = 0)
	{
		createTexture(GL_TEXTURE_2D, Img->width(), Img->height(), InternalFormat, Levels);
		glTextureSubImage2D(mId, 0, 0, 0, mWidth, mHeight, Format,
							Img->isHDR() ? GL_FLOAT : GL_UNSIGNED_BYTE,
							Img->pixels<void>());
		if (mLevels > 1)
		{
			GenerateMipmap();
		}
	}

	Texture(GLenum Target, GLint Width, GLint Height, GLenum Format, GLenum InternalFormat,
			int Levels = 0, GLenum Type = GL_UNSIGNED_BYTE, const void *DataPtr = nullptr)
	{
		createTexture(Target, Width, Height, InternalFormat, Levels);
		glTextureSubImage2D(mId, 0, 0, 0, mWidth, mHeight, Format, Type, DataPtr);
		if (mLevels > 1)
		{
			GenerateMipmap();
		}
	}

	GLint GetLevels() const { return mLevels; }

	void AttachTo(GLuint Fb, GLenum Attachment) const override
	{
		glNamedFramebufferTexture(Fb, Attachment, mId, 0);
	}

	void Storage(GLenum InternalFormat, GLint Width, GLint Height, GLint Levels)
	{
		if (0 == mWidth && 0 == mHeight)
		{
			glTextureStorage2D(mId, Levels, InternalFormat, Width, Height);
			mWidth = Width;
			mHeight = Height;
			mLevels = Levels;
		}
	}

	void BindTextureUnit(GLuint unit) const
	{
		glBindTextureUnit(unit, mId);
	}

	void BindImageTexture(GLuint Unit, GLint Level, GLboolean Layered, GLint Layer, GLenum Access, GLenum Format) const
	{
		glBindImageTexture(Unit, mId, Level, Layered, Layer, Access, Format);
	}

	void GenerateMipmap() const
	{
		glGenerateTextureMipmap(mId);
	}

	void CopyImageSubData(GLenum SrcTarget, GLint SrcLevel, GLint SrcX, GLint SrcY, GLint SrcZ,
		const Texture &DstTex, GLenum DstTarget, GLint DstLevel, GLint DstX, GLint DstY, GLint DstZ,
		GLsizei SrcDepth) const
	{
		glCopyImageSubData(mId, SrcTarget, SrcLevel, SrcX, SrcY, SrcZ, DstTex.mId, DstTarget, DstLevel, DstX, DstY, DstZ, mWidth, mHeight, SrcDepth);
	}

	void SetWrap(GLint WrapS, GLint WrapT) const
	{
		glTextureParameteri(mId, GL_TEXTURE_WRAP_S, WrapS);
		glTextureParameteri(mId, GL_TEXTURE_WRAP_T, WrapT);
	}

	void Release() override
	{
		if (0 != mId)
		{
			glDeleteTextures(1, &mId);
			mId = 0;
			RenderTarget::Release();
		}
		mLevels = 0;
	}

protected:
	void createTexture(GLenum Target, int Width, int Height, GLenum InternalFormat, int Levels = 0)
	{
		mWidth  = Width;
		mHeight = Height;
		mLevels = (Levels > 0) ? Levels : int(1.f + log(std::max(Width, Height)) / log(2.f));//Utility::numMipmapLevels(Width, Height);

		glCreateTextures(Target, 1, &mId);
		glTextureStorage2D(mId, mLevels, InternalFormat, mWidth, mHeight);
		glTextureParameteri(mId, GL_TEXTURE_MIN_FILTER, (mLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTextureParameteri(mId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		static float maxAnisotropy = -1;
		if (maxAnisotropy < 0)
		{
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
		}
		glTextureParameterf(mId, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
	}

	GLint mLevels;
};

class Environment : public Texture
{
protected:
	static constexpr int kEnvMapSize = 1024;
	static constexpr int kIrradianceMapSize = 32;
	static constexpr int kBRDF_LUT_Size = 256;

public:
	Environment()
		: Texture() {}

	~Environment() override { Release(); }

	Environment(Environment &&Other)
		: Texture(std::move(Other))
			, mIrmap(std::move(Other.mIrmap))
			, mSpBrdfLut(std::move(Other.mSpBrdfLut))
	{
	}

	Environment &operator = (Environment &&Other)
	{
		if (&Other != this)
		{
			Release();

			Texture::operator = (std::move(Other));
			mIrmap = std::move(Other.mIrmap);
			mSpBrdfLut = std::move(Other.mSpBrdfLut);
		}
		return *this;
	}

	Environment(const std::shared_ptr<class Image>& Img)
		: Texture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F)
	{	//------------------------------------------------------------------------------------------------------------------
		Texture envTextureEquirect{ Img, GL_RGB, GL_RGB16F, 1 };
		Texture envTextureUnfiltered{ GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F };
		ShaderProgram equirectToCubeProgram =
			ShaderProgram{{ std::make_tuple(GL_COMPUTE_SHADER, Shader::GetFileContents("data/shaders/equirect2cube_cs.glsl")) }};

		equirectToCubeProgram.Use();
		envTextureEquirect.BindTextureUnit(0);
		envTextureUnfiltered.BindImageTexture(0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		ShaderProgram::DispatchCompute(envTextureUnfiltered.GetWidth() / 32, envTextureUnfiltered.GetHeight() / 32, 6);

		envTextureEquirect.Release();
		equirectToCubeProgram.Release();
		envTextureUnfiltered.GenerateMipmap();
		//-------------------------------------------------------------------------------------------------------------------
		ShaderProgram spmapProgram =
			ShaderProgram{{ std::make_tuple(GL_COMPUTE_SHADER, Shader::GetFileContents("data/shaders/spmap_cs.glsl")) }};

		// Copy 0th mipmap level into destination environment map
		envTextureUnfiltered.CopyImageSubData(GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0, /*m_envTexture*/
											  *this, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0, 6);

		spmapProgram.Use();
		envTextureUnfiltered.BindTextureUnit(0);

		// Pre-filter rest of the mip chain.
		const float deltaRoughness = 1.0f / glm::max(float(/*m_envTexture.*/this->GetLevels() - 1), 1.0f);
		for (int level = 1, size = kEnvMapSize / 2; level <= /*m_envTexture.*/this->GetLevels(); ++level, size /= 2)
		{
			const GLuint numGroups = glm::max(1, size / 32);
			/*m_envTexture.*/this->BindImageTexture(0, level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			spmapProgram.SetFloat(0, level * deltaRoughness);
			ShaderProgram::DispatchCompute(numGroups, numGroups, 6);
		}
		spmapProgram.Release();
		envTextureUnfiltered.Release();
		//-------------------------------------------------------------------------------------------------------------------
		mIrmap = Texture{ GL_TEXTURE_CUBE_MAP, kIrradianceMapSize, kIrradianceMapSize, GL_RGBA16F, 1 };

		ShaderProgram irmapProgram =
			ShaderProgram{{ std::make_tuple(GL_COMPUTE_SHADER, Shader::GetFileContents("data/shaders/irmap_cs.glsl")) }};

		irmapProgram.Use();
		/*m_envTexture.*/BindTextureUnit(0);
		mIrmap.BindImageTexture(0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		ShaderProgram::DispatchCompute(mIrmap.GetWidth() / 32, mIrmap.GetHeight() / 32, 6);
		irmapProgram.Release();
		//-------------------------------------------------------------------------------------------------------------------
		mSpBrdfLut = Texture{ GL_TEXTURE_2D, kBRDF_LUT_Size, kBRDF_LUT_Size, GL_RG16F, 1 };
		mSpBrdfLut.SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		ShaderProgram spBRDFProgram =
			ShaderProgram{{ std::make_tuple(GL_COMPUTE_SHADER, Shader::GetFileContents("data/shaders/spbrdf_cs.glsl")) }};

		spBRDFProgram.Use();
		mSpBrdfLut.BindImageTexture(0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
		ShaderProgram::DispatchCompute(mSpBrdfLut.GetWidth() / 32, mSpBrdfLut.GetHeight() / 32, 1);
		spBRDFProgram.Release();
		//-------------------------------------------------------------------------------------------------------------------
		glFinish();
	}

	void Release() override
	{
		Texture::Release();
		mIrmap.Release();
		mSpBrdfLut.Release();
	}

	const Texture &GetIrmapTexture() const { return mIrmap; }
	const Texture &GetSpBrdfLutTexture() const { return mSpBrdfLut; }

protected:
	Texture mIrmap, mSpBrdfLut;
};

class Renderbuffer : public RenderTarget
{
public:
	Renderbuffer()
		: RenderTarget()
	{
		glCreateRenderbuffers(1, &mId);
	}

	~Renderbuffer() override { Release(); }

	Renderbuffer(Renderbuffer &&Other)
		: RenderTarget(std::move(Other))
	{
	}

	Renderbuffer &operator = (Renderbuffer &&Other)
	{
		if (&Other != this)
		{
			Release();

			RenderTarget::operator = (std::move(Other));
		}

		return *this;
	}

	void AttachTo(GLuint Fb, GLenum Attachment) const override
	{
		glNamedFramebufferRenderbuffer(Fb, Attachment, GL_RENDERBUFFER, mId);
	}

	void Storage(GLenum Format, GLint Width, GLint Height, GLint Samples = 0)
	{
		if (Samples > 0)
		{
			glNamedRenderbufferStorageMultisample(mId, Samples, Format, Width, Height);
		}
		else
		{
			glNamedRenderbufferStorage(mId, Format, Width, Height);
		}
		mWidth = Width;
		mHeight = Height;
	}

	void Release() override
	{
		if (0 != mId)
		{
			glDeleteRenderbuffers(1, &mId);
			mId = 0;
			RenderTarget::Release();
		}
	}

protected:
};

class Framebuffer : public NonCopyable
{
protected:
	enum RenderTargetType { TypeRenderBuffer = 0, TypeTexture };
public:
	Framebuffer()
	{
		glCreateFramebuffers(1, &mId);
	}

	~Framebuffer() override { Release(); }

	Framebuffer(Framebuffer &&Other)
		: mId(Other.mId)
	{
		Other.mId = 0;
	}

	Framebuffer & operator = (Framebuffer && Other)
	{
		if (&Other != this)
		{
			Release();

			std::swap(mId, Other.mId);
		}
		return *this;
	}

	GLuint GetId() { return mId; }

	void Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mId);
	}

	void Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void AttachRenderbuffer(GLenum Attachment, GLenum Format, GLint Width, GLint Height, GLint Samples = 0)
	{
		mRbParams[Attachment] = std::make_tuple(RenderTargetType::TypeRenderBuffer, Format, Samples);
		recreateIfNeeded(Attachment, Width, Height);
		updateDrawBuffers();
	}

	void AttachTexture(GLenum Attachment, GLenum Format, GLint Width, GLint Height)
	{
		mRbParams[Attachment] = std::make_tuple(RenderTargetType::TypeTexture, Format, 0);
		recreateIfNeeded(Attachment, Width, Height);
		updateDrawBuffers();
	}

	void ResizeAll(GLint Width, GLint Height)
	{
		for (const auto &rb : mRenderbuffers)
		{
			recreateIfNeeded(rb.first, Width, Height);
		}
	}

	const std::shared_ptr<const RenderTarget> GetRenderTarget(GLenum Attachment) const
	{
		return mRenderbuffers.at(Attachment);
	}

	GLenum CheckStatus() const
	{
		return glCheckNamedFramebufferStatus(mId, GL_DRAW_FRAMEBUFFER);
	}

	void Blit(glm::ivec2 SrcP0, glm::ivec2 SrcP1, const Framebuffer& Dst, glm::ivec2 DstP0, glm::ivec2 DstP1, GLbitfield Mask, GLenum Filter) const
	{
		glBlitNamedFramebuffer(mId, Dst.mId, SrcP0.x, SrcP0.y, SrcP1.x, SrcP1.y, DstP0.x, DstP0.y, DstP1.x, DstP1.y, Mask, Filter);
	}

	void InvalidateAttachments(std::vector<GLenum> Attachments) const
	{
		glInvalidateNamedFramebufferData(mId, GLsizei(Attachments.size()), (Attachments.size() > 0) ? &Attachments[0] : nullptr);
	}

	void SetReadBuffer(GLenum Attachment) const
	{
		glNamedFramebufferReadBuffer(mId, Attachment);
	}

	void SetDrawBuffer(GLenum Attachment) const
	{
		glNamedFramebufferDrawBuffer(mId, Attachment);
	}

	void SetDrawBuffers(std::vector<GLenum> Buffers) const
	{
		glNamedFramebufferDrawBuffers(mId, Buffers.size(), Buffers.size() ? &Buffers[0] : nullptr);
	}

	void Resolve(const Framebuffer& Dst, std::vector<std::tuple<GLenum, GLenum, GLbitfield, GLenum>> List) const
	{
		for (const auto & copyParams: List)
		{
			auto attach1 = std::get<0>(copyParams);
			auto attach2 = std::get<1>(copyParams);
			auto bitfieldMask = std::get<2>(copyParams);
			auto filter = std::get<3>(copyParams);
			const auto &rt1 = GetRenderTarget(attach1);
			const auto &rt2 = Dst.GetRenderTarget(attach2);
			SetReadBuffer(attach1);
			Dst.SetDrawBuffer(attach2);
			Blit(glm::ivec2{0, 0}, glm::ivec2{rt1->GetWidth(), rt1->GetHeight()},
				 Dst, glm::ivec2{0, 0}, glm::ivec2{rt2->GetWidth(), rt2->GetHeight()}, bitfieldMask, filter);
		}
	}

	void Release() override
	{
		if (0 != mId)
		{
			mRenderbuffers.clear();
			glDeleteFramebuffers(1, &mId);
			mId = 0;
		}
	}

protected:
	void updateDrawBuffers()
	{
		std::vector<GLenum> buffers;
		for (const auto &buffer : mRenderbuffers)
		{
			if (GL_DEPTH_ATTACHMENT != buffer.first)
			{
				buffers.emplace_back(buffer.first);
			}
		}
		glNamedFramebufferDrawBuffers(mId, buffers.size(), buffers.size() ? &buffers[0] : nullptr);
	}

	void recreateIfNeeded(GLenum Attachment, GLint Width, GLint Height)
	{
		if (0 == mRenderbuffers.count(Attachment)
			|| mRenderbuffers[Attachment]->GetWidth() != Width
			|| mRenderbuffers[Attachment]->GetHeight() != Height)
		{
			const auto &params = mRbParams[Attachment];
			const auto type = std::get<0>(params);
			const auto format = std::get<1>(params);
			const auto samples = std::get<2>(params);
			switch (type)
			{
				case RenderTargetType::TypeRenderBuffer:
					{
						auto renderbufferPtr = std::make_shared<Renderbuffer>();
						mRenderbuffers[Attachment] = std::static_pointer_cast<RenderTarget>(renderbufferPtr);
						renderbufferPtr->Storage(format, Width, Height, samples);
					}
					break;
				case RenderTargetType::TypeTexture:
					{
						auto texturePtr = std::make_shared<Texture>(GL_TEXTURE_2D);
						mRenderbuffers[Attachment] = std::static_pointer_cast<RenderTarget>(texturePtr);
						texturePtr->Storage(format, Width, Height, 1);
					}
					break;
			}
			mRenderbuffers[Attachment]->AttachTo(mId, Attachment);
		}
	}

	GLuint mId;
	std::unordered_map<GLenum, std::shared_ptr<RenderTarget>> mRenderbuffers;
	std::unordered_map<GLenum, std::tuple<RenderTargetType, GLenum, GLint>> mRbParams;
};

template <class T>
class UniformBuffer : public NonCopyable
{
public:
	UniformBuffer()
		: mId(0)
	{}

	UniformBuffer(UniformBuffer &&Other)
		: mId(Other.mId)
	{
		Other.mId = 0;
	}

	UniformBuffer &operator = (UniformBuffer &&Other)
	{
		if (&Other != this)
		{
			Release();

			std::swap(mId, Other.mId);
		}
		return *this;
	}

	void Create()
	{
		mId = -1;
		glCreateBuffers(1, &mId);
		glNamedBufferStorage(mId, sizeof(*this), nullptr, GL_DYNAMIC_STORAGE_BIT);
	}

	void Update()
	{
		glNamedBufferSubData(mId, 0, sizeof(T), &mData);
	}

	void Bind(GLuint Slot)
	{
		if (0 == mId)
		{
			Create();
		}
		Update();
		glBindBufferBase(GL_UNIFORM_BUFFER, Slot, mId);
	}

	T &GetReference() { return mData; }

	void Release() override
	{
		if (-1 != mId)
		{
			glDeleteBuffers(1, &mId);
		}
		mId = 0;
	}

protected:
	GLuint mId;
	T mData;
};

struct MeshBuffer
{
	MeshBuffer() : vbo(0), ibo(0), vao(0), numElements(0) {}
	GLuint vbo, ibo, vao;
	GLuint numElements;
};

class MeshGeometry : public NonCopyable
{
public:
	MeshGeometry() : mEmpty(false), mDrawPatches(false), mVbo(0), mIbo(0), mVao(0), mNumElements(0)
	{
	}

	MeshGeometry(MeshGeometry &&Other)
		: mEmpty(Other.mEmpty)
		, mDrawPatches(Other.mDrawPatches)
		, mVbo(Other.mVbo)
		, mIbo(Other.mIbo)
		, mVao(Other.mVao)
		, mNumElements(Other.mNumElements)
	{
		Other.mEmpty = false;
		Other.mDrawPatches = false;
		Other.mVao = 0;
		Other.mVbo = 0;
		Other.mIbo = 0;
		Other.mNumElements = 0;
	}

	MeshGeometry &operator = (MeshGeometry &&Other)
	{
		if (&Other != this)
		{
			Release();

			std::swap(mEmpty, Other.mEmpty);
			std::swap(mDrawPatches, Other.mDrawPatches);
			std::swap(mVao, Other.mVao);
			std::swap(mVbo, Other.mVbo);
			std::swap(mIbo, Other.mIbo);
			std::swap(mNumElements, Other.mNumElements);
		}
		return *this;
	}

	MeshGeometry(const std::shared_ptr<Mesh> &MeshPtr, bool FullScreenTriangle = false, bool DrawPatches = false)
	{
		mEmpty = FullScreenTriangle;
		mDrawPatches = DrawPatches;
		if (false != FullScreenTriangle)
		{
			mNumElements = 0;
			mVbo = 0;
			mIbo = 0;
			glCreateVertexArrays(1, &mVao);
		}
		else if (nullptr != MeshPtr)
		{
			mNumElements = static_cast<GLuint>(MeshPtr->faces().size()) * 3;

			const size_t vertexDataSize = MeshPtr->vertices().size() * sizeof(Mesh::Vertex);
			const size_t indexDataSize  = MeshPtr->faces().size() * sizeof(Mesh::Face);

			glCreateBuffers(1, &mVbo);
			glNamedBufferStorage(mVbo, vertexDataSize, reinterpret_cast<const void*>(&MeshPtr->vertices()[0]), 0);
			glCreateBuffers(1, &mIbo);
			glNamedBufferStorage(mIbo, indexDataSize, reinterpret_cast<const void*>(&MeshPtr->faces()[0]), 0);

			glCreateVertexArrays(1, &mVao);
			glVertexArrayElementBuffer(mVao, mIbo);
			std::array<GLint, Mesh::NumAttributes> sizes =
			{
				sizeof(Mesh::Vertex::position),
				sizeof(Mesh::Vertex::normal),
				sizeof(Mesh::Vertex::tangent),
				sizeof(Mesh::Vertex::bitangent),
				sizeof(Mesh::Vertex::texcoord),
			};
			for (int i = 0; i < Mesh::NumAttributes; i++)
			{
				glVertexArrayVertexBuffer(mVao, i, mVbo, i * sizeof(glm::vec3), sizeof(Mesh::Vertex));
				glEnableVertexArrayAttrib(mVao, i);
				glVertexArrayAttribFormat(mVao, i, sizes[i] / sizeof(GLfloat), GL_FLOAT, GL_FALSE, 0);
				glVertexArrayAttribBinding(mVao, i, i);
			}
		}
	}

	void Release() override
	{
		if (0 != mVao)
		{
			glDeleteVertexArrays(1, &mVao);
			mVao = 0;
		}
		if (0 != mVbo)
		{
			glDeleteBuffers(1, &mVbo);
			mVbo = 0;
		}
		if (0 != mIbo)
		{
			glDeleteBuffers(1, &mIbo);
			mIbo = 0;
		}
		mNumElements = 0;
		mEmpty = false;
		mDrawPatches = false;
	}

	void Render()
	{
		glBindVertexArray(mVao);
		if (false != mEmpty)
		{
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
		else if (false != mDrawPatches)
		{
			glDrawElements(GL_PATCHES, mNumElements, GL_UNSIGNED_INT, 0);
		}
		else
		{
			glDrawElements(GL_TRIANGLES, mNumElements, GL_UNSIGNED_INT, 0);
		}
	}

protected:
	GLboolean mEmpty, mDrawPatches;
	GLuint mVbo, mIbo, mVao;
	GLuint mNumElements;
};

class PbrMeshBase : public MeshGeometry
{
public:
	PbrMeshBase()
		: MeshGeometry() {}

	PbrMeshBase(PbrMeshBase &&Other)
		: MeshGeometry(std::move(Other)),
            mAlbedo(std::move(Other.mAlbedo)),
            mNormals(std::move(Other.mNormals)),
            mMetalness(std::move(Other.mMetalness)),
            mRoughness(std::move(Other.mRoughness)),
            mEnvironmentPtr(std::move(Other.mEnvironmentPtr)) {}
	
	PbrMeshBase &operator = (PbrMeshBase &&Other)
	{
		if (&Other != this)
		{
            Release();
			MeshGeometry::operator = (std::move(Other));
			mAlbedo = std::move(Other.mAlbedo);
			mNormals = std::move(Other.mNormals);
			mMetalness = std::move(Other.mMetalness);
			mRoughness = std::move(Other.mRoughness);
			mEnvironmentPtr = std::move(Other.mEnvironmentPtr);
		}

		return *this;
	}

	PbrMeshBase(const std::shared_ptr<Mesh> &MeshPtr, const std::shared_ptr<const Environment> &EnvironmentPtr = nullptr, bool DrawPatches = false)
		: MeshGeometry(MeshPtr, false, DrawPatches)
	{
		const std::string texturesPathStr = "data/textures/";
		auto albedoFileName = MeshPtr->textureName(Mesh::TextureType::Albedo);
		if (albedoFileName.empty())
		{
			const GLubyte pix[] = { 128, 128, 128, 255 };
			mAlbedo = Texture{ GL_TEXTURE_2D, 1, 1, GL_RGBA, GL_RGBA8, 0, GL_UNSIGNED_BYTE, &pix };
		}
		else
		{
			mAlbedo = Texture{ Image::fromFile(texturesPathStr + albedoFileName, 4), GL_RGBA, GL_SRGB8_ALPHA8 };
		}

		auto normalsFileName = MeshPtr->textureName(Mesh::TextureType::Normals);
		if (normalsFileName.empty())
		{
			const GLubyte pix[] = { 0, 0, 255 };
			mNormals = Texture{ GL_TEXTURE_2D, 1, 1, GL_RGB, GL_RGB8, 0, GL_UNSIGNED_BYTE, &pix };
		}
		else
		{
			mNormals = Texture{ Image::fromFile(texturesPathStr + normalsFileName, 3), GL_RGB, GL_RGB8 };
		}

		auto metalnessFileName = MeshPtr->textureName(Mesh::TextureType::Metalness);
		if (metalnessFileName.empty())
		{
			const GLubyte pix[] = { 128 };
			mMetalness = Texture{ GL_TEXTURE_2D, 1, 1, GL_RED, GL_R8, 0, GL_UNSIGNED_BYTE, &pix };
		}
		else
		{
			mMetalness = Texture{ Image::fromFile(texturesPathStr + metalnessFileName, 1), GL_RED, GL_R8 };
		}

		auto roughnessFileName = MeshPtr->textureName(Mesh::TextureType::Roughness);
		if (roughnessFileName.empty())
		{
			const GLubyte pix[] = { 128 };
			mRoughness = Texture{ GL_TEXTURE_2D, 1, 1, GL_RED, GL_R8, 0, GL_UNSIGNED_BYTE, &pix };
		}
		else
		{
			mRoughness = Texture{ Image::fromFile(texturesPathStr + roughnessFileName, 1), GL_RED, GL_R8 };
		}
	}

	void Release() override
	{
		mAlbedo.Release();
		mNormals.Release();
		mMetalness.Release();
		mRoughness.Release();
		mEnvironmentPtr = nullptr;

		MeshGeometry::Release();
	}

	virtual void SetShadingUniforms(
		std::array<SceneSettings::Light, SceneSettings::NumLights> Lights,
		const glm::vec4 &Viewport,
		const glm::mat4 &ProjectionMat,
		const glm::mat4 &ViewMat,
		const glm::mat4 &ModelMat)
	{
	}

	virtual void Render(bool OpaquePass) {}

protected:
	Texture mAlbedo, mNormals, mMetalness, mRoughness;
	std::shared_ptr<const Environment> mEnvironmentPtr;
};

class PbrMesh : public PbrMeshBase
{
public:
	PbrMesh()
		: PbrMeshBase()
	{}

	PbrMesh(PbrMesh &&Other)
		: PbrMeshBase(std::move(Other))
	{
	}

	PbrMesh &operator = (PbrMesh &&Other)
	{
		PbrMeshBase::operator = (std::move(Other));
		return *this;
	}

	PbrMesh(const std::shared_ptr<Mesh> &MeshPtr, const std::shared_ptr<const Environment> &EnvironmentPtr = nullptr)
		: PbrMeshBase(MeshPtr, EnvironmentPtr) {}

	~PbrMesh() { Release(); }

	void SetShadingUniforms(
		std::array<SceneSettings::Light, SceneSettings::NumLights> Lights,
		const glm::vec4 &Viewport,
		const glm::mat4 &ProjectionMat,
		const glm::mat4 &ViewMat,
		const glm::mat4 &ModelMat) override
	{
		auto &transformUniforms = mTransformUB.GetReference();
		transformUniforms.viewProjectionMatrix = ProjectionMat * ViewMat;
		transformUniforms.modelMatrix = ModelMat;

		auto &shadingUniforms = mShadingUB.GetReference();
		for (int i = 0; i < Lights.size(); i++)
		{
			const SceneSettings::Light& light = Lights[i];
			shadingUniforms.lights[i].direction = glm::vec4{ light.direction, 0.0f };
			shadingUniforms.lights[i].radiance = light.enabled ? glm::vec4{ light.radiance, 0.0f } : glm::vec4{};
		}

		const glm::vec3 eyePosition = glm::vec3{ glm::inverse(ViewMat) * glm::vec4{ 0, 0, 0, 1.f } };
		shadingUniforms.eyePosition = glm::vec4{ eyePosition, 0.0f };
	}

	void Render(bool OpaquePass) override
	{
		static ShaderProgram pbrProgram;
		if (!pbrProgram.IsUsable())
		{
			pbrProgram =
				ShaderProgram{{ std::make_tuple(GL_VERTEX_SHADER, Shader::GetFileContents("data/shaders/pbr_vs.glsl")),
								std::make_tuple(GL_FRAGMENT_SHADER, Shader::GetFileContents("data/shaders/pbr_fs.glsl")) }};
		}
		pbrProgram.Use();

		mTransformUB.Bind(0);
		mShadingUB.Bind(1);											// update and bind uniform buffers
		mBaseInfoUB.GetReference().opaquePass = OpaquePass ? 1 : 0;	// don't draw transparent geometry
		mBaseInfoUB.Bind(2);

		mAlbedo.BindTextureUnit(0);
		mNormals.BindTextureUnit(1);
		mMetalness.BindTextureUnit(2);
		mRoughness.BindTextureUnit(3);
		if (nullptr != mEnvironmentPtr)
		{
			mEnvironmentPtr->BindTextureUnit(4);
			mEnvironmentPtr->GetIrmapTexture().BindTextureUnit(5);
			mEnvironmentPtr->GetSpBrdfLutTexture().BindTextureUnit(6);
		}
		MeshGeometry::Render();
	}

	void Release() override
	{
		mTransformUB.Release();
		mShadingUB.Release();
		mBaseInfoUB.Release();

		PbrMeshBase::Release();
	}

protected:
	struct TransformUB
	{
		glm::mat4 viewProjectionMatrix;
		glm::mat4 modelMatrix;
	};
	UniformBuffer<TransformUB> mTransformUB;

	struct ShadingUB
	{
		struct
		{
			glm::vec4 direction;
			glm::vec4 radiance;
		}
		lights[SceneSettings::NumLights];
		glm::vec4 eyePosition;
	};
	UniformBuffer<ShadingUB> mShadingUB;

	struct BaseInfoUB
	{
		int opaquePass;
	};
	UniformBuffer<BaseInfoUB> mBaseInfoUB;
};

class PbrAsteroid : public PbrMeshBase
{
public:
	PbrAsteroid()
		: PbrMeshBase()
	{
	}

	PbrAsteroid(PbrAsteroid &&Other)
		: PbrMeshBase(std::move(Other))
	{
	}

	PbrAsteroid &operator = (PbrAsteroid &&Other)
	{
		PbrMeshBase::operator = (std::move(Other));

		return *this;
	}

	PbrAsteroid(const std::shared_ptr<Mesh> &MeshPtr, const std::shared_ptr<const Environment> &EnvironmentPtr = nullptr)
		: PbrMeshBase(MeshPtr, EnvironmentPtr, true)
	{
	}

	void Release() override
	{
		mTessControlUB.Release();
		mTransformsUB.Release();
		mViewProjectionUB.Release();
		mShadingUB.Release();
		mBaseInfoUB.Release();

		PbrMeshBase::Release();
	}

	void SetShadingUniforms(
		std::array<SceneSettings::Light, SceneSettings::NumLights> Lights,
		const glm::vec4 &Viewport,
		const glm::mat4 &ProjectionMat,
		const glm::mat4 &ViewMat,
		const glm::mat4 &ModelMat) override
	{
		auto &transformUniforms = mTransformsUB.GetReference();
		transformUniforms.modelViewProjectionMatrix = ProjectionMat * ViewMat * ModelMat;

		auto &mvpUB = mViewProjectionUB.GetReference();
		mvpUB.viewport = Viewport;
		mvpUB.modelMat = ModelMat;
		mvpUB.viewMat = ViewMat;
		mvpUB.modelViewProjectionMat = transformUniforms.modelViewProjectionMatrix;

		auto &baseInfoUB = mBaseInfoUB.GetReference();
		baseInfoUB.modelMat = ModelMat;
		baseInfoUB.modelViewMat = ViewMat * ModelMat;

		auto &shadingUniforms = mShadingUB.GetReference();
		for (int i = 0; i < Lights.size(); i++)
		{
			const SceneSettings::Light& light = Lights[i];
			shadingUniforms.lights[i].direction = glm::vec4{ light.direction, 0.0f };
			shadingUniforms.lights[i].radiance = light.enabled ? glm::vec4{ light.radiance, 0.0f } : glm::vec4{};
		}

		const glm::vec3 eyePosition = glm::vec3{ glm::inverse(ViewMat) * glm::vec4{ 0, 0, 0, 1.f } };
		shadingUniforms.eyePosition = glm::vec4{ eyePosition, 0.0f };

		auto &tessUniforms = mTessControlUB.GetReference();
		tessUniforms.modelViewMat = ViewMat * ModelMat;
		tessUniforms.projectionMat = ProjectionMat;
		tessUniforms.viewport = Viewport;
		glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &tessUniforms.maxTessLevel);
	}

	void Render(bool OpaquePass) override
	{
		static ShaderProgram pbrProgram;
		if (!pbrProgram.IsUsable())
		{
			pbrProgram =
				ShaderProgram{{ std::make_tuple(GL_VERTEX_SHADER, 			Shader::GetFileContents("data/shaders/pbr_asteroid_vs.glsl")),
								std::make_tuple(GL_TESS_CONTROL_SHADER, 	Shader::GetFileContents("data/shaders/pbr_asteroid_cs.glsl")),
								std::make_tuple(GL_TESS_EVALUATION_SHADER, 	Shader::GetFileContents("data/shaders/pbr_asteroid_es.glsl")),
								std::make_tuple(GL_FRAGMENT_SHADER, 		Shader::GetFileContents("data/shaders/pbr_asteroid_fs.glsl")) }};
		}
		pbrProgram.Use();

		glPatchParameteri(GL_PATCH_VERTICES, 3);

		mTransformsUB.Bind(0);
		mShadingUB.Bind(1);											// update and bind uniform buffers
		mBaseInfoUB.GetReference().opaquePass = OpaquePass ? 1 : 0;	// don't draw transparent geometry
		mBaseInfoUB.Bind(2);
		mViewProjectionUB.Bind(3);
		mTessControlUB.Bind(4);

		mAlbedo.BindTextureUnit(0);
		mNormals.BindTextureUnit(1);

		if (nullptr != mEnvironmentPtr)
		{
			mEnvironmentPtr->BindTextureUnit(4);
			mEnvironmentPtr->GetIrmapTexture().BindTextureUnit(5);
			mEnvironmentPtr->GetSpBrdfLutTexture().BindTextureUnit(6);
		}

		MeshGeometry::Render();
	}


protected:
	struct ModelMatrixUB
	{
		glm::mat4 modelViewProjectionMatrix;
	};
	UniformBuffer<ModelMatrixUB> mTransformsUB;

	struct ShadingUB
	{
		struct
		{
			glm::vec4 direction;
			glm::vec4 radiance;
		}
		lights[SceneSettings::NumLights];
		glm::vec4 eyePosition;
	};
	UniformBuffer<ShadingUB> mShadingUB;

	struct BaseInfoUB
	{
		glm::mat4 modelMat;
		glm::mat4 modelViewMat;
		int opaquePass;
	};
	UniformBuffer<BaseInfoUB> mBaseInfoUB;

	struct ViewProjectionMatrixUB
	{
		glm::mat4 modelMat;
		glm::mat4 viewMat;
		glm::mat4 modelViewProjectionMat;
		glm::vec4 viewport;
	};
	UniformBuffer<ViewProjectionMatrixUB> mViewProjectionUB;

	struct TessControlMatrixUniform
	{
		glm::mat4 modelViewMat;
		glm::mat4 projectionMat;
		glm::vec4 viewport;
		GLint maxTessLevel;
	};
	UniformBuffer<TessControlMatrixUniform> mTessControlUB;
};

//==========================================================================================================================
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//==========================================================================================================================
class Renderer final : public RendererInterface
{
public:
	GLFWwindow* initialize(int width, int height, int maxSamples) override;
	void shutdown() override;
	std::function<void (int w, int h)> setup() override;
	void render(GLFWwindow* window, const ViewSettings& view, const SceneSettings& scene) override;

protected:
	void renderScene(bool OpaquePass);

#ifdef _DEBUG
	static void logMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
#endif

	std::shared_ptr<Framebuffer> mFramebuffer, mResolveFramebuffer;

	std::shared_ptr<Camera> mCameraPtr;

	MeshGeometry mFullScreenQuad;
	MeshGeometry mSkybox;
	PbrAsteroid mPbrAsteroid;

	MeshGeometry mEmptyVao;

	ShaderProgram mSkyboxProgram;
	ShaderProgram mTonemapProgram;

	std::shared_ptr<Environment> mEnvPtr;

	struct SkyboxUB
	{
		glm::mat4 skyViewProjectionMatrix;
	};
	UniformBuffer<SkyboxUB> mSkyboxUB;
};

} // OpenGL
