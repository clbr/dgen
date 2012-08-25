#ifdef WITH_JOYSTICK

#include <SDL.h>
#include <SDL_joystick.h>
#include "md.h"
#include "md-phil.h"

static SDL_Joystick *js_handle[2] = { NULL, NULL };
int js_index[2] = { -1, -1 };

void md::init_joysticks(int js1, int js2) {
  // Initialize the joystick support
  // Thanks to Cameron Moore <cameron@unbeatenpath.net>
  if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
    {
      fprintf(stderr, "joystick: Unable to initialize joystick system\n");
      return;
    }

  // Open the first couple of joysticks, if possible
  js_handle[0] = SDL_JoystickOpen(js1);
  js_handle[1] = SDL_JoystickOpen(js2);

  // If neither opened, quit
  if(!(js_handle[0] || js_handle[1]))
    {
      fprintf(stderr, "joystick: Unable to open any joysticks\n");
      return;
    }

  // Print the joystick names
  printf("joystick: Using ");
  if(js_handle[0]) {
    printf("%s (#%d) as pad1 ", SDL_JoystickName(js1), js1);
    js_index[0] = js1;
  }
  if(js_handle[0] && js_handle[1]) printf("and ");
  if(js_handle[1]) {
    printf("%s (#%d) as pad2 ", SDL_JoystickName(js2), js2);
    js_index[1] = js2;
  }
  printf("\n");

  // Enable joystick events
  SDL_JoystickEventState(SDL_ENABLE);
}

void md::deinit_joysticks()
{
	if (js_handle[0] != NULL) {
		SDL_JoystickClose(js_handle[0]);
		js_handle[0] = NULL;
		js_index[0] = -1;
	}
	if (js_handle[1] != NULL) {
		SDL_JoystickClose(js_handle[1]);
		js_handle[1] = NULL;
		js_index[1] = -1;
	}
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

#else // WITH_JOYSTICK

// Avoid empty translation unit.
typedef int md_phil;

#endif // WITH_JOYSTICK
