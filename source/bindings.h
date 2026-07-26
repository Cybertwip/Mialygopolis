#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct M3DSession M3DSession;

typedef struct M3DCharacterInfo
{
    const char* id;
    const char* name;
    const char* tagline;
    float accent_r, accent_g, accent_b;
    float bounds_min_x, bounds_min_y, bounds_min_z;
    float bounds_max_x, bounds_max_y, bounds_max_z;
} M3DCharacterInfo;

typedef struct M3DGroupInfo
{
    const char* id;
    const char* name;
    int required;
    int option_count;
} M3DGroupInfo;

typedef struct M3DOptionInfo
{
    const char* id;
    const char* name;
    const char* asset_file;
    const char* thumbnail;
    int variant_kind;
    int variant_count;
} M3DOptionInfo;

typedef struct M3DVariantInfo
{
    const char* id;
    const char* name;
    int texture_count;
    int color_count;
} M3DVariantInfo;

typedef struct M3DPlayerState
{
    float x, y, z;
    float yaw;
    float speed;
    float walk_phase;
} M3DPlayerState;

M3DSession* m3d_session_create(const char* catalog_path);
void m3d_session_destroy(M3DSession* session);
const char* m3d_last_error(void);

int m3d_session_character_count(const M3DSession* session);
M3DCharacterInfo m3d_session_character(const M3DSession* session, int index);
int m3d_session_mode(const M3DSession* session);
int m3d_session_selected_character(const M3DSession* session);
M3DPlayerState m3d_session_player(const M3DSession* session);
float m3d_session_selector_time(const M3DSession* session);
uint64_t m3d_session_customization_revision(const M3DSession* session);

void m3d_session_select_character(M3DSession* session, int index);
void m3d_session_select_relative(M3DSession* session, int delta);
void m3d_session_open_customizer(M3DSession* session);
void m3d_session_return_to_class_selector(M3DSession* session);
void m3d_session_enter_lobby(M3DSession* session);
void m3d_session_enter_world(M3DSession* session);
void m3d_session_return_to_customizer(M3DSession* session);

int m3d_session_group_count(const M3DSession* session);
M3DGroupInfo m3d_session_group(const M3DSession* session, int group_index);
int m3d_session_active_group(const M3DSession* session);
void m3d_session_select_group(M3DSession* session, int group_index);
void m3d_session_select_group_relative(M3DSession* session, int delta);

int m3d_session_selected_option(const M3DSession* session, int group_index);
M3DOptionInfo m3d_session_option(const M3DSession* session, int group_index, int option_index);
void m3d_session_select_option(M3DSession* session, int group_index, int option_index);
void m3d_session_select_option_relative(M3DSession* session, int delta);

int m3d_session_selected_variant(const M3DSession* session, int group_index);
M3DVariantInfo m3d_session_variant(const M3DSession* session, int group_index, int option_index, int variant_index);
const char* m3d_session_variant_texture(const M3DSession* session, int group_index, int option_index, int variant_index, int texture_index);
void m3d_session_variant_color(const M3DSession* session, int group_index, int option_index, int variant_index, int color_index, float* r, float* g, float* b);
void m3d_session_select_variant(M3DSession* session, int group_index, int variant_index);
void m3d_session_select_variant_relative(M3DSession* session, int delta);
void m3d_session_randomize(M3DSession* session);

void m3d_session_set_move(M3DSession* session, float forward, float strafe, int running, float view_yaw);
void m3d_session_update(M3DSession* session, float delta_seconds);

#ifdef __cplusplus
}
#endif
