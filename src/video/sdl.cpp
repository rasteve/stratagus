//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name sdl.cpp - SDL video support. */
//
//      (c) Copyright 1999-2011 by Lutz Sammer, Jimmy Salmon, Nehal Mistry and
//                                 Pali Rohár
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

//@{

/*----------------------------------------------------------------------------
-- Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"

#include "game.h"
#include "network.h"
#include "online_service.h"
#include "parameters.h"
#include "sound_server.h"
#include "translate.h"
#include "ui.h"
#include "unit.h"
#include "video.h"
#include "widgets.h"

#include <climits>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_syswm.h>

#ifdef USE_BEOS
#include <sys/socket.h>
#endif

#ifdef USE_WIN32
# include <shellapi.h>
#else
# ifdef DEBUG
#  include <signal.h>
# endif
# include <sys/stat.h>
# include <sys/types.h>
#endif

/*----------------------------------------------------------------------------
--  Declarations
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

SDL_Window *TheWindow; /// Internal screen
SDL_Renderer *TheRenderer = nullptr; /// Internal screen
SDL_Texture *TheTexture; /// Internal screen
SDL_Surface *TheScreen; /// Internal screen

static SDL_Rect Rects[100];
static int NumRects;

static std::map<int, std::string> Key2Str;
static std::map<std::string, int> Str2Key;

/// Frame length in ms
static double FrameTicks;

/// Target refresh rate for renderer
int RefreshRate = 0;

const EventCallback *Callbacks;

bool IsSDLWindowVisible = true;

/// Just a counter to cache window data on in other places when the size changes
uint8_t SizeChangeCounter = 0;

static bool dummyRenderer = false;

uint32_t SDL_CUSTOM_KEY_UP;

/*----------------------------------------------------------------------------
--  Sync
----------------------------------------------------------------------------*/

static int GetRefreshRate()
{
	if (!RefreshRate) {
		int displayCount = SDL_GetNumVideoDisplays();
		SDL_DisplayMode mode;
		for (int i = 0; i < displayCount; i++) {
			SDL_GetDesktopDisplayMode(0, &mode);
			if (mode.refresh_rate > RefreshRate) {
				RefreshRate = mode.refresh_rate;
			}
		}
		if (!RefreshRate) {
			RefreshRate = 60;
		}
	}
	return RefreshRate;
}

/**
**  Initialise video sync.
**  Calculate the length of video frame and any simulation skips.
**
**  @see CyclesPerSecond @see SkipCycles @see SkipFrames @see FrameTicks
*/
void SetVideoSync()
{
	int fps = GetRefreshRate();
	int nativeFps = fps;
	if (fps < CyclesPerSecond) {
		fprintf(stdout, "WARNING: Game speed is faster than monitor refresh rate.\n");
		FrameTicks = 1000.0 / CyclesPerSecond;
		SDL_GL_SetSwapInterval(0); // disable vsync, so we can run faster than the refresh
	} else {
		FrameTicks = 1000.0 / fps;
		if (SDL_GL_SetSwapInterval(-1) < 0) { // try to set adaptive vsync
			SDL_GL_SetSwapInterval(1); // if it failed, set vsync
		}
	}
	SkipCycles = (static_cast<double>(fps) / CyclesPerSecond) - 1;

	DebugPrint("native fps: %d, render frame skip: %d, game cycle skip: %f\n",
	           nativeFps,
	           Preference.FrameSkip,
	           SkipCycles);
}

/*----------------------------------------------------------------------------
--  Video
----------------------------------------------------------------------------*/

/**
**  Initialize SDLKey to string map
*/
static void InitKey2Str()
{
	Str2Key[_("esc")] = SDLK_ESCAPE;

	if (!Key2Str.empty()) {
		return;
	}

	Key2Str[SDLK_BACKSPACE] = "backspace";
	Key2Str[SDLK_TAB] = "tab";
	Key2Str[SDLK_CLEAR] = "clear";
	Key2Str[SDLK_RETURN] = "return";
	Key2Str[SDLK_PAUSE] = "pause";
	Key2Str[SDLK_ESCAPE] = "escape";
	Key2Str[SDLK_SPACE] = " ";
	Key2Str[SDLK_EXCLAIM] = "!";
	Key2Str[SDLK_QUOTEDBL] = "\"";
	Key2Str[SDLK_HASH] = "#";
	Key2Str[SDLK_DOLLAR] = "$";
	Key2Str[SDLK_AMPERSAND] = "&";
	Key2Str[SDLK_QUOTE] = "'";
	Key2Str[SDLK_LEFTPAREN] = "(";
	Key2Str[SDLK_RIGHTPAREN] = ")";
	Key2Str[SDLK_ASTERISK] = "*";
	Key2Str[SDLK_PLUS] = "+";
	Key2Str[SDLK_COMMA] = ",";
	Key2Str[SDLK_MINUS] = "-";
	Key2Str[SDLK_PERIOD] = ".";
	Key2Str[SDLK_SLASH] = "/";

	for (int i = SDLK_0; i <= SDLK_9; ++i) {
		Key2Str[i] = std::string(1, static_cast<char>(i));
	}

	Key2Str[SDLK_COLON] = ":";
	Key2Str[SDLK_SEMICOLON] = ";";
	Key2Str[SDLK_LESS] = "<";
	Key2Str[SDLK_EQUALS] = "=";
	Key2Str[SDLK_GREATER] = ">";
	Key2Str[SDLK_QUESTION] = "?";
	Key2Str[SDLK_AT] = "@";
	Key2Str[SDLK_LEFTBRACKET] = "[";
	Key2Str[SDLK_BACKSLASH] = "\\";
	Key2Str[SDLK_RIGHTBRACKET] = "]";
	Key2Str[SDLK_BACKQUOTE] = "`";

	for (int i = SDLK_a; i <= SDLK_z; ++i) {
		Key2Str[i] = std::string(1, static_cast<char>(i));
	}

	Key2Str[SDLK_DELETE] = "delete";

	for (int i = SDLK_KP_0; i <= SDLK_KP_9; ++i) {
		Key2Str[i] = "kp_" + std::to_string(i - SDLK_KP_0);
	}

	Key2Str[SDLK_KP_PERIOD] = "kp_period";
	Key2Str[SDLK_KP_DIVIDE] = "kp_divide";
	Key2Str[SDLK_KP_MULTIPLY] = "kp_multiply";
	Key2Str[SDLK_KP_MINUS] = "kp_minus";
	Key2Str[SDLK_KP_PLUS] = "kp_plus";
	Key2Str[SDLK_KP_ENTER] = "kp_enter";
	Key2Str[SDLK_KP_EQUALS] = "kp_equals";
	Key2Str[SDLK_UP] = "up";
	Key2Str[SDLK_DOWN] = "down";
	Key2Str[SDLK_RIGHT] = "right";
	Key2Str[SDLK_LEFT] = "left";
	Key2Str[SDLK_INSERT] = "insert";
	Key2Str[SDLK_HOME] = "home";
	Key2Str[SDLK_END] = "end";
	Key2Str[SDLK_PAGEUP] = "pageup";
	Key2Str[SDLK_PAGEDOWN] = "pagedown";

	for (int i = SDLK_F1; i <= SDLK_F15; ++i) {
		Key2Str[i] = "f" + std::to_string(i - SDLK_F1 + 1);
		Str2Key["F" + std::to_string(i - SDLK_F1 + 1)] = i;
	}

	Key2Str[SDLK_HELP] = "help";
	Key2Str[SDLK_PRINTSCREEN] = "print";
	Key2Str[SDLK_SYSREQ] = "sysreq";
	Key2Str[SDLK_PAUSE] = "break";
	Key2Str[SDLK_MENU] = "menu";
	Key2Str[SDLK_POWER] = "power";
	//Key2Str[SDLK_EURO] = "euro";
	Key2Str[SDLK_UNDO] = "undo";
}

#ifdef USE_WIN32
enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
};

static void setDpiAware() {
	HRESULT(WINAPI *SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS dpiAwareness); // Windows 8.1 and later

	if (void* shcoreDLL = SDL_LoadObject("SHCORE.DLL")) {
		SetProcessDpiAwareness = (HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS)) SDL_LoadFunction(shcoreDLL, "SetProcessDpiAwareness");
	} else {
		SetProcessDpiAwareness = nullptr;
	}
	if (SetProcessDpiAwareness) {
		/* Try Windows 8.1+ version */
		HRESULT result = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
		DebugPrint("called SetProcessDpiAwareness: %d", (result == S_OK) ? 1 : 0);
	} else {
		if (void* userDLL = SDL_LoadObject("USER32.DLL")) {
			BOOL(WINAPI *SetProcessDPIAware)(void); // Vista and later
			SetProcessDPIAware = (BOOL(WINAPI *)(void)) SDL_LoadFunction(userDLL, "SetProcessDPIAware");
			if (SetProcessDPIAware) {
				/* Try Vista - Windows 8 version.
				   This has a constant scale factor for all monitors.
				*/
				BOOL success = SetProcessDPIAware();
				DebugPrint("called SetProcessDPIAware: %d", (int)success);
			}
		}
		// In any case, on these old Windows versions we have to do a bit of
		// compatibility hacking. Windows 7 and below don't play well with
		// opengl rendering and (for some odd reason) fullscreen.
		fprintf(stdout, "\n!!! Detected old Windows version - forcing software renderer and windowed mode!!!\n\n");
		SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
		VideoForceFullScreen = 1;
		Video.FullScreen = 0;
	}

}
#else
static void setDpiAware() {
}
#endif

/**
**  Initialize the video part for SDL.
*/
void InitVideoSdl()
{
	Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI;

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		// Fix tablet input in full-screen mode
		SDL_setenv("SDL_MOUSE_RELATIVE", "0", 1);
		int res = SDL_Init(
					  SDL_INIT_AUDIO | SDL_INIT_VIDEO |
					  SDL_INIT_EVENTS | SDL_INIT_TIMER);
		if (res < 0) {
			ErrorPrint("Couldn't initialize SDL: %s\n", SDL_GetError());
			exit(1);
		}

		SDL_CUSTOM_KEY_UP = SDL_RegisterEvents(1);
		SDL_StartTextInput();

		// Clean up on exit
		atexit(SDL_Quit);

		// If debug is enabled, Stratagus disable SDL Parachute.
		// So we need gracefully handle segfaults and aborts.
#if defined(DEBUG) && !defined(USE_WIN32)
		const auto cleanExit = [](int) {
			// Clean SDL
			SDL_Quit();
			// Reestablish normal behaviour for next abort call
			signal(SIGABRT, SIG_DFL);
			// Generates a core dump
			abort();
		};
		signal(SIGSEGV, +cleanExit);
		signal(SIGABRT, +cleanExit);
#endif
	}

	// Initialize the display

	setDpiAware();

	// Sam said: better for windows.
	/* SDL_HWSURFACE|SDL_HWPALETTE | */
	if (Video.FullScreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
		flags |= SDL_WINDOW_RESIZABLE;
	}

	if (!Video.Width || !Video.Height) {
		Video.Width = 640;
		Video.Height = 480;
	}
	if (!Video.WindowWidth || !Video.WindowHeight) {
		Video.WindowWidth = Video.Width;
		Video.WindowHeight = Video.Height;
	}
	if (!Video.Depth) {
		Video.Depth = 32;
	}

	const char *win_title = "Stratagus";
	// Set WindowManager Title
	if (!FullGameName.empty()) {
		win_title = FullGameName.c_str();
	} else if (!Parameters::Instance.applicationName.empty()) {
		win_title = Parameters::Instance.applicationName.c_str();
	}

	TheWindow = SDL_CreateWindow(win_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                             Video.WindowWidth, Video.WindowHeight, flags);
	if (TheWindow == nullptr) {
		ErrorPrint("Couldn't set %dx%dx%d video mode: %s\n",
		           Video.Width,
		           Video.Height,
		           Video.Depth,
		           SDL_GetError());
		exit(1);
	}
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	int rendererFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;
	if (!Parameters::Instance.benchmark) {
		rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
	}
	if (!TheRenderer) {
		TheRenderer = SDL_CreateRenderer(TheWindow, -1, rendererFlags);
	}
	SDL_RendererInfo rendererInfo;
	if (!SDL_GetRendererInfo(TheRenderer, &rendererInfo)) {
		printf("[Renderer] %s\n", rendererInfo.name);
		if (strlen(rendererInfo.name) == 0) {
			dummyRenderer = true;
		}
		if (starts_with(rendererInfo.name, "opengl")) {
			LoadShaderExtensions();
		}
	}
	SDL_SetRenderDrawColor(TheRenderer, 0, 0, 0, 255);
	Video.ResizeScreen(Video.Width, Video.Height);

// #ifdef USE_WIN32
// 	HWND hwnd = nullptr;
// 	HICON hicon = nullptr;
// 	SDL_SysWMinfo info;
// 	SDL_VERSION(&info.version);

// 	if (SDL_GetWindowWMInfo(TheWindow, &info)) {
// 		hwnd = info.win.window;
// 	}

// 	if (hwnd) {
// 		hicon = ExtractIcon(GetModuleHandle(nullptr), Parameters::Instance.applicationName.c_str(), 0);
// 	}

// 	if (hicon) {
// 		SendMessage(hwnd, (UINT)WM_SETICON, ICON_SMALL, (LPARAM)hicon);
// 		SendMessage(hwnd, (UINT)WM_SETICON, ICON_BIG, (LPARAM)hicon);
// 	}
// #endif

#if !defined(USE_WIN32) && !defined(USE_MAEMO)
		std::string FullGameNameL = FullGameName;
		for (size_t i = 0; i < FullGameNameL.size(); ++i) {
			FullGameNameL[i] = tolower(FullGameNameL[i]);
		}

		std::string ApplicationName = Parameters::Instance.applicationName;
		std::string ApplicationNameL = ApplicationName;
		for (auto& c : ApplicationNameL) {
			c = tolower(c);
		}

		std::vector<fs::path> pixmaps
		{
			fs::path(PIXMAPS) / (FullGameName + ".png"),
			fs::path(PIXMAPS) / (FullGameNameL + ".png"),
			fs::path("/usr/share/pixmaps") / (FullGameName + ".png"),
			fs::path("/usr/share/pixmaps") / (FullGameNameL + ".png"),
			fs::path(PIXMAPS) / (ApplicationName + ".png"),
			fs::path(PIXMAPS) / (ApplicationNameL + ".png"),
			fs::path("/usr/share/pixmaps") / (ApplicationName + ".png"),
			fs::path("/usr/share/pixmaps") / (ApplicationNameL + ".png"),
			fs::path(PIXMAPS) / "Stratagus.png",
			fs::path(PIXMAPS) / "stratagus.png",
			fs::path("/usr/share/pixmaps/Stratagus.png"),
			fs::path("/usr/share/pixmaps/stratagus.png")
		};

		std::shared_ptr<CGraphic> g;
		SDL_Surface *icon = nullptr;

		for (const auto &p : pixmaps) {
			if (fs::exists(p)) {
				g = CGraphic::New(p.u8string());
				g->Load();
				icon = g->getSurface();
				if (icon) { break; }
			}
		}
		if (icon) {
			SDL_SetWindowIcon(TheWindow, icon);
		}
#endif
	Video.FullScreen = (SDL_GetWindowFlags(TheWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
	Video.Depth = TheScreen->format->BitsPerPixel;

	// Must not allow SDL to switch to relative mouse coordinates when going
	// fullscreen. So we don't hide the cursor, but instead set a transparent
	// 1px cursor
	Uint8 emptyCursor[] = {'\0'};
	Video.blankCursor.reset(SDL_CreateCursor(emptyCursor, emptyCursor, 1, 1, 0, 0));
	SDL_SetCursor(Video.blankCursor.get());

	InitKey2Str();

	ColorBlack = Video.MapRGB(TheScreen->format, 0, 0, 0);
	ColorDarkGreen = Video.MapRGB(TheScreen->format, 48, 100, 4);
	ColorLightBlue = Video.MapRGB(TheScreen->format, 52, 113, 166);
	ColorBlue = Video.MapRGB(TheScreen->format, 0, 0, 252);
	ColorOrange = Video.MapRGB(TheScreen->format, 248, 140, 20);
	ColorWhite = Video.MapRGB(TheScreen->format, 252, 248, 240);
	ColorLightGray = Video.MapRGB(TheScreen->format, 192, 192, 192);
	ColorGray = Video.MapRGB(TheScreen->format, 128, 128, 128);
	ColorDarkGray = Video.MapRGB(TheScreen->format, 64, 64, 64);
	ColorRed = Video.MapRGB(TheScreen->format, 252, 0, 0);
	ColorGreen = Video.MapRGB(TheScreen->format, 0, 252, 0);
	ColorYellow = Video.MapRGB(TheScreen->format, 252, 252, 0);

	UI.MouseWarpPos.x = UI.MouseWarpPos.y = -1;
}

/**
**  Invalidate some area
**
**  @param x  screen pixel X position.
**  @param y  screen pixel Y position.
**  @param w  width of rectangle in pixels.
**  @param h  height of rectangle in pixels.
*/
void InvalidateArea(int x, int y, int w, int h)
{
	Assert(NumRects != sizeof(Rects) / sizeof(*Rects));
	Assert(x >= 0 && y >= 0 && x + w <= Video.Width && y + h <= Video.Height);
	Rects[NumRects].x = x;
	Rects[NumRects].y = y;
	Rects[NumRects].w = w;
	Rects[NumRects].h = h;
	++NumRects;
}

/**
**  Invalidate whole window
*/
void Invalidate()
{
	Rects[0].x = 0;
	Rects[0].y = 0;
	Rects[0].w = Video.Width;
	Rects[0].h = Video.Height;
	NumRects = 1;
}

static bool isTextInput(int key) {
	return key >= 32 && key <= 128 && !(KeyModifiers & (ModifierAlt | ModifierControl | ModifierSuper));
}

/**
**  Handle interactive input event.
**
**  @param callbacks  Callback structure for events.
**  @param event      SDL event structure pointer.
*/
static void SdlDoEvent(const EventCallback &callbacks, SDL_Event &event)
{
	unsigned int keysym = 0;

	switch (event.type) {
		case SDL_MOUSEBUTTONDOWN:
			event.button.y = static_cast<int>(std::floor(event.button.y / Video.VerticalPixelSize + 0.5));
			InputMouseButtonPress(callbacks, SDL_GetTicks(), event.button.button);
			break;

		case SDL_MOUSEBUTTONUP:
			event.button.y = static_cast<int>(std::floor(event.button.y / Video.VerticalPixelSize + 0.5));
			InputMouseButtonRelease(callbacks, SDL_GetTicks(), event.button.button);
			break;

		case SDL_MOUSEMOTION:
			event.motion.y = static_cast<int>(std::floor(event.button.y / Video.VerticalPixelSize + 0.5));
			InputMouseMove(callbacks, SDL_GetTicks(), event.motion.x, event.motion.y);
			break;

		case SDL_MOUSEWHEEL:
			{   // similar to Squeak, we fabricate Ctrl+Alt+PageUp/Down for wheel events
				SDL_Keycode key = event.wheel.y > 0 ? SDLK_PAGEUP : SDLK_PAGEDOWN;
				SDL_Event event;
				SDL_zero(event);
				event.type = SDL_KEYDOWN;
				event.key.keysym.sym = SDLK_LCTRL;
				SDL_PushEvent(&event);
				SDL_zero(event);
				event.type = SDL_KEYDOWN;
				event.key.keysym.sym = SDLK_LALT;
				SDL_PushEvent(&event);
				SDL_zero(event);
				event.type = SDL_KEYDOWN;
				event.key.keysym.sym = key;
				SDL_PushEvent(&event);
				SDL_zero(event);
				event.type = SDL_KEYUP;
				event.key.keysym.sym = key;
				SDL_PushEvent(&event);
				SDL_zero(event);
				event.type = SDL_KEYUP;
				event.key.keysym.sym = SDLK_LALT;
				SDL_PushEvent(&event);
				SDL_zero(event);
				event.type = SDL_KEYUP;
				event.key.keysym.sym = SDLK_LCTRL;
				SDL_PushEvent(&event);
			}
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				SizeChangeCounter++;
				break;

				case SDL_WINDOWEVENT_ENTER:
				case SDL_WINDOWEVENT_LEAVE:
				{
					static bool InMainWindow = true;

					if (InMainWindow && (event.window.event == SDL_WINDOWEVENT_LEAVE)) {
						InputMouseExit(callbacks, SDL_GetTicks());
					}
					InMainWindow = (event.window.event == SDL_WINDOWEVENT_ENTER);
				}
				break;

				case SDL_WINDOWEVENT_FOCUS_GAINED:
				case SDL_WINDOWEVENT_FOCUS_LOST:
				{
				if (!IsNetworkGame() && Preference.PauseOnLeave /*(SDL_GetWindowFlags(TheWindow) & SDL_WINDOW_INPUT_FOCUS)*/) {
					static bool DoTogglePause = false;

					if (IsSDLWindowVisible && (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
						IsSDLWindowVisible = false;
						if (!GamePaused) {
							DoTogglePause = !GamePaused;
							GamePaused = true;
						}
					} else if (!IsSDLWindowVisible && (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)) {
						IsSDLWindowVisible = true;
						if (GamePaused && DoTogglePause) {
							DoTogglePause = false;
							GamePaused = false;
						}
					}
				}
				}
				break;
			}
			break;

		case SDL_TEXTINPUT:
		{
			char *text = event.text.text;
			if (isTextInput((uint8_t) text[0])) {
				// we only accept US-ascii chars for now
				char lastKey = text[0];
				InputKeyButtonPress(callbacks, SDL_GetTicks(), lastKey, lastKey);
				// fabricate a keyup event for later
				SDL_Event event;
				SDL_zero(event);
				event.type = SDL_CUSTOM_KEY_UP;
				event.user.code = lastKey;
				SDL_PeepEvents(&event, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
			}
			break;
		}

		case SDL_KEYDOWN:
			keysym = event.key.keysym.sym;
			if (!isTextInput(keysym)) {
				// only report non-printing keys here, the characters will be reported with the textinput event
				InputKeyButtonPress(callbacks, SDL_GetTicks(), keysym, keysym < 128 ? keysym : 0);
			}
			break;

		case SDL_KEYUP:
			keysym = event.key.keysym.sym;
			if (!isTextInput(keysym)) {
				// only report non-printing keys here, the characters will be reported with the textinput event
				InputKeyButtonRelease(callbacks, SDL_GetTicks(), keysym, keysym < 128 ? keysym : 0);
			}
			break;

		case SDL_QUIT:
			Exit(0);
			break;

		default:
			if (event.type == SDL_SOUND_FINISHED) {
				HandleSoundEvent(event);
			} else if (event.type == SDL_CUSTOM_KEY_UP) {
				char key = static_cast<char>(event.user.code);
				InputKeyButtonRelease(callbacks, SDL_GetTicks(), key, key);
			}
			break;
	}

	if (&callbacks == GetCallbacks()) {
		handleInput(&event);
	}
}

/**
**  Set the current callbacks
*/
void SetCallbacks(const EventCallback *callbacks)
{
	Callbacks = callbacks;
}

/**
**  Get the current callbacks
*/
const EventCallback *GetCallbacks()
{
	return Callbacks;
}

/**
**  Wait for interactive input event for one frame.
**
**  Handles system events, joystick, keyboard, mouse.
**  Handles the network messages.
**  Handles the sound queue.
**
**  All events available are fetched. Sound and network only if available.
**  Returns if the time for one frame is over.
*/
void WaitEventsOneFrame()
{
	if (dummyRenderer) {
		return;
	}

	Uint32 ticks = SDL_GetTicks();
	if (ticks > NextFrameTicks) { // We are too slow :(
		++SlowFrameCounter;
	}

	InputMouseTimeout(*GetCallbacks(), ticks);
	InputKeyTimeout(*GetCallbacks(), ticks);
	CursorAnimate(ticks);

	int interrupts = Parameters::Instance.benchmark;

	for (;;) {
		// Time of frame over? This makes the CPU happy. :(
		ticks = SDL_GetTicks();
		if (!interrupts && ticks < NextFrameTicks) {
			SDL_Delay(NextFrameTicks - ticks);
			ticks = SDL_GetTicks();
		}
		while (ticks >= (unsigned long)(NextFrameTicks)) {
			++interrupts;
			NextFrameTicks += FrameTicks;
		}

		SDL_Event event[1];
		const int i = SDL_PollEvent(event);
		if (i) { // Handle SDL event
			SdlDoEvent(*GetCallbacks(), *event);
		}

		// Network
		int s = 0;
		if (IsNetworkGame()) {
			s = NetworkFildes.HasDataToRead(0);
			if (s > 0) {
				if (GetCallbacks()->NetworkEvent) {
					GetCallbacks()->NetworkEvent();
				}
			}
		}

		// Online session
		OnlineContextHandler->doOneStep();

		// No more input and time for frame over: return
		if (!i && s <= 0 && interrupts) {
			break;
		}
	}
	handleInput(nullptr);

	if (SkipGameCycle < 0) {
		SkipGameCycle += SkipCycles;
	} else {
		SkipGameCycle--;
	}
}

/**
**  Realize video memory.
*/

static Uint32 LastTick = 0;

static void RenderBenchmarkOverlay()
{
	int RefreshRate = GetRefreshRate();
	// show a bar representing fps, where the entire bar is the max refresh rate of attached displays
	Uint32 nextTick = SDL_GetTicks();
	Uint32 frameTime = nextTick - LastTick;
	int fps = std::min(RefreshRate, static_cast<int>(frameTime > 0 ? (1000.0 / frameTime) : 0));
	LastTick = nextTick;

	// draw the full bar
	SDL_SetRenderDrawColor(TheRenderer, 255, 0, 0, 255);
	SDL_Rect frame = { Video.Width - 10, 2, 8, RefreshRate };
	SDL_RenderDrawRect(TheRenderer, &frame);

	// draw the inner fps gage
	SDL_SetRenderDrawColor(TheRenderer, 0, 255, 0, 255);
	SDL_Rect bar = { Video.Width - 8, 2 + RefreshRate - fps, 4, fps };
	SDL_RenderFillRect(TheRenderer, &bar);

	SDL_SetRenderDrawColor(TheRenderer, 0, 0, 0, 255);
}

void RealizeVideoMemory()
{
	++FrameCounter;
	if (dummyRenderer) {
		return;
	}
	if (Preference.FrameSkip && (FrameCounter & Preference.FrameSkip)) {
		return;
	}
	if (NumRects) {
		//SDL_UpdateWindowSurfaceRects(TheWindow, Rects, NumRects);
		SDL_UpdateTexture(TheTexture, nullptr, TheScreen->pixels, TheScreen->pitch);
		if (!RenderWithShader(TheRenderer, TheWindow, TheTexture)) {
			SDL_RenderClear(TheRenderer);
			//for (int i = 0; i < NumRects; i++)
			//    SDL_UpdateTexture(TheTexture, &Rects[i], TheScreen->pixels, TheScreen->pitch);
			SDL_RenderCopy(TheRenderer, TheTexture, nullptr, nullptr);
		}
		if (Parameters::Instance.benchmark) {
			RenderBenchmarkOverlay();
		}
		SDL_RenderPresent(TheRenderer);
		NumRects = 0;
	}
	if (!Preference.HardwareCursor) {
		HideCursor();
	}
}

/**
**  Lock the screen for write access.
*/
void SdlLockScreen()
{
	if (SDL_MUSTLOCK(TheScreen)) {
		SDL_LockSurface(TheScreen);
	}
}

/**
**  Unlock the screen for write access.
*/
void SdlUnlockScreen()
{
	if (SDL_MUSTLOCK(TheScreen)) {
		SDL_UnlockSurface(TheScreen);
	}
}

/**
**  Convert a SDLKey to a string
*/
const char *SdlKey2Str(int key)
{
	return Key2Str[key].c_str();
}

/**
**  Convert a string to SDLKey
*/
int Str2SdlKey(const char *str)
{
	InitKey2Str();

	for (auto &[sdlkey, s] : Key2Str) {
		if (!strcasecmp(str, s.c_str())) {
			return sdlkey;
		}
	}
	for (auto &[s, sdlkey] : Str2Key) {
		if (!strcasecmp(str, s.c_str())) {
			return sdlkey;
		}
	}
	return 0;
}

/**
**  Check if the mouse is grabbed
*/
bool SdlGetGrabMouse()
{
	return SDL_GetWindowGrab(TheWindow);
}

/**
**  Toggle grab mouse.
**
**  @param mode  Wanted mode, 1 grab, -1 not grab, 0 toggle.
*/
void ToggleGrabMouse(int mode)
{
	bool grabbed = SdlGetGrabMouse();

	if (mode <= 0 && grabbed) {
		SDL_SetWindowGrab(TheWindow, SDL_FALSE);
	} else if (mode >= 0 && !grabbed) {
		SDL_SetWindowGrab(TheWindow, SDL_TRUE);
	}
}

/**
**  Toggle full screen mode.
*/
void ToggleFullScreen()
{
	if (!TheWindow) { // don't bother if there's no surface.
		return;
	}
	const Uint32 flags = SDL_GetWindowFlags(TheWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP;
	SDL_SetWindowFullscreen(TheWindow, flags ^ SDL_WINDOW_FULLSCREEN_DESKTOP);

#ifdef USE_WIN32
	Invalidate(); // Update display
#endif
	Video.FullScreen = (flags ^ SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

//@}
