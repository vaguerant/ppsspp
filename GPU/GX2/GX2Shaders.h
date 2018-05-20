#pragma once

#include <wiiu/gx2/shaders.h>

#ifdef __cplusplus
extern "C" {
#endif

extern GX2VertexShader defVShaderGX2;
extern GX2PixelShader defPShaderGX2;
extern GX2PixelShader stencilUploadPSshaderGX2;

extern GX2VertexShader STVshaderGX2;
extern GX2PixelShader STPshaderGX2;

extern GX2PixelShader PUberShaderGX2;

extern GX2VertexShader ST_VShaderGX2;
extern GX2VertexShader TAL_VShaderGX2;

#ifdef __cplusplus
}
#endif