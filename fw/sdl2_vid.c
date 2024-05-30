#include <SDL2/SDL.h>

#include "fw_priv.h"

// XXX: Enums (keyboard)

enum eKey
{
  eKey_up,
  eKey_down,
  eKey_left,
  eKey_right,
  eKey_shoot,
  eKey_cancel,
  eKey_pause,
  eKey_count,
};
typedef enum eKey eKey;

enum eKeyState
{
  eKeyState_off = 0x00,        // 0b00
  eKeyState_up = 0x01,         // 0b01
  eKeyState_pressed = 0x02,    // 0b10
  eKeyState_held = 0x03,       // 0b11
  eKeyState_active_bit = 0x02, // 0b10
};
typedef enum eKeyState eKeyState;

// XXX: Static variables

static VIDINFO s_video = {0};

static SDL_Window* s_window = NULL;
static SDL_Renderer* s_renderer = NULL;
static SDL_Texture* s_texture = NULL;

static unsigned char s_key_states[eKey_count] = {0};

static SDL_Scancode s_key_map[eKey_count] = {
  [eKey_up] = SDL_SCANCODE_W,
  [eKey_down] = SDL_SCANCODE_A,
  [eKey_left] = SDL_SCANCODE_S,
  [eKey_right] = SDL_SCANCODE_D,
  [eKey_shoot] = SDL_SCANCODE_SPACE,
  [eKey_cancel] = SDL_SCANCODE_ESCAPE,
  [eKey_pause] = SDL_SCANCODE_P,
};

// Small hack to keep the initial resolution in memory.
// Relying on s_video.width will make scaling impossible.
static int s_init_window_width;

// XXX: Keyboard functions

static void
def_keyboard_func(int key)
{
  (void)key;
}
static void (*keyboard_func)(int key) = def_keyboard_func;
static void (*keyboardup_func)(int key) = def_keyboard_func;

// is_down is a bool
static void
key_state_update(unsigned char* state, unsigned char is_down)
{
  switch (*state) // look at the previous state
  {
    case eKeyState_held:
    case eKeyState_pressed:
      *state = is_down ? eKeyState_held : eKeyState_up;
      break;
    case eKeyState_off:
    case eKeyState_up:
    default:
      *state = is_down ? eKeyState_pressed : eKeyState_off;
      break;
  }
}

static void
key_process(void)
{
  int numkeys = 0;
  const unsigned char* keystate = SDL_GetKeyboardState(&numkeys);

  for (int i = 0; i < eKey_count; ++i)
  {
    const int scancode = s_key_map[i];

    unsigned char is_down = 0; // false

    if (scancode && scancode < numkeys)
    {
      is_down |= 0 != keystate[scancode];
    }

    key_state_update(&s_key_states[i], is_down);
  }
}

// XXX: Debug functions

#ifndef DEBUG
#define DEBUG 0
#endif

static void
debug_printf(const char* fmt, const char* fn, const char* fmt2, ...)
{
#if DEBUG
  va_list ap;

  printf(fmt, fn);

  va_start(ap, fmt2);
  vprintf(fmt2, ap);
  va_end(ap);

  puts("\033[0m");
#else
  (void)fmt;
  (void)fn;
  (void)fmt2;
#endif
}

static inline void
SDL_PanicCheck(const SDL_bool condition, const char* function)
{
#if DEBUG
  if (condition)
  {
    debug_printf("\033[0;31m[PANIC] SDL_%s(): ", function, "%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
#else
  (void)condition;
  (void)function;
#endif
}

// XXX: extern functions

extern int
vid_open(char* title, int width, int height, int scale, int flags)
{
  // Backup default values
  s_init_window_width = width;

  // Video info init
  s_video = (VIDINFO){
    .flags = flags,
    .bytespp = 4, // 4 bytes per pixel
    .width = FW_BYTE_ALIGN(width, 4),
    .height = FW_BYTE_ALIGN(height, 4),
    .pitch = FW_CALC_PITCH(FW_BYTE_ALIGN(width, 4), 4),
    .video = calloc(FW_BYTE_ALIGN(width, 4) * FW_BYTE_ALIGN(height, 4), 4),
  };

  // SDL2 Init
  const int sdl_init_result = SDL_Init(SDL_INIT_VIDEO);
  SDL_PanicCheck(sdl_init_result, "Init");

  s_window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_UTILITY);
  SDL_PanicCheck(!s_window, "CreateWindow");

  s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_PanicCheck(!s_renderer, "CreateRenderer");

  s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
  SDL_PanicCheck(!s_texture, "CreateTexture");

  const int sdl_rsls_result = SDL_RenderSetLogicalSize(s_renderer, width, height);
  SDL_PanicCheck(sdl_rsls_result, "RenderSetLogicalSize");

  const int sdl_rsis_result = SDL_RenderSetIntegerScale(s_renderer, SDL_TRUE);
  SDL_PanicCheck(sdl_rsls_result, "RenderSetIntegerScale");
}

extern void
clk_init(void)
{
  // Handled automatically by SDL2
}

extern void
clk_mode(int mode)
{
  puts("clk_mode");
}

extern utime
clk_sample(void)
{
  return SDL_GetTicks();
}

extern void
sys_keybfunc(void (*keyboard)(int key))
{
  keyboard_func = keyboard;
}

extern void
sys_keybupfunc(void (*keyboard)(int key))
{
  keyboard_func = keyboard;
}

extern void
vid_blit()
{
  SDL_UpdateTexture(s_texture, NULL, s_video.video, s_video.bytespp * s_init_window_width);
  SDL_RenderClear(s_renderer);
  SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
  SDL_RenderPresent(s_renderer);
}

extern VIDINFO*
vid_getinfo(void)
{
  return &s_video;
}

extern void
vid_sync(void)
{
  // Handled automatically by SDL2
}

extern int
wnd_osm_handle(void)
{
  SDL_PumpEvents();
  SDL_Event event = {0};

  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_QUIT:
        sys_kill();
        return 0;
        break;

      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
        {
          sys_kill();
          return 0;
          break;
        }
        break;

      case SDL_KEYDOWN:
      case SDL_KEYUP:
        key_process();
        for (int i = 0; i < eKey_count; ++i)
        {
          if (s_key_map[i])
          {
            keyboard_func(SDL_GetKeyFromScancode(s_key_map[i]));
          }
          else
          {
            keyboardup_func(SDL_GetKeyFromScancode(s_key_map[i]));
          }
        }
        break;

      default:
        break;
    }
  }

  return 1;
}

extern void
wnd_term(void)
{
  SDL_DestroyTexture(s_texture);
  SDL_DestroyRenderer(s_renderer);
  SDL_DestroyWindow(s_window);
  SDL_Quit();
}
