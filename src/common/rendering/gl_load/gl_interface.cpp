/*
** gl_interface.cpp
** OpenGL system interface
**
**---------------------------------------------------------------------------
** Copyright 2005-2019 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl_system.h"
#include "engineerrors.h"
#include "tarray.h"
#include "basics.h"
#include "m_argv.h"
#include "version.h"
#include "v_video.h"
#include "printf.h"
#include "gl_interface.h"

static TArray<FString>  m_Extensions;
RenderContext gl;
static double realglversion;	// this is public so the statistics code can access it.

//==========================================================================
//
// 
//
//==========================================================================

static void CollectExtensions()
{
	const char *extension;

	int max = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &max);

	// Use modern method to collect extensions
	for (int i = 0; i < max; i++)
	{
		extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
		m_Extensions.Push(FString(extension));
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static bool CheckExtension(const char *ext)
{
	for (unsigned int i = 0; i < m_Extensions.Size(); ++i)
	{
		if (m_Extensions[i].CompareNoCase(ext) == 0) return true;
	}

	return false;
}



//==========================================================================
//
// 
//
//==========================================================================

static void InitContext()
{
	gl.flags=0;
}

//==========================================================================
//
// 
//
//==========================================================================

#define FUDGE_FUNC(name, ext) 	if (_ptrc_##name == NULL) _ptrc_##name = _ptrc_##name##ext;


void gl_LoadExtensions()
{
	InitContext();
	CollectExtensions();

	const char *glversion = (const char*)glGetString(GL_VERSION);

	const char *version = Args->CheckValue("-glversion");
	realglversion = strtod(glversion, NULL);


	if (version == NULL)
	{
		version = glversion;
	}
	else
	{
		double v1 = strtod(version, NULL);
		if (v1 >= 3.0 && v1 < 3.3)
		{
			v1 = 3.3;	// promote '3' to 3.3 to avoid falling back to the legacy path.
			version = "3.3";
		}
		if (realglversion < v1) version = glversion;
		else Printf("Emulating OpenGL v %s\n", version);
	}

	float gl_version = (float)strtod(version, NULL) + 0.01f;

	// Don't even start if it's lower than 2.0 or no framebuffers are available (The framebuffer extension is needed for glGenerateMipmapsEXT!)
	if (gl_version < 3.3f)
	{
		I_FatalError("Unsupported OpenGL version.\nAt least OpenGL 3.3 is required to run " GAMENAME ".\n");
	}


	// add 0.01 to account for roundoff errors making the number a tad smaller than the actual version
	gl.glslversion = strtod((char*)glGetString(GL_SHADING_LANGUAGE_VERSION), NULL) + 0.01f;

	gl.vendorstring = (char*)glGetString(GL_VENDOR);
	gl.modelstring = (char*)glGetString(GL_RENDERER);

	// first test for optional features
	if (CheckExtension("GL_ARB_texture_compression")) gl.flags |= RFL_TEXTURE_COMPRESSION;
	if (CheckExtension("GL_EXT_texture_compression_s3tc")) gl.flags |= RFL_TEXTURE_COMPRESSION_S3TC;

	if (gl_version < 4.f)
	{
#ifdef _WIN32
		if (strstr(gl.vendorstring, "ATI Tech"))
		{
			gl.flags |= RFL_NO_CLIP_PLANES;	// gl_ClipDistance is horribly broken on ATI GL3 drivers for Windows. (TBD: Relegate to vintage build? Maybe after the next survey.)
		}
#endif
		gl.glslversion = 3.31f;	// Force GLSL down to 3.3.
	}
	else if (gl_version < 4.5f)
	{
		// don't use GL 4.x features when running a GL 3.x context.
		if (CheckExtension("GL_ARB_buffer_storage"))
		{
			// work around a problem with older AMD drivers: Their implementation of shader storage buffer objects is piss-poor and does not match uniform buffers even closely.
			// Recent drivers, GL 4.4 don't have this problem, these can easily be recognized by also supporting the GL_ARB_buffer_storage extension.
			if (CheckExtension("GL_ARB_shader_storage_buffer_object"))
			{
				gl.flags |= RFL_SHADER_STORAGE_BUFFER;
			}
			gl.flags |= RFL_BUFFER_STORAGE;
		}
	}
	else
	{
		// Assume that everything works without problems on GL 4.5 drivers where these things are core features.
		gl.flags |= RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;
	}

	// Mesa implements shader storage only for fragment shaders.
	// Just disable the feature there. The light buffer may just use a uniform buffer without any adverse effects.
	int v = 0;
	glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &v);
	if (v == 0)
		gl.flags &= ~RFL_SHADER_STORAGE_BUFFER;


	if (gl_version >= 4.3f || CheckExtension("GL_ARB_invalidate_subdata")) gl.flags |= RFL_INVALIDATE_BUFFER;
	if (gl_version >= 4.3f || CheckExtension("GL_KHR_debug")) gl.flags |= RFL_DEBUG;

	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
	gl.maxuniforms = v;
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
	gl.maxuniformblock = v;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
	gl.uniformblockalignment = v;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl.max_texturesize);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

//==========================================================================
//
// 
//
//==========================================================================

void gl_PrintStartupLog()
{
	int v = 0;
	glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &v);

	Printf ("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	Printf ("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	Printf ("GL_VERSION: %s (%s profile)\n", glGetString(GL_VERSION), (v & GL_CONTEXT_CORE_PROFILE_BIT)? "Core" : "Compatibility");
	Printf ("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	Printf (PRINT_DEFAULT, PRINTF_LOGONLY, "GL_EXTENSIONS:");
	for (unsigned i = 0; i < m_Extensions.Size(); i++)
	{
		Printf(PRINT_DEFAULT, PRINTF_LOGONLY, " %s", m_Extensions[i].GetChars());
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
	Printf("\nMax. texture size: %d\n", v);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v);
	Printf ("Max. texture units: %d\n", v);
	glGetIntegerv(GL_MAX_VARYING_FLOATS, &v);
	Printf ("Max. varying: %d\n", v);
	
	if (gl.flags & RFL_SHADER_STORAGE_BUFFER)
	{
		glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &v);
		Printf("Max. combined shader storage blocks: %d\n", v);
		glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &v);
		Printf("Max. vertex shader storage blocks: %d\n", v);
	}
	else
	{
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
		Printf("Max. uniform block size: %d\n", v);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
		Printf("Uniform block alignment: %d\n", v);
	}
}

std::pair<double, bool> gl_getInfo()
{
	// gl_ARB_bindless_texture is the closest we can get to determine Vulkan support from OpenGL.
	// This isn't foolproof because Intel doesn't support it but for NVidia and AMD support of this extension means Vulkan support.
	return std::make_pair(realglversion, CheckExtension("GL_ARB_bindless_texture"));
}
