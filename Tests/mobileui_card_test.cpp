#define SDL_MAIN_HANDLED

#include <cassert>
#include <iostream>

// Exercise the actual Android card renderer/input without a Java Activity.
// Including the implementation keeps inspection hooks out of production APIs.
extern "C" void* SDL_AndroidGetJNIEnv() { return 0; }
extern "C" void* SDL_AndroidGetActivity() { return 0; }
#include "../FeLib/Source/mobileui.cpp"

truth iosystem::IsOnMenu() { return true; }
bool iosystem::IsInUse() { return true; }
truth felist::isAnyFelistCurrentlyDrawn() { return State.MenuActive; }

namespace
{
  std::vector<Uint32> Pixels(SDL_Renderer* Renderer, const SDL_Rect& Area)
  {
    std::vector<Uint32> Result(Area.w * Area.h);
    assert(SDL_RenderReadPixels(Renderer, &Area, SDL_PIXELFORMAT_ARGB8888,
                                Result.data(), Area.w * sizeof(Uint32)) == 0);
    return Result;
  }

  void CheckMenu(int Width, int Height, const char* ScreenshotPrefix)
  {
    SDL_Surface* Surface = SDL_CreateRGBSurfaceWithFormat(0, Width, Height,
                                                         32, SDL_PIXELFORMAT_ARGB8888);
    assert(Surface);
    SDL_Renderer* Renderer = SDL_CreateSoftwareRenderer(Surface);
    assert(Renderer);
    mobileui::SetSafeInsets(0, 24, 0, 24, 0, 0, 1.75f);
    const char* Options[] = { "encrypted scroll", "Test deity" };
    std::string LongDescription = "@ITEM_DESCRIPTION@\n";
    for(int I = 0; I < 60; ++I)
      LongDescription += "A long item description with details that must remain readable. ";
    LongDescription += "FINAL DESCRIPTION LINE\n\nMISSING REQUIREMENTS\n"
      "A long material requirement which must wrap without disappearing.\n"
      "Have: 3 units\nExact final requirement remains visible";
    std::string GodInfo = "Test deity, the god of descriptions\n\n"
      "Alignment: N\n\nPrayer history:\nYou have never prayed to this god.\n\nLast known response:\n";
    for(int I = 0; I < 80; ++I)
      GodInfo += "A remembered prayer response can be longer than the panel. ";
    GodInfo += "FINAL PRAYER RESPONSE";
    const auto CraftCard = MobileItemCardText(LongDescription, "Weight: 20");
    assert(CraftCard.Description.find("\n\n") != std::string::npos);
    assert(CraftCard.Description.find("FINAL DESCRIPTION LINE") != std::string::npos);
    assert(CraftCard.Description.find("MISSING REQUIREMENTS") == std::string::npos);
    assert(CraftCard.Requirements.find("Exact final requirement") != std::string::npos);
    assert(CraftCard.Missing && !CraftCard.Ready);
    const char* Details[] = { LongDescription.c_str(), GodInfo.c_str() };
    const SDL_Rect Icons[] = { {0, 0, 0, 0}, {0, 0, 0, 0} };
    const auto SetSelection = [&](int Selected)
    {
      mobileui::SetMenu("Card regression", "", Options, 2, Selected, 1, 1);
      adaptiveui::SetMenuPresentation(Details, Icons, 2, adaptiveui::MENU_ITEM_GRID);
      adaptiveui::ItemMetrics Metrics[2];
      Metrics[0].Present = true;
      Metrics[0].Weight = 20;
      adaptiveui::SetMenuItemMetrics(Metrics, 2);
    };
    const auto Draw = [&]()
    {
      mobileui::UpdateLayout(Renderer, 800, 600);
      mobileui::DrawBackground(Renderer);
      mobileui::DrawGame(Renderer, 0);
      mobileui::Draw(Renderer);
      SDL_RenderPresent(Renderer);
    };
    SetSelection(0);
    Draw();
    assert(State.DetailMaxScrollY > 0 && State.DetailScrollY == 0);
    assert(State.DetailViewport.w > 0 && State.DetailViewport.h >= 36);
    const SDL_Rect Title = { State.DetailViewport.x,
      State.DetailViewport.y - 40, State.DetailViewport.w, 30 };
    const auto InitialTitle = Pixels(Renderer, Title);
    const auto InitialConfirm = Pixels(Renderer, State.MenuConfirm);
    const int InitialSelection = State.MenuSelected;
    const int X = State.DetailViewport.x + State.DetailViewport.w / 2;
    const int Y = State.DetailViewport.y + State.DetailViewport.h - 5;
    const int TargetY = State.DetailViewport.y + 5;
    const int InitialMenuScroll = State.MenuScrollY;
    for(int I = 0; I < 1000 && State.DetailScrollY < State.DetailMaxScrollY; ++I)
    {
      mobileui::HandleFingerDown(float(X) / Width, float(Y) / Height);
      mobileui::HandleFingerMotion(float(X) / Width, float(TargetY) / Height);
      const auto Up = mobileui::HandleFinger(float(X) / Width, float(TargetY) / Height);
      assert(Up.Kind == mobileui::touchresult::TOUCH_REDRAW);
      StopMenuFling();
      Draw();
    }
    assert(State.DetailScrollY == State.DetailMaxScrollY);
    assert(State.MenuSelected == InitialSelection);
    assert(State.MenuScrollY == InitialMenuScroll);
    assert(Pixels(Renderer, Title) == InitialTitle);
    assert(Pixels(Renderer, State.MenuConfirm) == InitialConfirm);
    assert(!SDL_RenderIsClipEnabled(Renderer));
    if(ScreenshotPrefix)
    {
      const std::string Name = std::string(ScreenshotPrefix)
        + (Width < Height ? "-portrait.bmp" : "-landscape.bmp");
      assert(SDL_SaveBMP(Surface, Name.c_str()) == 0);
    }
    const int EndOffset = State.DetailScrollY;
    SetSelection(0);
    Draw();
    assert(State.DetailScrollY == EndOffset); // Ordinary redraw retains position.
    // A drag ending over SELECT must not equip/confirm the highlighted item.
    mobileui::HandleFingerDown(float(X) / Width, float(Y) / Height);
    mobileui::HandleFingerMotion(float(X) / Width,
      float(State.MenuConfirm.y + 2) / Height);
    assert(mobileui::HandleFinger(float(State.MenuConfirm.x + 2) / Width,
      float(State.MenuConfirm.y + 2) / Height).Kind != mobileui::touchresult::TOUCH_KEY);
    StopMenuFling();

    SetSelection(1);
    Draw();
    assert(State.DetailScrollY == 0); // New god/item preview starts at the top.
    assert(State.DetailMaxScrollY > 0);
    mobileui::HandleFingerDown(float(X) / Width, float(Y) / Height);
    mobileui::HandleFingerMotion(float(X) / Width, float(TargetY) / Height);
    assert(mobileui::HandleFinger(float(X) / Width, float(TargetY) / Height).Kind
           != mobileui::touchresult::TOUCH_KEY); // Reading never prays.
    StopMenuFling();
    Draw();
    assert(State.DetailScrollY > 0 && State.MenuSelected == 1);
    for(int I = 0; I < 1000 && State.DetailScrollY < State.DetailMaxScrollY; ++I)
    {
      mobileui::HandleFingerDown(float(X) / Width, float(Y) / Height);
      mobileui::HandleFingerMotion(float(X) / Width, float(TargetY) / Height);
      assert(mobileui::HandleFinger(float(X) / Width, float(TargetY) / Height).Kind
             != mobileui::touchresult::TOUCH_KEY);
      StopMenuFling();
      Draw();
    }
    assert(State.DetailScrollY == State.DetailMaxScrollY);
    const int TextScale = 3; // Long card's minimum readable description scale.
    const SDL_Rect LastLine = { State.DetailViewport.x + TextScale * 2,
      State.DetailViewport.y + State.DetailViewport.h - TextScale * 10,
      State.DetailViewport.w - TextScale * 4, TextScale * 7 };
    const auto LastPixels = Pixels(Renderer, LastLine);
    assert(std::count(LastPixels.begin(), LastPixels.end(), Uint32(0xfff0e6ca)) > 0);
    if(ScreenshotPrefix)
    {
      const std::string Name = std::string(ScreenshotPrefix)
        + (Width < Height ? "-prayer-portrait.bmp" : "-prayer-landscape.bmp");
      assert(SDL_SaveBMP(Surface, Name.c_str()) == 0);
    }
    // Restore a caller's clip, including the no-intersection case.
    const SDL_Rect CallerClip = { 0, 0, 4, 4 };
    SDL_RenderSetClipRect(Renderer, &CallerClip);
    PaintMobileItemCard(Renderer, { 30, 30, 300, 400 }, "deity",
                        GodInfo, "", 0, 0);
    SDL_Rect RestoredClip;
    SDL_RenderGetClipRect(Renderer, &RestoredClip);
    assert(SDL_RenderIsClipEnabled(Renderer));
    assert(RestoredClip.x == CallerClip.x && RestoredClip.y == CallerClip.y
           && RestoredClip.w == CallerClip.w && RestoredClip.h == CallerClip.h);
    SDL_RenderSetClipRect(Renderer, 0);
    Draw();
    const auto Confirm = mobileui::HandleFinger(
      float(State.MenuConfirm.x + State.MenuConfirm.w / 2) / Width,
      float(State.MenuConfirm.y + State.MenuConfirm.h / 2) / Height);
    assert(Confirm.Kind == mobileui::touchresult::TOUCH_KEY
           && Confirm.KeyCode == KEY_MOBILE_MENU_SELECT_BASE + 1);
    mobileui::ClearMenu();
    assert(State.DetailScrollY == 0 && State.DetailMaxScrollY == 0
           && State.DetailViewport.w == 0);
    mobileui::DeInit();
    SDL_DestroyRenderer(Renderer);
    SDL_FreeSurface(Surface);
  }
}

int main(int Argc, char** Argv)
{
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  CheckMenu(720, 1600, Argc > 1 ? Argv[1] : 0);
  CheckMenu(1600, 720, Argc > 1 ? Argv[1] : 0);
  CheckMenu(480, 800, 0);
  std::cout << "Mobile cards passed: portrait/landscape, last text line, independent "
               "scrolling, selection reset, fixed controls and safe prayer preview.\n";
  SDL_Quit();
  return 0;
}
