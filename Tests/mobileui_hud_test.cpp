#define SDL_MAIN_HANDLED

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <set>

// Exercise the real Android HUD renderer/input without a Java Activity.
extern "C" void* SDL_AndroidGetJNIEnv() { return 0; }
extern "C" void* SDL_AndroidGetActivity() { return 0; }
#include "../FeLib/Source/mobileui.cpp"

// MinGW's CRT assertion handler can open a modal dialog instead of failing CI.
// Keep test diagnostics portable and fail immediately, without interactive UI.
#undef assert
#define assert(Condition) do { if(!(Condition)) { \
  std::cerr << "Assertion failed: " << #Condition << " at " << __FILE__ \
            << ':' << __LINE__ << '\n'; std::exit(EXIT_FAILURE); } } while(false)

namespace { bool InMenu = false; }
truth iosystem::IsOnMenu() { return InMenu; }
bool iosystem::IsInUse() { return InMenu; }
truth felist::isAnyFelistCurrentlyDrawn() { return State.MenuActive; }

namespace
{
  bool Inside(const SDL_Rect& Inner, const SDL_Rect& Outer)
  {
    return Inner.x >= Outer.x && Inner.y >= Outer.y
      && Inner.x + Inner.w <= Outer.x + Outer.w
      && Inner.y + Inner.h <= Outer.y + Outer.h;
  }

  bool Same(const SDL_Rect& A, const SDL_Rect& B)
  {
    return A.x == B.x && A.y == B.y && A.w == B.w && A.h == B.h;
  }

  void CheckSeverity()
  {
    const char* Labels[] = { "Overload!", "Stressed", "Very hungry",
      "Starving!", "Fainting!", "Poisoned", "Panicked", "Burdened",
      "Hungry", "Exhausted", "Overfed!", "Slowed", "Hiccups", "Bloated",
      "Hasted", "Fearless", "Invisible", "Infravision", "ESP", "Levitating",
      "PolyControl", "TeleControl", "Vampirism", "Searching", "RUNNING",
      "ON SHIP", "Polymorphed", "Ethereal", "fastening equipment", "", 0 };
    adaptiveui::SetConditions(Labels, 31);
    const auto& Conditions = adaptiveui::GetHudModel().Conditions;
    assert(Conditions.size() == 29); // No blank/null chips.
    for(size_t Index = 0; Index < Conditions.size(); ++Index)
    {
      const auto Severity = Index < 7 ? adaptiveui::STATUS_DANGER
        : (Index < 14 ? adaptiveui::STATUS_WARNING
           : (Index < 22 ? adaptiveui::STATUS_BENEFICIAL
              : adaptiveui::STATUS_NEUTRAL));
      assert(Conditions[Index].Severity == Severity);
      if(Severity == adaptiveui::STATUS_DANGER)
        assert(Conditions[Index].Red == 235 && Conditions[Index].Green == 80);
      else if(Severity == adaptiveui::STATUS_WARNING)
        assert(Conditions[Index].Red == 224 && Conditions[Index].Green == 181);
      else if(Severity == adaptiveui::STATUS_BENEFICIAL)
        assert(Conditions[Index].Green == 191 && Conditions[Index].Red == 88);
    }
    adaptiveui::SetConditions(0, 5);
    assert(adaptiveui::GetHudModel().Conditions.empty());
  }

  void CheckHud(int Width, int Height, bool Cutout, const char* ScreenshotPrefix,
                bool ControllerOnLeft = true)
  {
    mobileui::DeInit();
    InMenu = false;
    SDL_Surface* Surface = SDL_CreateRGBSurfaceWithFormat(0, Width, Height,
                                                         32, SDL_PIXELFORMAT_ARGB8888);
    assert(Surface);
    SDL_Renderer* Renderer = SDL_CreateSoftwareRenderer(Surface);
    assert(Renderer);
    const int Camera[] = { Width / 2 - 30, 0, Width / 2 + 30, 72 };
    mobileui::SetSafeInsets(0, 0, 0, 24, Cutout ? Camera : 0,
                            Cutout ? 1 : 0, 1.75f);
    mobileui::SetControllerOnLeft(ControllerOnLeft);
    mobileui::SetStats("HP 20/36  MANA 9  GOLD 46",
      "ARM 9  LEG 11  DEX 10  AGI 11", "END 10  PER 10  INT 10  WIS 11",
      "WILL 10  CHA 10  HT 175  WT 72");
    adaptiveui::SetLocationTime("FIRST FLOOR", "DAY 1  TIME 12:26  TURN 80");
    const auto Draw = [&]()
    {
      mobileui::UpdateLayout(Renderer, 800, 600);
      mobileui::DrawBackground(Renderer);
      mobileui::Draw(Renderer);
      SDL_RenderPresent(Renderer);
    };
    adaptiveui::SetConditions(0, 0);
    Draw();
    assert(State.Gameplay && State.Effects.h > 0 && State.EffectChips.empty());
    const SDL_Rect EmptyMap = State.Game;
    const SDL_Rect OriginalStats = State.Stats;
    const SDL_Rect OriginalControls = State.Controls;
    const SDL_Rect OriginalToggle = State.Toggle;
    const SDL_Rect OriginalEffects = State.Effects;
    const SDL_Rect OriginalLog = State.Log;
    std::vector<Uint32> EmptyPixels(Width * Height);
    assert(SDL_RenderReadPixels(Renderer, 0, SDL_PIXELFORMAT_ARGB8888,
      EmptyPixels.data(), Width * sizeof(Uint32)) == 0);
    const auto CheckFixedGeometry = [&]()
    {
      assert(Same(EmptyMap, State.Game) && Same(OriginalStats, State.Stats));
      assert(Same(OriginalControls, State.Controls)
             && Same(OriginalToggle, State.Toggle));
      assert(Same(OriginalEffects, State.Effects) && Same(OriginalLog, State.Log));
    };
    const char* Single[] = { "Burdened" };
    adaptiveui::SetConditions(Single, 1);
    Draw();
    CheckFixedGeometry();
    assert(State.EffectChips.size() == 1);
    assert(State.Effects.x == State.Game.x && State.Effects.w == State.Game.w);
    if(Width < Height)
      assert(State.EffectChips[0].x == State.Effects.x);
    else
    {
      assert(State.EffectChips[0].x + State.EffectChips[0].w
             == State.Effects.x + State.Effects.w);
      assert(State.Toggle.y == State.Effects.y);
      assert(!SDL_HasIntersection(&State.Effects, &State.Toggle));
    }
    const char* Wrapped[] = { "Burdened", "Hungry", "Exhausted", "Slowed",
                             "Overfed!", "Hiccups" };
    adaptiveui::SetConditions(Wrapped, 6);
    Draw();
    CheckFixedGeometry();
    // Even a label too long for the smallest font cannot draw over the map,
    // stats or controls: all pixels outside the reserved area stay unchanged.
    const std::string LongLabel(512, 'A');
    const char* Long[] = { LongLabel.c_str() };
    adaptiveui::SetConditions(Long, 1);
    Draw();
    CheckFixedGeometry();
    std::vector<Uint32> LongPixels(Width * Height);
    assert(SDL_RenderReadPixels(Renderer, 0, SDL_PIXELFORMAT_ARGB8888,
      LongPixels.data(), Width * sizeof(Uint32)) == 0);
    for(int Y = 0; Y < Height; ++Y)
      for(int X = 0; X < Width; ++X)
        if(!Contains(State.Effects, X, Y))
          assert(LongPixels[Y * Width + X] == EmptyPixels[Y * Width + X]);
    const char* Labels[] = { "Hasted", "Hungry", "Overload!" };
    adaptiveui::SetConditions(Labels, 3);
    Draw();
    CheckFixedGeometry();
    assert(!State.EffectIndices.empty() && State.EffectIndices[0] == 2);
    for(size_t Index = 1; Index < State.EffectIndices.size(); ++Index)
      assert(State.EffectIndices[Index - 1] > State.EffectIndices[Index]);
    assert(State.Effects.y >= State.Stats.y + State.Stats.h);
    assert(State.Game.y >= State.Effects.y + State.Effects.h);
    assert(Inside(State.Effects, State.Safe));
    assert(State.Game.h > 0 && State.Controls.w > 0);
    if(Width < Height)
      assert(State.Game.y + State.Game.h <= State.Toggle.y);
    else
      assert(!SDL_HasIntersection(&State.Game, &State.Toggle));
    const SDL_Rect CameraRect = { Camera[0], Camera[1],
      Camera[2] - Camera[0], Camera[3] - Camera[1] };
    assert(!Cutout || !SDL_HasIntersection(&State.Effects, &CameraRect));
    for(size_t Index = 0; Index < State.EffectChips.size(); ++Index)
    {
      const SDL_Rect Chip = State.EffectChips[Index];
      assert(Inside(Chip, State.Effects));
      assert(!SDL_HasIntersection(&Chip, &State.Game));
      std::vector<Uint32> Pixels(Chip.w * Chip.h);
      assert(SDL_RenderReadPixels(Renderer, &Chip, SDL_PIXELFORMAT_ARGB8888,
        Pixels.data(), Chip.w * sizeof(Uint32)) == 0);
      const auto& Condition = adaptiveui::GetHudModel().Conditions[State.EffectIndices[Index]];
      const Uint32 Color = 0xff000000 | (Uint32(Condition.Red) << 16)
        | (Uint32(Condition.Green) << 8) | Condition.Blue;
      assert(std::count(Pixels.begin(), Pixels.end(), Color) > Chip.w + Chip.h);
      const auto Tap = mobileui::HandleFinger(float(Chip.x + Chip.w / 2) / Width,
                                             float(Chip.y + Chip.h / 2) / Height);
      assert(Tap.Kind == mobileui::touchresult::TOUCH_NONE);
    }
    if(ScreenshotPrefix)
    {
      const std::string Name = std::string(ScreenshotPrefix)
        + (Width < Height ? "-portrait.bmp" : "-landscape.bmp");
      assert(SDL_SaveBMP(Surface, Name.c_str()) == 0);
    }

    const char* Many[32];
    for(int Index = 0; Index < 32; ++Index)
      Many[Index] = Index % 2 ? "Polymorph Locked" : "Poisoned";
    adaptiveui::SetConditions(Many, 32);
    Draw();
    CheckFixedGeometry();
    assert(State.EffectPages > 1 && State.EffectPage == 0);
    const int Pages = State.EffectPages;
    const SDL_Rect PagedMap = State.Game;
    const SDL_Rect PagedControls = State.Controls;
    std::set<int> Seen;
    for(int Page = 0; Page < Pages; ++Page)
    {
      assert(State.EffectPage == Page && Inside(State.EffectsMore, State.Effects));
      for(size_t Index = 0; Index < State.EffectChips.size(); ++Index)
      {
        Seen.insert(State.EffectIndices[Index]);
        assert(Inside(State.EffectChips[Index], State.Effects));
        assert(!SDL_HasIntersection(&State.EffectChips[Index], &State.EffectsMore));
      }
      // Refeeding identical conditions and ordinary redraws retain this page.
      adaptiveui::SetConditions(Many, 32);
      Draw();
      CheckFixedGeometry();
      assert(State.EffectPage == Page);
      const float X = float(State.EffectsMore.x + State.EffectsMore.w / 2) / Width;
      const float Y = float(State.EffectsMore.y + State.EffectsMore.h / 2) / Height;
      assert(mobileui::HandleFingerDown(X, Y).Kind == mobileui::touchresult::TOUCH_NONE);
      const auto Next = mobileui::HandleFinger(X, Y);
      assert(Next.Kind == mobileui::touchresult::TOUCH_REDRAW && Next.KeyCode == 0);
      Draw();
      CheckFixedGeometry();
      assert(Same(PagedMap, State.Game) && Same(PagedControls, State.Controls));
    }
    assert(Seen.size() == 32 && State.EffectPage == 0);
    mobileui::HandleFinger(float(State.EffectsMore.x + 2) / Width,
                           float(State.EffectsMore.y + 2) / Height);
    Draw();
    assert(State.EffectPage == 1);
    adaptiveui::SetConditions(Labels, 3);
    Draw();
    CheckFixedGeometry();
    assert(State.EffectPage == 0);
    adaptiveui::SetConditions(0, 0);
    Draw();
    CheckFixedGeometry();
    assert(State.EffectChips.empty() && State.EffectsMore.w == 0);
    InMenu = true;
    adaptiveui::SetConditions(Labels, 3);
    Draw();
    assert(!State.Gameplay && State.Effects.h == 0 && State.EffectChips.empty());
    mobileui::DeInit();
    SDL_DestroyRenderer(Renderer);
    SDL_FreeSurface(Surface);
  }
}

int main(int Argc, char** Argv)
{
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  CheckSeverity();
  CheckHud(720, 1600, false, Argc > 1 ? Argv[1] : 0);
  CheckHud(1600, 720, false, Argc > 1 ? Argv[1] : 0);
  CheckHud(1600, 720, false, 0, false);
  CheckHud(800, 480, false, 0);
  CheckHud(800, 480, false, 0, false);
  CheckHud(480, 800, false, 0);
  CheckHud(970, 2160, true, 0);
  std::cout << "Android effects HUD passed: severity, pixels, rotation/cutout, "
               "fixed geometry, left/right anchors, bounded overflow, "
               "no game commands, and clearing/menu state.\n";
  SDL_Quit();
  return 0;
}
