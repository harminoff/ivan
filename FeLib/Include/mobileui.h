#ifndef __MOBILEUI_H__
#define __MOBILEUI_H__

#ifdef ANDROID
#include "SDL.h"

namespace mobileui
{
  enum { REDRAW_EVENT_CODE = 0x4956414E,
         DIRECTION_REPEAT_EVENT_CODE = 0x4956414F };
  enum actiongroup
  {
    ACTION_CONTEXT = 0,
    ACTION_ITEMS = 1,
    ACTION_CHARACTER = 2,
    ACTION_MOVE = 3,
    ACTION_SYSTEM = 4,
    ACTION_GROUPS = 5
  };

  struct touchresult
  {
    enum kind { TOUCH_NONE, TOUCH_KEY, TOUCH_MOUSE, TOUCH_REDRAW } Kind;
    int KeyCode;
    int MouseX;
    int MouseY;
    touchresult() : Kind(TOUCH_NONE), KeyCode(0), MouseX(0), MouseY(0) { }
  };

  void SetSafeInsets(int Left, int Top, int Right, int Bottom, float Density);
  void SetControllerOnLeft(bool OnLeft);
  void SetStatusBarHidden(bool Hidden);
  void SetMapFocus(int X, int Y);
  void SetStats(const char* Line1, const char* Line2,
                const char* Line3, const char* Line4);
  void SetLog(const char* Message);
  void SetPrompt(const char* Prompt, const char* Input = 0,
                 bool Numeric = false);
  void ClearPrompt();
  void SetMapScreen(bool Active);
  void SetMapSourceBounds(int X, int Y, int Width, int Height);
  void SetScreenText(const char* Text);
  void ClearScreenText();
  void SetActions(const char* const* Labels, const int* Keys,
                  const int* Groups, int Count);
  void SetQuestionChoices(const int* Keys, int Count);
  void SetMenu(const char* Title, const char* Subtitle,
               const char* const* Options, int Count, int Selected,
               int Page, int Pages);
  void ClearMenu();
  void UpdateLayout(SDL_Renderer* Renderer, int GameWidth, int GameHeight);
  const SDL_Rect& GetGameRect();
  void DrawBackground(SDL_Renderer* Renderer);
  void DrawGame(SDL_Renderer* Renderer, SDL_Texture* GameTexture);
  void Draw(SDL_Renderer* Renderer);
  touchresult HandleFingerDown(float NormalizedX, float NormalizedY);
  touchresult HandleDirectionRepeat();
  touchresult HandleFinger(float NormalizedX, float NormalizedY);
}
#endif

#endif
