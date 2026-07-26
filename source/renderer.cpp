#include "renderer.h"

#include "stb_image_write.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace m3d {
namespace {

constexpr UiColor White{0.94f, 0.96f, 1.0f, 1.0f};
constexpr UiColor Muted{0.58f, 0.65f, 0.76f, 1.0f};
constexpr UiColor Panel{0.025f, 0.04f, 0.085f, 0.90f};
constexpr UiColor PanelSoft{0.04f, 0.065f, 0.12f, 0.82f};

GLuint compileShader(GLenum type, const char* source, std::string& error)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vertexSource, const char* fragmentSource, std::string& error)
{
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource, error);
    if (!vertex) return 0;
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (!fragment) { glDeleteShader(vertex); return 0; }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        error = log;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void setMatrix(GLuint program, const char* name, const Mat4& matrix)
{
    glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, &matrix[0][0]);
}

UiColor fromVec3(const Vec3& color, float alpha = 1.0f)
{
    return {color.x, color.y, color.z, alpha};
}

float loadoutScale(const CharacterLoadout& loadout, float desiredHeight)
{
    return desiredHeight / std::max(0.1f, loadout.height());
}

Vec3 characterAccent(const M3DSession* session)
{
    const auto info = m3d_session_character(session, m3d_session_selected_character(session));
    return {info.accent_r, info.accent_g, info.accent_b};
}

UiColor roleColor(const std::string& role)
{
    if (role == "admin") return {1.0f, 0.36f, 0.30f, 1.0f};
    if (role == "moderator") return {0.82f, 0.48f, 1.0f, 1.0f};
    return {0.20f, 0.87f, 1.0f, 1.0f};
}

std::string roleLabel(const std::string& role)
{
    if (role == "admin") return "ADMINISTRADOR";
    if (role == "moderator") return "MODERADOR";
    return "JUGADOR";
}

std::string clipped(std::string value, std::size_t maxLength)
{
    if (value.size() > maxLength) value.resize(maxLength);
    return value;
}

} // namespace

bool Renderer::initialize(std::string& error)
{
    if (!createPrograms(error) || !createWorldGeometry(error) || !ui_.initialize(error)) {
        shutdown();
        return false;
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_MULTISAMPLE);
    return true;
}

bool Renderer::createPrograms(std::string& error)
{
    static constexpr const char* worldVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec4 a_glyph_position_size;
layout(location=3) in vec4 a_glyph_normal_material;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_voxel_scale;
out vec3 v_world_position;
out vec3 v_normal;
flat out int v_material;
void main(){
 vec3 local=a_glyph_position_size.xyz+a_position*a_glyph_position_size.w;
 v_world_position=local*u_voxel_scale; v_normal=a_normal;
 v_material=int(a_glyph_normal_material.w+0.5);
 gl_Position=u_projection*u_view*vec4(v_world_position,1.0);
})GLSL";
    static constexpr const char* worldFragment = R"GLSL(
#version 330 core
in vec3 v_world_position; in vec3 v_normal; flat in int v_material;
uniform vec3 u_camera; out vec4 out_color;
uniform vec3 u_sky;
vec3 palette(int id){
 if(id==1)return vec3(.68,.58,.42); if(id==2)return vec3(.78,.80,.76);
 if(id==3)return vec3(.30,.30,.31); if(id==4)return vec3(.05,.38,.62);
 if(id==5)return vec3(.58,.18,.08); if(id==6)return vec3(.18,.58,.22);
 if(id==7)return vec3(.28,.13,.06); if(id==8)return vec3(.90,.58,.08);
 if(id==9)return vec3(.20,.21,.23); if(id==10)return vec3(.95,.35,.45);
 if(id==11)return vec3(.08,.32,.12); if(id==12)return vec3(.30,.68,.24);
 if(id==13)return vec3(.10,.72,.82); if(id==14)return vec3(.88,.88,.82);
 if(id==15)return vec3(.68,.04,.04); return vec3(.5,.1,.6);
}
void main(){
 vec3 base=palette(v_material); vec3 n=normalize(v_normal);
 float d=max(dot(n,normalize(vec3(.42,.88,.30))),0.0); float hemi=n.y*.5+.5;
 vec3 color=base*(.34+d*.54+hemi*.16);
 if(v_material==8||v_material==10||v_material==13)color+=base*.24;
 float fog=1.0-exp(-length(v_world_position-u_camera)*.035);
 color=mix(color,u_sky,clamp(fog,0.0,.82)); out_color=vec4(color,1.0);
})GLSL";

    static constexpr const char* avatarVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 a_position; layout(location=1) in vec3 a_normal; layout(location=2) in vec2 a_uv;
layout(location=3) in vec3 a_walk_a_position; layout(location=4) in vec3 a_walk_a_normal;
layout(location=5) in vec3 a_walk_b_position; layout(location=6) in vec3 a_walk_b_normal;
layout(location=7) in vec3 a_run_a_position; layout(location=8) in vec3 a_run_a_normal;
layout(location=9) in vec3 a_run_b_position; layout(location=10) in vec3 a_run_b_normal;
layout(location=11) in vec3 a_wave_position; layout(location=12) in vec3 a_wave_normal;
layout(location=13) in vec3 a_cheer_position; layout(location=14) in vec3 a_cheer_normal;
layout(location=15) in vec3 a_dance_position;
uniform mat4 u_model; uniform mat4 u_view; uniform mat4 u_projection;
uniform float u_phase; uniform float u_motion; uniform int u_action; uniform float u_action_weight;
out vec3 v_world_position; out vec3 v_normal; out vec2 v_uv;
void main(){
 float wave=sin(u_phase); float wa=max(wave,0.0); float wb=max(-wave,0.0);
 vec3 walkP=a_position+(a_walk_a_position-a_position)*wa+(a_walk_b_position-a_position)*wb;
 vec3 walkN=a_normal+(a_walk_a_normal-a_normal)*wa+(a_walk_b_normal-a_normal)*wb;
 vec3 runP=a_position+(a_run_a_position-a_position)*wa+(a_run_b_position-a_position)*wb;
 vec3 runN=a_normal+(a_run_a_normal-a_normal)*wa+(a_run_b_normal-a_normal)*wb;
 float walkMix=clamp(u_motion,0.0,1.0); float runMix=clamp(u_motion-1.0,0.0,1.0);
 vec3 p=mix(a_position,walkP,walkMix); p=mix(p,runP,runMix);
 vec3 n=normalize(mix(a_normal,walkN,walkMix)); n=normalize(mix(n,runN,runMix));
 if(u_action==1){p=mix(p,a_wave_position,u_action_weight);n=normalize(mix(n,a_wave_normal,u_action_weight));}
 else if(u_action==2){p=mix(p,a_cheer_position,u_action_weight);n=normalize(mix(n,a_cheer_normal,u_action_weight));}
 else if(u_action==3){p=mix(p,a_dance_position,u_action_weight);}
 vec4 world=u_model*vec4(p,1.0); v_world_position=world.xyz;
 v_normal=normalize(mat3(u_model)*n); v_uv=a_uv; gl_Position=u_projection*u_view*world;
})GLSL";
    static constexpr const char* avatarFragment = R"GLSL(
#version 330 core
in vec3 v_world_position; in vec3 v_normal; in vec2 v_uv;
uniform sampler2D u_texture; uniform bool u_has_texture; uniform vec4 u_base_color;
uniform vec3 u_camera; uniform vec3 u_accent; uniform float u_accent_mix;
uniform bool u_has_tint; uniform vec3 u_tint; uniform int u_alpha_mode; uniform float u_alpha_cutoff;
out vec4 out_color;
void main(){
 vec4 sampled=u_has_texture?texture(u_texture,v_uv):vec4(1.0); vec4 base=sampled*u_base_color;
 if(u_has_tint)base.rgb*=u_tint; if(u_alpha_mode==2&&base.a<u_alpha_cutoff)discard;
 vec3 n=normalize(v_normal); float diffuse=max(dot(n,normalize(vec3(-.35,.82,.42))),0.0);
 float bands=floor((diffuse*.76+.24)*4.0)/3.0; vec3 viewDir=normalize(u_camera-v_world_position);
 float rim=pow(1.0-max(dot(n,viewDir),0.0),2.2); vec3 color=base.rgb*(.38+bands*.70);
 color=mix(color,u_accent,u_accent_mix); color+=u_accent*rim*(.12+u_accent_mix*.7);
 float fog=1.0-exp(-length(v_world_position-u_camera)*.035);
 color=mix(color,vec3(.015,.026,.065),clamp(fog,0.0,.72)); out_color=vec4(color,base.a);
})GLSL";

    worldProgram_ = linkProgram(worldVertex, worldFragment, error);
    if (!worldProgram_) return false;
    avatarProgram_ = linkProgram(avatarVertex, avatarFragment, error);
    return avatarProgram_ != 0;
}

bool Renderer::createWorldGeometry(std::string& error)
{
    (void)error;
    static constexpr float cube[] = {
         .5f,-.5f,-.5f,1,0,0, .5f,.5f,-.5f,1,0,0, .5f,.5f,.5f,1,0,0,
         .5f,-.5f,-.5f,1,0,0, .5f,.5f,.5f,1,0,0, .5f,-.5f,.5f,1,0,0,
        -.5f,-.5f,.5f,-1,0,0, -.5f,.5f,.5f,-1,0,0, -.5f,.5f,-.5f,-1,0,0,
        -.5f,-.5f,.5f,-1,0,0, -.5f,.5f,-.5f,-1,0,0, -.5f,-.5f,-.5f,-1,0,0,
        -.5f,.5f,-.5f,0,1,0, -.5f,.5f,.5f,0,1,0, .5f,.5f,.5f,0,1,0,
        -.5f,.5f,-.5f,0,1,0, .5f,.5f,.5f,0,1,0, .5f,.5f,-.5f,0,1,0,
        -.5f,-.5f,.5f,0,-1,0, -.5f,-.5f,-.5f,0,-1,0, .5f,-.5f,-.5f,0,-1,0,
        -.5f,-.5f,.5f,0,-1,0, .5f,-.5f,-.5f,0,-1,0, .5f,-.5f,.5f,0,-1,0,
         .5f,-.5f,.5f,0,0,1, .5f,.5f,.5f,0,0,1, -.5f,.5f,.5f,0,0,1,
         .5f,-.5f,.5f,0,0,1, -.5f,.5f,.5f,0,0,1, -.5f,-.5f,.5f,0,0,1,
        -.5f,-.5f,-.5f,0,0,-1, -.5f,.5f,-.5f,0,0,-1, .5f,.5f,-.5f,0,0,-1,
        -.5f,-.5f,-.5f,0,0,-1, .5f,.5f,-.5f,0,0,-1, .5f,-.5f,-.5f,0,0,-1,
    };
    glGenVertexArrays(1,&worldVao_); glGenBuffers(1,&worldVertexBuffer_); glGenBuffers(1,&worldInstanceBuffer_);
    glBindVertexArray(worldVao_); glBindBuffer(GL_ARRAY_BUFFER,worldVertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cube),cube,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(float)*6,nullptr);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(float)*6,reinterpret_cast<void*>(sizeof(float)*3));
    glBindBuffer(GL_ARRAY_BUFFER,worldInstanceBuffer_);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(Cubiquity::Glyph),reinterpret_cast<void*>(offsetof(Cubiquity::Glyph,position))); glVertexAttribDivisor(2,1);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Cubiquity::Glyph),reinterpret_cast<void*>(offsetof(Cubiquity::Glyph,normal))); glVertexAttribDivisor(3,1);
    glBindVertexArray(0); return true;
}

bool Renderer::uploadWorld(const VoxelWorld& world, std::string& error)
{
    if (world.glyphs().size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        error = "El mundo vóxel contiene demasiados glifos"; return false;
    }
    worldGlyphCount_ = static_cast<GLsizei>(world.glyphs().size());
    worldSkyColor_ = world.skyColor();
    glBindBuffer(GL_ARRAY_BUFFER,worldInstanceBuffer_);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(world.glyphs().size()*sizeof(Cubiquity::Glyph)),world.glyphs().data(),GL_STATIC_DRAW);
    return true;
}

void Renderer::shutdown()
{
    ui_.shutdown();
    if(worldInstanceBuffer_)glDeleteBuffers(1,&worldInstanceBuffer_);
    if(worldVertexBuffer_)glDeleteBuffers(1,&worldVertexBuffer_);
    if(worldVao_)glDeleteVertexArrays(1,&worldVao_);
    if(avatarProgram_)glDeleteProgram(avatarProgram_); if(worldProgram_)glDeleteProgram(worldProgram_);
    worldInstanceBuffer_=worldVertexBuffer_=worldVao_=avatarProgram_=worldProgram_=0;
}

void Renderer::drawWorld(const Mat4& view,const Mat4& projection,const Vec3& camera)
{
    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glDisable(GL_BLEND);
    glUseProgram(worldProgram_); setMatrix(worldProgram_,"u_view",view); setMatrix(worldProgram_,"u_projection",projection);
    glUniform1f(glGetUniformLocation(worldProgram_,"u_voxel_scale"),VoxelWorld::VoxelScale);
    glUniform3f(glGetUniformLocation(worldProgram_,"u_camera"),camera.x,camera.y,camera.z);
    glUniform3f(glGetUniformLocation(worldProgram_,"u_sky"),worldSkyColor_.x,worldSkyColor_.y,worldSkyColor_.z);
    glBindVertexArray(worldVao_); glDrawArraysInstanced(GL_TRIANGLES,0,36,worldGlyphCount_); glBindVertexArray(0);
}

void Renderer::drawLoadout(const CharacterLoadout& loadout,const Mat4& model,const Mat4& view,
                           const Mat4& projection,const Vec3& camera,float accentMix,float phase,float motion,
                           int action,float actionWeight)
{
    glUseProgram(avatarProgram_); setMatrix(avatarProgram_,"u_model",model); setMatrix(avatarProgram_,"u_view",view); setMatrix(avatarProgram_,"u_projection",projection);
    glUniform3f(glGetUniformLocation(avatarProgram_,"u_camera"),camera.x,camera.y,camera.z);
    glUniform3f(glGetUniformLocation(avatarProgram_,"u_accent"),loadout.accent().x,loadout.accent().y,loadout.accent().z);
    glUniform1f(glGetUniformLocation(avatarProgram_,"u_accent_mix"),accentMix);
    glUniform1f(glGetUniformLocation(avatarProgram_,"u_phase"),phase);
    glUniform1f(glGetUniformLocation(avatarProgram_,"u_motion"),motion);
    glUniform1i(glGetUniformLocation(avatarProgram_,"u_action"),action);
    glUniform1f(glGetUniformLocation(avatarProgram_,"u_action_weight"),actionWeight);
    glUniform1i(glGetUniformLocation(avatarProgram_,"u_texture"),0);

    for(const CharacterPart& part:loadout.parts()){
        if(!part.asset)continue;
        const AvatarAsset* asset=part.asset.get();
        glUniform1i(glGetUniformLocation(avatarProgram_,"u_has_tint"),part.tintEnabled?1:0);
        glUniform3f(glGetUniformLocation(avatarProgram_,"u_tint"),part.tint.x,part.tint.y,part.tint.z);
        for(const auto& primitive:asset->primitives()){
            const bool doubleSided=(primitive.flags&1U)!=0, alphaBlend=(primitive.flags&2U)!=0, alphaMask=(primitive.flags&4U)!=0;
            if(doubleSided)glDisable(GL_CULL_FACE);else glEnable(GL_CULL_FACE);
            if(alphaBlend){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}else glDisable(GL_BLEND);
            const GLuint texture=part.textureFor(primitive);
            glUniform1i(glGetUniformLocation(avatarProgram_,"u_has_texture"),texture?1:0);
            glUniform4fv(glGetUniformLocation(avatarProgram_,"u_base_color"),1,primitive.baseColor.data());
            glUniform1i(glGetUniformLocation(avatarProgram_,"u_alpha_mode"),alphaBlend?1:(alphaMask?2:0));
            glUniform1f(glGetUniformLocation(avatarProgram_,"u_alpha_cutoff"),primitive.alphaCutoff);
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glBindVertexArray(primitive.vao);
            glDrawElements(GL_TRIANGLES,primitive.indexCount,GL_UNSIGNED_INT,nullptr);
        }
    }
    glBindVertexArray(0);glDisable(GL_BLEND);
}

void Renderer::renderClassSelector(const CharacterLoadout& loadout,const M3DSession* session,float time,int width,int height)
{
    glViewport(0,0,width,height);glClearColor(worldSkyColor_.x*.42f,worldSkyColor_.y*.42f,worldSkyColor_.z*.42f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const float aspect=static_cast<float>(width)/std::max(1,height);const Vec3 camera{0,2.05f,5.35f};
    const Mat4 view=linalg::lookat_matrix(camera,Vec3{0,.95f,0},Vec3{0,1,0});const Mat4 projection=linalg::perspective_matrix(51*Pi/180,aspect,.05f,120.f);
    drawWorld(view,projection,camera);const float pulse=1+std::sin(time*3)*.018f;
    const Mat4 model=modelMatrix(Vec3{0,.38f,0},0,loadoutScale(loadout,1.72f)*pulse,loadout.localOffset());
    drawLoadout(loadout,model,view,projection,camera,.10f,time,0,0,0);drawClassSelectorUi(session,width,height);
}

void Renderer::renderCustomizer(const CharacterLoadout& loadout,const M3DSession* session,float time,int width,int height)
{
    glViewport(0,0,width,height);glClearColor(worldSkyColor_.x*.42f,worldSkyColor_.y*.42f,worldSkyColor_.z*.42f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const float aspect=static_cast<float>(width)/std::max(1,height);const Vec3 camera{0,1.75f,4.35f};
    const Mat4 view=linalg::lookat_matrix(camera,Vec3{0,.92f,0},Vec3{0,1,0});const Mat4 projection=linalg::perspective_matrix(48*Pi/180,aspect,.05f,120.f);
    drawWorld(view,projection,camera);const Mat4 model=modelMatrix(Vec3{0,.13f,0},std::sin(time*.35f)*.12f,loadoutScale(loadout,1.72f),loadout.localOffset());
    drawLoadout(loadout,model,view,projection,camera,.04f,time,0,0,0);drawCustomizerUi(session,width,height);
}

void Renderer::renderLobby(const CharacterLoadout& loadout,const M3DSession* session,
                           const MultiplayerSnapshot& multiplayer,int selectedWorld,bool chatActive,const std::string& chatInput,
                           float time,int width,int height)
{
    glViewport(0,0,width,height);glClearColor(worldSkyColor_.x*.42f,worldSkyColor_.y*.42f,worldSkyColor_.z*.42f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const float aspect=static_cast<float>(width)/std::max(1,height);const Vec3 camera{-1.35f,1.8f,4.7f};
    const Mat4 view=linalg::lookat_matrix(camera,Vec3{-1.35f,.9f,0},Vec3{0,1,0});const Mat4 projection=linalg::perspective_matrix(49*Pi/180,aspect,.05f,120.f);
    drawWorld(view,projection,camera);const Mat4 model=modelMatrix(Vec3{-1.35f,.13f,0},std::sin(time*.35f)*.08f,loadoutScale(loadout,1.72f),loadout.localOffset());
    drawLoadout(loadout,model,view,projection,camera,.05f,time,0,0,0);drawLobbyUi(session,multiplayer,selectedWorld,chatActive,chatInput,width,height);
}

void Renderer::renderWorld(const CharacterLoadout& loadout,const PlayerState& player,
                           const MultiplayerSnapshot& multiplayer,const NpcSystem& npcSystem,int selectedWorld,bool chatActive,const std::string& chatInput,
                           float cameraYaw,float cameraDistance,int action,float actionTime,float time,int width,int height)
{
    glViewport(0,0,width,height);glClearColor(worldSkyColor_.x*.42f,worldSkyColor_.y*.42f,worldSkyColor_.z*.42f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const float aspect=static_cast<float>(width)/std::max(1,height);const Vec3 forward{std::sin(cameraYaw),0,std::cos(cameraYaw)};
    const Vec3 target=player.position+Vec3{0,.88f,0};const Vec3 camera=target-forward*cameraDistance+Vec3{0,2.25f,0};
    const Mat4 view=linalg::lookat_matrix(camera,target,Vec3{0,1,0});const Mat4 projection=linalg::perspective_matrix(56*Pi/180,aspect,.05f,140.f);
    drawWorld(view,projection,camera);const float motion=clamp(player.speed/2.45f,0,1.72f);const float scale=loadoutScale(loadout,1.72f);
    float actionWeight=0.0f;float jumpOffset=0.0f;
    if(action==1){const float t=clamp(actionTime/2.4f,0,1);actionWeight=std::sin(Pi*t)*(.45f+.55f*std::abs(std::sin(actionTime*7.0f)));}
    else if(action==2){const float t=clamp(actionTime/2.2f,0,1);actionWeight=std::sin(Pi*t);}
    else if(action==3){const float t=clamp(actionTime/5.5f,0,1);actionWeight=std::sin(Pi*std::min(1.0f,t*1.4f))*.92f;}
    else if(action==4){const float t=clamp(actionTime/1.0f,0,1);jumpOffset=std::sin(Pi*t)*.78f;}
    const Mat4 model=modelMatrix(player.position+Vec3{0,jumpOffset,0},player.yaw,scale,loadout.localOffset());
    drawLoadout(loadout,model,view,projection,camera,.025f,player.walkPhase,motion,action,actionWeight);
    int npcIndex=0;
    for(const auto& npc:npcSystem.npcs()){
        const Mat4 npcModel=modelMatrix(npc.position,npc.yaw,scale*.96f,loadout.localOffset());
        const float npcPhase=time*1.2f+static_cast<float>(npcIndex);
        const int npcAction=(npcIndex++%3)==0?1:0;
        const float npcActionWeight=npcAction?(.15f+.10f*std::abs(std::sin(time*.8f))):0.0f;
        drawLoadout(loadout,npcModel,view,projection,camera,.08f,npcPhase,0.0f,npcAction,npcActionWeight);
    }
    for(const auto& remote:multiplayer.remotePlayers){
        if(remote.world!=selectedWorld)continue;
        const Mat4 remoteModel=modelMatrix(remote.position,remote.yaw,scale,loadout.localOffset());
        drawLoadout(loadout,remoteModel,view,projection,camera,.12f,remote.phase,clamp(remote.speed/2.45f,0,1.72f),0,0);
    }
    drawWorldUi(loadout,player,multiplayer,npcSystem,selectedWorld,chatActive,chatInput,width,height);
}

void Renderer::drawClassSelectorUi(const M3DSession* session,int width,int height)
{
    ui_.begin(width,height);const float s=clamp(std::min(width/1280.f,height/720.f),.72f,1.65f);
    ui_.rectangle(0,0,width,82*s,Panel);ui_.text(30*s,22*s,4*s,"MIALYGOPOLIS 3D",White);
    ui_.text(width-245*s,30*s,2*s,"PROTOTIPO NATIVO",Muted);ui_.textCentered(width*.5f,92*s,2.2f,"ELIGE UN PERSONAJE BASE",White);
    const int selected=m3d_session_selected_character(session),count=m3d_session_character_count(session);
    const int visible=std::min(count,5);const float gap=12*s;
    const float cardW=std::min(220*s,(width-70*s-gap*(visible-1))/std::max(1,visible));
    const float total=cardW*visible+gap*(visible-1);const float start=(width-total)*.5f,cardY=height-150*s;
    for(int slot=0;slot<visible;slot++){
        const int offset=slot-visible/2;const int i=((selected+offset)%count+count)%count;
        const auto info=m3d_session_character(session,i);const Vec3 accent{info.accent_r,info.accent_g,info.accent_b};const float x=start+slot*(cardW+gap);const bool active=i==selected;
        ui_.rectangle(x,cardY,cardW,88*s,active?PanelSoft:Panel);ui_.outline(x,cardY,cardW,88*s,active?3*s:1*s,active?fromVec3(accent):UiColor{.18f,.24f,.36f,.9f});
        ui_.text(x+12*s,cardY+17*s,2.1f*s,info.name?info.name:"PERSONAJE",active?fromVec3(accent):White);
        ui_.text(x+12*s,cardY+52*s,1.15f*s,info.tagline?info.tagline:"",Muted);
    }
    const Vec3 accent=characterAccent(session);ui_.rectangle((width-280*s)*.5f,height-53*s,280*s,42*s,fromVec3(accent,.92f));
    ui_.textCentered(width*.5f,height-41*s,2.0f*s,"PERSONALIZAR PERSONAJE",UiColor{.01f,.02f,.05f,1});
    ui_.text(22*s,height-25*s,1.35f*s,"IZQUIERDA / DERECHA  ELEGIR",Muted);ui_.text(width-220*s,height-25*s,1.35f*s,"ENTER  PERSONALIZAR",Muted);ui_.flush();
}

void Renderer::drawCustomizerUi(const M3DSession* session,int width,int height)
{
    ui_.begin(width,height);const float s=clamp(std::min(width/1280.f,height/720.f),.72f,1.65f);const Vec3 accent=characterAccent(session);
    ui_.rectangle(0,0,width,68*s,Panel);const auto character=m3d_session_character(session,m3d_session_selected_character(session));
    ui_.text(25*s,20*s,3.0f*s,character.name?character.name:"PERSONAJE",fromVec3(accent));ui_.text(width-260*s,25*s,1.6f*s,"MODO ESTUDIO DE PERSONAJES",Muted);
    const float leftW=225*s;ui_.rectangle(18*s,88*s,leftW,height-170*s,Panel);
    const int groupCount=m3d_session_group_count(session),active=m3d_session_active_group(session);
    for(int i=0;i<groupCount;i++){
        const auto group=m3d_session_group(session,i);const float y=108*s+i*45*s;const bool selected=i==active;
        if(selected)ui_.rectangle(28*s,y-8*s,leftW-20*s,35*s,fromVec3(accent,.18f));
        ui_.text(40*s,y,1.75f*s,group.name?group.name:"RASGO",selected?fromVec3(accent):White);
        if(group.required)ui_.text(185*s,y,1.25f*s,"REQ",Muted);
    }
    const float rightW=330*s,rightX=width-rightW-18*s;ui_.rectangle(rightX,88*s,rightW,height-170*s,Panel);
    const auto group=m3d_session_group(session,active);const int optionIndex=m3d_session_selected_option(session,active);
    ui_.text(rightX+22*s,108*s,2.2f*s,group.name?group.name:"RASGO",fromVec3(accent));ui_.text(rightX+22*s,148*s,1.25f*s,"OPCIÓN DE MODELO",Muted);
    std::string optionName="NINGUNO";int optionCount=group.option_count;if(optionIndex>=0){const auto option=m3d_session_option(session,active,optionIndex);if(option.name)optionName=option.name;}
    ui_.rectangle(rightX+20*s,170*s,rightW-40*s,48*s,PanelSoft);ui_.textCentered(rightX+rightW*.5f,185*s,1.8f*s,"<  "+optionName+"  >",White);
    const std::string indexText=optionIndex<0?"NINGUNO":std::to_string(optionIndex+1)+" / "+std::to_string(optionCount);ui_.textCentered(rightX+rightW*.5f,228*s,1.25f*s,indexText,Muted);
    const int variantIndex=m3d_session_selected_variant(session,active);if(optionIndex>=0){const auto option=m3d_session_option(session,active,optionIndex);if(option.variant_count>0){
        ui_.text(rightX+22*s,266*s,1.25f*s,option.variant_kind==1?"TEXTURA":"COLOR",Muted);const auto variant=m3d_session_variant(session,active,optionIndex,std::max(0,variantIndex));
        ui_.rectangle(rightX+20*s,287*s,rightW-40*s,44*s,PanelSoft);ui_.textCentered(rightX+rightW*.5f,300*s,1.55f*s,variant.name?variant.name:"VARIANTE",fromVec3(accent));
        ui_.textCentered(rightX+rightW*.5f,342*s,1.2f*s,"[ / ]  CAMBIAR VARIANTE",Muted);
    }}
    ui_.text(rightX+22*s,height-236*s,1.3f*s,"ARRIBA / ABAJO  CATEGORÍA",Muted);ui_.text(rightX+22*s,height-207*s,1.3f*s,"IZQUIERDA / DERECHA  MODELO",Muted);
    ui_.text(rightX+22*s,height-178*s,1.3f*s,"R  ALEATORIO",Muted);ui_.text(rightX+22*s,height-149*s,1.3f*s,"RETROCESO  PERSONAJE",Muted);
    ui_.rectangle((width-250*s)*.5f,height-58*s,250*s,44*s,fromVec3(accent,.94f));ui_.textCentered(width*.5f,height-45*s,2*s,"ENTRAR AL LOBBY",UiColor{.01f,.02f,.05f,1});ui_.flush();
}

void Renderer::drawChatUi(const MultiplayerSnapshot& multiplayer,bool chatActive,const std::string& chatInput,int width,int height,float s)
{
    const float boxW=520*s,boxH=155*s,x=20*s,y=height-225*s;
    ui_.rectangle(x,y,boxW,boxH,Panel);
    ui_.outline(x,y,boxW,boxH,1*s,UiColor{.15f,.24f,.36f,.85f});
    const int available=static_cast<int>(multiplayer.messages.size());const int first=std::max(0,available-5);
    int row=0;for(int i=first;i<available;i++,row++){
        const auto& message=multiplayer.messages[static_cast<std::size_t>(i)];
        const UiColor color=message.system?UiColor{1.0f,.72f,.22f,1}:roleColor(message.role);
        const std::string prefix=message.system?"SISTEMA":clipped(message.sender,14)+" ["+roleLabel(message.role)+"]";
        ui_.text(x+12*s,y+(12+row*25)*s,1.15f*s,prefix,color);
        ui_.text(x+205*s,y+(12+row*25)*s,1.15f*s,clipped(message.text,42),White);
    }
    ui_.rectangle(x,y+boxH-30*s,boxW,30*s,chatActive?PanelSoft:UiColor{.02f,.03f,.06f,.9f});
    ui_.text(x+10*s,y+boxH-21*s,1.15f*s,chatActive?"> "+clipped(chatInput,60):"T  ABRIR CHAT",chatActive?White:Muted);
}

void Renderer::drawLobbyUi(const M3DSession* session,const MultiplayerSnapshot& multiplayer,int selectedWorld,bool chatActive,const std::string& chatInput,int width,int height)
{
    ui_.begin(width,height);const float s=clamp(std::min(width/1280.f,height/720.f),.72f,1.65f);const Vec3 accent=characterAccent(session);
    ui_.rectangle(0,0,width,68*s,Panel);ui_.text(25*s,20*s,3*s,"LOBBY DE MIALYGOPOLIS",fromVec3(accent));
    ui_.text(width-390*s,25*s,1.45f*s,multiplayer.statusText,roleColor(multiplayer.localRole));
    const WorldTheme theme=static_cast<WorldTheme>(selectedWorld);
    ui_.rectangle(330*s,92*s,420*s,92*s,Panel);
    ui_.textCentered(540*s,108*s,1.35f*s,"MUNDO PREDETERMINADO",Muted);
    ui_.textCentered(540*s,134*s,2.15f*s,"<  "+std::string(worldName(theme))+"  >",fromVec3(accent));
    ui_.textCentered(540*s,164*s,1.05f*s,std::string(worldDescription(theme)),White);
    const float panelX=width-410*s;ui_.rectangle(panelX,88*s,390*s,height-170*s,Panel);
    ui_.text(panelX+20*s,108*s,2.1f*s,"JUGADORES CONECTADOS",White);
    int row=0;for(const auto& member:multiplayer.members){if(row>=12)break;const float y=(148+row*32)*s;
        ui_.text(panelX+22*s,y,1.35f*s,clipped(member.name,18),roleColor(member.role));
        ui_.text(panelX+235*s,y,1.15f*s,roleLabel(member.role),Muted);row++;}
    if(multiplayer.members.empty())ui_.text(panelX+22*s,155*s,1.3f*s,"ESPERANDO JUGADORES...",Muted);
    drawChatUi(multiplayer,chatActive,chatInput,width,height,s);
    ui_.rectangle((width-250*s)*.5f,height-58*s,250*s,44*s,fromVec3(accent,.94f));ui_.textCentered(width*.5f,height-45*s,2*s,"ENTRAR AL MUNDO",UiColor{.01f,.02f,.05f,1});
    ui_.text(panelX+20*s,height-142*s,1.15f*s,"IZQ / DER  MUNDO   T CHAT   ENTER DESPLEGAR",Muted);ui_.text(panelX+20*s,height-115*s,1.15f*s,"ESC  VOLVER AL ESTUDIO",Muted);ui_.flush();
}

void Renderer::drawWorldUi(const CharacterLoadout& loadout,const PlayerState& player,const MultiplayerSnapshot& multiplayer,const NpcSystem& npcSystem,int selectedWorld,bool chatActive,const std::string& chatInput,int width,int height)
{
    ui_.begin(width,height);const float s=clamp(std::min(width/1280.f,height/720.f),.72f,1.65f);
    ui_.rectangle(18*s,18*s,315*s,82*s,Panel);ui_.outline(18*s,18*s,315*s,82*s,2*s,fromVec3(loadout.accent()));
    ui_.text(34*s,34*s,2.35f*s,loadout.name(),fromVec3(loadout.accent()));
    const std::string worldStatus=(player.speed>3?"CORRIENDO / ":"EN LÍNEA / ")+std::string(worldName(static_cast<WorldTheme>(selectedWorld)));
    ui_.text(34*s,67*s,1.10f*s,worldStatus,White);
    ui_.text(205*s,67*s,1.1f*s,std::to_string(multiplayer.members.size())+" EN LOBBY",roleColor(multiplayer.localRole));
    drawChatUi(multiplayer,chatActive,chatInput,width,height,s);
    const float barW=720*s,barX=(width-barW)*.5f,barY=height-50*s;ui_.rectangle(barX,barY,barW,34*s,Panel);
    ui_.textCentered(width*.5f,barY+10*s,1.3f*s,"WASD MOVER  SHIFT CORRER  1 SALUDAR  2 CELEBRAR  3 BAILAR  ESPACIO SALTAR",Muted);
    ui_.rectangle(width*.5f-7*s,height*.5f-1*s,14*s,2*s,UiColor{1,1,1,.6f});ui_.rectangle(width*.5f-1*s,height*.5f-7*s,2*s,14*s,UiColor{1,1,1,.6f});
    if(npcSystem.dialogue().active){
        const auto& dialogue=npcSystem.dialogue();const float boxW=620*s,boxX=(width-boxW)*.5f,boxY=height-225*s;
        ui_.rectangle(boxX,boxY,boxW,130*s,Panel);ui_.outline(boxX,boxY,boxW,130*s,2*s,UiColor{.92f,.70f,.22f,1});
        ui_.text(boxX+18*s,boxY+16*s,1.8f*s,dialogue.name,UiColor{.92f,.70f,.22f,1});
        ui_.text(boxX+190*s,boxY+18*s,1.1f*s,dialogue.title,Muted);
        ui_.text(boxX+18*s,boxY+52*s,1.25f*s,clipped(dialogue.text,74),White);
        ui_.text(boxX+18*s,boxY+102*s,1.05f*s,"E  SIGUIENTE RESPUESTA   ESC  CERRAR",Muted);
    }else if(const NpcDefinition* nearby=npcSystem.nearest(player.position);nearby){
        const std::string prompt="E  HABLAR CON "+nearby->name+" / "+nearby->title;
        ui_.rectangle((width-520*s)*.5f,height-94*s,520*s,32*s,Panel);
        ui_.textCentered(width*.5f,height-84*s,1.2f*s,prompt,UiColor{.92f,.70f,.22f,1});
    }
    ui_.flush();
}

bool Renderer::capturePng(const std::filesystem::path& path,int width,int height,std::string& error) const
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4),flipped(pixels.size());glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    const std::size_t row=static_cast<std::size_t>(width)*4;for(int y=0;y<height;y++)std::memcpy(flipped.data()+static_cast<std::size_t>(y)*row,pixels.data()+static_cast<std::size_t>(height-1-y)*row,row);
    std::error_code ec;if(path.has_parent_path())std::filesystem::create_directories(path.parent_path(),ec);
    if(!stbi_write_png(path.string().c_str(),width,height,4,flipped.data(),width*4)){error="No se pudo guardar la captura: "+path.string();return false;}return true;
}

} // namespace m3d
