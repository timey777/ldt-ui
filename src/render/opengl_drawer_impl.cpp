#include "opengl_drawer_impl.h"
#if defined(LDT_ENABLE_RENDERING)
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include "engine/resource_manager.h"
#include "misc/logger.h"
#include "misc/utf8util.h"

// Performance / validation toggles
// - LDT_RENDER_STATS: compile-time default for stats output (0/1)
// - env var LDT_RENDER_STATS=1: runtime override to enable stats output
// - LDT_RENDER_VALIDATE: compile-time enable GL error checks (0/1)
#ifndef LDT_RENDER_STATS
#define LDT_RENDER_STATS 0
#endif

#ifndef LDT_RENDER_VALIDATE
#define LDT_RENDER_VALIDATE 0
#endif

struct Glyph {
	// Glyph texture comes from a font atlas page (not per-glyph texture)
	unsigned int tex = 0;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
	float width = 0.0f;
	float height = 0.0f;
	float bearingX = 0.0f;
	float bearingY = 0.0f;
	float advance = 0.0f; // in DIP units (converted from 1/64 pixels)
};

class OpenGLDrawerImpl::ImplData {
public:
	int width = 800, height = 600;
	float scaleX = 1.0f, scaleY = 1.0f;

	unsigned int progColor = 0;
	unsigned int progTex = 0;
	unsigned int progTexText = 0; // text shader: alpha from red channel
	unsigned int vao = 0, vbo = 0; // for textured quad (pos + uv)
	size_t vboFloatCapacity = 0; // capacity of vbo in floats
	unsigned int vaoColor = 0, vboColor = 0; // for rect batch (pos + rect + color + params)
	size_t vboColorFloatCapacity = 0; // capacity of vboColor in floats
	unsigned int whiteTex = 0;

	// uniform locations
	int uProj_color = -1;
	int uColor = -1;
	int uRect = -1;
	int uCornerRadius = -1;
	int uStrokeWidth = -1;
	int uSdfClip_color = -1;
	int uSdfClipRadius_color = -1;

	int uProj_tex = -1;
	int uTexSampler = -1;
	int uTexColor = -1;
	int uSdfClip_tex = -1;
	int uSdfClipRadius_tex = -1;

	int uProj_texText = -1;
	int uTexSamplerText = -1;
	int uTexColorText = -1;
	int uSdfClip_texText = -1;
	int uSdfClipRadius_texText = -1;

	// clip state (top-left origin, pixels)
	bool hasClip = false;
	render::Rect clip{ 0,0,0,0 };
	float clipCornerRadius = 0.0f; // SDF clip corner radius (0 = rectangular scissor only)

	// lightweight GL state cache (best-effort, scoped to this drawer)
	struct GLStateCache {
		unsigned int program = 0;
		unsigned int vao = 0;
		unsigned int arrayBuffer = 0;
		unsigned int tex0 = 0;
		bool scissorEnabled = false;
		int scissorX = -1;
		int scissorY = -1;
		int scissorW = -1;
		int scissorH = -1;
	} gl;

	// transform state
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	std::vector<std::pair<float, float>> transformStack;

	// FreeType
	FT_Library ft = nullptr;

	// simple per-frame render statistics
	bool stat_enabled = (LDT_RENDER_STATS != 0);
	int stat_drawCalls = 0;
	int stat_drawCalls_rect = 0;
	int stat_drawCalls_image = 0;
	int stat_drawCalls_text = 0;
	int stat_image_quads = 0;
	int stat_texBatch_flushes = 0;
	int stat_texBatch_flush_keyClip = 0;
	int stat_texBatch_flush_keyColor = 0;
	int stat_texBatch_flush_keyTex = 0;
	int stat_texBatch_flush_keyKind = 0;
	int stat_texBatch_flush_ordering = 0;
	int stat_texBatch_flush_clipChange = 0;
	int stat_texBatch_flush_present = 0;
	int stat_textureBinds = 0;
	int stat_bufferSubData = 0;
	int stat_bufferSubData_rect = 0;
	int stat_bufferSubData_image = 0;
	int stat_bufferSubData_text = 0;
	int stat_useProgram = 0;
	int stat_vaoBinds = 0;
	int stat_text_batches = 0;
	int stat_text_glyphs = 0;
	int stat_frames = 0;
	std::chrono::steady_clock::time_point stat_lastTime;
	struct FontKey {
		std::string family;
		int size;
		float scale;
		bool operator<(const FontKey& o) const {
			if (family != o.family) return family < o.family;
			if (size != o.size) return size < o.size;
			return scale < o.scale;
		}
	};
	std::map<FontKey, std::map<uint32_t, Glyph>> glyphCache;
	std::map<FontKey, FT_Face> faceCache;

	struct FontAtlasPage {
		unsigned int tex = 0;
		int width = 0;
		int height = 0;
		int penX = 0;
		int penY = 0;
		int rowH = 0;
	};
	std::map<FontKey, std::vector<FontAtlasPage>> atlasPages;

	enum class TexBatchKind : unsigned char {
		None = 0,
		Text = 1,
		Image = 2,
	};
	struct TexturedBatch {
		TexBatchKind kind = TexBatchKind::None;
		unsigned int tex = 0;
		float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool hasClip = false;
		render::Rect clip{ 0,0,0,0 };
		std::vector<float> verts; // pos(x,y) + uv(u,v) => 4 floats
		int textGlyphs = 0;
		int imageQuads = 0;
	} texBatch;

	struct RectBatch {
		bool active = false;
		bool hasClip = false;
		render::Rect clip{ 0,0,0,0 };
		std::vector<float> verts; // per-vertex: pos2 + rect4 + color4 + params2 => 12 floats
		int rects = 0;
	} rectBatch;

};

// Forward declarations for helpers used early in this translation unit
static void gl_apply_scissor(OpenGLDrawerImpl::ImplData* d, bool enable, int x, int y, int w, int h);
static void gl_apply_clip_rect(OpenGLDrawerImpl::ImplData* d, bool hasClip, const render::Rect& clip);
static void gl_bind_program(OpenGLDrawerImpl::ImplData* d, unsigned int prog);
static void gl_bind_vao(OpenGLDrawerImpl::ImplData* d, unsigned int vao);
static void gl_bind_array_buffer(OpenGLDrawerImpl::ImplData* d, unsigned int buf);
static void gl_bind_tex0(OpenGLDrawerImpl::ImplData* d, unsigned int tex);

static void rect_batch_flush(OpenGLDrawerImpl::ImplData* d);

static void ensure_textured_vbo_capacity(OpenGLDrawerImpl::ImplData* d, size_t requiredFloatCount) {
	if (!d) return;
	if (requiredFloatCount <= d->vboFloatCapacity) return;
	// grow (1.5x) and round up
	size_t newCap = std::max(requiredFloatCount, d->vboFloatCapacity + d->vboFloatCapacity / 2 + 256);
	gl_bind_array_buffer(d, d->vbo);
	glBufferData(GL_ARRAY_BUFFER, newCap * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	d->vboFloatCapacity = newCap;
}

static void ensure_rect_vbo_capacity(OpenGLDrawerImpl::ImplData* d, size_t requiredFloatCount) {
	if (!d) return;
	if (requiredFloatCount <= d->vboColorFloatCapacity) return;
	// grow (1.5x) and round up
	size_t newCap = std::max(requiredFloatCount, d->vboColorFloatCapacity + d->vboColorFloatCapacity / 2 + 512);
	gl_bind_array_buffer(d, d->vboColor);
	glBufferData(GL_ARRAY_BUFFER, newCap * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	d->vboColorFloatCapacity = newCap;
}

static bool rect_exact_equal(const render::Rect& a, const render::Rect& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static void textured_batch_reset(OpenGLDrawerImpl::ImplData* d) {
	if (!d) return;
	d->texBatch.kind = OpenGLDrawerImpl::ImplData::TexBatchKind::None;
	d->texBatch.tex = 0;
	d->texBatch.color[0] = 1.0f;
	d->texBatch.color[1] = 1.0f;
	d->texBatch.color[2] = 1.0f;
	d->texBatch.color[3] = 1.0f;
	d->texBatch.hasClip = false;
	d->texBatch.clip = { 0,0,0,0 };
	d->texBatch.verts.clear();
	d->texBatch.textGlyphs = 0;
	d->texBatch.imageQuads = 0;
}

// Forward declare flush so wrapper helpers can call it.
static void textured_batch_flush(OpenGLDrawerImpl::ImplData* d);

enum class TexBatchFlushReason : unsigned char {
	Unknown = 0,
	KeyClip = 1,
	KeyColor = 2,
	KeyTex = 3,
	KeyKind = 4,
	Ordering = 5,
	ClipChange = 6,
	Present = 7,
};

static void textured_batch_note_flush(OpenGLDrawerImpl::ImplData* d, TexBatchFlushReason reason) {
	if (!d) return;
	auto& b = d->texBatch;
	bool hadWork = (b.kind != OpenGLDrawerImpl::ImplData::TexBatchKind::None && b.tex != 0 && !b.verts.empty());
	if (!hadWork) return;
	++d->stat_texBatch_flushes;
	switch (reason) {
	case TexBatchFlushReason::KeyClip: ++d->stat_texBatch_flush_keyClip; break;
	case TexBatchFlushReason::KeyColor: ++d->stat_texBatch_flush_keyColor; break;
	case TexBatchFlushReason::KeyTex: ++d->stat_texBatch_flush_keyTex; break;
	case TexBatchFlushReason::KeyKind: ++d->stat_texBatch_flush_keyKind; break;
	case TexBatchFlushReason::Ordering: ++d->stat_texBatch_flush_ordering; break;
	case TexBatchFlushReason::ClipChange: ++d->stat_texBatch_flush_clipChange; break;
	case TexBatchFlushReason::Present: ++d->stat_texBatch_flush_present; break;
	default: break;
	}
}

static void textured_batch_flush_with_reason(OpenGLDrawerImpl::ImplData* d, TexBatchFlushReason reason) {
	textured_batch_note_flush(d, reason);
	textured_batch_flush(d);
}

static void textured_batch_flush(OpenGLDrawerImpl::ImplData* d) {
	if (!d) return;
	auto& b = d->texBatch;
	if (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::None || b.tex == 0 || b.verts.empty()) {
		textured_batch_reset(d);
		return;
	}

	// Apply scissor from batch snapshot (do NOT rely on current d->clip).
	gl_apply_clip_rect(d, b.hasClip, b.clip);

	// Issue one draw (pick shader by kind)
	unsigned int prog = (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Text) ? d->progTexText : d->progTex;
	int uSampler = (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Text) ? d->uTexSamplerText : d->uTexSampler;
	int uColor = (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Text) ? d->uTexColorText : d->uTexColor;
	gl_bind_program(d, prog);
	if (uSampler >= 0) glUniform1i(uSampler, 0);
	if (uColor >= 0) glUniform4f(uColor, b.color[0], b.color[1], b.color[2], b.color[3]);
	gl_bind_vao(d, d->vao);
	gl_bind_tex0(d, b.tex);
	ensure_textured_vbo_capacity(d, b.verts.size());
	gl_bind_array_buffer(d, d->vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, b.verts.size() * sizeof(float), b.verts.data());
	++d->stat_bufferSubData;

	if (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Text) {
		++d->stat_bufferSubData_text;
	} else if (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Image) {
		++d->stat_bufferSubData_image;
	}

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(b.verts.size() / 4));
	++d->stat_drawCalls;
	if (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Text) {
		++d->stat_drawCalls_text;
		++d->stat_text_batches;
		d->stat_text_glyphs += b.textGlyphs;
	} else if (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Image) {
		++d->stat_drawCalls_image;
		d->stat_image_quads += b.imageQuads;
	}

	textured_batch_reset(d);
}

static void textured_batch_begin(OpenGLDrawerImpl::ImplData* d,
	OpenGLDrawerImpl::ImplData::TexBatchKind kind,
	unsigned int tex,
	const float color[4]) {
	if (!d) return;
	auto& b = d->texBatch;
	bool clipMatch = (b.hasClip == d->hasClip) && (!b.hasClip || rect_exact_equal(b.clip, d->clip));
	bool colorMatch = (b.color[0] == color[0] && b.color[1] == color[1] && b.color[2] == color[2] && b.color[3] == color[3]);
	bool keyMatch = (b.kind == kind) && (b.tex == tex) && clipMatch && colorMatch;
	if (b.kind != OpenGLDrawerImpl::ImplData::TexBatchKind::None && !keyMatch) {
		// Record the primary mismatch reason (best-effort)
		if (b.kind != kind) {
			textured_batch_flush_with_reason(d, TexBatchFlushReason::KeyKind);
		}
		else if (b.tex != tex) {
			textured_batch_flush_with_reason(d, TexBatchFlushReason::KeyTex);
		}
		else if (!clipMatch) {
			textured_batch_flush_with_reason(d, TexBatchFlushReason::KeyClip);
		}
		else if (!colorMatch) {
			textured_batch_flush_with_reason(d, TexBatchFlushReason::KeyColor);
		}
		else {
			textured_batch_flush_with_reason(d, TexBatchFlushReason::Unknown);
		}
	}
	if (b.kind == OpenGLDrawerImpl::ImplData::TexBatchKind::None) {
		b.kind = kind;
		b.tex = tex;
		b.color[0] = color[0];
		b.color[1] = color[1];
		b.color[2] = color[2];
		b.color[3] = color[3];
		b.hasClip = d->hasClip;
		b.clip = d->clip;
		if (b.verts.capacity() == 0) b.verts.reserve(6 * 4 * 256);
	}
}

static void textured_batch_append_quad(OpenGLDrawerImpl::ImplData* d,
	OpenGLDrawerImpl::ImplData::TexBatchKind kind,
	unsigned int tex,
	const float color[4],
	float x, float y, float w, float h,
	float u0, float v0, float u1, float v1) {
	if (!d || tex == 0) return;
	textured_batch_begin(d, kind, tex, color);
	auto& b = d->texBatch;
	b.verts.insert(b.verts.end(), {
		x, y, u0, v0,
		x + w, y, u1, v0,
		x + w, y + h, u1, v1,
		x + w, y + h, u1, v1,
		x, y + h, u0, v1,
		x, y, u0, v0
	});
	if (kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Text) {
		++b.textGlyphs;
	} else if (kind == OpenGLDrawerImpl::ImplData::TexBatchKind::Image) {
		++b.imageQuads;
	}
}

static void rect_batch_reset(OpenGLDrawerImpl::ImplData* d) {
	if (!d) return;
	auto& b = d->rectBatch;
	b.active = false;
	b.hasClip = false;
	b.clip = { 0,0,0,0 };
	b.verts.clear();
	b.rects = 0;
}

static void rect_batch_begin(OpenGLDrawerImpl::ImplData* d) {
	if (!d) return;
	auto& b = d->rectBatch;
	bool clipMatch = (b.hasClip == d->hasClip) && (!b.hasClip || rect_exact_equal(b.clip, d->clip));
	if (b.active && !clipMatch) {
		rect_batch_flush(d);
	}
	if (!b.active) {
		b.active = true;
		b.hasClip = d->hasClip;
		b.clip = d->clip;
		if (b.verts.capacity() == 0) b.verts.reserve(6 * 12 * 256);
	}
}

static void rect_batch_append(OpenGLDrawerImpl::ImplData* d,
	float x, float y, float w, float h,
	float cornerRadius, float strokeWidth,
	const float color[4]) {
	if (!d) return;
	if (w <= 0.0f || h <= 0.0f) return;
	if (color[3] <= 0.0f) return;
	rect_batch_begin(d);
	auto& b = d->rectBatch;
	auto pushVert = [&](float px, float py) {
		b.verts.insert(b.verts.end(), {
			px, py,
			x, y, w, h,
			color[0], color[1], color[2], color[3],
			cornerRadius, strokeWidth
		});
	};
	pushVert(x, y);
	pushVert(x + w, y);
	pushVert(x + w, y + h);
	pushVert(x + w, y + h);
	pushVert(x, y + h);
	pushVert(x, y);
	++b.rects;
}

static void rect_batch_flush(OpenGLDrawerImpl::ImplData* d) {
	if (!d) return;
	auto& b = d->rectBatch;
	if (!b.active || b.verts.empty()) {
		rect_batch_reset(d);
		return;
	}

	// Apply scissor from batch snapshot.
	gl_apply_clip_rect(d, b.hasClip, b.clip);

	constexpr int kFloatsPerVert = 12;
	GLsizei vertCount = (GLsizei)(b.verts.size() / kFloatsPerVert);
	if (vertCount <= 0) {
		rect_batch_reset(d);
		return;
	}

	gl_bind_program(d, d->progColor);
	gl_bind_vao(d, d->vaoColor);
	ensure_rect_vbo_capacity(d, b.verts.size());
	gl_bind_array_buffer(d, d->vboColor);
	glBufferSubData(GL_ARRAY_BUFFER, 0, b.verts.size() * sizeof(float), b.verts.data());
	++d->stat_bufferSubData;
	++d->stat_bufferSubData_rect;

	glDrawArrays(GL_TRIANGLES, 0, vertCount);
	++d->stat_drawCalls;
	++d->stat_drawCalls_rect;

	rect_batch_reset(d);
}

static OpenGLDrawerImpl::ImplData::FontAtlasPage createFontAtlasPage(int w, int h) {
	OpenGLDrawerImpl::ImplData::FontAtlasPage page;
	page.width = w;
	page.height = h;
	page.penX = 1;
	page.penY = 1;
	page.rowH = 0;

	glGenTextures(1, &page.tex);
	glBindTexture(GL_TEXTURE_2D, page.tex);

	// Allocate single-channel texture
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

#if !defined(__ANDROID__)
	// Make sampling return (1,1,1,red) so the desktop text shader can read alpha directly.
	GLint swizzle[4] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
#endif

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);
	return page;
}

#if LDT_RENDER_VALIDATE
static void gl_check_error(const char* label) {
	for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
		LDT_ERROR("[GL] error=0x" << std::hex << (unsigned int)err << std::dec << " at " << (label ? label : "(null)"));
	}
}
#else
static void gl_check_error(const char*) {}
#endif

static void gl_bind_program(OpenGLDrawerImpl::ImplData* d, unsigned int prog) {
	if (!d) return;
	if (d->gl.program == prog) return;
	glUseProgram(prog);
	d->gl.program = prog;
	if (prog != 0) ++d->stat_useProgram;
}

static void gl_bind_vao(OpenGLDrawerImpl::ImplData* d, unsigned int vao) {
	if (!d) return;
	if (d->gl.vao == vao) return;
	glBindVertexArray(vao);
	d->gl.vao = vao;
	if (vao != 0) ++d->stat_vaoBinds;
}

static void gl_bind_array_buffer(OpenGLDrawerImpl::ImplData* d, unsigned int buf) {
	if (!d) return;
	if (d->gl.arrayBuffer == buf) return;
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	d->gl.arrayBuffer = buf;
}

static void gl_bind_tex0(OpenGLDrawerImpl::ImplData* d, unsigned int tex) {
	if (!d) return;
	if (d->gl.tex0 == tex) return;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	d->gl.tex0 = tex;
	if (tex != 0) ++d->stat_textureBinds;
}

static void gl_apply_scissor(OpenGLDrawerImpl::ImplData* d, bool enable, int x, int y, int w, int h) {
	if (!d) return;
	if (enable != d->gl.scissorEnabled) {
		if (enable) glEnable(GL_SCISSOR_TEST);
		else glDisable(GL_SCISSOR_TEST);
		d->gl.scissorEnabled = enable;
	}
	if (!enable) return;
	if (x == d->gl.scissorX && y == d->gl.scissorY && w == d->gl.scissorW && h == d->gl.scissorH) return;
	glScissor(x, y, w, h);
	d->gl.scissorX = x;
	d->gl.scissorY = y;
	d->gl.scissorW = w;
	d->gl.scissorH = h;
}

static void gl_apply_sdf_clip(OpenGLDrawerImpl::ImplData* d, unsigned int prog,
                              int uSdfClipLoc, int uSdfClipRadiusLoc,
                              bool hasClip, const render::Rect& clip, float cornerRadius) {
	if (!d || uSdfClipLoc < 0 || uSdfClipRadiusLoc < 0) return;
	if (!hasClip || cornerRadius <= 0.0f) {
		// Disable SDF clip: pass zero-size rect
		glUniform4f(uSdfClipLoc, 0.0f, 0.0f, 0.0f, 0.0f);
		glUniform1f(uSdfClipRadiusLoc, 0.0f);
		return;
	}
	// Convert top-left clip (DIP) to bottom-left window pixels (matching gl_FragCoord)
	float sx = clip.x * d->scaleX;
	float sy = d->height - (clip.y + clip.h) * d->scaleY;
	float sw = clip.w * d->scaleX;
	float sh = clip.h * d->scaleY;
	if (sw < 0.0f) sw = 0.0f;
	if (sh < 0.0f) sh = 0.0f;
	glUniform4f(uSdfClipLoc, sx, sy, sw, sh);
	glUniform1f(uSdfClipRadiusLoc, cornerRadius * d->scaleX); // scale radius to pixels
}

static void gl_apply_clip_rect(OpenGLDrawerImpl::ImplData* d, bool hasClip, const render::Rect& clip) {
	if (!d) return;
	if (!hasClip) {
		gl_apply_scissor(d, false, 0, 0, 0, 0);
		// Also clear SDF clip on all programs
		unsigned int prevProg = d->gl.program;
		if (d->uSdfClip_color >= 0) {
			gl_bind_program(d, d->progColor);
			gl_apply_sdf_clip(d, d->progColor, d->uSdfClip_color, d->uSdfClipRadius_color, false, clip, 0.0f);
		}
		if (d->uSdfClip_tex >= 0) {
			gl_bind_program(d, d->progTex);
			gl_apply_sdf_clip(d, d->progTex, d->uSdfClip_tex, d->uSdfClipRadius_tex, false, clip, 0.0f);
		}
		if (d->uSdfClip_texText >= 0) {
			gl_bind_program(d, d->progTexText);
			gl_apply_sdf_clip(d, d->progTexText, d->uSdfClip_texText, d->uSdfClipRadius_texText, false, clip, 0.0f);
		}
		gl_bind_program(d, prevProg);
		return;
	}
	// convert top-left clip (DIP) to bottom-left scissor (pixels)
	int sx = (int)(clip.x * d->scaleX);
	int sy = (int)(d->height - (clip.y + clip.h) * d->scaleY);
	int sw = (int)(clip.w * d->scaleX);
	int sh = (int)(clip.h * d->scaleY);
	if (sw < 0) sw = 0;
	if (sh < 0) sh = 0;
	gl_apply_scissor(d, true, sx, sy, sw, sh);

	// Apply SDF clip on all programs for rounded corners
	float cr = d->clipCornerRadius;
	unsigned int prevProg = d->gl.program;
	if (d->uSdfClip_color >= 0) {
		gl_bind_program(d, d->progColor);
		gl_apply_sdf_clip(d, d->progColor, d->uSdfClip_color, d->uSdfClipRadius_color, true, clip, cr);
	}
	if (d->uSdfClip_tex >= 0) {
		gl_bind_program(d, d->progTex);
		gl_apply_sdf_clip(d, d->progTex, d->uSdfClip_tex, d->uSdfClipRadius_tex, true, clip, cr);
	}
	if (d->uSdfClip_texText >= 0) {
		gl_bind_program(d, d->progTexText);
		gl_apply_sdf_clip(d, d->progTexText, d->uSdfClip_texText, d->uSdfClipRadius_texText, true, clip, cr);
	}
	gl_bind_program(d, prevProg);
}

static void gl_apply_clip(OpenGLDrawerImpl::ImplData* d) {
	if (!d) return;
	gl_apply_clip_rect(d, d->hasClip, d->clip);
}

static unsigned int compileShader(unsigned int type, const char* src) {
	unsigned int id = glCreateShader(type);
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);
	int success;
	glGetShaderiv(id, GL_COMPILE_STATUS, &success);
	if (!success) {
		char info[512]; glGetShaderInfoLog(id, 512, nullptr, info);
		std::string msg = "Shader compile error: ";
		msg += info;
		glDeleteShader(id);
		throw std::runtime_error(msg);
	}
	return id;
}
static unsigned int createProgram(const char* vs, const char* fs) {
	unsigned int vsId = compileShader(GL_VERTEX_SHADER, vs);
	unsigned int fsId = compileShader(GL_FRAGMENT_SHADER, fs);
	unsigned int prog = glCreateProgram();
	glAttachShader(prog, vsId);
	glAttachShader(prog, fsId);
	glLinkProgram(prog);
	int success; glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		char info[512]; glGetProgramInfoLog(prog, 512, nullptr, info);
		std::string msg = "Program link error: ";
		msg += info;
		glDeleteProgram(prog);
		glDeleteShader(vsId);
		glDeleteShader(fsId);
		throw std::runtime_error(msg);
	}
	glDeleteShader(vsId);
	glDeleteShader(fsId);
	return prog;
}

#if defined(__ANDROID__)
static const char* vsColorSrc = R"(
#version 300 es
layout(location = 0) in vec2 aPos;   // pixel coords
layout(location = 1) in vec4 aRect;  // x,y,w,h
layout(location = 2) in vec4 aColor; // rgba
layout(location = 3) in vec2 aParams; // cornerRadius, strokeWidth

out vec2 vPos;
out vec4 vRect;
out vec4 vColor;
out vec2 vParams;

uniform mat4 uProj;
void main() {
	vPos = aPos;
	vRect = aRect;
	vColor = aColor;
	vParams = aParams;
	gl_Position = uProj * vec4(aPos.xy, 0.0, 1.0);
}
)";
static const char* fsColorSrc = R"(
#version 300 es
precision mediump float;
in vec2 vPos;
in vec4 vRect;
in vec4 vColor;
in vec2 vParams;
out vec4 FragColor;

// vParams.x = cornerRadius, vParams.y = strokeWidth

uniform vec4 uSdfClip;       // x,y,w,h in window pixels (z<=0 || w<=0 means no clip)
uniform float uSdfClipRadius;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
	vec2 halfSize = vRect.zw * 0.5;
	vec2 center = vRect.xy + halfSize;
	float r = clamp(vParams.x, 0.0, min(halfSize.x, halfSize.y));
	float strokeW = vParams.y;
    
    // Signed distance (negative inside, positive outside)
    float d = sdRoundedBox(vPos - center, halfSize, r);
    
    // Anti-aliasing range (approx 1 pixel)
    float aa = 0.5;
    
    // Outer shape alpha: 1 inside, 0 outside
    float alpha = 1.0 - smoothstep(-aa, aa, d);
    
    // Inner cut-out for hollow border
	if (strokeW > 0.0) {
		// Cut out everything where d < -strokeW
		// We want alpha=0 when d < -strokeW
		// We want alpha=1 when d > -strokeW
		float alphaInner = smoothstep(-strokeW - aa, -strokeW + aa, d);
        alpha *= alphaInner;
    }

    // SDF clip: discard fragments outside the rounded clip rect
    if (uSdfClip.z > 0.0 && uSdfClip.w > 0.0) {
        vec2 clipHalf = uSdfClip.zw * 0.5;
        vec2 clipCenter = uSdfClip.xy + clipHalf;
        float cr = clamp(uSdfClipRadius, 0.0, min(clipHalf.x, clipHalf.y));
        float cd = sdRoundedBox(gl_FragCoord.xy - clipCenter, clipHalf, cr);
        if (cd > 0.0) discard;
    }

	FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)";

static const char* vsTexSrc = R"(
#version 300 es
layout(location = 0) in vec2 aPos; // pixel coords
layout(location = 1) in vec2 aUV;
out vec2 vUV;
uniform mat4 uProj;
void main() {
    vUV = aUV;
    gl_Position = uProj * vec4(aPos.xy, 0.0, 1.0);
}
)";
static const char* fsTexSrc = R"(
#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec4 uColor;

uniform vec4 uSdfClip;       // x,y,w,h in window pixels (z<=0 || w<=0 means no clip)
uniform float uSdfClipRadius;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec4 sampled = texture(uTex, vUV);
    // Modulate texture color with uniform color
    // For text (white texture with alpha), this results in uColor with alpha from texture.
    // For images, this results in image color tinted by uColor (usually white).
    vec4 color = sampled * uColor;

    // SDF clip: discard fragments outside the rounded clip rect
    if (uSdfClip.z > 0.0 && uSdfClip.w > 0.0) {
        vec2 clipHalf = uSdfClip.zw * 0.5;
        vec2 clipCenter = uSdfClip.xy + clipHalf;
        float cr = clamp(uSdfClipRadius, 0.0, min(clipHalf.x, clipHalf.y));
        float cd = sdRoundedBox(gl_FragCoord.xy - clipCenter, clipHalf, cr);
        if (cd > 0.0) discard;
    }

    FragColor = color;
}
)";

static const char* fsTextSrc = R"(
#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec4 uColor;

uniform vec4 uSdfClip;       // x,y,w,h in window pixels (z<=0 || w<=0 means no clip)
uniform float uSdfClipRadius;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
	float a = texture(uTex, vUV).r;
	vec4 color = vec4(uColor.rgb, uColor.a * a);

    // SDF clip: discard fragments outside the rounded clip rect
    if (uSdfClip.z > 0.0 && uSdfClip.w > 0.0) {
        vec2 clipHalf = uSdfClip.zw * 0.5;
        vec2 clipCenter = uSdfClip.xy + clipHalf;
        float cr = clamp(uSdfClipRadius, 0.0, min(clipHalf.x, clipHalf.y));
        float cd = sdRoundedBox(gl_FragCoord.xy - clipCenter, clipHalf, cr);
        if (cd > 0.0) discard;
    }

	FragColor = color;
}
)";
#else
static const char* vsColorSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;   // pixel coords
layout(location = 1) in vec4 aRect;  // x,y,w,h
layout(location = 2) in vec4 aColor; // rgba
layout(location = 3) in vec2 aParams; // cornerRadius, strokeWidth

out vec2 vPos;
out vec4 vRect;
out vec4 vColor;
out vec2 vParams;

uniform mat4 uProj;
void main() {
	vPos = aPos;
	vRect = aRect;
	vColor = aColor;
	vParams = aParams;
	gl_Position = uProj * vec4(aPos.xy, 0.0, 1.0);
}
)";
static const char* fsColorSrc = R"(
#version 330 core
in vec2 vPos;
in vec4 vRect;
in vec4 vColor;
in vec2 vParams;
out vec4 FragColor;

// vParams.x = cornerRadius, vParams.y = strokeWidth

uniform vec4 uSdfClip;       // x,y,w,h in window pixels (z<=0 || w<=0 means no clip)
uniform float uSdfClipRadius;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
	vec2 halfSize = vRect.zw * 0.5;
	vec2 center = vRect.xy + halfSize;
	float r = clamp(vParams.x, 0.0, min(halfSize.x, halfSize.y));
	float strokeW = vParams.y;
    
    // Signed distance (negative inside, positive outside)
    float d = sdRoundedBox(vPos - center, halfSize, r);
    
    // Anti-aliasing range (approx 1 pixel)
    float aa = 0.5;
    
    // Outer shape alpha: 1 inside, 0 outside
    float alpha = 1.0 - smoothstep(-aa, aa, d);
    
    // Inner cut-out for hollow border
	if (strokeW > 0.0) {
		// Cut out everything where d < -strokeW
		// We want alpha=0 when d < -strokeW
		// We want alpha=1 when d > -strokeW
		float alphaInner = smoothstep(-strokeW - aa, -strokeW + aa, d);
        alpha *= alphaInner;
    }

    // SDF clip: discard fragments outside the rounded clip rect
    if (uSdfClip.z > 0.0 && uSdfClip.w > 0.0) {
        vec2 clipHalf = uSdfClip.zw * 0.5;
        vec2 clipCenter = uSdfClip.xy + clipHalf;
        float cr = clamp(uSdfClipRadius, 0.0, min(clipHalf.x, clipHalf.y));
        float cd = sdRoundedBox(gl_FragCoord.xy - clipCenter, clipHalf, cr);
        if (cd > 0.0) discard;
    }

	FragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)";

static const char* vsTexSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos; // pixel coords
layout(location = 1) in vec2 aUV;
out vec2 vUV;
uniform mat4 uProj;
void main() {
    vUV = aUV;
    gl_Position = uProj * vec4(aPos.xy, 0.0, 1.0);
}
)";
static const char* fsTexSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec4 uColor;

uniform vec4 uSdfClip;       // x,y,w,h in window pixels (z<=0 || w<=0 means no clip)
uniform float uSdfClipRadius;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec4 sampled = texture(uTex, vUV);
    // Modulate texture color with uniform color
    // For text (white texture with alpha), this results in uColor with alpha from texture.
    // For images, this results in image color tinted by uColor (usually white).
    vec4 color = sampled * uColor;

    // SDF clip: discard fragments outside the rounded clip rect
    if (uSdfClip.z > 0.0 && uSdfClip.w > 0.0) {
        vec2 clipHalf = uSdfClip.zw * 0.5;
        vec2 clipCenter = uSdfClip.xy + clipHalf;
        float cr = clamp(uSdfClipRadius, 0.0, min(clipHalf.x, clipHalf.y));
        float cd = sdRoundedBox(gl_FragCoord.xy - clipCenter, clipHalf, cr);
        if (cd > 0.0) discard;
    }

    FragColor = color;
}
)";

static const char* fsTextSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec4 uColor;

uniform vec4 uSdfClip;       // x,y,w,h in window pixels (z<=0 || w<=0 means no clip)
uniform float uSdfClipRadius;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
	// Font atlas texture uses swizzle so that coverage ends up in alpha.
	float a = texture(uTex, vUV).a;
	vec4 color = vec4(uColor.rgb, uColor.a * a);

    // SDF clip: discard fragments outside the rounded clip rect
    if (uSdfClip.z > 0.0 && uSdfClip.w > 0.0) {
        vec2 clipHalf = uSdfClip.zw * 0.5;
        vec2 clipCenter = uSdfClip.xy + clipHalf;
        float cr = clamp(uSdfClipRadius, 0.0, min(clipHalf.x, clipHalf.y));
        float cd = sdRoundedBox(gl_FragCoord.xy - clipCenter, clipHalf, cr);
        if (cd > 0.0) discard;
    }

	FragColor = color;
}
)";
#endif

static void create1x1white(unsigned int* outTex) {
	unsigned int tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	unsigned char white[4] = { 255,255,255,255 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	*outTex = tex;
}

OpenGLDrawerImpl::OpenGLDrawerImpl() { d = new ImplData(); }
OpenGLDrawerImpl::~OpenGLDrawerImpl() { shutdown(); delete d; d = nullptr; }

static void ortho_proj(float width, float height, float out[16]) {
	float L = 0.0f, R = width, B = height, T = 0.0f;
	// map pixels (0..width, 0..height) with origin top-left to NDC
	// we compute matrix that takes pixel coords where y increases downward:
	// First map x: [0,width] -> [-1,1]; y: [0,height] -> [1,-1] (invert y)
	// Column-major
	out[0] = 2.0f / (R - L); out[1] = 0; out[2] = 0; out[3] = 0;
	out[4] = 0; out[5] = 2.0f / (T - B); out[6] = 0; out[7] = 0;
	out[8] = 0; out[9] = 0; out[10] = -1.0f; out[11] = 0;
	out[12] = -(R + L) / (R - L); out[13] = -(T + B) / (T - B); out[14] = 0; out[15] = 1.0f;
}

#else // LDT_ENABLE_RENDERING

#include <iostream>

// No-op implementations when rendering is disabled
OpenGLDrawerImpl::OpenGLDrawerImpl() {}
OpenGLDrawerImpl::~OpenGLDrawerImpl() {}
void OpenGLDrawerImpl::init(int, int) {}
void OpenGLDrawerImpl::resize(const ldt::FramebufferSizePx&, const ldt::ContentScale&) {}
void OpenGLDrawerImpl::clear(float, float, float, float) {}
void OpenGLDrawerImpl::drawRect(float, float, float, float, const DrawStyle&) {}
void OpenGLDrawerImpl::drawRoundedRect(float, float, float, float, float, float, const DrawStyle&) {}
void OpenGLDrawerImpl::drawImage(ImageHandle, float, float, float, float, const DrawStyle&) {}
void OpenGLDrawerImpl::drawText(const std::string&, float, float, const Font&, const DrawStyle&, float, const TextLayoutParams&) {}
TextMetrics OpenGLDrawerImpl::measureText(const std::u32string&, const FontDesc&, float) { return TextMetrics(); }
TextMetrics OpenGLDrawerImpl::measureText(const std::string&, const FontDesc&, float) { return TextMetrics(); }
float OpenGLDrawerImpl::getLineHeight(const FontDesc&) { return 0.0f; }
ImageHandle OpenGLDrawerImpl::createImageFromMemory(const unsigned char*, int, int, int) { return (ImageHandle)0; }
void OpenGLDrawerImpl::unloadImage(ImageHandle) {}
void OpenGLDrawerImpl::setClipRect(const render::Rect&) {}
bool OpenGLDrawerImpl::intersectClipRect(const render::Rect&, render::Rect*) { return false; }
bool OpenGLDrawerImpl::getClipRect(render::Rect*) const { return false; }
void OpenGLDrawerImpl::resetClip() {}
void OpenGLDrawerImpl::setClipCornerRadius(float) {}
float OpenGLDrawerImpl::getClipCornerRadius() const { return 0.0f; }
void OpenGLDrawerImpl::pushTransform(float, float) {}
void OpenGLDrawerImpl::popTransform() {}
void OpenGLDrawerImpl::present() {}
void OpenGLDrawerImpl::shutdown() {}

#endif // LDT_ENABLE_RENDERING

#if defined(LDT_ENABLE_RENDERING)

void OpenGLDrawerImpl::init(int width, int height) {
	d->width = width; d->height = height;

	// Runtime override for stats (default off unless compiled with LDT_RENDER_STATS=1)
	// d->stat_enabled = ldt_env_truthy("LDT_RENDER_STATS", d->stat_enabled); // Removed environment dependency

	// create programs
	d->progColor = createProgram(vsColorSrc, fsColorSrc);
	d->uProj_color = glGetUniformLocation(d->progColor, "uProj");
	d->uColor = glGetUniformLocation(d->progColor, "uColor");
	d->uRect = glGetUniformLocation(d->progColor, "uRect");
	d->uCornerRadius = glGetUniformLocation(d->progColor, "uCornerRadius");
	d->uStrokeWidth = glGetUniformLocation(d->progColor, "uStrokeWidth");
	d->uSdfClip_color = glGetUniformLocation(d->progColor, "uSdfClip");
	d->uSdfClipRadius_color = glGetUniformLocation(d->progColor, "uSdfClipRadius");

	d->progTex = createProgram(vsTexSrc, fsTexSrc);
	d->uProj_tex = glGetUniformLocation(d->progTex, "uProj");
	d->uTexSampler = glGetUniformLocation(d->progTex, "uTex");
	d->uTexColor = glGetUniformLocation(d->progTex, "uColor");
	d->uSdfClip_tex = glGetUniformLocation(d->progTex, "uSdfClip");
	d->uSdfClipRadius_tex = glGetUniformLocation(d->progTex, "uSdfClipRadius");

	d->progTexText = createProgram(vsTexSrc, fsTextSrc);
	d->uProj_texText = glGetUniformLocation(d->progTexText, "uProj");
	d->uTexSamplerText = glGetUniformLocation(d->progTexText, "uTex");
	d->uTexColorText = glGetUniformLocation(d->progTexText, "uColor");
	d->uSdfClip_texText = glGetUniformLocation(d->progTexText, "uSdfClip");
	d->uSdfClipRadius_texText = glGetUniformLocation(d->progTexText, "uSdfClipRadius");

    // set initial projection once (avoid updating per-draw)
    {
        float proj[16];
        ortho_proj(d->width, d->height, proj);
		gl_bind_program(d, d->progColor);
        if (d->uProj_color >= 0) glUniformMatrix4fv(d->uProj_color, 1, GL_FALSE, proj);
		gl_bind_program(d, d->progTex);
        if (d->uProj_tex >= 0) glUniformMatrix4fv(d->uProj_tex, 1, GL_FALSE, proj);
		gl_bind_program(d, d->progTexText);
		if (d->uProj_texText >= 0) glUniformMatrix4fv(d->uProj_texText, 1, GL_FALSE, proj);
		gl_bind_program(d, 0);
    }

	// VAO/VBO for textured quad (pos + uv)
	glGenVertexArrays(1, &d->vao);
	glGenBuffers(1, &d->vbo);
	glBindVertexArray(d->vao);
	glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
	// reserve space: 6 verts * (2+2) floats
	d->vboFloatCapacity = 6 * 4;
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * d->vboFloatCapacity, nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glBindVertexArray(0);

	// VAO/VBO for rect batch (pos + rect + color + params)
	glGenVertexArrays(1, &d->vaoColor);
	glGenBuffers(1, &d->vboColor);
	glBindVertexArray(d->vaoColor);
	glBindBuffer(GL_ARRAY_BUFFER, d->vboColor);
	// initial reserve: 256 rects
	constexpr size_t kRectFloatsPerVert = 12;
	d->vboColorFloatCapacity = 6 * kRectFloatsPerVert * 256;
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * d->vboColorFloatCapacity, nullptr, GL_DYNAMIC_DRAW);
	GLsizei stride = (GLsizei)(kRectFloatsPerVert * sizeof(float));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
	glBindVertexArray(0);

	create1x1white(&d->whiteTex);

	// init FreeType
	if (FT_Init_FreeType(&d->ft)) {
		LDT_ERROR("Failed to init FreeType");
		d->ft = nullptr;
	}

	// Enable blending for proper glyph alpha compositing and general UI transparency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Ensure pixel storage alignment for glyph uploads (glyph rows are byte-aligned)
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// init stats timer
	d->stat_lastTime = std::chrono::steady_clock::now();
}

void OpenGLDrawerImpl::resize(const ldt::FramebufferSizePx& framebufferSize, const ldt::ContentScale& contentScale) {
	// preserve ordering: flush pending batches before changing projection
	rect_batch_flush(d);
	textured_batch_flush(d);
	const ldt::ContentScale safeScale = ldt::sanitizeScale(contentScale);
	d->width = framebufferSize.width; d->height = framebufferSize.height;
    d->scaleX = safeScale.x.value; d->scaleY = safeScale.y.value;
    
	// update projection uniforms to DIP coordinate system
    const ldt::ViewportSizeDp viewportSize = ldt::toViewportSizeDp(framebufferSize, safeScale);
    
	float proj[16];
	ortho_proj(viewportSize.width.value, viewportSize.height.value, proj);
	gl_bind_program(d, d->progColor);
	if (d->uProj_color >= 0) glUniformMatrix4fv(d->uProj_color, 1, GL_FALSE, proj);
	gl_bind_program(d, d->progTex);
	if (d->uProj_tex >= 0) glUniformMatrix4fv(d->uProj_tex, 1, GL_FALSE, proj);
	gl_bind_program(d, d->progTexText);
	if (d->uProj_texText >= 0) glUniformMatrix4fv(d->uProj_texText, 1, GL_FALSE, proj);
	gl_bind_program(d, 0);
	glViewport(0, 0, framebufferSize.width, framebufferSize.height);
}

void OpenGLDrawerImpl::clear(float r, float g, float b, float a) {
	// Reset GL state that affects clearing (scissor/FBO/viewport) before glClear.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
	//glViewport(0, 0, d->width, d->height);
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
	// External renderers (e.g., ImGui) may change GL state between frames.
	// Reset our state cache so subsequent draws rebind the correct state.
	d->gl.program = 0;
	d->gl.vao = 0;
	d->gl.arrayBuffer = 0;
	d->gl.tex0 = 0;
	d->gl.scissorEnabled = false;
	d->gl.scissorX = -1;
	d->gl.scissorY = -1;
	d->gl.scissorW = -1;
	d->gl.scissorH = -1;
	// Reset SDF clip state (will be re-set by next clip push if needed)
	d->clipCornerRadius = 0.0f;
}

void OpenGLDrawerImpl::drawRect(float x, float y, float w, float h, const DrawStyle& style) {
	x += d->offsetX;
	y += d->offsetY;
	// preserve ordering: flush any pending textured batch before rect batching
	textured_batch_flush(d);

	float corner = style.cornerRadius;
	float strokeAlpha = style.strokeColor.a * style.opacity;
	float fillAlpha = style.fillColor.a * style.opacity;

	if (style.strokeWidths.isUniform()) {
		float strokeW = style.strokeWidths.top;
		if (strokeW > 0.0f && strokeAlpha > 0.0f) {
			float strokeColor[4] = { style.strokeColor.r, style.strokeColor.g, style.strokeColor.b, strokeAlpha };
			rect_batch_append(d, x, y, w, h, corner, strokeW, strokeColor);

			float ix = x + strokeW;
			float iy = y + strokeW;
			float iw = w - 2.0f * strokeW;
			float ih = h - 2.0f * strokeW;
			if (iw > 0.0f && ih > 0.0f && fillAlpha > 0.0f) {
				float fillColor[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, fillAlpha };
				rect_batch_append(d, ix, iy, iw, ih, std::max(0.0f, corner - strokeW), 0.0f, fillColor);
			}
		}
		else {
			if (fillAlpha > 0.0f) {
				float fillColor[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, fillAlpha };
				rect_batch_append(d, x, y, w, h, corner, 0.0f, fillColor);
			}
		}
	}
	else {
		// Draw fill first
		if (fillAlpha > 0.0f) {
			float fillColor[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, fillAlpha };
			rect_batch_append(d, x, y, w, h, corner, 0.0f, fillColor);
		}

		// Draw strokes as separate rectangles (lines)
		if (strokeAlpha > 0.0f) {
			float strokeColor[4] = { style.strokeColor.r, style.strokeColor.g, style.strokeColor.b, strokeAlpha };

			// Top
			if (style.strokeWidths.top > 0)
				rect_batch_append(d, x, y, w, style.strokeWidths.top, 0, 0, strokeColor);
			// Right
			if (style.strokeWidths.right > 0)
				rect_batch_append(d, x + w - style.strokeWidths.right, y, style.strokeWidths.right, h, 0, 0, strokeColor);
			// Bottom
			if (style.strokeWidths.bottom > 0)
				rect_batch_append(d, x, y + h - style.strokeWidths.bottom, w, style.strokeWidths.bottom, 0, 0, strokeColor);
			// Left
			if (style.strokeWidths.left > 0)
				rect_batch_append(d, x, y, style.strokeWidths.left, h, 0, 0, strokeColor);
		}
	}
}

void OpenGLDrawerImpl::drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style) {
	x += d->offsetX;
	y += d->offsetY;
	// preserve ordering: flush any pending textured batch before rect batching
	textured_batch_flush_with_reason(d, TexBatchFlushReason::Ordering);
	// Use corner radius from style when available
	float corner = style.cornerRadius;
	if (corner <= 0.0f) corner = std::max(rx, ry);

	float strokeW = style.strokeWidths.top;
	float strokeAlpha = style.strokeColor.a * style.opacity;
	if (strokeW > 0.0f && strokeAlpha > 0.0f) {
		float strokeColor[4] = { style.strokeColor.r, style.strokeColor.g, style.strokeColor.b, strokeAlpha };
		rect_batch_append(d, x, y, w, h, corner, strokeW, strokeColor);

		float ix = x + strokeW;
		float iy = y + strokeW;
		float iw = w - 2.0f * strokeW;
		float ih = h - 2.0f * strokeW;
		float fillAlpha = style.fillColor.a * style.opacity;
		if (iw > 0.0f && ih > 0.0f && fillAlpha > 0.0f) {
			float fillColor[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, fillAlpha };
			rect_batch_append(d, ix, iy, iw, ih, std::max(0.0f, corner - strokeW), 0.0f, fillColor);
		}
	}
	else {
		float fillAlpha = style.fillColor.a * style.opacity;
		if (fillAlpha > 0.0f) {
			float fillColor[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, fillAlpha };
			rect_batch_append(d, x, y, w, h, corner, 0.0f, fillColor);
		}
	}

}

void OpenGLDrawerImpl::drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style) {
	x += d->offsetX;
	y += d->offsetY;
	// preserve ordering: flush any pending rect batch before textured batching
	rect_batch_flush(d);
	// defer to textured batch (keeps ordering via flush points)
	// bind texture
	unsigned int texID = (unsigned int)img;
	if (texID == 0) texID = d->whiteTex;
	float finalAlpha = style.fillColor.a * style.opacity;
	float color[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, finalAlpha };
	textured_batch_append_quad(d, ImplData::TexBatchKind::Image, texID, color, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f);

}

void OpenGLDrawerImpl::present() {
	// flush any pending batched rects before presenting
	rect_batch_flush(d);
	// flush any pending batched textured quads before presenting
	textured_batch_flush_with_reason(d, TexBatchFlushReason::Present);
	// update frame counter and (optionally) print stats once per second
	d->stat_frames++;
	auto now = std::chrono::steady_clock::now();
	// if (ldt_env_truthy("LDT_RENDER_GLFLUSH", false)) { glFlush(); } // Removed environment dependency
	gl_check_error("present(begin)");

	if (!d->stat_enabled) return;

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - d->stat_lastTime).count();
	if (ms >= 1000) {
		float frames = (d->stat_frames > 0) ? (float)d->stat_frames : 1.0f;
		LDT_LOG("[RenderStats] frames=" << d->stat_frames
			<< " drawCalls=" << d->stat_drawCalls
			<< " (rect=" << d->stat_drawCalls_rect
			<< " image=" << d->stat_drawCalls_image
			<< " text=" << d->stat_drawCalls_text
			<< ")"
			<< " imageQuads=" << d->stat_image_quads
			<< " texBatchFlush=" << d->stat_texBatch_flushes
			<< " (keyClip=" << d->stat_texBatch_flush_keyClip
			<< " keyColor=" << d->stat_texBatch_flush_keyColor
			<< " keyTex=" << d->stat_texBatch_flush_keyTex
			<< " keyKind=" << d->stat_texBatch_flush_keyKind
			<< " ordering=" << d->stat_texBatch_flush_ordering
			<< " clip=" << d->stat_texBatch_flush_clipChange
			<< " present=" << d->stat_texBatch_flush_present
			<< ")"
			<< " texBinds=" << d->stat_textureBinds
			<< " bufSubData=" << d->stat_bufferSubData
			<< " (rect=" << d->stat_bufferSubData_rect
			<< " image=" << d->stat_bufferSubData_image
			<< " text=" << d->stat_bufferSubData_text
			<< ")"
			<< " useProgram=" << d->stat_useProgram
			<< " vaoBinds=" << d->stat_vaoBinds
			<< " textBatches=" << d->stat_text_batches
			<< " textGlyphs=" << d->stat_text_glyphs
			<< " avgDraw/frame=" << (d->stat_drawCalls / frames)
			<< " avgTexBind/frame=" << (d->stat_textureBinds / frames));

		d->stat_drawCalls = 0;
		d->stat_drawCalls_rect = 0;
		d->stat_drawCalls_image = 0;
		d->stat_drawCalls_text = 0;
		d->stat_image_quads = 0;
		d->stat_texBatch_flushes = 0;
		d->stat_texBatch_flush_keyClip = 0;
		d->stat_texBatch_flush_keyColor = 0;
		d->stat_texBatch_flush_keyTex = 0;
		d->stat_texBatch_flush_keyKind = 0;
		d->stat_texBatch_flush_ordering = 0;
		d->stat_texBatch_flush_clipChange = 0;
		d->stat_texBatch_flush_present = 0;
		d->stat_textureBinds = 0;
		d->stat_bufferSubData = 0;
		d->stat_bufferSubData_rect = 0;
		d->stat_bufferSubData_image = 0;
		d->stat_bufferSubData_text = 0;
		d->stat_useProgram = 0;
		d->stat_vaoBinds = 0;
		d->stat_text_batches = 0;
		d->stat_text_glyphs = 0;
		d->stat_frames = 0;
		d->stat_lastTime = now;
	}
}

void OpenGLDrawerImpl::shutdown() {
	// delete GL resources
	if (!d) return;

	if (d->progColor) { glDeleteProgram(d->progColor); d->progColor = 0; }
	if (d->progTex) { glDeleteProgram(d->progTex); d->progTex = 0; }
	if (d->progTexText) { glDeleteProgram(d->progTexText); d->progTexText = 0; }
	if (d->vbo) { glDeleteBuffers(1, &d->vbo); d->vbo = 0; }
	if (d->vao) { glDeleteVertexArrays(1, &d->vao); d->vao = 0; }
	if (d->vboColor) { glDeleteBuffers(1, &d->vboColor); d->vboColor = 0; }
	if (d->vaoColor) { glDeleteVertexArrays(1, &d->vaoColor); d->vaoColor = 0; }
	if (d->whiteTex) { glDeleteTextures(1, &d->whiteTex); d->whiteTex = 0; }

	// FreeType cleanup
	if (d->ft) {
		for (auto& kv : d->atlasPages) {
			for (auto& page : kv.second) {
				if (page.tex) glDeleteTextures(1, &page.tex);
				page.tex = 0;
			}
		}
		for (auto& f : d->faceCache) {
			if (f.second) {
				FT_Done_Face(f.second);
			}
		}
		d->atlasPages.clear();
		FT_Done_FreeType(d->ft);
		d->ft = nullptr;
	}
}

static unsigned int createGlyphTextureFromBitmap(unsigned char* buffer, int w, int h) {
	unsigned int tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	// Ensure proper alignment for single-channel glyph data
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// upload as RGBA (expand single-channel to alpha) so shader can sample .a
	std::vector<unsigned char> rgba;
	rgba.resize(w * h * 4);
	for (int i = 0; i < w * h; ++i) {
		unsigned char v = buffer[i];
		rgba[i * 4 + 0] = 255;
		rgba[i * 4 + 1] = 255;
		rgba[i * 4 + 2] = 255;
		rgba[i * 4 + 3] = v;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// restore default alignment if desired (optional)
	// glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

// use utf8 utilities in misc/utf8util.h

static void ensureGlyph(OpenGLDrawerImpl::ImplData* d, const OpenGLDrawerImpl::ImplData::FontKey& key, uint32_t c) {
	// load face if needed
	auto itFace = d->faceCache.find(key);
	if (itFace == d->faceCache.end()) {
		FT_Face face;

		// 尝试使用 ResourceManager 获取字体路径
		std::string fontPath;
		const FontResource* fontRes = ResourceManager::getInstance().getFontByName(key.family);

		if (fontRes) {
			// 使用缓存的字体路径
			fontPath = fontRes->filePath;
			LDT_LOG("[OpenGLDrawer] Loading font '" << key.family
				<< "' from ResourceManager: " << fontPath);
		}
		else {
			// 回退到直接使用 key.family 作为路径（保持向后兼容）
			fontPath = key.family;
			LDT_LOG("[OpenGLDrawer] Font '" << key.family
				<< "' not found in ResourceManager, using as direct path");
		}

		if (FT_New_Face(d->ft, fontPath.c_str(), 0, &face)) {
			LDT_ERROR("Failed to load font face: " << fontPath);
			return;
		}
		FT_Set_Pixel_Sizes(face, 0, (int)(key.size * key.scale));
		d->faceCache[key] = face;
	}
	// check glyph exists
	auto& glyphMap = d->glyphCache[key];
	if (glyphMap.find(c) != glyphMap.end()) return;

	FT_Face face = d->faceCache[key];
	if (!face) return;

	if (FT_Load_Char(face, (FT_ULong)c, FT_LOAD_RENDER)) {
		LDT_ERROR("Failed to load glyph: " << c);
		return;
	}
	FT_GlyphSlot g = face->glyph;
	int pixelW = g->bitmap.width;
	int pixelH = g->bitmap.rows;

	Glyph gg;
	gg.width = (float)pixelW / key.scale;
	gg.height = (float)pixelH / key.scale;
	gg.bearingX = (float)g->bitmap_left / key.scale;
	gg.bearingY = (float)g->bitmap_top / key.scale;
	gg.advance = (float)g->advance.x / 64.0f / key.scale; // convert 1/64 pixels to DIP units
	if (pixelW > 0 && pixelH > 0 && g->bitmap.buffer) {
		// Pack into atlas page (simple shelf packing)
		constexpr int kAtlasW = 1024;
		constexpr int kAtlasH = 1024;
		constexpr int kPad = 1;

		auto& pages = d->atlasPages[key];
		if (pages.empty() || pages.back().tex == 0) {
			pages.push_back(createFontAtlasPage(kAtlasW, kAtlasH));
		}

		auto tryPack = [&](OpenGLDrawerImpl::ImplData::FontAtlasPage& page) -> bool {
			// new row if needed
			if (page.penX + pixelW + kPad > page.width) {
				page.penX = kPad;
				page.penY += page.rowH + kPad;
				page.rowH = 0;
			}
			// new page if no space
			if (page.penY + pixelH + kPad > page.height) {
				return false;
			}

			int x = page.penX;
			int y = page.penY;
			page.penX += pixelW + kPad;
			page.rowH = std::max<int>(page.rowH, pixelH);

			// upload bitmap as GL_RED
			gl_bind_tex0(d, page.tex); // Use wrapper to track state
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, pixelW, pixelH, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
			// Do NOT unbind here. Let the state tracker handle it later.
			// glBindTexture(GL_TEXTURE_2D, 0);

			gg.tex = page.tex;
			gg.u0 = (float)x / (float)page.width;
			gg.v0 = (float)y / (float)page.height;
			gg.u1 = (float)(x + pixelW) / (float)page.width;
			gg.v1 = (float)(y + pixelH) / (float)page.height;
			return true;
		};

		if (!tryPack(pages.back())) {
			pages.push_back(createFontAtlasPage(kAtlasW, kAtlasH));
			// If it still doesn't fit (very large glyph), just drop it.
			if (!tryPack(pages.back())) {
				gg.tex = 0;
			}
		}
	}
	glyphMap[c] = gg;
}

void OpenGLDrawerImpl::drawText(const std::string& text, float x, float y, const Font& font, const DrawStyle& style, float maxWidth, const TextLayoutParams& layout) {
	x += d->offsetX;
	y += d->offsetY;
	// preserve ordering: flush any pending rect batch before textured batching
	rect_batch_flush(d);
	if (!d->ft) return;
	ImplData::FontKey key{ font.family, (int)font.size, d->scaleX };
	// Note: we batch textured quads; actual GL draw happens at state changes or present()

	// Build lines first (operate on UTF-32 codepoints for proper Unicode handling)
	std::u32string u32text = utf8_to_utf32(text);
	std::vector<std::u32string> lines;
	std::vector<float> lineWidths;
	std::u32string cur;
	float curWidth = 0.0f;
	auto appendLine = [&]() {
		if (cur.empty()) return;
		lines.push_back(cur);
		lineWidths.push_back(curWidth);
		cur.clear(); curWidth = 0.0f;
	};

	for (char32_t ch : u32text) {
		if (ch == U'\n') {
			appendLine();
			continue;
		}
		ensureGlyph(d, key, (uint32_t)ch);
		auto& glyphMap = d->glyphCache[key];
		auto git = glyphMap.find((uint32_t)ch);
		if (git == glyphMap.end()) continue;
		const Glyph& g = git->second;
		float advancePx = g.advance;
		// wrapping decision: only wrap when layout permits wrapping
		if (layout.wrap && maxWidth > 0.0f && !cur.empty() && (curWidth + advancePx) > maxWidth) {
			appendLine();
		}
		cur.push_back(ch);
		curWidth += advancePx;
	}
	appendLine();

	// determine actual line height
	float lineH = (layout.lineHeight > 0.0f) ? layout.lineHeight : font.size * 1.2f;
	float baseline = y + font.size;

	// draw each line with alignment
	const float finalAlpha = style.fillColor.a * style.opacity;
	float color[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, finalAlpha };

	for (size_t i = 0; i < lines.size(); ++i) {
		const std::u32string& ln = lines[i];
		float lw = lineWidths[i];
		// available width for alignment: only consider maxWidth when wrapping is enabled
		float avail = (/*layout.wrap &&*/ maxWidth > 0.0f) ? maxWidth : lw;
		float lineX = x;
		std::string align = layout.textAlign;
		for (auto& c : align) c = static_cast<char>(::tolower(c));
		if (align == "center") {
			lineX = x + (avail - lw) * 0.5f;
		}
		else if (align == "right") {
			lineX = x + (avail - lw);
		}

		float pen = lineX;
		for (char32_t ch : ln) {
			auto git = d->glyphCache[key].find((uint32_t)ch);
			if (git == d->glyphCache[key].end()) continue;
			const Glyph& g = git->second;
			float gx = pen + g.bearingX;
			float gy = baseline - g.bearingY;

			if (g.tex) {
				textured_batch_append_quad(d, ImplData::TexBatchKind::Text, g.tex, color, gx, gy, g.width, g.height, g.u0, g.v0, g.u1, g.v1);
			}

			pen += g.advance;
		}

		baseline += lineH;
	}
	// keep program/vao/scissor cached; actual draw is deferred
}




// use utf8/utf32 helpers from misc/utf8util.h

TextMetrics OpenGLDrawerImpl::measureText(const std::u32string& text, const FontDesc& fontDesc, float wrapWidth) {
	return measureText(utf32_to_utf8(text), fontDesc, wrapWidth);
}

TextMetrics OpenGLDrawerImpl::measureText(const std::string& utf8text, const FontDesc& fontDesc, float wrapWidth) {
	TextMetrics m;
	// map FontDesc -> Font
	Font f;
	f.family = fontDesc.family;
	f.size = fontDesc.size;

	if (!d->ft) return m;

	ImplData::FontKey key{ f.family.empty() ? ResourceManager::getInstance().getDefaultFontName() : f.family, (int)f.size, d->scaleX };
	if (d->faceCache.find(key) == d->faceCache.end()) {
		FT_Face face;
		std::string fontPath;
		const FontResource* fontRes = ResourceManager::getInstance().getFontByName(key.family);
		if (fontRes) fontPath = fontRes->filePath; else fontPath = key.family;
		if (FT_New_Face(d->ft, fontPath.c_str(), 0, &face) == 0) {
			FT_Set_Pixel_Sizes(face, 0, (int)(key.size * key.scale));
			d->faceCache[key] = face;
		}
	}
	auto it = d->faceCache.find(key);
	if (it == d->faceCache.end() || !it->second) return m;

	FT_Face face = it->second;
	if (!face->size) return m;

	FT_Size_Metrics metrics = face->size->metrics;
	m.ascent = (float)(metrics.ascender >> 6) / d->scaleX;
	m.descent = (float)(-(metrics.descender >> 6)) / d->scaleX;
	float singleLineHeight = (float)(metrics.height >> 6) / d->scaleX;
	if (singleLineHeight <= 0.0f) singleLineHeight = f.size * 1.2f;

	float maxW = 0.0f;
	float curW = 0.0f;
	int lines = 1;

	// operate on UTF-32 codepoints to avoid iterating raw bytes
	std::u32string u32text = utf8_to_utf32(utf8text);
	for (char32_t ch : u32text) {
		if (ch == U'\n') {
			if (curW > maxW) maxW = curW;
			curW = 0.0f;
			lines++;
			continue;
		}
		if (FT_Load_Char(face, (FT_ULong)ch, FT_LOAD_DEFAULT)) continue;
		float advance = (float)face->glyph->advance.x / 64.0f / d->scaleX;

		if (wrapWidth > 0.0f && curW + advance > wrapWidth && curW > 0.0f) {
			if (curW > maxW) maxW = curW;
			curW = advance;
			lines++;
		} else {
			curW += advance;
		}
	}
	if (curW > maxW) maxW = curW;

	m.width = maxW;
	m.lineHeight = lines * singleLineHeight;

	return m;
}

float OpenGLDrawerImpl::getLineHeight(const FontDesc& fontDesc) {
	// prefer FreeType metrics when available
	if (d->ft) {
		ImplData::FontKey key{ fontDesc.family.empty() ? ResourceManager::getInstance().getDefaultFontName() : fontDesc.family, (int)fontDesc.size, d->scaleX };
		if (d->faceCache.find(key) == d->faceCache.end()) {
			FT_Face face;
			std::string fontPath;
			const FontResource* fontRes = ResourceManager::getInstance().getFontByName(key.family);
			if (fontRes) fontPath = fontRes->filePath; else fontPath = key.family;
			if (FT_New_Face(d->ft, fontPath.c_str(), 0, &face) == 0) {
				FT_Set_Pixel_Sizes(face, 0, (int)(key.size * key.scale));
				d->faceCache[key] = face;
			}
		}
		auto it = d->faceCache.find(key);
		if (it != d->faceCache.end() && it->second && it->second->size) {
			return (float)(it->second->size->metrics.height >> 6) / d->scaleX;
		}
	}
	// fallback heuristic
	return fontDesc.size * 1.2f;
}

ImageHandle OpenGLDrawerImpl::createImageFromMemory(const unsigned char* data, int width, int height, int channels) {
	if (!data || width <= 0 || height <= 0) return 0;

	unsigned int tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLenum format = GL_RGBA;
	if (channels == 3) format = GL_RGB;
	else if (channels == 1) format = GL_RED;

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	return (ImageHandle)tex;
}

void OpenGLDrawerImpl::unloadImage(ImageHandle img) {
	if (img != 0) {
		unsigned int tex = (unsigned int)img;
		glDeleteTextures(1, &tex);
	}
}

// clip helpers
void OpenGLDrawerImpl::setClipRect(const render::Rect& r) {
	// If clip doesn't change, avoid unnecessary flush/state churn.
	if (d->hasClip && rect_exact_equal(d->clip, r)) return;
	// preserve ordering: flush batches before changing clip state
	rect_batch_flush(d);
	textured_batch_flush_with_reason(d, TexBatchFlushReason::ClipChange);
	d->clip = r;
	d->hasClip = true;
}
bool OpenGLDrawerImpl::intersectClipRect(const render::Rect& r, render::Rect* outNewClip) {
	// Transform input rect (local) to screen space
	render::Rect rScreen = r;
	rScreen.x += d->offsetX;
	rScreen.y += d->offsetY;

	render::Rect result;
	if (!d->hasClip) {
		result = rScreen;
	}
	else {
		float nx = (std::max)(d->clip.x, rScreen.x);
		float ny = (std::max)(d->clip.y, rScreen.y);
		float nr = (std::min)(d->clip.x + d->clip.w, rScreen.x + rScreen.w);
		float nb = (std::min)(d->clip.y + d->clip.h, rScreen.y + rScreen.h);
		if (nr <= nx || nb <= ny) {
			// 交集为空 → 设 clip 为零矩形，后续绘制全部被裁剪
			result.x = 0; result.y = 0; result.w = 0; result.h = 0;
			if (outNewClip) *outNewClip = result;
			rect_batch_flush(d);
			textured_batch_flush_with_reason(d, TexBatchFlushReason::ClipChange);
			d->clip = result;
			d->hasClip = true;
			return false;
		}
		result.x = nx; result.y = ny; result.w = nr - nx; result.h = nb - ny;
	}
	if (outNewClip) *outNewClip = result;
	// If clip doesn't change, avoid unnecessary flush/state churn.
	if (d->hasClip && rect_exact_equal(d->clip, result)) return true;
	// preserve ordering: flush batches before changing clip state
	rect_batch_flush(d);
	textured_batch_flush_with_reason(d, TexBatchFlushReason::ClipChange);
	d->clip = result;
	d->hasClip = true;
	return true;
}
bool OpenGLDrawerImpl::getClipRect(render::Rect* outCurrent) const {
	if (!d->hasClip) return false;
	if (outCurrent) *outCurrent = d->clip;
	return true;
}
void OpenGLDrawerImpl::resetClip() {
	if (!d->hasClip) return;
	// preserve ordering: flush batches before changing clip state
	rect_batch_flush(d);
	textured_batch_flush_with_reason(d, TexBatchFlushReason::ClipChange);
	d->hasClip = false;
}

void OpenGLDrawerImpl::setClipCornerRadius(float radius) {
	if (!d) return;
	if (d->clipCornerRadius == radius) return;
	// Flush pending batches before changing SDF clip uniforms
	rect_batch_flush(d);
	textured_batch_flush_with_reason(d, TexBatchFlushReason::ClipChange);
	d->clipCornerRadius = radius;
	// Uniforms will be set by gl_apply_clip_rect when batches are flushed
}

float OpenGLDrawerImpl::getClipCornerRadius() const {
	return d ? d->clipCornerRadius : 0.0f;
}

void OpenGLDrawerImpl::pushTransform(float dx, float dy) {
	d->transformStack.push_back({ d->offsetX, d->offsetY });
	d->offsetX += dx;
	d->offsetY += dy;
}

void OpenGLDrawerImpl::popTransform() {
	if (!d->transformStack.empty()) {
		auto prev = d->transformStack.back();
		d->transformStack.pop_back();
		d->offsetX = prev.first;
		d->offsetY = prev.second;
	}
	else {
		d->offsetX = 0.0f;
		d->offsetY = 0.0f;
	}
}



class OpenGLTextLayout : public ITextLayout {
public:
	struct GlyphQuad {
		unsigned int textureID;
		float x, y, w, h;
		float u0, v0, u1, v1;
		int originalIndex;
	};

	std::string m_text;
	std::u32string m_codepoints;
	render::Rect m_bounds{ 0,0,0,0 };
	std::vector<GlyphQuad> m_glyphs;
	std::vector<render::Rect> m_charRects;
	std::vector<int> m_lineBreaks;
	float m_lineHeight = 12.0f; // Default fallback

	render::Rect getBounds() const override { return m_bounds; }
	std::string getText() const override { return m_text; }
	int getLineCount() const override { return (int)m_lineBreaks.size(); }

	int getCharIndexAt(float x, float y) const override {
		if (m_charRects.empty()) return 0;

		int bestLineStart = 0;
		int bestLineEnd = (int)m_charRects.size();

		// Find line
		for (size_t i = 0; i < m_lineBreaks.size(); ++i) {
			int start = m_lineBreaks[i];
			int end = (i + 1 < m_lineBreaks.size()) ? m_lineBreaks[i + 1] : (int)m_charRects.size();
			if (start >= (int)m_charRects.size()) continue;

			const render::Rect& r = m_charRects[start];
			// Check vertical range (approximate using max height of line or just check if y is near)
			// Since we don't store line height explicitly per line, we assume standard flow.
			// But we can check the rect of the first char.
			// Better: check if y < next line's y.
			
			float lineY = r.y;
			float nextLineY = lineY + r.h; // approximate
			if (i + 1 < m_lineBreaks.size() && m_lineBreaks[i+1] < (int)m_charRects.size()) {
				nextLineY = m_charRects[m_lineBreaks[i+1]].y;
			} else {
				nextLineY = lineY + r.h * 1.5f; // last line
			}

			if (y >= lineY && y < nextLineY) {
				bestLineStart = start;
				bestLineEnd = end;
				break;
			}
			if (y >= nextLineY) {
				// keep looking, but default to last line if we run out
				bestLineStart = start;
				bestLineEnd = end;
			}
		}

		// Find char in line
		for (int i = bestLineStart; i < bestLineEnd; ++i) {
			if (i >= (int)m_charRects.size()) break;
			const render::Rect& r = m_charRects[i];
			float centerX = r.x + r.w * 0.5f;
			if (x < centerX) {
				return i;
			}
		}

		// If we are here, x is to the right of all characters in this line.
		// If the line ends with a newline, return the index of the newline (cursor before newline).
		if (bestLineEnd > bestLineStart) {
			int lastCharIdx = bestLineEnd - 1;
			if (lastCharIdx < (int)m_codepoints.size() && m_codepoints[lastCharIdx] == U'\n') {
				return lastCharIdx;
			}
		}

		return bestLineEnd;
	}

	render::Rect getCursorRect(int index) const override {
		if (m_charRects.empty()) return { 0,0,2.0f,m_lineHeight };
		if (index < 0) index = 0;
		
		if (index < (int)m_charRects.size()) {
			render::Rect r = m_charRects[index];
			return { r.x, r.y, 2.0f, r.h };
		}
		else {
			// After last char
			if (!m_charRects.empty()) {
				render::Rect r = m_charRects.back();
				// If the last character in text is a newline, the cursor should be on the next line
				if (!m_codepoints.empty() && m_codepoints.back() == U'\n') {
					// Assuming left alignment for now as Input uses left.
					// X = 0 (relative to layout bounds)
					// Y = r.y + r.h (next line)
					return { 0.0f, r.y + r.h, 2.0f, r.h };
				}
				return { r.x + r.w, r.y, 2.0f, r.h };
			}
		}
		return { 0,0,2.0f,m_lineHeight };
	}

	std::vector<render::Rect> getSelectionRects(int startIndex, int endIndex) const override {
		std::vector<render::Rect> res;
		if (startIndex > endIndex) std::swap(startIndex, endIndex);
		if (startIndex < 0) startIndex = 0;
		if (endIndex > (int)m_charRects.size()) endIndex = (int)m_charRects.size();

		for (int i = startIndex; i < endIndex; ++i) {
			if (i >= (int)m_charRects.size()) break;
			const render::Rect& r = m_charRects[i];

			if (!res.empty()) {
				render::Rect& last = res.back();
				// Merge if same line (y matches) and adjacent
				if (std::abs(last.y - r.y) < 1.0f && std::abs((last.x + last.w) - r.x) < 1.0f) {
					last.w += r.w;
					last.h = std::max(last.h, r.h);
					continue;
				}
			}
			res.push_back(r);
		}
		return res;
	}
};

std::shared_ptr<ITextLayout> OpenGLDrawerImpl::createTextLayout(const std::string& text, const Font& font, const TextLayoutParams& layout, float maxWidth) {
	auto impl = std::make_shared<OpenGLTextLayout>();
	impl->m_text = text;
	
	// Initialize line height with provided params or font size, ensuring valid cursor logic even if text is empty or font fails
	impl->m_lineHeight = (layout.lineHeight > 0.0f) ? layout.lineHeight : ((font.size > 0.0f) ? font.size * 1.2f : 12.0f);

	if (!d->ft) return impl;
	ImplData::FontKey key{ font.family, (int)font.size, d->scaleX };

	// Ensure font loaded
	if (d->faceCache.find(key) == d->faceCache.end()) {
		FT_Face face;
		std::string fontPath;
		const FontResource* fontRes = ResourceManager::getInstance().getFontByName(key.family);
		
		// Fallback logic: if font not found, try default or first available
		if (!fontRes) {
			std::string defName = ResourceManager::getInstance().getDefaultFontName();
			if (!defName.empty()) {
				fontRes = ResourceManager::getInstance().getFontByName(defName);
			}
			if (!fontRes) {
				auto names = ResourceManager::getInstance().getAllFontNames();
				if (!names.empty()) {
					fontRes = ResourceManager::getInstance().getFontByName(names[0]);
				}
			}
		}

		if (fontRes) fontPath = fontRes->filePath; else fontPath = key.family;
		if (FT_New_Face(d->ft, fontPath.c_str(), 0, &face) == 0) {
			FT_Set_Pixel_Sizes(face, 0, (int)(key.size * key.scale));
			d->faceCache[key] = face;
		}
	}
	auto itFace = d->faceCache.find(key);
	if (itFace == d->faceCache.end() || !itFace->second) return impl;
	FT_Face face = itFace->second;

	// Metrics
	float defaultSingleLineHeight = (float)(face->size->metrics.height >> 6) / d->scaleX;
	if (defaultSingleLineHeight <= 0.0f) defaultSingleLineHeight = font.size * 1.2f;
	float lineHeight = (layout.lineHeight > 0.0f) ? layout.lineHeight : defaultSingleLineHeight;
	float ascent = (float)(face->size->metrics.ascender >> 6) / d->scaleX;
	
	// Layout state
	float curX = 0.0f;
	float curY = 0.0f;
	float maxLineW = 0.0f;

	// Decode UTF-8 into codepoints while keeping byte offsets (use helper in utf8util)
	std::vector<int> cpByteIdx; // starting byte index for each codepoint
	std::u32string cps = utf8_to_utf32_with_offsets(text, cpByteIdx);

	// Pre-calculate lines for wrapping based on codepoints (use codepoint indices)
	struct LineSpec { int startCP; int lengthCP; float width; };
	std::vector<LineSpec> lines;

	int currentLineStartCP = 0;
	float currentLineWidth = 0.0f;

	auto getAdvanceCP = [&](uint32_t cp) -> float {
		if (cp == '\r') return 0.0f;
		ensureGlyph(d, key, cp);
		auto& gm = d->glyphCache[key];
		auto git = gm.find(cp);
		if (git != gm.end()) return git->second.advance;
		return 0.0f;
	};

	for (int ci = 0; ci < (int)cps.size(); ++ci) {
		char32_t cp = cps[ci];
		if (cp == U'\n') {
			// end current line (exclude newline from line content)
			lines.push_back({ currentLineStartCP, ci - currentLineStartCP, currentLineWidth });
			currentLineStartCP = ci + 1; // skip the newline codepoint
			currentLineWidth = 0.0f;
			continue;
		}
		float adv = getAdvanceCP((uint32_t)cp);
		if (layout.wrap && maxWidth > 0.0f && currentLineWidth + adv > maxWidth && currentLineWidth > 0.0f) {
			// wrap: end current line before this codepoint and reprocess it
			lines.push_back({ currentLineStartCP, ci - currentLineStartCP, currentLineWidth });
			currentLineStartCP = ci;
			currentLineWidth = 0.0f;
			--ci; // re-process this cp in new line
			continue;
		}
		currentLineWidth += adv;
	}
	// last line - from currentLineStartCP to end (may be empty)
	lines.push_back({ currentLineStartCP, (int)cps.size() - currentLineStartCP, currentLineWidth });

	// Generate geometry. m_charRects will be per-codepoint so caretPos_ will be codepoint index.
	impl->m_codepoints.assign(cps.begin(), cps.end());
	impl->m_charRects.resize(cps.size());
	impl->m_lineBreaks.reserve(lines.size());

	for (const auto& ln : lines) {
		// line spec already uses codepoint indices
		int startCP = ln.startCP;
		impl->m_lineBreaks.push_back(startCP);

		// Alignment
		float lineX = 0.0f;
		float avail = (maxWidth > 0.0f) ? maxWidth : ln.width;
		std::string align = layout.textAlign;
		for (auto& c : align) c = static_cast<char>(::tolower(c));
		if (align == "center") lineX = (avail - ln.width) * 0.5f;
		else if (align == "right") lineX = (avail - ln.width);

		float penX = lineX;


		// Walk codepoints that fall within this line range (by cp indices)
		int endCP = ln.startCP + ln.lengthCP;
		int ci = startCP;
		for (; ci < endCP && ci < (int)cps.size(); ++ci) {
			char32_t cp = cps[ci];
			if (cp == U'\n') {
				// newline gets a zero-width rect at current penX
				if (ci < (int)impl->m_charRects.size()) impl->m_charRects[ci] = { penX, curY, 0.0f, lineHeight };
				continue;
			}

			float adv = getAdvanceCP((uint32_t)cp);

			// Store char rect for this codepoint
			if (ci >= 0 && ci < (int)impl->m_charRects.size()) {
				impl->m_charRects[ci] = { penX, curY, adv, lineHeight };
			}

			// Generate glyph quad if available
			auto& gm = d->glyphCache[key];
			auto git = gm.find((uint32_t)cp);
			if (git != gm.end() && git->second.tex) {
				const Glyph& g = git->second;
				float gx = penX + g.bearingX;
				float gy = curY + ascent - g.bearingY;

				OpenGLTextLayout::GlyphQuad quad;
				quad.textureID = g.tex;
				quad.x = gx;
				quad.y = gy;
				quad.w = (float)g.width;
				quad.h = (float)g.height;
				quad.u0 = g.u0; quad.v0 = g.v0;
				quad.u1 = g.u1; quad.v1 = g.v1;
				quad.originalIndex = (int)ci;
				impl->m_glyphs.push_back(quad);
			}

			penX += adv;
		}

		// If the immediate next codepoint after the line is a newline, mark it with zero-width rect
		int newlineCpIdx = ln.startCP + ln.lengthCP;
		if (newlineCpIdx < (int)cps.size() && cps[newlineCpIdx] == U'\n') {
			if (newlineCpIdx < (int)impl->m_charRects.size()) {
				impl->m_charRects[newlineCpIdx] = { penX, curY, 0.0f, lineHeight };
			}
		}

		if (ln.width > maxLineW) maxLineW = ln.width;
		curY += lineHeight;
	}

	impl->m_bounds = { 0, 0, maxLineW, curY };
	return impl;
}

void OpenGLDrawerImpl::drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style) {
	auto impl = std::dynamic_pointer_cast<OpenGLTextLayout>(layout);
	if (!impl) return;

	x += d->offsetX;
	y += d->offsetY;

	// Note: we batch textured quads; actual GL draw happens at state changes or present()
	// preserve ordering: flush any pending rect batch before textured batching
	rect_batch_flush(d);
	// Set per-quad uniform color via batch key
	float finalAlpha = style.fillColor.a * style.opacity;
	float color[4] = { style.fillColor.r, style.fillColor.g, style.fillColor.b, finalAlpha };

	for (const auto& g : impl->m_glyphs) {
		if (g.textureID == 0) continue;
		textured_batch_append_quad(d, ImplData::TexBatchKind::Text, g.textureID, color, x + g.x, y + g.y, g.w, g.h, g.u0, g.v0, g.u1, g.v1);
	}

	// keep program/vao/scissor cached
}

// end of file


#endif // LDT_ENABLE_RENDERING
