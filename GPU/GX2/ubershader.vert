#version 430
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define USE_HW_TRANSFORM
//#define FLAT_SHADE
//#define GPU_ROUND_DEPTH_TO_16BIT

layout(std140, set = 0, binding = 3) uniform baseVars
{
   mat4 proj_mtx;
   mat4 proj_through_mtx;
   mat3x4 view_mtx;
   mat3x4 world_mtx;
   mat3x4 tex_mtx;
   vec4 uvscaleoffset;
   vec4 depthRange;
   vec2 fogcoef;
   float stencilReplace;
   vec4 matambientalpha;
   uint spline_counts;
   uint depal_mask_shift_off_fmt;
   int pad2;
   int pad3;
   vec3 fogcolor;
   vec3 texenv;
   ivec4 alphacolorref;
   ivec4 alphacolormask;
   vec3 blendFixA;
   vec3 blendFixB;
   vec4 texclamp;
   vec2 texclampoff;
} base;

layout(std140, set = 0, binding = 4) uniform lightVars
{
   vec4 u_ambient;
   vec3 matdiffuse;
   vec4 matspecular;
   vec3 matemissive;
   vec3 pos[4];
   vec3 dir[4];
   vec3 att[4];
   vec2 angle_spotCoef[4];
   vec3 ambient[4];
   vec3 diffuse[4];
   vec3 specular[4];
} light;

layout(std140, set = 0, binding = 5) uniform boneVars{ mat3x4 m[8]; } bone;

layout(std140, set = 0, binding = 6) uniform UB_VSID
{
   bool VS_BIT_LMODE;
   bool VS_BIT_IS_THROUGH;
   bool VS_BIT_ENABLE_FOG;
   bool VS_BIT_HAS_COLOR;
   bool VS_BIT_DO_TEXTURE;
   bool VS_BIT_DO_TEXTURE_TRANSFORM;
   bool VS_BIT_USE_HW_TRANSFORM;
   bool VS_BIT_HAS_NORMAL;
   bool VS_BIT_NORM_REVERSE;
   bool VS_BIT_HAS_TEXCOORD;
   bool VS_BIT_HAS_COLOR_TESS;
   bool VS_BIT_HAS_TEXCOORD_TESS;
   bool VS_BIT_NORM_REVERSE_TESS;
   int  VS_BIT_UVGEN_MODE;
   int  VS_BIT_UVPROJ_MODE;
   int  VS_BIT_LS0;
   int  VS_BIT_LS1;
   int  VS_BIT_BONES;
   bool VS_BIT_ENABLE_BONES;
   int  VS_BIT_LIGHT0_COMP;
   int  VS_BIT_LIGHT0_TYPE;
   int  VS_BIT_LIGHT1_COMP;
   int  VS_BIT_LIGHT1_TYPE;
   int  VS_BIT_LIGHT2_COMP;
   int  VS_BIT_LIGHT2_TYPE;
   int  VS_BIT_LIGHT3_COMP;
   int  VS_BIT_LIGHT3_TYPE;
   int  VS_BIT_MATERIAL_UPDATE;
   bool VS_BIT_SPLINE;
   bool VS_BIT_LIGHT0_ENABLE;
   bool VS_BIT_LIGHT1_ENABLE;
   bool VS_BIT_LIGHT2_ENABLE;
   bool VS_BIT_LIGHT3_ENABLE;
   bool VS_BIT_LIGHTING_ENABLE;
   int  VS_BIT_WEIGHT_FMTSCALE;
   bool VS_BIT_FLATSHADE;
   bool VS_BIT_BEZIER;
};
#ifdef USE_HW_TRANSFORM
struct TessData
{
   vec4 pos;
   vec4 uv;
   vec4 color;
};
layout(std430, set = 0, binding = 8) buffer s_tess_data
{
   TessData data[];
} tess_data;
#endif
layout(location = 0) in vec4 position;
layout(location = 1) in vec3 texcoord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 w1;
layout(location = 4) in vec4 w2;
layout(location = 5) in vec4 color0;
layout(location = 6) in vec3 color1;

out gl_PerVertex { vec4 gl_Position; };

#define LIGHT_OFF 0
#define LIGHT_SHADE 1
#define LIGHT_FULL 2


#define GE_LIGHTTYPE_DIRECTIONAL 0
#define GE_LIGHTTYPE_POINT 1
#define GE_LIGHTTYPE_SPOT 2
#define GE_LIGHTTYPE_UNKNOWN 3
#define GE_LIGHTCOMP_ONLYDIFFUSE 0
#define GE_LIGHTCOMP_BOTH 1
#define GE_LIGHTCOMP_BOTHWITHPOWDIFFUSE 2

#define GE_TEXMAP_TEXTURE_COORDS 0
#define GE_TEXMAP_TEXTURE_MATRIX 1
#define GE_TEXMAP_ENVIRONMENT_MAP 2
#define GE_TEXMAP_UNKNOWN 3

#define GE_PROJMAP_POSITION 0
#define GE_PROJMAP_UV 1
#define GE_PROJMAP_NORMALIZED_NORMAL 2
#define GE_PROJMAP_NORMAL 3

#ifdef FLAT_SHADE
#define shading flat
#else
#define shading
#endif

layout(location = 0) out vec3 v_texcoord;
layout(location = 1) shading out vec4 v_color0;
layout(location = 2) shading out vec3 v_color1;
layout(location = 3) out float v_fogdepth;
#if defined(GPU_ROUND_DEPTH_TO_16BIT)
vec4 depthRoundZVP(vec4 v)
{
   float z = v.z / v.w;
   z = z * depthRange.x + depthRange.y;
   z = floor(z);
   z = (z - depthRange.z) * depthRange.w;
   return vec4(v.x, v.y, z * v.w, v.w);
}
#endif

vec2 tess_sample(in vec2 points[16], in vec2 weights[4])
{
   vec2 pos = vec2(0);
   for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
         pos = pos + weights[j].x * weights[i].y * points[i * 4 + j];
   return pos;
}
vec3 tess_sample(in vec3 points[16], in vec2 weights[4])
{
   vec3 pos = vec3(0);
   for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
         pos = pos + weights[j].x * weights[i].y * points[i * 4 + j];
   return pos;
}
vec4 tess_sample(in vec4 points[16], in vec2 weights[4])
{
   vec4 pos = vec4(0);
   for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
         pos = pos + weights[j].x * weights[i].y * points[i * 4 + j];
   return pos;
}

void spline_knot(ivec2 num_patches, ivec2 type, out vec2 knot[6], ivec2 patch_pos)
{
   for (int i = 0; i < 6; ++i)
      knot[i] = vec2(i + patch_pos.x - 2, i + patch_pos.y - 2);

   if ((type.x & 1) != 0)
   {
      if (patch_pos.x <= 2)
         knot[0].x = 0;
      if (patch_pos.x <= 1)
         knot[1].x = 0;
   }
   if ((type.x & 2) != 0)
   {
      if (patch_pos.x >= (num_patches.x - 2))
         knot[5].x = num_patches.x;
      if (patch_pos.x == (num_patches.x - 1))
         knot[4].x = num_patches.x;
   }
   if ((type.y & 1) != 0)
   {
      if (patch_pos.y <= 2)
         knot[0].y = 0;
      if (patch_pos.y <= 1)
         knot[1].y = 0;
   }
   if ((type.y & 2) != 0)
   {
      if (patch_pos.y >= (num_patches.y - 2))
         knot[5].y = num_patches.y;
      if (patch_pos.y == (num_patches.y - 1))
         knot[4].y = num_patches.y;
   }
}

void spline_weight(vec2 t, in vec2 knot[6], out vec2 weights[4])
{
   vec2 t0 = (t - knot[0]);
   vec2 t1 = (t - knot[1]);
   vec2 t2 = (t - knot[2]);
   vec2 f30 = t0 / (knot[3] - knot[0]);
   vec2 f41 = t1 / (knot[4] - knot[1]);
   vec2 f52 = t2 / (knot[5] - knot[2]);
   vec2 f31 = t1 / (knot[3] - knot[1]);
   vec2 f42 = t2 / (knot[4] - knot[2]);
   vec2 f32 = t2 / (knot[3] - knot[2]);
   vec2 a = (1 - f30) * (1 - f31);
   vec2 b = (f31 * f41);
   vec2 c = (1 - f41) * (1 - f42);
   vec2 d = (f42 * f52);
   weights[0] = a - (a * f32);
   weights[1] = 1 - a - b + ((a + b + c - 1) * f32);
   weights[2] = b + ((1 - b - c - d) * f32);
   weights[3] = d * f32;
}

void main()
{
   v_color1 = vec3(0.0);
#ifdef USE_HW_TRANSFORM
   if (VS_BIT_USE_HW_TRANSFORM)
   {
      vec4 col = color0;
      vec2 tex = texcoord.xy;
      vec3 worldpos;
      mediump vec3 worldnormal;
      // Step 1: World Transform / Skinning
      if (!VS_BIT_ENABLE_BONES)
      {
         if (VS_BIT_BEZIER || VS_BIT_SPLINE)
         {
            vec3 _pos[16];
            vec2 _tex[16];
            vec4 _col[16];
            int spline_count_u = int(base.spline_counts & 0xff);
            int spline_count_v = int((base.spline_counts >> 8) & 0xff);
            int num_patches_u = VS_BIT_BEZIER ? (spline_count_u - 1) / 3 : spline_count_u - 3;
            int u = int(mod(gl_InstanceIndex, num_patches_u));
            int v = gl_InstanceIndex / num_patches_u;
            ivec2 patch_pos = ivec2(u, v);
            for (int i = 0; i < 4; i++)
            {
               for (int j = 0; j < 4; j++)
               {
                  int idx = (i + v * (VS_BIT_BEZIER ? 3 : 1)) * spline_count_u + (j + u * (VS_BIT_BEZIER ? 3 : 1));
                  _pos[i * 4 + j] = tess_data.data[idx].pos.xyz;
                  if (VS_BIT_DO_TEXTURE && VS_BIT_HAS_TEXCOORD && VS_BIT_HAS_TEXCOORD_TESS)
                     _tex[i * 4 + j] = tess_data.data[idx].uv.xy;
                  if (VS_BIT_HAS_COLOR && VS_BIT_HAS_COLOR_TESS)
                     _col[i * 4 + j] = tess_data.data[idx].color;
               }
            }
            vec2 tess_pos = position.xy;
            vec2 weights[4];
            vec2 knots[6];
            if (VS_BIT_BEZIER)
            {
               // Bernstein 3D
               weights[0] = (1 - tess_pos) * (1 - tess_pos) * (1 - tess_pos);
               weights[1] = 3 * tess_pos * (1 - tess_pos) * (1 - tess_pos);
               weights[2] = 3 * tess_pos * tess_pos * (1 - tess_pos);
               weights[3] = tess_pos * tess_pos * tess_pos;
            }
            else     // Spline
            {
               ivec2 spline_num_patches = ivec2(spline_count_u - 3, spline_count_v - 3);
               int spline_type_u = int((base.spline_counts >> 16) & 0xff);
               int spline_type_v = int((base.spline_counts >> 24) & 0xff);
               ivec2 spline_type = ivec2(spline_type_u, spline_type_v);
               spline_knot(spline_num_patches, spline_type, knots, patch_pos);
               spline_weight(tess_pos + patch_pos, knots, weights);
            }
            vec3 pos = tess_sample(_pos, weights);
            if (VS_BIT_DO_TEXTURE && VS_BIT_HAS_TEXCOORD)
            {
               if (VS_BIT_HAS_TEXCOORD_TESS)
                  tex = tess_sample(_tex, weights);
               else
                  tex = tess_pos + patch_pos;
            }
            if (VS_BIT_HAS_COLOR)
            {
               if (VS_BIT_HAS_COLOR_TESS)
                  col = tess_sample(_col, weights);
               else
                  col = tess_data.data[0].color;
            }
            vec3 nrm;
            if (VS_BIT_HAS_NORMAL)
            {
               vec3 du, dv;
               // Curved surface is probably always need to compute normal(not sampling from control points)
               if (VS_BIT_BEZIER)
               {
                  // Bernstein derivative
                  vec2 bernderiv[4];
                  bernderiv[0] = -3 * (tess_pos - 1) * (tess_pos - 1);
                  bernderiv[1] = 9 * tess_pos * tess_pos - 12 * tess_pos + 3;
                  bernderiv[2] = 3 * (2 - 3 * tess_pos) * tess_pos;
                  bernderiv[3] = 3 * tess_pos * tess_pos;

                  vec2 bernderiv_u[4];
                  vec2 bernderiv_v[4];
                  for (int i = 0; i < 4; i++)
                  {
                     bernderiv_u[i] = vec2(bernderiv[i].x, weights[i].y);
                     bernderiv_v[i] = vec2(weights[i].x, bernderiv[i].y);
                  }

                  du = tess_sample(_pos, bernderiv_u);
                  dv = tess_sample(_pos, bernderiv_v);
               }
               else     // Spline
               {
                  vec2 tess_next_u = vec2(normal.x, 0);
                  vec2 tess_next_v = vec2(0, normal.y);
                  // Right
                  vec2 tess_pos_r = tess_pos + tess_next_u;
                  spline_weight(tess_pos_r + patch_pos, knots, weights);
                  vec3 pos_r = tess_sample(_pos, weights);
                  // Left
                  vec2 tess_pos_l = tess_pos - tess_next_u;
                  spline_weight(tess_pos_l + patch_pos, knots, weights);
                  vec3 pos_l = tess_sample(_pos, weights);
                  // Down
                  vec2 tess_pos_d = tess_pos + tess_next_v;
                  spline_weight(tess_pos_d + patch_pos, knots, weights);
                  vec3 pos_d = tess_sample(_pos, weights);
                  // Up
                  vec2 tess_pos_u = tess_pos - tess_next_v;
                  spline_weight(tess_pos_u + patch_pos, knots, weights);
                  vec3 pos_u = tess_sample(_pos, weights);

                  du = pos_r - pos_l;
                  dv = pos_d - pos_u;
               }
               nrm = normalize(cross(du, dv));
            }
            worldpos = vec4(pos.xyz, 1.0) * base.world_mtx;
            if (VS_BIT_HAS_NORMAL)
               worldnormal = normalize(vec4(VS_BIT_NORM_REVERSE_TESS ? -nrm : nrm, 0.0) * base.world_mtx);
            else
               worldnormal = vec3(0.0, 0.0, 1.0);
         }
         else
         {
            // No skinning, just standard T&L.
            worldpos = vec4(position.xyz, 1.0) * base.world_mtx;
            if (VS_BIT_HAS_NORMAL)
               worldnormal = normalize(vec4((VS_BIT_NORM_REVERSE ? -normal : normal), 0.0) * base.world_mtx);
            else
               worldnormal = vec3(0.0, 0.0, 1.0);
         }
      }
      else
      {
         float rescale[4] = { 1.0, 1.9921875, 1.999969482421875, 1.0 }; // 2*127.5f/128.f, 2*32767.5f/32768.f, 1.0f};
         float factor = rescale[VS_BIT_WEIGHT_FMTSCALE];
         float boneWeightAttr[8] = { w1.x, w1.y, w1.z, w1.w, w2.x, w2.y, w2.z, w2.w};

         mat3x4 skinMatrix = w1.x * bone.m[0];
         float numBoneWeights = 1 + VS_BIT_BONES;
         if (numBoneWeights > 1)
         {
            for (int i = 1; i < numBoneWeights; i++)
               skinMatrix += boneWeightAttr[i] * bone.m[i];
         }

         // Trying to simplify this results in bugs in LBP...
         vec3 skinnedpos = (vec4(position.xyz, 1.0) * skinMatrix) * factor;
         worldpos = vec4(skinnedpos, 1.0) * base.world_mtx;
         mediump vec3 skinnednormal;
         if (VS_BIT_HAS_NORMAL)
            skinnednormal = vec4((VS_BIT_NORM_REVERSE ? -normal : normal), 0.0) * skinMatrix * factor;
         else
            skinnednormal = vec4(0.0, 0.0, (VS_BIT_NORM_REVERSE ? -1.0 : 1.0), 0.0) * skinMatrix  * factor;

         worldnormal = normalize(vec4(skinnednormal, 0.0) * base.world_mtx);
      }

      vec4 viewPos = vec4(vec4(worldpos, 1.0) * base.view_mtx, 1.0);

      // Final view and projection transforms.
#ifdef GPU_ROUND_DEPTH_TO_16BIT
      gl_Position = depthRoundZVP(base.proj_mtx * viewPos);
#else
      gl_Position = base.proj_mtx * viewPos;
#endif

      // TODO: Declare variables for dots for shade mapping if needed
      vec4 ambient = base.matambientalpha;
      vec3 diffuse = light.matdiffuse;
      vec3 specular = light.matspecular.rgb;

      if (VS_BIT_HAS_COLOR)
      {
         if (bool(VS_BIT_MATERIAL_UPDATE & 1))
            ambient = col;

         if (bool(VS_BIT_MATERIAL_UPDATE & 2))
            diffuse = col.rgb;

         if (bool(VS_BIT_MATERIAL_UPDATE & 4))
            specular = col.rgb;
      }

      bool diffuseIsZero = true;
      bool specularIsZero = true;

      int light_types[4] = {VS_BIT_LIGHT0_TYPE, VS_BIT_LIGHT1_TYPE, VS_BIT_LIGHT2_TYPE, VS_BIT_LIGHT3_TYPE};
      int light_comps[4] = {VS_BIT_LIGHT0_COMP, VS_BIT_LIGHT1_COMP, VS_BIT_LIGHT2_COMP, VS_BIT_LIGHT3_COMP};
      bool lightEnable[4] = {VS_BIT_LIGHT0_ENABLE, VS_BIT_LIGHT1_ENABLE, VS_BIT_LIGHT2_ENABLE, VS_BIT_LIGHT3_ENABLE};
      int doLight[4] = { LIGHT_OFF, LIGHT_OFF, LIGHT_OFF, LIGHT_OFF };
      int shadeLight0 = (VS_BIT_UVGEN_MODE == GE_TEXMAP_ENVIRONMENT_MAP) ? VS_BIT_LS0 : -1;
      int shadeLight1 = (VS_BIT_UVGEN_MODE == GE_TEXMAP_ENVIRONMENT_MAP) ? VS_BIT_LS1 : -1;

      for (int i = 0; i < 4; i++)
      {
         if (i == shadeLight0 || i == shadeLight1)
            doLight[i] = LIGHT_SHADE;
         if (VS_BIT_LIGHTING_ENABLE && lightEnable[i])
            doLight[i] = LIGHT_FULL;
      }
      if (VS_BIT_LIGHTING_ENABLE)
      {
         for (int i = 0; i < 4; i++)
         {
            int type = light_types[i];
            int comp = light_comps[i];
            if (doLight[i] != LIGHT_FULL)
               continue;
            diffuseIsZero = false;
            if (comp != GE_LIGHTCOMP_ONLYDIFFUSE)
               specularIsZero = false;
         }
      }
      vec4 lightSum0 = light.u_ambient * ambient + vec4(light.matemissive, 0.0);
      vec3 lightSum1 = vec3(0.0);
      vec3 toLight;
      float distance;
      float lightScale;

      // Calculate lights if needed. If shade mapping is enabled, lights may need to be
      // at least partially calculated.
      for (int i = 0; i < 4; i++)
      {
         if (doLight[i] != LIGHT_FULL)
            continue;

         int type = light_types[i];
         int comp = light_comps[i];

         if (type == GE_LIGHTTYPE_DIRECTIONAL)
         {
            // We prenormalize light positions for directional lights.
            toLight = light.pos[i];
         }
         else
         {
            toLight = light.pos[i] - worldpos;
            distance = length(toLight);
            toLight /= distance;
         }

         mediump float doti = max(dot(toLight, worldnormal), 0.0);
         if (comp == GE_LIGHTCOMP_BOTHWITHPOWDIFFUSE)
         {
            // pow(0.0, 0.0) may be undefined, but the PSP seems to treat it as 1.0.
            // Seen in Tales of the World: Radiant Mythology (#2424.)
            if (doti == 0.0 && light.matspecular.a == 0.0)
               doti = 1.0;
            else
               doti = pow(doti, light.matspecular.a);
         }

         // Attenuation
         switch (type)
         {
         case GE_LIGHTTYPE_DIRECTIONAL:
            lightScale = 1.0;
            break;
         case GE_LIGHTTYPE_POINT:
            lightScale = clamp(1.0 / dot(light.att[i], vec3(1.0, distance, distance * distance)), 0.0, 1.0);
            break;
         case GE_LIGHTTYPE_SPOT:
         case GE_LIGHTTYPE_UNKNOWN:
            float anglei = dot(normalize(light.dir[i]), toLight);
            if (anglei >= light.angle_spotCoef[i].x)
            {
               lightScale = clamp(1.0 / dot(light.att[i], vec3(1.0, distance, distance * distance)), 0.0, 1.0) * pow(anglei,
                            light.angle_spotCoef[i].y);
            }
            else
               lightScale = 0.0;
            break;
         default:
            // ILLEGAL
            break;
         }

         diffuse = (light.diffuse[i] * diffuse) * doti;
         if (comp != GE_LIGHTCOMP_ONLYDIFFUSE)
         {
            doti = dot(normalize(toLight + vec3(0.0, 0.0, 1.0)), worldnormal);
            if (doti > 0.0)
               lightSum1 += light.specular[i] * specular * (pow(doti, light.matspecular.a) * lightScale);
         }
         lightSum0.rgb += (light.ambient[i] * ambient.rgb + diffuse) * lightScale;
      }

      if (VS_BIT_LIGHTING_ENABLE)
      {
         // Sum up ambient, emissive here.
         if (VS_BIT_LMODE)
         {
            v_color0 = clamp(lightSum0, 0.0, 1.0);
            if (!specularIsZero)
               v_color1 = clamp(lightSum1, 0.0, 1.0);
         }
         else
         {
            if (specularIsZero)
               v_color0 = clamp(lightSum0, 0.0, 1.0);
            else
               v_color0 = clamp(clamp(lightSum0, 0.0, 1.0) + vec4(lightSum1, 0.0), 0.0, 1.0);
         }
      }
      else
      {
         // Lighting doesn't affect color.
         if (VS_BIT_HAS_COLOR)
         {
            if (VS_BIT_BEZIER || VS_BIT_SPLINE)
               v_color0 = col;
            else
               v_color0 = color0;
         }
         else
            v_color0 = base.matambientalpha;
         if (VS_BIT_LMODE)
            v_color1 = vec3(0.0);
      }

      bool scaleUV = !VS_BIT_IS_THROUGH && (VS_BIT_UVGEN_MODE == GE_TEXMAP_TEXTURE_COORDS
                                            || VS_BIT_UVGEN_MODE == GE_TEXMAP_UNKNOWN);

      // Step 3: UV generation
      if (VS_BIT_DO_TEXTURE)
      {
         switch (VS_BIT_UVGEN_MODE)
         {
         case GE_TEXMAP_TEXTURE_COORDS:  // Scale-offset. Easy.
         case GE_TEXMAP_UNKNOWN: // Not sure what this is, but Riviera uses it.  Treating as coords works.
            if (scaleUV)
            {
               if (VS_BIT_HAS_TEXCOORD)
               {
                  if (VS_BIT_BEZIER || VS_BIT_SPLINE)
                     v_texcoord = vec3(tex.xy * base.uvscaleoffset.xy + base.uvscaleoffset.zw, 0.0);
                  else
                     v_texcoord = vec3(texcoord.xy * base.uvscaleoffset.xy, 0.0);
               }
               else
                  v_texcoord = vec3(0.0);
            }
            else
            {
               if (VS_BIT_HAS_TEXCOORD)
                  v_texcoord = vec3(tex.xy * base.uvscaleoffset.xy + base.uvscaleoffset.zw, 0.0);
               else
                  v_texcoord = vec3(base.uvscaleoffset.zw, 0.0);
            }
            break;

         case GE_TEXMAP_TEXTURE_MATRIX:  // Projection mapping.
         {
            vec4 temp_tc;
            switch (VS_BIT_UVPROJ_MODE)
            {
            case GE_PROJMAP_POSITION:  // Use model space XYZ as source
               temp_tc = vec4(position.xyz, 1.0);
               break;
            case GE_PROJMAP_UV:  // Use unscaled UV as source
            {
               // scaleUV is false here.
               if (VS_BIT_HAS_TEXCOORD)
                  temp_tc = vec4(texcoord.xy, 0.0, 1.0);
               else
                  temp_tc = vec4(0.0, 0.0, 0.0, 1.0);
            }
            break;
            case GE_PROJMAP_NORMALIZED_NORMAL:  // Use normalized transformed normal as source
               if (VS_BIT_HAS_NORMAL)
                  temp_tc = vec4(normalize((VS_BIT_NORM_REVERSE ? -normal : normal)), 1.0);
               else
                  temp_tc = vec4(0.0, 0.0, 1.0, 1.0);
               break;
            case GE_PROJMAP_NORMAL:  // Use non-normalized transformed normal as source
               if (VS_BIT_HAS_NORMAL)
                  temp_tc = vec4((VS_BIT_NORM_REVERSE ? -normal : normal), 1.0);
               else
                  temp_tc = vec4(0.0, 0.0, 1.0, 1.0);
               break;
            }
            // Transform by texture matrix. XYZ as we are doing projection mapping.
            v_texcoord = (temp_tc * base.tex_mtx).xyz * vec3(base.uvscaleoffset.xy, 1.0);
         }
         break;

         case GE_TEXMAP_ENVIRONMENT_MAP:  // Shade mapping - use dots from light sources.
            v_texcoord = vec3(base.uvscaleoffset.xy * vec2(1.0 + dot(normalize(light.pos[VS_BIT_LS0]), worldnormal),
                              1.0 + dot(normalize(light.pos[VS_BIT_LS1]), worldnormal)) * 0.5, 1.0);
            break;
         default:
            // ILLEGAL
            break;
         }
      }
      // Compute fogdepth
      if (VS_BIT_ENABLE_FOG)
         v_fogdepth = (viewPos.z + base.fogcoef.x) * base.fogcoef.y;
   }
   else
#endif
   {
      // Simple pass-through of vertex data to fragment shader
      if (VS_BIT_DO_TEXTURE)
      {
         if (VS_BIT_DO_TEXTURE_TRANSFORM && !VS_BIT_IS_THROUGH)
            v_texcoord = texcoord;
         else
            v_texcoord = vec3(texcoord.xy, 1.0);
      }
      if (VS_BIT_HAS_COLOR)
      {
         v_color0 = color0;
         if (VS_BIT_LMODE)
            v_color1 = color1;
      }
      else
         v_color0 = base.matambientalpha;
      if (VS_BIT_ENABLE_FOG)
         v_fogdepth = position.w;
      if (VS_BIT_IS_THROUGH)
         gl_Position = base.proj_through_mtx * vec4(position.xyz, 1.0);
      else
      {
         // The viewport is used in this case, so need to compensate for that.
#ifdef GPU_ROUND_DEPTH_TO_16BIT
         gl_Position = depthRoundZVP(base.proj_mtx * vec4(position.xyz, 1.0));
#else
         gl_Position = base.proj_mtx * vec4(position.xyz, 1.0);
#endif
      }
   }
}
