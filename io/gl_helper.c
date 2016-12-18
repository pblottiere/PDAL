#define GLFAST_IMPLEMENTATION
#include "glfast.h"

#define STR(x) #x
#define countof(x) (sizeof(x) / sizeof((x)[0]))

#define QFPC_IMPLEMENTATION
#include "qfpc.h"

typedef struct
{
    union { f32 x; f32 u; };
    union { f32 y; f32 v; };
} vec2;

typedef struct
{
    union { f32 x; f32 r; };
    union { f32 y; f32 g; };
    union { f32 z; f32 b; };
} vec3;

typedef struct
{
    union { f32 x; f32 r; };
    union { f32 y; f32 g; };
    union { f32 z; f32 b; };
    union { f32 w; f32 a; };
} vec4;

typedef struct
{
    f32 sd_x, up_x, fw_x;
    f32 sd_y, up_y, fw_y;
    f32 sd_z, up_z, fw_z;
} mat3;


typedef struct gl_helper_ctx {
    SDL_Window  * sdl_window;
    SDL_GLContext sdl_glcontext;

    gpu_cmd_t* cmds;
    u32* state_textures;
    size_t cmd_size;
    size_t cmd_capacity;
    u32 pp;
    u32 vs_mesh, gs_mesh;

    vec3 cam_pos;// = {23.518875f, 15.673130f, 26.649000f};
    vec4 cam_rot;// = {-0.351835f, 0.231701f, 0.090335f, 0.902411f};
    vec4 cam_prj;// = {};
    mat3 cam_mat;// = {};
    u32 t_prev;

    float cam_speed;
} gl_helper_ctx_t;

void Perspective(
    f32 * proj,
    f32 aspect,
    f32 fov_y_rad,
    f32 n,
    f32 f)
{
    f32 d = 1.f / (f32)tan(fov_y_rad * 0.5f);
    proj[0] = d / aspect;
    proj[1] = d;
    proj[2] = (n + f) / (n - f);
    proj[3] = (2.f * n * f) / (n - f);
}

f32 Aspect(SDL_Window * sdl_window)
{
    i32 w, h;
    SDL_GetWindowSize(sdl_window, &w, &h);
    return w / (f32)h;
}


void* init_graphics() {
    gl_helper_ctx_t* ctx = (gl_helper_ctx_t*) malloc(sizeof(gl_helper_ctx_t));

    ctx->cmds = NULL;
    ctx->state_textures = NULL;
    ctx->cmd_size = 0;
    ctx->cmd_capacity = 0;

    ctx->cam_rot.x = -0.351835f;
    ctx->cam_rot.y = 0.231701f;
    ctx->cam_rot.z = 0.090335f;
    ctx->cam_rot.w = 0.902411f;
    ctx->cam_speed = 1;

    gfWindow(&ctx->sdl_window, &ctx->sdl_glcontext, 0, 0, "pdal", 1280, 720, 4);

  /* point vertex shader */
    char * vs_str = VERT_HEAD STR
    (
        layout(location = 0) uniform vec3 cam_pos;
        layout(location = 1) uniform vec4 cam_rot;
        layout(location = 2) uniform vec4 cam_prj;

        layout(binding = 0) uniform  samplerBuffer in_pos;
        layout(binding = 1) uniform  samplerBuffer in_color;

        vec4 qconj(vec4 q)
        {
            return vec4(-q.xyz, q.w);
        }

        vec3 qrot(vec3 v, vec4 q)
        {
            return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + v * q.w);
        }

        vec4 proj(vec3 mv, vec4 p)
        {
            return vec4(mv.xy * p.xy, mv.z * p.z + p.w, -mv.z);
        }

        flat out vec3 vs_color;

        void main()
        {
            vec3 pos = texelFetch(in_pos, gl_VertexID).xyz;
            vec4 c = texelFetch(in_color, gl_VertexID).xyzw;
            if (c.w < 0.0) {
                vs_color = vec3(c.w);
            } else
            {
                vs_color = vec3(c.xyz);
            }

            pos -= cam_pos;
            pos  = qrot(pos, qconj(cam_rot));

            gl_Position = proj(pos, cam_prj);
        }
        );

    char * fs_str = FRAG_HEAD STR
    (
        flat in vec3 f_color;
        out vec4 color;

        void main()
        {
            color = vec4(f_color, 1.f);
        }
        );

    char * gs_str = GEOM_HEAD STR
    (
        precision highp float;
        layout(location = 0) uniform int interactive;
        layout (points) in;
        layout (points, max_vertices = 1) out;

        flat in vec3 vs_color[];
        flat out vec3 f_color;

        void main() {
            if (interactive > 0) {
                if ((gl_PrimitiveIDIn % interactive) != 0) {
                    return;
                }
            }

            gl_Position = gl_in[0].gl_Position;
            float z = (1.0f + gl_Position.z / gl_Position.w) * 0.5f;
            gl_PointSize = mix(0.3f, 2.f, z) * (1.0 + interactive / 30.0f);

            f_color = vs_color[0];

            EmitVertex();
            EndPrimitive();
        }
        );

    ctx->vs_mesh = gfProgramCreateFromString(VERT, vs_str);
    u32 fs = gfProgramCreateFromString(FRAG, fs_str);
    ctx->gs_mesh = gfProgramCreateFromString(GEOM, gs_str);
    ctx->pp = gfProgramPipelineCreate(ctx->vs_mesh, fs, ctx->gs_mesh);

    SDL_SetRelativeMouseMode(1);
    ctx->t_prev = SDL_GetTicks();

    glEnable(DO_DEPTH);

    return ctx;
}

// probably not a good idea anyway: gpu_cmd_t have a big overhead
// #define MAX_POINTS_PER_CMD 100000000000L




void push_points(void* _ctx, float* xyz, uint8_t* rgbi, size_t count, float* minmax) {
    gl_helper_ctx_t* ctx = (gl_helper_ctx_t*)_ctx;

    size_t offset = 0;
    while (count > 0) {
        size_t s = count; // count <= MAX_POINTS_PER_CMD ? count : MAX_POINTS_PER_CMD;

        // position buffer
        gpu_buffer_t position = gfBufferCreate((gpu_buffer_t){.format = xyz_f32, .count = s});
        memcpy(position.as_f32, xyz, count * 3 * sizeof(float));

        // color buffer
        gpu_buffer_t color = gfBufferCreate((gpu_buffer_t){.format = xyzw_b8, .count = count});
        memcpy(color.as_u8, rgbi, count * 4 * sizeof(uint8_t));
        // memset(color.as_u8, 255, count * 4);
        // for (size_t i=0; i<count; i++) {
        //  color.as_u8[4*i + 0] /*.x*/ = rgbi[4*(offset + i)+0] * 255;
        //  color.as_u8[4*i + 1] /*.y*/ = rgbi[4*(offset + i)+1] * 255;
        //  color.as_u8[4*i + 2] /*.z*/ = rgbi[4*(offset + i)+2] * 255;
        //  color.as_u8[4*i + 3] /*.w*/ = rgbi[4*(offset + i)+3] * 255;
        // }

        while (ctx->cmd_capacity < (ctx->cmd_size + 1)) {
            ctx->cmd_capacity = (ctx->cmd_capacity ? ctx->cmd_capacity : 1) * 2;
            ctx->cmds = realloc(ctx->cmds, sizeof(gpu_cmd_t) * ctx->cmd_capacity);
            ctx->state_textures = realloc(ctx->state_textures, sizeof(u32) * ctx->cmd_capacity * 2);
        }

        ctx->cmds[ctx->cmd_size].count = position.count;
        ctx->cmds[ctx->cmd_size].first = 0;
        ctx->cmds[ctx->cmd_size].instance_first = 0;
        ctx->cmds[ctx->cmd_size].instance_count = 1;
        ctx->state_textures[2 * ctx->cmd_size] = position.id;
        ctx->state_textures[2 * ctx->cmd_size + 1] = color.id;
        ctx->cmd_size++;

        offset += s;
        count -= s;
    }

    ctx->cam_pos.x = minmax[1];
    ctx->cam_pos.y = minmax[3];
    ctx->cam_pos.z = minmax[5];
    float maxdiff = 0;
    for (int i=0; i<3; i++) {
        float d = minmax[2*i+1] - minmax[2*i];
        maxdiff = d > maxdiff ? d : maxdiff;
    }

    float far_z = maxdiff;

    Perspective(
        &ctx->cam_prj.x,
        Aspect(ctx->sdl_window),
        85.f * QFPC_TO_RAD,
        0.1f, 1000.f
        );

    ctx->cam_speed = maxdiff / 150.f;
}

#define INTERACTIVE_STEPS 50

void render_one_frame(void* _ctx, int interactive) {
    gl_helper_ctx_t* ctx = (gl_helper_ctx_t*)_ctx;

    glProgramUniform3fv(ctx->vs_mesh, 0, 1, &ctx->cam_pos.x);
    glProgramUniform4fv(ctx->vs_mesh, 1, 1, &ctx->cam_rot.x);
    glProgramUniform4fv(ctx->vs_mesh, 2, 1, &ctx->cam_prj.x);

    glProgramUniform1iv(ctx->gs_mesh, 0, 1, &interactive);

    interactive = 0;

    glClear(CLEAR_COLOR | CLEAR_DEPTH);

    for (size_t i=0; i<ctx->cmd_size; i++) {
        glBindTextures(0 /* starts at GL_TEXTURE0 */, 2 /* 2 buffer: position/color */, &ctx->state_textures[2*i]);

        gpu_cmd_t cmd = ctx->cmds[i];


        if (interactive) {
            #define DRIVER_NOT_BUGGY 1
            #if DRIVER_NOT_BUGGY
             // should draw some evenly spaced points but cmd.first seems ignored
            int step_offset = cmd.count / INTERACTIVE_STEPS;
            cmd.count = step_offset / INTERACTIVE_STEPS;
            for (int step=0; step<INTERACTIVE_STEPS; step++) {
                cmd.first = step * step_offset;
                gfDraw(ctx->pp, 1, &cmd);
            }
            #else
            static int next_cmd = 0;
            if (i != next_cmd) {
                continue;
            }
            cmd = ctx->cmds[next_cmd];
            next_cmd = (next_cmd + 1) % ctx->cmd_size;
            gfDraw(ctx->pp, 1, &cmd);
            break;
            #endif
        } else {
            gfDraw(ctx->pp, 1, &cmd);
        }
    }

    SDL_GL_SwapWindow(ctx->sdl_window);
    glFinish();
}

void render_loop(void* _ctx) {
    gl_helper_ctx_t* ctx = (gl_helper_ctx_t*)_ctx;

    render_one_frame(_ctx, 0);

    #if 0
    SDL_Event event;
    while(1) {
        int interactive = (SDL_WaitEventTimeout(&event, 100 /* ms */) == 1);

        u32 t_curr = SDL_GetTicks();
        if (!interactive) {
            t_prev = t_curr;
        }
        f64 dt = 0.016;//((t_curr - t_prev) * 60.0) / 1000.0;


        // SDL_PumpEvents();
        i32 mouse_x_rel = 0;
        i32 mouse_y_rel = 0;
        SDL_GetRelativeMouseState(&mouse_x_rel, &mouse_y_rel);
        const u8 * key = SDL_GetKeyboardState(NULL);

        // printf("render_loop  %d %f\n", interactive, dt);

        quatFirstPersonCamera(
          &cam_pos.x,
          &cam_rot.x,
          &cam_mat.sd_x,
          0.10f,
          0.25f * (f32)dt,
          mouse_x_rel,
          mouse_y_rel,
          key[SDL_SCANCODE_W],
          key[SDL_SCANCODE_A],
          key[SDL_SCANCODE_S],
          key[SDL_SCANCODE_D],
          key[SDL_SCANCODE_E],
          key[SDL_SCANCODE_Q]
        );

        glProgramUniform3fv(vs_mesh, 0, 1, &cam_pos.x);
        glProgramUniform4fv(vs_mesh, 1, 1, &cam_rot.x);
        glProgramUniform4fv(vs_mesh, 2, 1, &cam_prj.x);
        // glProgramUniform3fv(fs_mesh, 0, 1, &cam_pos.x);


        render_one_frame(interactive);

        // while(SDL_PollEvent(&event))
        {
          if(event.type == SDL_QUIT) {//} || event.type == SDL_KEYUP) {
            return;
          }
        }
        t_prev = t_curr;
    }
    #else
    render_one_frame(_ctx, 0);
    while(1)
    {
        u32 t_curr = SDL_GetTicks();
        f64 dt = ((t_curr - ctx->t_prev) * 60.0) / 1000.0;

        SDL_PumpEvents();
        i32 mouse_x_rel = 0;
        i32 mouse_y_rel = 0;
        SDL_GetRelativeMouseState(&mouse_x_rel, &mouse_y_rel);
        const u8 * key = SDL_GetKeyboardState(NULL);

        vec3 prev_cam_pos = ctx->cam_pos;
        vec4 prev_cam_rot = ctx->cam_rot;

        quatFirstPersonCamera(
            &ctx->cam_pos.x,
            &ctx->cam_rot.x,
            &ctx->cam_mat.sd_x,
            0.10f,
            ctx->cam_speed * (f32)dt,
            mouse_x_rel,
            mouse_y_rel,
            key[SDL_SCANCODE_W],
            key[SDL_SCANCODE_A],
            key[SDL_SCANCODE_S],
            key[SDL_SCANCODE_D],
            key[SDL_SCANCODE_E],
            key[SDL_SCANCODE_Q]
            );

        if (memcmp(&prev_cam_rot, &ctx->cam_rot, sizeof(ctx->cam_rot)) ||
            memcmp(&prev_cam_pos, &ctx->cam_pos, sizeof(ctx->cam_pos))) {
            /* draw 1 point out of 100 */
            render_one_frame(_ctx, 100);
        } else {
            render_one_frame(_ctx, 0);
        }

        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_QUIT)
                return;
        }

        ctx->t_prev = t_curr;
    }
    #endif
}
