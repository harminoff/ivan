#include "mobileui.h"

#ifdef ANDROID
#include "adaptiveui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <jni.h>
#include <sstream>
#include <string>
#include <vector>

#include "felibdef.h"
#include "feio.h"
#include "felist.h"
#include "SDL_system.h"

namespace
{
  struct actiondef
  {
    const char* Label;
    int KeyCode;
  };

  enum { ACTION_ROWS = 3, ACTION_COLUMNS = 3, ACTION_COUNT = 9,
         ACTIONS_PER_PAGE = 8, MAX_MOBILE_ACTIONS = 48,
         MAX_QUESTION_CHOICES = 9, MAX_MENU_OPTIONS = 512,
         MAX_PICKUP_ACTIONS = 3,
         MAX_DISPLAY_CUTOUTS = 8,
         CONTROL_MOVEMENT = 0, CONTROL_ACTIONS = 1,
         CONTROL_SECTION_COUNT = mobileui::ACTION_GROUPS + 1 };

  const actiondef MenuNavigation[ACTION_COUNT] = {
    { "PG UP", KEY_PAGE_UP }, { "UP", KEY_UP },
    { "PG DOWN", KEY_PAGE_DOWN },
    { "LEFT", KEY_LEFT }, { "SELECT", KEY_CONTROLLER_A },
    { "RIGHT", KEY_RIGHT },
    { "ALT", KEY_CONTROLLER_Y }, { "DOWN", KEY_DOWN },
    { "BACK", KEY_CONTROLLER_B }
  };

  const actiondef MenuDirections[ACTION_COUNT] = {
    { "NW", KEY_CONTROLLER_DIRECTION + 1 },
    { "N", KEY_CONTROLLER_DIRECTION + 2 },
    { "NE", KEY_CONTROLLER_DIRECTION + 3 },
    { "W", KEY_CONTROLLER_DIRECTION + 4 }, { "SELF", '.' },
    { "E", KEY_CONTROLLER_DIRECTION + 6 },
    { "SW", KEY_CONTROLLER_DIRECTION + 7 },
    { "S", KEY_CONTROLLER_DIRECTION + 8 },
    { "SE", KEY_CONTROLLER_DIRECTION + 9 }
  };

  struct layoutstate
  {
    int Width = 1;
    int Height = 1;
    int GameWidth = 800;
    int GameHeight = 600;
    int Left = 0;
    int Top = 0;
    int Right = 0;
    int Bottom = 0;
    SDL_Rect DisplayCutout = { 0, 0, 0, 0 };
    SDL_Rect DisplayCutouts[MAX_DISPLAY_CUTOUTS];
    int DisplayCutoutCount = 0;
    float Density = 1.f;
    int ActionPage = 0;
    int ControlMode = CONTROL_MOVEMENT;
    int PinnedActionGroup = -1;
    int LastControlSectionTap = -1;
    Uint32 LastControlSectionTapTime = 0;
    bool DirectionPressActive = false;
    int DirectionPressKey = 0;
    SDL_TimerID DirectionRepeatTimer = 0;
    bool LogVisible = false;
    bool LogPressActive = false;
    Uint32 LogPressStarted = 0;
    Uint32 LogHideDeadline = 0;
    SDL_TimerID LogHideTimer = 0;
    bool MenuDirectionMode = false;
    bool Gameplay = false;
    bool ControllerOnLeft = false;
    int QuestionChoiceCount = 0;
    int QuestionChoices[MAX_QUESTION_CHOICES] = { 0 };
    int MapFocusX = 352;
    int MapFocusY = 240;
    int PlayerMapX = 0;
    int PlayerMapY = 0;
    bool HasPlayerMapPosition = false;
    float CanvasZoom = 2.f;
    int CanvasPanX = 0;
    int CanvasPanY = 0;
    bool CanvasPressActive = false;
    bool CanvasPanning = false;
    Uint32 CanvasPressStarted = 0;
    int CanvasPressX = 0;
    int CanvasPressY = 0;
    int CanvasLastX = 0;
    int CanvasLastY = 0;
    int SuppressedFingerUps = 0;
    bool HapticsEnabled = true;
    int HapticStrength = 75;
    SDL_Rect Safe = { 0, 0, 1, 1 };
    SDL_Rect Header = { 0, 0, 1, 1 };
    SDL_Rect Game = { 0, 0, 1, 1 };
    SDL_Rect Stats = { 0, 0, 1, 1 };
    SDL_Rect Log = { 0, 0, 1, 1 };
    SDL_Rect Controls = { 0, 0, 1, 1 };
    SDL_Rect Toggle = { 0, 0, 1, 1 };
    SDL_Rect MapSource = { 0, 0, 1, 1 };
    SDL_Rect CanvasDestination = { 0, 0, 1, 1 };
    std::string StatsLines[4];
    std::string LogMessage;
    bool PromptActive = false;
    bool PromptGameplay = false;
    bool PositionPrompt = false;
    bool PromptShowsInput = false;
    bool PromptNumeric = false;
    std::string PromptText;
    std::string PromptDetail;
    std::string PromptInput;
    bool ScreenTextActive = false;
    std::string ScreenText;
    std::string ScreenTextTitle = "STORY";
    bool PaperDollScreen = false;
    SDL_Rect PaperDollSource = { 0, 0, 0, 0 };
    bool MapScreen = false;
    SDL_Rect MapOverlaySource = { 0, 0, 0, 0 };
    enum { MAX_MAP_NOTES = 12 };
    std::string MapNoteLabels[MAX_MAP_NOTES];
    SDL_Point MapNotePoints[MAX_MAP_NOTES];
    int MapNoteCount = 0;
    std::string ActionLabels[MAX_MOBILE_ACTIONS];
    int ActionKeys[MAX_MOBILE_ACTIONS] = { 0 };
    int ActionGroups[MAX_MOBILE_ACTIONS] = { 0 };
    int ActionCount = 0;
    bool MenuActive = false;
    std::string MenuTitle;
    std::string MenuSubtitle;
    std::string MenuOptions[MAX_MENU_OPTIONS];
    SDL_Rect MenuRows[MAX_MENU_OPTIONS];
    int MenuOptionCount = 0;
    int MenuSelected = -1;
    int MenuPage = 1;
    int MenuPages = 1;
    int MenuGridSelection = -1;
    SDL_Rect MenuViewport = { 0, 0, 0, 0 };
    SDL_Rect MenuConfirm = { 0, 0, 0, 0 };
    SDL_Rect MenuEquip = { 0, 0, 0, 0 };
    SDL_Rect MenuItemActions[MAX_PICKUP_ACTIONS];
    int MenuItemActionCodes[MAX_PICKUP_ACTIONS] = { 0 };
    int MenuItemActionCount = 0;
    SDL_Rect MenuBack = { 0, 0, 0, 0 };
    int MenuScrollY = 0;
    int MenuMaxScrollY = 0;
    int MenuScrollStep = 0;
    SDL_Rect ConditionViewport = { 0, 0, 0, 0 };
    int ConditionScrollY = 0;
    int ConditionMaxScrollY = 0;
    bool MenuPressActive = false;
    bool MenuPressConditions = false;
    bool MenuScrolling = false;
    bool MenuStoppedFlingOnPress = false;
    int MenuPressY = 0;
    int MenuLastY = 0;
    Uint32 MenuLastMotionTime = 0;
    SDL_TimerID MenuFlingTimer = 0;
    float MenuScrollVelocity = 0.f;
    float MenuFlingPosition = 0.f;
    Uint32 MenuFlingLastTime = 0;
    bool MenuFlingConditions = false;
    int MenuMotionY[8] = { 0 };
    Uint32 MenuMotionTime[8] = { 0 };
    int MenuMotionCount = 0;
  } State;

  SDL_Texture* GameplaySnapshot = 0;
  int GameplaySnapshotWidth = 0;
  int GameplaySnapshotHeight = 0;

  bool ConsoleDirty = true;

  enum { DIRECTION_REPEAT_DELAY_MS = 350,
         DIRECTION_REPEAT_INTERVAL_MS = 110,
         CONTROL_SECTION_DOUBLE_TAP_MS = 400,
         LOG_VISIBLE_MS = 6000,
         MENU_FLING_INTERVAL_MS = 16,
         CANVAS_PAN_HOLD_MS = 250,
         CANVAS_PAN_SLOP = 8 };

  Uint32 QueueDirectionRepeat(Uint32, void*)
  {
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = mobileui::DIRECTION_REPEAT_EVENT_CODE;
    SDL_PushEvent(&Event);
    return DIRECTION_REPEAT_INTERVAL_MS;
  }

  Uint32 QueueLogHide(Uint32, void*)
  {
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = mobileui::LOG_HIDE_EVENT_CODE;
    SDL_PushEvent(&Event);
    return 0;
  }

  Uint32 QueueMenuFling(Uint32, void*)
  {
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = mobileui::MENU_FLING_EVENT_CODE;
    SDL_PushEvent(&Event);
    return MENU_FLING_INTERVAL_MS;
  }

  void StopMenuFling()
  {
    if(State.MenuFlingTimer)
    {
      SDL_RemoveTimer(State.MenuFlingTimer);
      State.MenuFlingTimer = 0;
    }
    State.MenuScrollVelocity = 0.f;
  }

  void StartMenuFling(bool Conditions)
  {
    const float ReleaseVelocity = State.MenuScrollVelocity;
    StopMenuFling();
    State.MenuScrollVelocity = ReleaseVelocity;
    State.MenuFlingConditions = Conditions;
    State.MenuFlingPosition = float(Conditions
      ? State.ConditionScrollY : State.MenuScrollY);
    State.MenuFlingLastTime = SDL_GetTicks();
    State.MenuFlingTimer = SDL_AddTimer(MENU_FLING_INTERVAL_MS,
                                        QueueMenuFling, 0);
  }

  void CancelDirectionPress()
  {
    State.DirectionPressActive = false;
    State.DirectionPressKey = 0;
    if(State.DirectionRepeatTimer)
    {
      SDL_RemoveTimer(State.DirectionRepeatTimer);
      State.DirectionRepeatTimer = 0;
    }
  }

  int Clamp(int Value, int Low, int High)
  {
    return std::max(Low, std::min(Value, High));
  }

  void ResetMenuMotionSamples(int Y, Uint32 Time)
  {
    State.MenuMotionY[0] = Y;
    State.MenuMotionTime[0] = Time;
    State.MenuMotionCount = 1;
    State.MenuScrollVelocity = 0.f;
  }

  void AddMenuMotionSample(int Y, Uint32 Time)
  {
    enum { SAMPLE_COUNT = 8, VELOCITY_WINDOW_MS = 140 };
    if(State.MenuMotionCount == SAMPLE_COUNT)
    {
      for(int Index = 1; Index < SAMPLE_COUNT; ++Index)
      {
        State.MenuMotionY[Index - 1] = State.MenuMotionY[Index];
        State.MenuMotionTime[Index - 1] = State.MenuMotionTime[Index];
      }
      --State.MenuMotionCount;
    }
    State.MenuMotionY[State.MenuMotionCount] = Y;
    State.MenuMotionTime[State.MenuMotionCount] = Time;
    ++State.MenuMotionCount;

    // Retain a short history, like Android's VelocityTracker. Keeping one
    // older anchor sample makes sparse SDL finger events stable as well.
    while(State.MenuMotionCount > 2
          && Time - State.MenuMotionTime[1] > VELOCITY_WINDOW_MS)
    {
      for(int Index = 1; Index < State.MenuMotionCount; ++Index)
      {
        State.MenuMotionY[Index - 1] = State.MenuMotionY[Index];
        State.MenuMotionTime[Index - 1] = State.MenuMotionTime[Index];
      }
      --State.MenuMotionCount;
    }
  }

  float MenuReleaseVelocity(Uint32 ReleaseTime)
  {
    enum { RELEASE_DECAY_MS = 180 };
    if(State.MenuMotionCount < 2)
      return 0.f;
    const int Last = State.MenuMotionCount - 1;
    const Uint32 Duration = State.MenuMotionTime[Last]
                          - State.MenuMotionTime[0];
    if(!Duration)
      return 0.f;
    float Velocity = -float(State.MenuMotionY[Last]
                            - State.MenuMotionY[0]) / float(Duration);
    const Uint32 Idle = ReleaseTime - State.MenuMotionTime[Last];
    if(Idle >= RELEASE_DECAY_MS)
      return 0.f;
    Velocity *= 1.f - float(Idle) / float(RELEASE_DECAY_MS);
    const float Maximum = std::max(2.5f, State.Density * 2.f);
    return std::max(-Maximum, std::min(Velocity, Maximum));
  }

  bool Contains(const SDL_Rect& Rect, int X, int Y)
  {
    return X >= Rect.x && Y >= Rect.y
        && X < Rect.x + Rect.w && Y < Rect.y + Rect.h;
  }

  bool GameplayPresentation()
  {
    return (!iosystem::IsInUse()
            || (State.PromptActive && State.PromptGameplay))
        && !iosystem::IsOnMenu()
        && !felist::isAnyFelistCurrentlyDrawn();
  }

  bool MainMenuPresentation()
  {
    return State.MenuActive && State.MenuSubtitle == "MAIN MENU";
  }

  bool AdaptiveGridMenuPresentation()
  {
    if(!State.MenuActive)
      return false;
    const adaptiveui::MenuPresentationKind Kind =
      adaptiveui::GetHudModel().MenuKind;
    return Kind == adaptiveui::MENU_CATEGORY_GRID
        || Kind == adaptiveui::MENU_ITEM_GRID
        || Kind == adaptiveui::MENU_PICKUP_GRID
        || Kind == adaptiveui::MENU_BUTTON_ROWS;
  }

  void FitGameRect(int X, int Y, int Width, int Height)
  {
    const float GameAspect = float(State.GameWidth) / float(State.GameHeight);
    int ResultWidth = Width;
    int ResultHeight = int(ResultWidth / GameAspect);
    if(ResultHeight > Height)
    {
      ResultHeight = Height;
      ResultWidth = int(ResultHeight * GameAspect);
    }
    State.Game = { X + (Width - ResultWidth) / 2,
                   Y + (Height - ResultHeight) / 2,
                   std::max(1, ResultWidth), std::max(1, ResultHeight) };
  }

  SDL_Rect GridCell(const SDL_Rect& Grid, int Columns, int Rows, int Index,
                    int Inset = 0)
  {
    const int Column = Index % Columns;
    const int Row = Index / Columns;
    const int X0 = Grid.x + Grid.w * Column / Columns;
    const int X1 = Grid.x + Grid.w * (Column + 1) / Columns;
    const int Y0 = Grid.y + Grid.h * Row / Rows;
    const int Y1 = Grid.y + Grid.h * (Row + 1) / Rows;
    return { X0 + Inset, Y0 + Inset,
             std::max(1, X1 - X0 - Inset * 2),
             std::max(1, Y1 - Y0 - Inset * 2) };
  }

  const char* ActionGroupName(int Group)
  {
    static const char* Names[mobileui::ACTION_GROUPS] = {
      "CONTEXT", "ITEMS", "CHARACTER", "MOVE", "SYSTEM"
    };
    return Group >= 0 && Group < mobileui::ACTION_GROUPS
      ? Names[Group] : "ACTIONS";
  }

  int ActionCountForGroup(int Group)
  {
    int Count = 0;
    for(int Index = 0; Index < State.ActionCount; ++Index)
      if(State.ActionGroups[Index] == Group)
        ++Count;
    return Count;
  }

  int ActionPagesForGroup(int Group)
  {
    const int Count = ActionCountForGroup(Group);
    return (Count + ACTIONS_PER_PAGE - 1) / ACTIONS_PER_PAGE;
  }

  int FirstActionPageForGroup(int WantedGroup)
  {
    int Page = 0;
    for(int Group = 0; Group < mobileui::ACTION_GROUPS; ++Group)
    {
      if(Group == WantedGroup)
        return ActionPagesForGroup(Group) ? Page : -1;
      Page += ActionPagesForGroup(Group);
    }
    return -1;
  }

  int ActionPageCount()
  {
    int Pages = 0;
    for(int Group = 0; Group < mobileui::ACTION_GROUPS; ++Group)
    {
      Pages += ActionPagesForGroup(Group);
    }
    return std::max(1, Pages);
  }

  int BuildActionPage(int Page, int* Indices, int& Group);

  int CurrentActionGroup()
  {
    int Indices[ACTIONS_PER_PAGE];
    int Group = -1;
    BuildActionPage(State.ActionPage, Indices, Group);
    return Group;
  }

  void SelectActionGroup(int Group)
  {
    const int Page = FirstActionPageForGroup(Group);
    if(Page >= 0)
    {
      State.ControlMode = CONTROL_ACTIONS;
      State.ActionPage = Page;
    }
  }

  void AdvanceActionPageWithinGroup()
  {
    const int Group = CurrentActionGroup();
    const int FirstPage = FirstActionPageForGroup(Group);
    const int Pages = ActionPagesForGroup(Group);
    if(FirstPage >= 0 && Pages > 1)
      State.ActionPage = FirstPage
        + (State.ActionPage - FirstPage + 1) % Pages;
  }

  int BuildActionPage(int Page, int* Indices, int& Group)
  {
    Page = Clamp(Page, 0, ActionPageCount() - 1);
    int PageCursor = 0;
    for(Group = 0; Group < mobileui::ACTION_GROUPS; ++Group)
    {
      int GroupCount = 0;
      for(int Index = 0; Index < State.ActionCount; ++Index)
        if(State.ActionGroups[Index] == Group)
          ++GroupCount;
      const int GroupPages = (GroupCount + ACTIONS_PER_PAGE - 1)
                           / ACTIONS_PER_PAGE;
      if(Page >= PageCursor + GroupPages)
      {
        PageCursor += GroupPages;
        continue;
      }

      const int Skip = (Page - PageCursor) * ACTIONS_PER_PAGE;
      int Seen = 0;
      int ResultCount = 0;
      for(int Index = 0; Index < State.ActionCount
          && ResultCount < ACTIONS_PER_PAGE; ++Index)
        if(State.ActionGroups[Index] == Group)
        {
          if(Seen++ >= Skip)
            Indices[ResultCount++] = Index;
        }
      return ResultCount;
    }
    Group = -1;
    return 0;
  }

  SDL_Rect ActionGridRect()
  {
    return State.Controls;
  }

  void Color(SDL_Renderer* Renderer, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255)
  {
    SDL_SetRenderDrawColor(Renderer, R, G, B, A);
  }

  void Fill(SDL_Renderer* Renderer, const SDL_Rect& Rect,
            Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255)
  {
    Color(Renderer, R, G, B, A);
    SDL_RenderFillRect(Renderer, &Rect);
  }

  void Outline(SDL_Renderer* Renderer, const SDL_Rect& Rect,
               Uint8 R, Uint8 G, Uint8 B)
  {
    Color(Renderer, R, G, B);
    SDL_RenderDrawRect(Renderer, &Rect);
  }

  void Frame(SDL_Renderer* Renderer, const SDL_Rect& Rect)
  {
    const int Border = Clamp(int(2 * State.Density), 2, 7);
    SDL_Rect Outer = Rect;
    Fill(Renderer, Outer, 24, 22, 20, 245);
    Outline(Renderer, Outer, 170, 139, 82);
    for(int Index = 1; Index < Border; ++Index)
    {
      SDL_Rect Inner = { Rect.x + Index, Rect.y + Index,
                         Rect.w - Index * 2, Rect.h - Index * 2 };
      if(Inner.w > 1 && Inner.h > 1)
        Outline(Renderer, Inner, 77, 64, 46);
    }
  }

  // Native IVAN sprites reduced to their four source-lightness bands.  Keeping
  // the original 16x16 pixels makes the tabs recognizable without introducing
  // a second UI art style or loading another texture atlas into FeLib.
  const char* const ControlSectionIcons[CONTROL_SECTION_COUNT][16] = {
    {
      "332..........233", "32............23", "2..............2",
      "................", "................", "................",
      "................", "................", "................",
      "................", "................", "................",
      "................", "2..............2", "32............23",
      "332..........233"
    },
    {
      ".......44..44...", "......43344334..", ".....4333333334.",
      "......433443334.", ".....4433444334.", "....4333333334..",
      "...4333333334...", "...433333334....", "....433333334...",
      ".....43333334...", "....43333334....", "....4333334.....",
      ".....4333334....", "......4334334...", "......434.44....",
      ".......4........"
    },
    {
      "................", "................", "................",
      "...123333332....", "..12123333332...", "..121233333332..",
      "..111233313332..", "..121211111112..", "..121233111332..",
      "..121222111222..", "..121233333332..", "...11222222222..",
      "....1211111112..", "................", "................",
      "................"
    },
    {
      "................", "................", "....3......3....",
      "...333....233...", "..333311212222..", ".32333332222222.",
      "...2333322222...", "....23322222....", "....22222222....",
      "....33222222....", "....33322222....", "....33322221....",
      "....33322221....", "................", "................",
      "................"
    },
    {
      "................", ".....4.....4....", "....434...434...",
      "...43224.42234..", "..4322224222234.", ".4322224.4222234",
      "43222234.4322223", ".422222343222224", "..4244223224424.",
      "...4..43224..4..", ".....4322234....", "....432242234...",
      "...43224.42234..", "..43224...42234.", "...424.....424..",
      "....4.......4..."
    },
    {
      "................", "................", ".......22.......",
      "......23222.....", ".....23233322...", "....23233223322.",
      "...23222223332..", "..232232223324..", ".24223223232442.",
      ".2444223322442..", "..11144222442...", ".11111444442....",
      "11111224442.....", "..11...222......", "..1.............",
      "................"
    }
  };

  bool ShowControlSectionTabs()
  {
    return State.Gameplay && !State.PromptActive && !State.ScreenTextActive
        && !State.QuestionChoiceCount && !State.MapScreen;
  }

  bool ShowGameplayLog()
  {
    return State.PromptActive
        || (State.LogVisible && !State.LogMessage.empty());
  }

  int SelectedControlSection()
  {
    return State.ControlMode == CONTROL_MOVEMENT
      ? 0 : CurrentActionGroup() + 1;
  }

  SDL_Rect ControlSectionTab(int Index)
  {
    const int Gap = Clamp(int(3 * State.Density), 3, 10);
    const int Inset = Clamp(int(2 * State.Density), 2, 6);
    const SDL_Rect Area = State.Width < State.Height
      ? SDL_Rect{ State.Safe.x, State.Controls.y,
                  State.Safe.w, State.Controls.h }
      : SDL_Rect{ State.Toggle.x, State.Controls.y,
                  State.Toggle.w, State.Controls.h };
    SDL_Rect Side;
    if(Index < 3)
      Side = { Area.x, Area.y,
               std::max(1, State.Controls.x - Area.x - Gap), Area.h };
    else
      Side = { State.Controls.x + State.Controls.w + Gap, Area.y,
               std::max(1, Area.x + Area.w
                         - State.Controls.x - State.Controls.w - Gap), Area.h };
    return GridCell(Side, 1, 3, Index % 3, Inset);
  }

  void PaintControlSectionIcon(SDL_Renderer* Renderer, int Index,
                               const SDL_Rect& Button, bool Selected,
                               bool Enabled)
  {
    static const Uint8 Dark[CONTROL_SECTION_COUNT][3] = {
      { 38, 72, 28 }, { 92, 48, 13 }, { 65, 42, 20 },
      { 34, 67, 27 }, { 30, 70, 83 }, { 73, 58, 34 }
    };
    static const Uint8 Light[CONTROL_SECTION_COUNT][3] = {
      { 157, 218, 83 }, { 246, 207, 94 }, { 226, 177, 82 },
      { 176, 207, 108 }, { 170, 226, 225 }, { 227, 211, 157 }
    };
    // The action atlas' gold sprite is a foot/boot, which communicates travel
    // much better than the crossed tools.  Use the tools for contextual world
    // interactions and the foot for movement while retaining their native
    // palettes.
    const int SpriteIndex = Index == 1 ? 4 : (Index == 4 ? 1 : Index);
    const int Padding = Clamp(int(6 * State.Density), 7, 18);
    const int Pixel = std::max(1,
      std::min((Button.w - Padding * 2) / 16,
               (Button.h - Padding * 2) / 16));
    const int StartX = Button.x + (Button.w - Pixel * 16) / 2;
    const int StartY = Button.y + (Button.h - Pixel * 16) / 2;
    for(int Y = 0; Y < 16; ++Y)
      for(int X = 0; X < 16; ++X)
      {
        const char Shade = ControlSectionIcons[SpriteIndex][Y][X];
        if(Shade == '.')
          continue;
        const int Level = Shade - '1';
        Uint8 R = Uint8(Dark[SpriteIndex][0]
          + (Light[SpriteIndex][0] - Dark[SpriteIndex][0]) * Level / 3);
        Uint8 G = Uint8(Dark[SpriteIndex][1]
          + (Light[SpriteIndex][1] - Dark[SpriteIndex][1]) * Level / 3);
        Uint8 B = Uint8(Dark[SpriteIndex][2]
          + (Light[SpriteIndex][2] - Dark[SpriteIndex][2]) * Level / 3);
        if(Selected)
        {
          R = Uint8(std::min(255, int(R) + 22));
          G = Uint8(std::min(255, int(G) + 22));
          B = Uint8(std::min(255, int(B) + 22));
        }
        if(!Enabled)
        {
          const Uint8 Gray = Uint8((int(R) + G + B) / 3);
          R = Uint8((int(Gray) + 25) / 2);
          G = R;
          B = R;
        }
        const SDL_Rect PixelRect = { StartX + X * Pixel,
                                     StartY + Y * Pixel, Pixel, Pixel };
        Fill(Renderer, PixelRect, R, G, B, 255);
      }
  }

  void PaintControlSectionTabs(SDL_Renderer* Renderer)
  {
    const int Selected = SelectedControlSection();
    for(int Index = 0; Index < CONTROL_SECTION_COUNT; ++Index)
    {
      const SDL_Rect Button = ControlSectionTab(Index);
      const bool Enabled = Index == 0 || ActionCountForGroup(Index - 1) > 0;
      const bool IsSelected = Index == Selected;
      Fill(Renderer, Button, IsSelected ? 35 : 19,
           IsSelected ? 71 : 23, IsSelected ? 48 : 28, 246);
      Outline(Renderer, Button, Enabled ? 156 : 76,
              Enabled ? 137 : 68, Enabled ? 100 : 55);
      if(IsSelected && State.PinnedActionGroup == Index - 1)
      {
        const int Inset = Clamp(int(3 * State.Density), 3, 9);
        const SDL_Rect PinnedOutline = {
          Button.x + Inset, Button.y + Inset,
          std::max(1, Button.w - Inset * 2),
          std::max(1, Button.h - Inset * 2)
        };
        Outline(Renderer, PinnedOutline, 235, 218, 174);
      }
      PaintControlSectionIcon(Renderer, Index, Button, IsSelected, Enabled);
    }
  }

  void PaintMainMenuMotifs(SDL_Renderer* Renderer)
  {
    const SDL_Rect& Area = State.Game;
    const int CenterX = Area.x + Area.w / 2;
    const int CenterY = Area.y + Area.h * 3 / 5;
    const int Radius = std::max(40, std::min(Area.w, Area.h) * 27 / 100);
    SDL_Point Star[5];
    const double Pi = 3.14159265358979323846;
    for(int Index = 0; Index < 5; ++Index)
    {
      const double Angle = -Pi / 2.0 + Index * Pi * 2.0 / 5.0;
      Star[Index] = { CenterX + int(std::cos(Angle) * Radius),
                      CenterY + int(std::sin(Angle) * Radius) };
    }
    Color(Renderer, 142, 14, 18, 72);
    for(int Index = 0; Index < 5; ++Index)
    {
      SDL_RenderDrawLine(Renderer, Star[Index].x, Star[Index].y,
                         Star[(Index + 2) % 5].x,
                         Star[(Index + 2) % 5].y);
      const int Next = (Index + 1) % 5;
      SDL_RenderDrawLine(Renderer, Star[Index].x, Star[Index].y,
                         Star[Next].x, Star[Next].y);
    }

    const int WebWidth = Area.w * 34 / 100;
    const int WebHeight = Area.h * 32 / 100;
    for(int Side = 0; Side < 2; ++Side)
    {
      const int CornerX = Side ? Area.x + Area.w - 10 : Area.x + 10;
      const int CornerY = Area.y + 10;
      const int Sign = Side ? -1 : 1;
      SDL_Point Ends[5] = {
        { CornerX + Sign * WebWidth, CornerY },
        { CornerX + Sign * WebWidth * 9 / 10, CornerY + WebHeight / 4 },
        { CornerX + Sign * WebWidth * 3 / 4, CornerY + WebHeight / 2 },
        { CornerX + Sign * WebWidth / 2, CornerY + WebHeight * 3 / 4 },
        { CornerX, CornerY + WebHeight }
      };
      Color(Renderer, 112, 101, 82, 92);
      for(int Ray = 0; Ray < 5; ++Ray)
        SDL_RenderDrawLine(Renderer, CornerX, CornerY,
                           Ends[Ray].x, Ends[Ray].y);
      for(int Ring = 1; Ring <= 3; ++Ring)
        for(int Segment = 0; Segment < 4; ++Segment)
        {
          const SDL_Point A = {
            CornerX + (Ends[Segment].x - CornerX) * Ring / 4,
            CornerY + (Ends[Segment].y - CornerY) * Ring / 4
          };
          const SDL_Point B = {
            CornerX + (Ends[Segment + 1].x - CornerX) * Ring / 4,
            CornerY + (Ends[Segment + 1].y - CornerY) * Ring / 4
          };
          SDL_RenderDrawLine(Renderer, A.x, A.y, B.x, B.y);
        }
    }
  }

  const unsigned char* Glyph(char Character)
  {
    static const unsigned char Letters[26][7] = {
      {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
      {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
      {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
      {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
      {31,4,4,4,4,4,31}, {7,2,2,2,18,18,12},
      {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
      {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
      {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
      {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
      {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
      {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
      {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
      {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
    };
    static const unsigned char Lowercase[26][7] = {
      {0,0,14,1,15,17,15}, {16,16,30,17,17,17,30},
      {0,0,14,16,16,17,14}, {1,1,15,17,17,17,15},
      {0,0,14,17,31,16,14}, {6,9,8,28,8,8,8},
      {0,0,15,17,15,1,14}, {16,16,30,17,17,17,17},
      {4,0,12,4,4,4,14}, {2,0,6,2,2,18,12},
      {16,16,18,20,24,20,18}, {12,4,4,4,4,4,14},
      {0,0,26,21,21,17,17}, {0,0,30,17,17,17,17},
      {0,0,14,17,17,17,14}, {0,0,30,17,30,16,16},
      {0,0,15,17,15,1,1}, {0,0,22,25,16,16,16},
      {0,0,15,16,14,1,30}, {8,8,28,8,8,9,6},
      {0,0,17,17,17,19,13}, {0,0,17,17,17,10,4},
      {0,0,17,17,21,21,10}, {0,0,17,10,4,10,17},
      {0,0,17,17,15,1,14}, {0,0,31,2,4,8,31}
    };
    static const unsigned char Digits[10][7] = {
      {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
      {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
      {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
      {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
      {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
    };
    static const unsigned char Period[7] = {0,0,0,0,0,12,12};
    static const unsigned char Comma[7] = {0,0,0,0,0,12,8};
    static const unsigned char Colon[7] = {0,12,12,0,12,12,0};
    static const unsigned char Semicolon[7] = {0,12,12,0,12,8,0};
    static const unsigned char Slash[7] = {1,2,2,4,8,8,16};
    static const unsigned char Dash[7] = {0,0,0,31,0,0,0};
    static const unsigned char Underscore[7] = {0,0,0,0,0,0,31};
    static const unsigned char Plus[7] = {0,4,4,31,4,4,0};
    static const unsigned char Bang[7] = {4,4,4,4,4,0,4};
    static const unsigned char Question[7] = {14,17,1,2,4,0,4};
    static const unsigned char Apostrophe[7] = {4,4,8,0,0,0,0};
    static const unsigned char Quote[7] = {10,10,20,0,0,0,0};
    static const unsigned char Hash[7] = {10,31,10,10,31,10,0};
    static const unsigned char Dollar[7] = {4,15,20,14,5,30,4};
    static const unsigned char Ampersand[7] = {12,18,20,8,21,18,13};
    static const unsigned char Asterisk[7] = {0,21,14,31,14,21,0};
    static const unsigned char LeftParen[7] = {2,4,8,8,8,4,2};
    static const unsigned char RightParen[7] = {8,4,2,2,2,4,8};
    static const unsigned char Percent[7] = {17,2,4,8,16,17,0};
    static const unsigned char Equals[7] = {0,31,0,31,0,0,0};
    static const unsigned char Less[7] = {1,2,4,8,4,2,1};
    static const unsigned char Greater[7] = {16,8,4,2,4,8,16};
    static const unsigned char LeftBracket[7] = {14,8,8,8,8,8,14};
    static const unsigned char Backslash[7] = {16,8,8,4,2,2,1};
    static const unsigned char RightBracket[7] = {14,2,2,2,2,2,14};
    static const unsigned char Caret[7] = {4,10,17,0,0,0,0};
    static const unsigned char Grave[7] = {8,4,0,0,0,0,0};
    static const unsigned char LeftBrace[7] = {2,4,4,8,4,4,2};
    static const unsigned char Pipe[7] = {4,4,4,4,4,4,4};
    static const unsigned char RightBrace[7] = {8,4,4,2,4,4,8};
    static const unsigned char Tilde[7] = {0,0,13,18,0,0,0};
    static const unsigned char At[7] = {14,17,23,21,23,16,14};
    static const unsigned char Blank[7] = {0,0,0,0,0,0,0};
    if(Character >= 'a' && Character <= 'z')
      return Lowercase[Character - 'a'];
    if(Character >= 'A' && Character <= 'Z')
      return Letters[Character - 'A'];
    if(Character >= '0' && Character <= '9')
      return Digits[Character - '0'];
    switch(Character)
    {
     case '.': return Period;
     case ',': return Comma;
     case ':': return Colon;
     case ';': return Semicolon;
     case '/': return Slash;
     case '-': return Dash;
     case '_': return Underscore;
     case '+': return Plus;
     case '!': return Bang;
     case '?': return Question;
     case '\'': return Apostrophe;
     case '"': return Quote;
     case '#': return Hash;
     case '$': return Dollar;
     case '&': return Ampersand;
     case '*': return Asterisk;
     case '(': return LeftParen;
     case ')': return RightParen;
     case '%': return Percent;
     case '=': return Equals;
     case '<': return Less;
     case '>': return Greater;
     case '[': return LeftBracket;
     case '\\': return Backslash;
     case ']': return RightBracket;
     case '^': return Caret;
     case '`': return Grave;
     case '{': return LeftBrace;
     case '|': return Pipe;
     case '}': return RightBrace;
     case '~': return Tilde;
     case '@': return At;
    }
    return Blank;
  }

  int TextWidth(const char* Value, int Scale)
  {
    return Value && *Value ? int(std::strlen(Value)) * Scale * 6 - Scale : 0;
  }

  void Text(SDL_Renderer* Renderer, int X, int Y, const char* Value, int Scale,
            Uint8 R = 240, Uint8 G = 230, Uint8 B = 202)
  {
    Color(Renderer, R, G, B);
    SDL_Rect Pixels[256];
    int PixelCount = 0;
    for(; *Value; ++Value, X += Scale * 6)
    {
      const unsigned char* Rows = Glyph(*Value);
      for(int Row = 0; Row < 7; ++Row)
        for(int Column = 0; Column < 5; ++Column)
          if(Rows[Row] & (1 << (4 - Column)))
          {
            if(PixelCount == 256)
            {
              SDL_RenderFillRects(Renderer, Pixels, PixelCount);
              PixelCount = 0;
            }
            Pixels[PixelCount++] = { X + Column * Scale, Y + Row * Scale,
                                     Scale, Scale };
          }
    }
    if(PixelCount)
      SDL_RenderFillRects(Renderer, Pixels, PixelCount);
  }

  std::vector<std::string> WrapText(const std::string& Value, int Columns)
  {
    std::vector<std::string> Lines;
    std::string Line;
    size_t Position = 0;
    Columns = std::max(1, Columns);
    while(Position < Value.size())
    {
      if(Value[Position] == '\n')
      {
        if(!Line.empty())
        {
          Lines.push_back(Line);
          Line.clear();
        }
        else if(Lines.empty() || !Lines.back().empty())
          Lines.push_back("");
        ++Position;
        continue;
      }
      while(Position < Value.size() && Value[Position] != '\n'
            && std::isspace((unsigned char)Value[Position]))
        ++Position;
      if(Position >= Value.size())
        break;
      if(Value[Position] == '\n')
        continue;
      size_t End = Position;
      while(End < Value.size() && !std::isspace((unsigned char)Value[End]))
        ++End;
      std::string Word = Value.substr(Position, End - Position);
      Position = End;
      while((int)Word.size() > Columns)
      {
        if(!Line.empty())
        {
          Lines.push_back(Line);
          Line.clear();
        }
        Lines.push_back(Word.substr(0, Columns));
        Word.erase(0, Columns);
      }
      if(Line.empty())
        Line = Word;
      else if((int)(Line.size() + 1 + Word.size()) <= Columns)
        Line += " " + Word;
      else
      {
        Lines.push_back(Line);
        Line = Word;
      }
    }
    if(!Line.empty())
      Lines.push_back(Line);
    return Lines;
  }

  std::string FormatScreenText(const std::string& Value)
  {
    // The original text screens use single newlines to fit the 640-pixel
    // desktop console.  On a responsive mobile canvas those line endings
    // create ragged, prematurely broken prose.  Keep blank lines as paragraph
    // breaks and let the mobile wrapper reflow everything within a paragraph.
    std::string Result;
    size_t Position = 0;
    while(Position < Value.size())
    {
      if(Value[Position] == '\r')
      {
        ++Position;
        continue;
      }
      if(Value[Position] == '\n')
      {
        int Newlines = 0;
        while(Position < Value.size()
              && (Value[Position] == '\n' || Value[Position] == '\r'))
        {
          if(Value[Position] == '\n')
            ++Newlines;
          ++Position;
        }
        while(!Result.empty() && Result.back() == ' ')
          Result.pop_back();
        if(Newlines >= 2)
        {
          while(!Result.empty() && Result.back() == '\n')
            Result.pop_back();
          Result += "\n\n";
        }
        else if(!Result.empty() && Result.back() != '\n')
          Result += ' ';
        continue;
      }
      Result += Value[Position++];
    }
    while(!Result.empty()
          && (Result.back() == ' ' || Result.back() == '\n'))
      Result.pop_back();

    const char DesktopContinue[] = "Press any key to continue.";
    size_t Instruction = Result.find(DesktopContinue);
    while(Instruction != std::string::npos)
    {
      Result.replace(Instruction, sizeof(DesktopContinue) - 1,
                     "Tap anywhere to continue.");
      Instruction = Result.find(DesktopContinue,
                                Instruction + sizeof(DesktopContinue));
    }
    return Result;
  }

  std::string FormatPromptText(const std::string& Value)
  {
    std::string Result = Value;

    // Desktop commands embed keyboard directions in their question text.
    // The Android input surface already shows the available direction pad, so
    // replace those instructions without changing the underlying command.
    size_t Start = Result.find("[press a direction key");
    if(Start != std::string::npos)
    {
      const size_t End = Result.find(']', Start);
      if(End != std::string::npos)
        Result.replace(Start, End - Start + 1,
                       "Choose a direction below.");
    }

    const char* ObsoleteHelp[] = { "[F1 - help]", "[press F1 for help]" };
    for(size_t Index = 0;
        Index < sizeof(ObsoleteHelp) / sizeof(ObsoleteHelp[0]); ++Index)
    {
      while((Start = Result.find(ObsoleteHelp[Index])) != std::string::npos)
        Result.erase(Start, std::strlen(ObsoleteHelp[Index]));
    }

    const char ContinueText[] = "[press any key to continue]";
    while((Start = Result.find(ContinueText)) != std::string::npos)
      Result.replace(Start, sizeof(ContinueText) - 1,
                     "Tap a control to continue.");

    while(!Result.empty() && std::isspace((unsigned char)Result.back()))
      Result.pop_back();
    return Result;
  }

  bool IsSettingTextPrompt(const std::string& Prompt)
  {
    return Prompt.compare(0, 4, "Set ") == 0;
  }

  std::string SettingPromptTitle(const std::string& Prompt)
  {
    size_t Start = Prompt.compare(0, 8, "Set new ") == 0 ? 8 : 4;
    size_t End = Prompt.find('(', Start);
    const size_t Colon = Prompt.find(':', Start);
    if(End == std::string::npos || (Colon != std::string::npos && Colon < End))
      End = Colon;
    if(End == std::string::npos)
      End = Prompt.size();

    std::string Title = Prompt.substr(Start, End - Start);
    while(!Title.empty() && std::isspace((unsigned char)Title.back()))
      Title.pop_back();

    if(Title == "default name for the starting pet")
      return "STARTING PET NAME";
    if(Title == "default name")
      return "DEFAULT PLAYER NAME";
    if(Title == "RGB color")
      return "BACKGROUND COLOR";
    return Title.empty() ? "EDIT SETTING" : Title;
  }

  std::string SettingPromptHelp(const std::string& Prompt)
  {
    const size_t Details = Prompt.find('(');
    if(Details == std::string::npos)
      return "ENTER A NEW VALUE.";

    std::string Help = Prompt.substr(Details);
    while(!Help.empty() && (Help.back() == ':'
                            || std::isspace((unsigned char)Help.back())))
      Help.pop_back();
    if(!Help.empty() && Help.front() == '(')
    {
      const size_t Closing = Help.find(')');
      if(Closing != std::string::npos)
      {
        Help.erase(Closing, 1);
        Help.erase(0, 1);
        if(Closing - 1 < Help.size() && Help[Closing - 1] == ',')
          Help.erase(Closing - 1, 1);
      }
    }
    return Help;
  }

  void WrappedText(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                   const std::string& Value, bool CenterVertically = true,
                   int LineSpacingPercent = 0, int MaximumScale = 4)
  {
    if(Value.empty())
      return;
    int Scale = 1;
    std::vector<std::string> Lines;
    int LinesFit = 1;
    for(int Candidate = MaximumScale; Candidate >= 1; --Candidate)
    {
      const int Columns = std::max(1, (Rect.w - 20) / (Candidate * 6));
      std::vector<std::string> CandidateLines = WrapText(Value, Columns);
      const int LineAdvance = LineSpacingPercent
        ? std::max(1, (Candidate * 7 * LineSpacingPercent + 50) / 100)
        : Candidate * 8;
      const int CandidateLinesFit = std::max(1, (Rect.h - 16) / LineAdvance);
      if((int)CandidateLines.size() <= CandidateLinesFit || Candidate == 1)
      {
        Scale = Candidate;
        Lines.swap(CandidateLines);
        LinesFit = CandidateLinesFit;
        break;
      }
    }
    if((int)Lines.size() > LinesFit)
      Lines.resize(LinesFit);
    const int LineAdvance = LineSpacingPercent
      ? std::max(1, (Scale * 7 * LineSpacingPercent + 50) / 100)
      : Scale * 8;
    const int TotalHeight = Lines.empty() ? 0
      : (int(Lines.size()) - 1) * LineAdvance + Scale * 7;
    int Y = CenterVertically
      ? Rect.y + (Rect.h - TotalHeight) / 2 : Rect.y + 8;
    for(size_t Index = 0; Index < Lines.size(); ++Index, Y += LineAdvance)
      Text(Renderer, Rect.x + 10, Y, Lines[Index].c_str(), Scale);
  }

  std::string MobileItemTitle(const std::string& Label)
  {
    const size_t LegacySummary = Label.find(" [");
    return LegacySummary == std::string::npos
      ? Label : Label.substr(0, LegacySummary);
  }

  void CenteredWrappedText(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                           const std::string& Value, int MaximumScale = 4,
                           Uint8 R = 240, Uint8 G = 230, Uint8 B = 202)
  {
    if(Value.empty())
      return;
    int Scale = 1;
    std::vector<std::string> Lines;
    for(int Candidate = MaximumScale; Candidate >= 1; --Candidate)
    {
      const int Columns = std::max(1, (Rect.w - 20) / (Candidate * 6));
      std::vector<std::string> CandidateLines = WrapText(Value, Columns);
      const int LineAdvance = Candidate * 8;
      if((int(CandidateLines.size()) * LineAdvance <= Rect.h - 16)
         || Candidate == 1)
      {
        Scale = Candidate;
        Lines.swap(CandidateLines);
        break;
      }
    }
    const int LineAdvance = Scale * 8;
    const int TotalHeight = (int(Lines.size()) - 1) * LineAdvance
                          + Scale * 7;
    int Y = Rect.y + (Rect.h - TotalHeight) / 2;
    for(const std::string& Line : Lines)
    {
      const int Width = TextWidth(Line.c_str(), Scale);
      Text(Renderer, Rect.x + (Rect.w - Width) / 2, Y,
           Line.c_str(), Scale, R, G, B);
      Y += LineAdvance;
    }
  }

  void CenterText(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                  const char* Value, int MaximumScale = 8,
                  Uint8 R = 240, Uint8 G = 230, Uint8 B = 202)
  {
    const int Length = std::max(1, int(std::strlen(Value)));
    const int Scale = Clamp(std::min(Rect.h / 11, Rect.w / (Length * 6 + 2)),
                            1, MaximumScale);
    const int Width = TextWidth(Value, Scale);
    Text(Renderer, Rect.x + (Rect.w - Width) / 2,
         Rect.y + (Rect.h - Scale * 7) / 2, Value, Scale, R, G, B);
  }

  int FittingTextScale(const SDL_Rect& Rect, const std::string& Value,
                       int MaximumScale)
  {
    const int Length = std::max(1, int(Value.size()));
    return Clamp(std::min(Rect.h / 11, Rect.w / (Length * 6 + 2)),
                 1, MaximumScale);
  }

  void CenterTextAtScale(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                         const char* Value, int Scale,
                         Uint8 R = 240, Uint8 G = 230, Uint8 B = 202)
  {
    const int Width = TextWidth(Value, Scale);
    Text(Renderer, Rect.x + (Rect.w - Width) / 2,
         Rect.y + (Rect.h - Scale * 7) / 2, Value, Scale, R, G, B);
  }

  void LeftTextAtScale(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                       const char* Value, int Scale,
                       Uint8 R = 240, Uint8 G = 230, Uint8 B = 202)
  {
    Text(Renderer, Rect.x, Rect.y + (Rect.h - Scale * 7) / 2,
         Value, Scale, R, G, B);
  }

  void SingleLineText(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                      const std::string& Value)
  {
    std::string Visible = Value;
    std::replace(Visible.begin(), Visible.end(), '\n', ' ');
    std::replace(Visible.begin(), Visible.end(), '\r', ' ');
    const int Scale = Clamp(Rect.h / 11, 2, 4);
    const int MaximumCharacters = std::max(1, (Rect.w - 20) / (Scale * 6));
    if((int)Visible.size() > MaximumCharacters)
    {
      if(MaximumCharacters > 3)
        Visible = Visible.substr(0, MaximumCharacters - 3) + "...";
      else
        Visible.resize(MaximumCharacters);
    }
    SDL_Rect Inner = { Rect.x + 10, Rect.y, std::max(1, Rect.w - 20), Rect.h };
    LeftTextAtScale(Renderer, Inner, Visible.c_str(), Scale);
  }

  std::string FlattenGameplayBannerText(const std::string& Value)
  {
    std::string Visible = Value;
    std::replace(Visible.begin(), Visible.end(), '\n', ' ');
    std::replace(Visible.begin(), Visible.end(), '\r', ' ');
    return Visible;
  }

  bool GameplayBannerNeedsTwoLines(const SDL_Rect& Rect,
                                   const std::string& Value)
  {
    const int Scale = Clamp(Rect.h / 11, 2, 4);
    const int Columns = std::max(1, (Rect.w - 20) / (Scale * 6));
    return (int)FlattenGameplayBannerText(Value).size() > Columns;
  }

  bool BinaryConfirmationActive()
  {
    return State.QuestionChoiceCount == 2
        && State.QuestionChoices[0] == 'y'
        && State.QuestionChoices[1] == 'n';
  }

  SDL_Rect BinaryConfirmationButton(int Index)
  {
    const int Height = std::max(1, State.Controls.h / 3);
    return { State.Controls.x + Index * State.Controls.w / 2,
             State.Controls.y + State.Controls.h - Height,
             State.Controls.w / 2, Height };
  }

  bool GameplayPromptShowsLogContext()
  {
    if(!State.PromptActive || State.PromptShowsInput)
      return false;
    const bool BinaryConfirmation = BinaryConfirmationActive();
    if(BinaryConfirmation && !State.PromptDetail.empty())
      return true;
    if(!State.PromptDetail.empty() || State.LogMessage.empty())
      return false;
    std::string Prompt = State.PromptText;
    std::transform(Prompt.begin(), Prompt.end(), Prompt.begin(),
      [](unsigned char Character) { return char(std::tolower(Character)); });
    return Prompt.find("continue anyway") != std::string::npos
        || Prompt.find("still continue") != std::string::npos
        || Prompt == "continue? [y/n]";
  }

  SDL_Rect GameplayLogRect()
  {
    SDL_Rect Rect = State.Log;
    // Portrait binary confirmations receive dedicated space during layout;
    // do not grow that panel back over the map.
    if(BinaryConfirmationActive() && State.Width < State.Height)
      return Rect;
    if(GameplayPromptShowsLogContext())
    {
      const int Expansion = State.PromptDetail.empty() ? 2 : 4;
      Rect.y -= Rect.h * (Expansion - 1);
      Rect.h *= Expansion;
    }
    else if(!State.PromptActive
       && GameplayBannerNeedsTwoLines(Rect, State.LogMessage))
    {
      Rect.y -= Rect.h;
      Rect.h *= 2;
    }
    return Rect;
  }

  void GameplayBannerText(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                          const std::string& Value, int RowHeight)
  {
    const std::string Visible = FlattenGameplayBannerText(Value);
    const int Scale = Clamp(RowHeight / 11, 2, 4);
    const int Columns = std::max(1, (Rect.w - 20) / (Scale * 6));
    std::vector<std::string> Lines = WrapText(Visible, Columns);
    if(Lines.size() > 2)
    {
      Lines.resize(2);
      std::string& Last = Lines.back();
      if(Columns > 3)
      {
        Last.resize(std::min(Last.size(), size_t(Columns - 3)));
        while(!Last.empty() && std::isspace((unsigned char)Last.back()))
          Last.pop_back();
        Last += "...";
      }
    }

    const int LineAdvance = Scale * 8;
    const int TotalHeight = Lines.empty() ? 0
      : (int(Lines.size()) - 1) * LineAdvance + Scale * 7;
    int Y = Rect.y + (Rect.h - TotalHeight) / 2;
    for(const std::string& Line : Lines)
    {
      Text(Renderer, Rect.x + 10, Y, Line.c_str(), Scale);
      Y += LineAdvance;
    }
  }

  void PaintGameplayLog(SDL_Renderer* Renderer)
  {
    if(!ShowGameplayLog())
      return;
    const SDL_Rect LogRect = GameplayLogRect();
    Frame(Renderer, LogRect);
    std::string VisibleLog = State.LogMessage;
    if(State.PromptActive)
    {
      if(GameplayPromptShowsLogContext())
      {
        const int Pad = 7;
        const int PromptHeight = BinaryConfirmationActive()
                              && State.Width < State.Height
          ? Clamp(LogRect.h / 4, 64, 112)
          : std::max(State.Log.h, LogRect.h / 3);
        SDL_Rect Message = { LogRect.x + Pad, LogRect.y + Pad,
                             LogRect.w - Pad * 2,
                             LogRect.h - PromptHeight - Pad * 2 };
        SDL_Rect Prompt = { LogRect.x + Pad,
                            LogRect.y + LogRect.h - PromptHeight,
                            LogRect.w - Pad * 2, PromptHeight - Pad };
        const std::string& Context = State.PromptDetail.empty()
          ? State.LogMessage : State.PromptDetail;
        WrappedText(Renderer, Message, Context, false, 140, 4);
        SDL_SetRenderDrawColor(Renderer, 126, 102, 57, 220);
        SDL_RenderDrawLine(Renderer, Prompt.x, Prompt.y - 1,
                           Prompt.x + Prompt.w, Prompt.y - 1);
        WrappedText(Renderer, Prompt, State.PromptText, true, 0, 4);
        return;
      }
      // A detail string is the complete touch-facing prompt. Look mode uses
      // it for the current tile description; appending the desktop keyboard
      // hint (for example, "examine a (c)haracter") wastes a row and exposes
      // controls that are not present on the direction pad.
      VisibleLog = State.PromptDetail.empty()
        ? State.PromptText : State.PromptDetail;
      if(State.PromptShowsInput)
      {
        VisibleLog += "\n> ";
        VisibleLog += State.PromptInput;
        VisibleLog += "_";
      }
      WrappedText(Renderer, { LogRect.x + 5, LogRect.y + 5,
                              LogRect.w - 10, LogRect.h - 10 },
                  VisibleLog);
    }
    else
      GameplayBannerText(Renderer, LogRect, VisibleLog, State.Log.h);
  }

  void MenuRowText(SDL_Renderer* Renderer, const SDL_Rect& Row,
                   const std::string& Value, int Padding, int Scale,
                   Uint8 R, Uint8 G, Uint8 B)
  {
    // Match Story text's 140% baseline spacing so wrapped menu rows preserve
    // a clear gap between the 7-pixel-high glyph lines.
    const int LineSpacingPercent = 140;
    const int AvailableWidth = std::max(1, Row.w - Padding * 2);
    const int Columns = std::max(1, AvailableWidth / (Scale * 6));
    std::vector<std::string> Lines = WrapText(Value, Columns);
    const int LineAdvance = std::max(1,
      (Scale * 7 * LineSpacingPercent + 50) / 100);
    const int LinesFit = Row.h >= Scale * 7
      ? 1 + std::max(0, Row.h - Scale * 7) / LineAdvance : 1;

    if(Lines.empty())
      Lines.push_back("");
    if((int)Lines.size() > LinesFit)
    {
      Lines.resize(LinesFit);
      std::string& Last = Lines.back();
      if(Columns > 3)
      {
        Last.resize(std::min(Last.size(), size_t(Columns - 3)));
        while(!Last.empty() && std::isspace((unsigned char)Last.back()))
          Last.pop_back();
        Last += "...";
      }
      else
        Last.resize(std::min(Last.size(), size_t(Columns)));
    }

    const int TotalHeight = (int(Lines.size()) - 1) * LineAdvance + Scale * 7;
    int Y = Row.y + (Row.h - TotalHeight) / 2;
    for(size_t Index = 0; Index < Lines.size(); ++Index, Y += LineAdvance)
      Text(Renderer, Row.x + Padding, Y, Lines[Index].c_str(), Scale, R, G, B);
  }

  std::vector<std::string> StatGroups(const std::string& Value)
  {
    std::vector<std::string> Groups;
    size_t Begin = 0;
    while(Begin < Value.size())
    {
      const size_t End = Value.find("  ", Begin);
      Groups.push_back(Value.substr(Begin, End == std::string::npos
                                             ? std::string::npos : End - Begin));
      if(End == std::string::npos)
        break;
      Begin = End + 2;
      while(Begin < Value.size() && Value[Begin] == ' ')
        ++Begin;
    }
    return Groups;
  }

  std::string ExpandedStatValue(int Index, const std::string& Value)
  {
    static const char* Labels[15] = {
      "HEALTH", NULL, NULL, "ARM STRENGTH", "LEG STRENGTH",
      "DEXTERITY", "AGILITY", "ENDURANCE", "PERCEPTION",
      "INTELLIGENCE", "WISDOM", "WILLPOWER", "CHARISMA",
      "HEIGHT", "WEIGHT"
    };
    if(Index < 0 || Index >= 15 || !Labels[Index])
      return Value;
    const size_t Number = Value.find(' ');
    return std::string(Labels[Index])
      + (Number == std::string::npos ? "" : Value.substr(Number));
  }

  void PaintStats(SDL_Renderer* Renderer)
  {
    std::vector<std::string> Values;
    for(int Line = 0; Line < 4; ++Line)
    {
      const std::vector<std::string> LineValues = StatGroups(State.StatsLines[Line]);
      Values.insert(Values.end(), LineValues.begin(), LineValues.end());
    }
    if(Values.size() < 15)
      return;

    // Source order is HP/MANA/GOLD, ARM/LEG/DEX/AGI,
    // END/PER/INT/WIS, WILL/CHA/HEIGHT/WEIGHT.  Present it as semantic
    // vertical groups so related values remain together in either rotation.
    static const int Groups[5][4] = {
      { 0, 1, 2, -1 },       // resources: HP, mana, gold
      { 3, 4, 7, -1 },       // physical: arm, leg, endurance
      { 5, 6, 8, -1 },       // mobility/senses: dex, agility, perception
      { 9, 10, 11, 12 },     // mental/social: int, wis, will, charisma
      { 13, 14, -1, -1 }     // body: height, weight
    };
    static const int GroupSizes[5] = { 3, 3, 3, 4, 2 };
    const int Left = State.Stats.x + 8;
    const int Top = State.Stats.y + 5;
    const int Width = State.Stats.w - 16;
    const int Height = State.Stats.h - 10;
    const int CutoutRight = State.DisplayCutout.x + State.DisplayCutout.w;
    const int CutoutBottom = State.DisplayCutout.y + State.DisplayCutout.h;
    const bool PortraitCutout = State.Width < State.Height
      && State.DisplayCutout.w > 0 && State.DisplayCutout.h > 0
      && State.DisplayCutout.y <= State.Stats.y
      && CutoutBottom < State.Stats.y + State.Stats.h;
    if(PortraitCutout)
    {
      const int Pad = Clamp(int(3 * State.Density), 5, 14);
      // The columns already split around the camera cutout horizontally, so
      // all four stat rows can use equal heights.  Giving the bottom row the
      // old two-fifths allocation centered it much lower than the other rows.
      const int BottomRowTop = Top + Height * 3 / 4;
      const int Bottom = Top + Height;
      const int LeftWidth = std::max(1, State.DisplayCutout.x - Pad - Left);
      const int RightX = CutoutRight + Pad;
      const int RightWidth = std::max(1, Left + Width - RightX);

      std::vector<SDL_Rect> Cells;
      std::vector<int> CellValues;
      auto AddGroup = [&](int Group, int Rows, int X, int Y, int W, int H)
      {
        for(int Row = 0; Row < Rows; ++Row)
        {
          const int Y0 = Y + H * Row / Rows;
          const int Y1 = Y + H * (Row + 1) / Rows;
          Cells.push_back({ X + 4, Y0, std::max(1, W - 8),
                            std::max(1, Y1 - Y0) });
          CellValues.push_back(Groups[Group][Row]);
        }
      };

      const int LeftColumn = LeftWidth / 2;
      const int RightColumn = RightWidth / 2;
      AddGroup(0, 3, Left, Top, LeftColumn, BottomRowTop - Top);
      AddGroup(1, 3, Left + LeftColumn, Top,
               LeftWidth - LeftColumn, BottomRowTop - Top);
      // Keep complete attribute families together around the camera hole:
      // resources and body strength on the left, mobility/senses and mental
      // attributes on the right.
      AddGroup(2, 3, RightX, Top, RightColumn, BottomRowTop - Top);
      AddGroup(3, 3, RightX + RightColumn, Top,
               RightWidth - RightColumn, BottomRowTop - Top);

      Color(Renderer, 72, 62, 47, 180);
      SDL_RenderDrawLine(Renderer, Left + LeftColumn, Top,
                         Left + LeftColumn, BottomRowTop);
      SDL_RenderDrawLine(Renderer, RightX + RightColumn, Top,
                         RightX + RightColumn, BottomRowTop);

      // Complete the mental family and keep the two physical measurements in
      // a compact strip below the camera cutout.
      static const int BottomValues[3] = { 12, 13, 14 };
      for(int Column = 0; Column < 3; ++Column)
      {
        const int X0 = Left + Width * Column / 3;
        const int X1 = Left + Width * (Column + 1) / 3;
        Cells.push_back({ X0 + 4, BottomRowTop,
                          X1 - X0 - 8, std::max(1, Bottom - BottomRowTop) });
        CellValues.push_back(BottomValues[Column]);
        if(Column)
          SDL_RenderDrawLine(Renderer, X0, BottomRowTop, X0, Bottom);
      }

      int SharedScale = 6;
      for(size_t Index = 0; Index < Cells.size(); ++Index)
        SharedScale = std::min(SharedScale,
          FittingTextScale(Cells[Index], Values[CellValues[Index]], 6));
      for(size_t Index = 0; Index < Cells.size(); ++Index)
      {
        const int ValueIndex = CellValues[Index];
        const std::string Expanded = ExpandedStatValue(ValueIndex,
                                                        Values[ValueIndex]);
        if(FittingTextScale(Cells[Index], Expanded, 6) >= SharedScale)
          Values[ValueIndex] = Expanded;
      }
      for(size_t Index = 0; Index < Cells.size(); ++Index)
        LeftTextAtScale(Renderer, Cells[Index],
                        Values[CellValues[Index]].c_str(), SharedScale);
      return;
    }
    int SharedScale = 6;
    for(int Column = 0; Column < 5; ++Column)
      for(int Row = 0; Row < GroupSizes[Column]; ++Row)
      {
        const int X0 = Left + Width * Column / 5;
        const int X1 = Left + Width * (Column + 1) / 5;
        const int Y0 = Top + Height * Row / GroupSizes[Column];
        const int Y1 = Top + Height * (Row + 1) / GroupSizes[Column];
        const SDL_Rect Cell = { X0 + 4, Y0, X1 - X0 - 8, Y1 - Y0 };
        SharedScale = std::min(SharedScale,
          FittingTextScale(Cell, Values[Groups[Column][Row]], 6));
      }
    for(int Column = 0; Column < 5; ++Column)
      for(int Row = 0; Row < GroupSizes[Column]; ++Row)
      {
        const int X0 = Left + Width * Column / 5;
        const int X1 = Left + Width * (Column + 1) / 5;
        const int Y0 = Top + Height * Row / GroupSizes[Column];
        const int Y1 = Top + Height * (Row + 1) / GroupSizes[Column];
        const SDL_Rect Cell = { X0 + 4, Y0, X1 - X0 - 8, Y1 - Y0 };
        const int ValueIndex = Groups[Column][Row];
        const std::string Expanded = ExpandedStatValue(ValueIndex,
                                                        Values[ValueIndex]);
        if(FittingTextScale(Cell, Expanded, 6) >= SharedScale)
          Values[ValueIndex] = Expanded;
      }
    for(int Column = 0; Column < 5; ++Column)
    {
      const int X0 = Left + Width * Column / 5;
      const int X1 = Left + Width * (Column + 1) / 5;
      if(Column)
      {
        Color(Renderer, 72, 62, 47, 180);
        SDL_RenderDrawLine(Renderer, X0, Top + 5, X0, Top + Height - 5);
      }
      for(int Row = 0; Row < GroupSizes[Column]; ++Row)
      {
        const int Y0 = Top + Height * Row / GroupSizes[Column];
        const int Y1 = Top + Height * (Row + 1) / GroupSizes[Column];
        SDL_Rect Cell = { X0 + 4, Y0, X1 - X0 - 8, Y1 - Y0 };
        if(State.DisplayCutout.w > 0 && State.DisplayCutout.h > 0)
        {
          const int CellRight = Cell.x + Cell.w;
          const int CellBottom = Cell.y + Cell.h;
          const int CutoutRight = State.DisplayCutout.x + State.DisplayCutout.w;
          const int CutoutBottom = State.DisplayCutout.y + State.DisplayCutout.h;
          if(Cell.x < CutoutRight && CellRight > State.DisplayCutout.x
             && Cell.y < CutoutBottom && CellBottom > State.DisplayCutout.y)
          {
            const int Pad = Clamp(int(3 * State.Density), 4, 12);
            const int Below = CellBottom - CutoutBottom - Pad;
            const int LeftSpace = State.DisplayCutout.x - Cell.x - Pad;
            const int RightSpace = CellRight - CutoutRight - Pad;
            if(Below >= 14)
            {
              Cell.y = CutoutBottom + Pad;
              Cell.h = Below;
            }
            else if(LeftSpace >= RightSpace && LeftSpace > 0)
              Cell.w = LeftSpace;
            else if(RightSpace > 0)
            {
              Cell.x = CutoutRight + Pad;
              Cell.w = RightSpace;
            }
          }
        }
        LeftTextAtScale(Renderer, Cell,
                        Values[Groups[Column][Row]].c_str(), SharedScale);
      }
    }
  }

  void RenderTextureFit(SDL_Renderer* Renderer, SDL_Texture* Texture,
                        const SDL_Rect& Source, const SDL_Rect& Area)
  {
    if(!Texture || Source.w <= 0 || Source.h <= 0
       || Area.w <= 0 || Area.h <= 0)
      return;
    SDL_Rect Destination = Area;
    const float SourceAspect = float(Source.w) / Source.h;
    const float DestinationAspect = float(Area.w) / Area.h;
    if(DestinationAspect > SourceAspect)
    {
      Destination.w = std::max(1, int(Area.h * SourceAspect));
      Destination.x += (Area.w - Destination.w) / 2;
    }
    else
    {
      Destination.h = std::max(1, int(Area.w / SourceAspect));
      Destination.y += (Area.h - Destination.h) / 2;
    }
    SDL_RenderCopy(Renderer, Texture, &Source, &Destination);
  }

  void CaptureGameplaySnapshot(SDL_Renderer* Renderer,
                               SDL_Texture* GameTexture)
  {
    if(!Renderer || !GameTexture || State.GameWidth <= 0 || State.GameHeight <= 0)
      return;
    if(!GameplaySnapshot || GameplaySnapshotWidth != State.GameWidth
       || GameplaySnapshotHeight != State.GameHeight)
    {
      if(GameplaySnapshot)
        SDL_DestroyTexture(GameplaySnapshot);
      GameplaySnapshot = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, State.GameWidth, State.GameHeight);
      GameplaySnapshotWidth = GameplaySnapshot ? State.GameWidth : 0;
      GameplaySnapshotHeight = GameplaySnapshot ? State.GameHeight : 0;
      if(GameplaySnapshot)
        SDL_SetTextureBlendMode(GameplaySnapshot, SDL_BLENDMODE_NONE);
    }
    if(!GameplaySnapshot)
      return;

    SDL_Texture* PreviousTarget = SDL_GetRenderTarget(Renderer);
    Uint8 Red = 0, Green = 0, Blue = 0, Alpha = 255;
    SDL_GetRenderDrawColor(Renderer, &Red, &Green, &Blue, &Alpha);
    if(SDL_SetRenderTarget(Renderer, GameplaySnapshot) == 0)
    {
      SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
      SDL_RenderClear(Renderer);
      SDL_RenderCopy(Renderer, GameTexture, 0, 0);
      SDL_SetRenderTarget(Renderer, PreviousTarget);
    }
    SDL_SetRenderDrawColor(Renderer, Red, Green, Blue, Alpha);
  }

  void AppendMetric(std::ostringstream& Out, const char* Label, int Value)
  {
    Out << Label << ' ' << Value << '\n';
  }

  std::string MobileItemMetrics(const adaptiveui::ItemMetrics& Candidate)
  {
    if(!Candidate.Present)
      return "";
    std::ostringstream Out;
    if(Candidate.Weapon)
    {
      Out << "DAMAGE " << Candidate.MinimumDamage << '-'
          << Candidate.MaximumDamage << '\n';
      AppendMetric(Out, "HIT", Candidate.ToHit);
      if(!Candidate.Accuracy.empty())
        Out << "ACCURACY " << Candidate.Accuracy << '\n';
      if(!Candidate.Durability.empty())
        Out << "DURABILITY " << Candidate.Durability << '\n';
      if(Candidate.CategorySkill || Candidate.SpecificSkill)
        Out << "SKILL " << Candidate.CategorySkill << '/'
            << Candidate.SpecificSkill << '\n';
    }
    if(Candidate.Armor)
      AppendMetric(Out, "ARMOR", Candidate.ArmorValue);
    if(Candidate.Shield)
    {
      AppendMetric(Out, "BLOCK", Candidate.Block);
      if(!Candidate.BlockQuality.empty())
        Out << "BLOCK QUALITY " << Candidate.BlockQuality << '\n';
    }
    if(Candidate.Enchantment)
      AppendMetric(Out, "ENCHANTMENT", Candidate.Enchantment);
    AppendMetric(Out, "WEIGHT", int(Candidate.Weight));
    return Out.str();
  }

  struct mobilecomparisonrow
  {
    std::string Label;
    std::string Current;
    std::string Selected;
    int Advantage;
  };

  std::string NumberText(int Value)
  {
    std::ostringstream Out;
    Out << Value;
    return Out.str();
  }

  std::string DamageText(const adaptiveui::ItemMetrics& Metrics)
  {
    std::ostringstream Out;
    Out << Metrics.MinimumDamage << '-' << Metrics.MaximumDamage;
    return Out.str();
  }

  std::string SkillText(const adaptiveui::ItemMetrics& Metrics)
  {
    std::ostringstream Out;
    Out << Metrics.CategorySkill << '/' << Metrics.SpecificSkill;
    return Out.str();
  }

  void AddComparisonRow(std::vector<mobilecomparisonrow>& Rows,
                        const char* Label, const std::string& Current,
                        const std::string& Selected, int Advantage)
  {
    mobilecomparisonrow Row;
    Row.Label = Label;
    Row.Current = Current;
    Row.Selected = Selected;
    Row.Advantage = Advantage;
    Rows.push_back(Row);
  }

  std::vector<mobilecomparisonrow> MobileComparisonRows(
    const adaptiveui::ItemMetrics& Candidate,
    const adaptiveui::ItemMetrics& Current)
  {
    std::vector<mobilecomparisonrow> Rows;
    if(Candidate.Weapon && Current.Weapon
       && !Candidate.Shield && !Current.Shield)
    {
      AddComparisonRow(Rows, "DAMAGE", DamageText(Current),
        DamageText(Candidate), Candidate.MinimumDamage + Candidate.MaximumDamage
          - Current.MinimumDamage - Current.MaximumDamage);
      AddComparisonRow(Rows, "HIT", NumberText(Current.ToHit),
        NumberText(Candidate.ToHit), Candidate.ToHit - Current.ToHit);
      if(!Candidate.Accuracy.empty() || !Current.Accuracy.empty())
        AddComparisonRow(Rows, "ACCURACY", Current.Accuracy,
          Candidate.Accuracy, Candidate.ToHit - Current.ToHit);
      if(!Candidate.Durability.empty() || !Current.Durability.empty())
        AddComparisonRow(Rows, "DURABILITY", Current.Durability,
          Candidate.Durability, Candidate.ArmorValue - Current.ArmorValue);
      if(Candidate.CategorySkill || Candidate.SpecificSkill
         || Current.CategorySkill || Current.SpecificSkill)
        AddComparisonRow(Rows, "SKILL", SkillText(Current),
          SkillText(Candidate), Candidate.CategorySkill + Candidate.SpecificSkill
            - Current.CategorySkill - Current.SpecificSkill);
    }
    if(Candidate.Armor && Current.Armor)
      AddComparisonRow(Rows, "ARMOR", NumberText(Current.ArmorValue),
        NumberText(Candidate.ArmorValue),
        Candidate.ArmorValue - Current.ArmorValue);
    if(Candidate.Shield && Current.Shield)
    {
      AddComparisonRow(Rows, "BLOCK", NumberText(Current.Block),
        NumberText(Candidate.Block), Candidate.Block - Current.Block);
      if(!Candidate.BlockQuality.empty() || !Current.BlockQuality.empty())
        AddComparisonRow(Rows, "BLOCK QUALITY", Current.BlockQuality,
          Candidate.BlockQuality, Candidate.Block - Current.Block);
    }
    if(Candidate.Enchantment || Current.Enchantment)
      AddComparisonRow(Rows, "ENCHANTMENT", NumberText(Current.Enchantment),
        NumberText(Candidate.Enchantment),
        Candidate.Enchantment - Current.Enchantment);
    AddComparisonRow(Rows, "WEIGHT", NumberText(int(Current.Weight)),
      NumberText(int(Candidate.Weight)), int(Current.Weight - Candidate.Weight));
    return Rows;
  }

  struct mobileitemcardtext
  {
    std::string Description;
    std::string Requirements;
    std::string ComparisonLabel;
    std::vector<std::string> Metrics;
    bool Missing = false;
    bool Ready = false;
  };

  std::string TrimCardText(const std::string& Value)
  {
    size_t First = 0;
    while(First < Value.size()
          && std::isspace((unsigned char)Value[First]))
      ++First;
    size_t Last = Value.size();
    while(Last > First && std::isspace((unsigned char)Value[Last - 1]))
      --Last;
    return Value.substr(First, Last - First);
  }

  std::vector<std::string> CardLines(const std::string& Value)
  {
    std::vector<std::string> Lines;
    size_t Start = 0;
    while(Start <= Value.size())
    {
      const size_t End = Value.find('\n', Start);
      const std::string Line = TrimCardText(Value.substr(
        Start, End == std::string::npos ? std::string::npos : End - Start));
      if(!Line.empty())
        Lines.push_back(Line);
      if(End == std::string::npos)
        break;
      Start = End + 1;
    }
    return Lines;
  }

  mobileitemcardtext MobileItemCardText(
    const std::string& Detail, const std::string& Metrics,
    const adaptiveui::ItemMetrics* Current)
  {
    mobileitemcardtext Result;
    static const std::string DescriptionMarker = "@ITEM_DESCRIPTION@\n";
    if(Detail.compare(0, DescriptionMarker.size(), DescriptionMarker) == 0)
    {
      const size_t DescriptionEnd = Detail.find("\n\n",
                                                DescriptionMarker.size());
      if(DescriptionEnd == std::string::npos)
        Result.Description = TrimCardText(Detail.substr(
          DescriptionMarker.size()));
      else
      {
        Result.Description = TrimCardText(Detail.substr(
          DescriptionMarker.size(),
          DescriptionEnd - DescriptionMarker.size()));
        Result.Requirements = TrimCardText(Detail.substr(DescriptionEnd + 2));
      }
    }
    else
      Result.Description = TrimCardText(Detail);

    Result.Metrics = CardLines(Metrics);
    if(Current && Current->Present)
      Result.ComparisonLabel = Current->Label.empty()
        ? "equipped item" : Current->Label;
    Result.Missing = Result.Requirements.find("MISSING REQUIREMENTS")
                  != std::string::npos;
    Result.Ready = !Result.Missing
                && Result.Requirements.find("READY") != std::string::npos;
    return Result;
  }

  int WrappedLineCount(const std::string& Value, int Width, int Scale)
  {
    if(Value.empty())
      return 0;
    const int Columns = std::max(1, (Width - Scale * 2) / (Scale * 6));
    return std::max(1, int(WrapText(Value, Columns).size()));
  }

  bool CardHeading(const std::string& Value);

  int CardRequirementSectionGapCount(const std::string& Value)
  {
    int Gaps = 0;
    const std::vector<std::string> Lines = CardLines(Value);
    for(size_t Index = 1; Index < Lines.size(); ++Index)
      if(CardHeading(Lines[Index])
         || Lines[Index].compare(0, 6, "Exact ") == 0)
        ++Gaps;
    return Gaps;
  }

  int CardRequirementLineCount(const std::string& Value, int Width, int Scale)
  {
    int Lines = 0;
    const std::vector<std::string> SourceLines = CardLines(Value);
    for(size_t Index = 0; Index < SourceLines.size(); ++Index)
      Lines += WrappedLineCount(SourceLines[Index], Width, Scale);
    return Lines;
  }

  int MobileItemCardScale(const mobileitemcardtext& Card, int Width,
                          int Height)
  {
    for(int Scale = 5; Scale >= 1; --Scale)
    {
      const int Advance = Scale * 8;
      const int DescriptionScale = std::min(5, Scale + 1);
      const int DescriptionLines = WrappedLineCount(
        Card.Description, Width, DescriptionScale);
      const int RequirementLines = CardRequirementLineCount(
        Card.Requirements, Width, Scale);
      const int MetricRows = Card.ComparisonLabel.empty()
        ? (int(Card.Metrics.size()) + 1) / 2
        : int(Card.Metrics.size());
      const int Sections = (!Card.Description.empty() ? 1 : 0)
                         + (!Card.ComparisonLabel.empty() ? 1 : 0)
                         + (!Card.Metrics.empty() ? 1 : 0)
                         + (!Card.Requirements.empty() ? 1 : 0);
      const int Needed = DescriptionLines * DescriptionScale * 8
                       + RequirementLines * Advance
                       + MetricRows * (Card.ComparisonLabel.empty()
                          ? Advance + Scale * 2 : Scale * 11)
                       + (!Card.ComparisonLabel.empty()
                          ? Scale * 31 : 0)
                       + Sections * Scale * 5
                       + CardRequirementSectionGapCount(Card.Requirements)
                         * Scale * 4;
      if(Needed <= Height)
        return Scale;
    }
    return 1;
  }

  bool CardHeading(const std::string& Value)
  {
    bool HasLetter = false;
    for(size_t Index = 0; Index < Value.size(); ++Index)
    {
      const unsigned char Character = (unsigned char)Value[Index];
      if(std::isalpha(Character))
      {
        HasLetter = true;
        if(std::islower(Character))
          return false;
      }
      else if(!std::isspace(Character))
        return false;
    }
    return HasLetter;
  }

  int PaintCardParagraph(SDL_Renderer* Renderer, const SDL_Rect& Area,
                         const std::string& Value, int Scale,
                         Uint8 R, Uint8 G, Uint8 B)
  {
    if(Value.empty() || Area.h <= 0)
      return 0;
    const int Columns = std::max(1, (Area.w - Scale * 2) / (Scale * 6));
    const std::vector<std::string> Lines = WrapText(Value, Columns);
    const int Advance = Scale * 8;
    int Y = Area.y;
    for(size_t Index = 0; Index < Lines.size()
        && Y + Scale * 7 <= Area.y + Area.h; ++Index, Y += Advance)
      Text(Renderer, Area.x, Y, Lines[Index].c_str(), Scale, R, G, B);
    return std::max(0, Y - Area.y);
  }

  int PaintCardRequirements(SDL_Renderer* Renderer, const SDL_Rect& Area,
                            const mobileitemcardtext& Card, int Scale)
  {
    if(Card.Requirements.empty() || Area.h <= 0)
      return 0;
    const int Columns = std::max(1, (Area.w - Scale * 2) / (Scale * 6));
    const int Advance = Scale * 8;
    int Y = Area.y;
    const std::vector<std::string> SourceLines = CardLines(Card.Requirements);
    for(size_t Source = 0; Source < SourceLines.size(); ++Source)
    {
      const std::string& SourceLine = SourceLines[Source];
      const bool Heading = CardHeading(SourceLine);
      const bool ExactNote = SourceLine.compare(0, 6, "Exact ") == 0;
      if(Source > 0 && (Heading || ExactNote))
        Y += Scale * 4;
      const std::vector<std::string> Lines = WrapText(SourceLine, Columns);
      Uint8 R = 224, G = 211, B = 166;
      if(SourceLine == "MISSING REQUIREMENTS")
      {
        R = 239; G = 109; B = 91;
      }
      else if(SourceLine == "READY")
      {
        R = 154; G = 220; B = 119;
      }
      else if(Heading)
      {
        R = 222; G = 189; B = 91;
      }
      else if(SourceLine.compare(0, 5, "Have:") == 0)
      {
        R = Card.Missing ? 190 : 154;
        G = Card.Missing ? 179 : 220;
        B = Card.Missing ? 151 : 119;
      }
      else if(ExactNote)
      {
        R = 155; G = 145; B = 119;
      }
      for(size_t Line = 0; Line < Lines.size()
          && Y + Scale * 7 <= Area.y + Area.h; ++Line, Y += Advance)
        Text(Renderer, Area.x, Y, Lines[Line].c_str(), Scale, R, G, B);
    }
    return std::max(0, Y - Area.y);
  }

  int PaintMobileComparison(SDL_Renderer* Renderer, const SDL_Rect& Area,
                            const adaptiveui::ItemMetrics& Candidate,
                            const adaptiveui::ItemMetrics& Current,
                            const std::string& SelectedLabel, int Scale)
  {
    const std::vector<mobilecomparisonrow> Rows =
      MobileComparisonRows(Candidate, Current);
    if(Rows.empty() || Area.h <= 0)
      return 0;

    const int HeaderHeight = Scale * 31;
    const int RowHeight = Scale * 11;
    const int Height = std::min(Area.h,
      HeaderHeight + int(Rows.size()) * RowHeight);
    SDL_Rect Panel = { Area.x, Area.y, Area.w, Height };
    Fill(Renderer, Panel, 12, 17, 13, 245);
    const int ValueWidth = Panel.w * 38 / 100;
    const int LabelWidth = Panel.w - ValueWidth * 2;
    const int SelectedX = Panel.x + ValueWidth + LabelWidth;

    SDL_Rect CurrentHeader = { Panel.x, Panel.y, ValueWidth, HeaderHeight };
    SDL_Rect SelectedHeader = { SelectedX, Panel.y, ValueWidth, HeaderHeight };
    SDL_Rect CurrentHeading = { CurrentHeader.x, CurrentHeader.y,
                                CurrentHeader.w, Scale * 8 };
    SDL_Rect SelectedHeading = { SelectedHeader.x, SelectedHeader.y,
                                 SelectedHeader.w, Scale * 8 };
    CenterText(Renderer, CurrentHeading, "EQUIPPED",
               std::min(5, Scale + 2), 181, 169, 143);
    CenterText(Renderer, SelectedHeading, "SELECTED",
               std::min(5, Scale + 2), 154, 220, 119);
    SDL_Rect CurrentName = { CurrentHeader.x + Scale,
      CurrentHeader.y + Scale * 8, CurrentHeader.w - Scale * 2,
      CurrentHeader.h - Scale * 8 };
    SDL_Rect SelectedName = { SelectedHeader.x + Scale,
      SelectedHeader.y + Scale * 8, SelectedHeader.w - Scale * 2,
      SelectedHeader.h - Scale * 8 };
    CenteredWrappedText(Renderer, CurrentName,
      Current.Label.empty() ? "equipped item" : Current.Label,
      std::min(5, Scale + 1), 225, 211, 176);
    CenteredWrappedText(Renderer, SelectedName, SelectedLabel,
      std::min(5, Scale + 1), 232, 226, 194);

    SDL_SetRenderDrawColor(Renderer, 92, 83, 55, 210);
    SDL_RenderDrawLine(Renderer, Panel.x, Panel.y + HeaderHeight,
                       Panel.x + Panel.w, Panel.y + HeaderHeight);
    for(size_t Index = 0; Index < Rows.size(); ++Index)
    {
      const int RowY = Panel.y + HeaderHeight + int(Index) * RowHeight;
      if(RowY + RowHeight > Panel.y + Panel.h)
        break;
      if(Index)
      {
        SDL_SetRenderDrawColor(Renderer, 52, 49, 38, 180);
        SDL_RenderDrawLine(Renderer, Panel.x, RowY,
                           Panel.x + Panel.w, RowY);
      }
      const SDL_Rect CurrentValue = { Panel.x, RowY, ValueWidth, RowHeight };
      const SDL_Rect MetricLabel = { Panel.x + ValueWidth, RowY,
                                      LabelWidth, RowHeight };
      const SDL_Rect SelectedValue = { SelectedX, RowY,
                                        ValueWidth, RowHeight };
      CenterText(Renderer, CurrentValue, Rows[Index].Current.c_str(),
                 std::min(4, Scale), 181, 169, 143);
      CenterText(Renderer, MetricLabel, Rows[Index].Label.c_str(),
                 std::min(3, Scale), 222, 189, 91);
      const Uint8 SelectedR = Rows[Index].Advantage > 0 ? 154
                            : Rows[Index].Advantage < 0 ? 239 : 232;
      const Uint8 SelectedG = Rows[Index].Advantage > 0 ? 220
                            : Rows[Index].Advantage < 0 ? 109 : 226;
      const Uint8 SelectedB = Rows[Index].Advantage > 0 ? 119
                            : Rows[Index].Advantage < 0 ? 91 : 194;
      CenterText(Renderer, SelectedValue, Rows[Index].Selected.c_str(),
                 std::min(4, Scale), SelectedR, SelectedG, SelectedB);
    }
    return Height;
  }

  void PaintMobileItemCard(SDL_Renderer* Renderer, const SDL_Rect& Area,
                           const std::string& TitleText,
                           const std::string& DetailText,
                           const std::string& MetricsText,
                           const adaptiveui::ItemMetrics* Candidate,
                           const adaptiveui::ItemMetrics* Current)
  {
    const int Pad = Clamp(int(4 * State.Density), 7, 16);
    const int Gap = Clamp(int(2 * State.Density), 4, 10);
    const mobileitemcardtext Card = MobileItemCardText(
      DetailText, MetricsText, Current);
    Fill(Renderer, Area, 7, 9, 8, 248);
    Outline(Renderer, Area, 126, 102, 57);

    std::string DisplayTitle = TitleText;
    if(!DisplayTitle.empty())
      DisplayTitle[0] = char(std::toupper((unsigned char)DisplayTitle[0]));
    const int TextWidth = std::max(1, Area.w - Pad * 2);
    int TitleScale = 6;
    std::vector<std::string> TitleLines;
    for(; TitleScale > 1; --TitleScale)
    {
      TitleLines = WrapText(DisplayTitle,
        std::max(1, TextWidth / (TitleScale * 6)));
      if(TitleLines.size() <= 2)
        break;
    }
    if(TitleLines.empty())
      TitleLines = WrapText(DisplayTitle,
        std::max(1, TextWidth / (TitleScale * 6)));
    const int TitleHeight = std::max(TitleScale * 11 + Pad,
      int(TitleLines.size()) * TitleScale * 8 + Pad);
    SDL_Rect Title = { Area.x + 1, Area.y + 1,
                       std::max(1, Area.w - 2),
                       std::min(std::max(1, Area.h - 2), TitleHeight) };
    Fill(Renderer, Title, 28, 43, 29, 250);
    const SDL_Rect TitleInner = { Title.x + Pad, Title.y,
                                  std::max(1, Title.w - Pad * 2), Title.h };
    CenteredWrappedText(Renderer, TitleInner, DisplayTitle, TitleScale);

    int Y = Title.y + Title.h + Gap;
    int Available = std::max(0, Area.y + Area.h - Pad - Y);
    const int Scale = MobileItemCardScale(Card, TextWidth, Available);
    const int Advance = Scale * 8;

    if(!Card.Description.empty() && Available > 0)
    {
      const int DescriptionScale = std::min(5, Scale + 1);
      const int Height = WrappedLineCount(Card.Description, TextWidth,
                                          DescriptionScale)
                       * DescriptionScale * 8 + Pad;
      SDL_Rect Description = { Area.x + Pad, Y, TextWidth,
                               std::min(Available, Height) };
      Fill(Renderer, Description, 12, 16, 14, 245);
      const SDL_Rect DescriptionTextArea = {
        Description.x + DescriptionScale * 2,
        Description.y + DescriptionScale * 2,
        std::max(1, Description.w - DescriptionScale * 4),
        std::max(1, Description.h - DescriptionScale * 4) };
      PaintCardParagraph(Renderer, DescriptionTextArea, Card.Description,
                         DescriptionScale, 240, 230, 202);
      Y += Description.h + Gap;
      Available = std::max(0, Area.y + Area.h - Pad - Y);
    }

    if(!Card.Metrics.empty() && Available > 0)
    {
      if(Candidate && Current && Current->Present)
      {
        const int RowCount = int(MobileComparisonRows(
          *Candidate, *Current).size());
        const int ReservedComparisonHeight = std::min(Available,
          Scale * 31 + RowCount * Scale * 11);
        const int ComparisonY = std::max(Y,
          Area.y + Area.h - Pad - ReservedComparisonHeight);
        const SDL_Rect ComparisonArea = { Area.x + Pad, ComparisonY,
                                          TextWidth, ReservedComparisonHeight };
        const int PaintedComparisonHeight = PaintMobileComparison(
          Renderer, ComparisonArea, *Candidate, *Current, DisplayTitle, Scale);
        Y = ComparisonY + PaintedComparisonHeight + Gap;
        Available = std::max(0, Area.y + Area.h - Pad - Y);
      }
      else
      {
        const int Columns = Card.Metrics.size() == 1 ? 1 : 2;
        const int Rows = (int(Card.Metrics.size()) + Columns - 1) / Columns;
        const int CellHeight = Advance + Scale * 2;
        const int MetricsHeight = Rows * CellHeight;
        const int MetricsY = Card.Requirements.empty()
          ? std::max(Y, Area.y + Area.h - Pad - MetricsHeight) : Y;
        SDL_Rect MetricsArea = { Area.x + Pad, MetricsY, TextWidth,
                                 std::min(Available, MetricsHeight) };
        const int CellWidth = MetricsArea.w / Columns;
        Fill(Renderer, MetricsArea, 14, 18, 14, 242);
        SDL_SetRenderDrawColor(Renderer, 92, 83, 55, 210);
        SDL_RenderDrawLine(Renderer, MetricsArea.x, MetricsArea.y,
                           MetricsArea.x + MetricsArea.w, MetricsArea.y);
        SDL_RenderDrawLine(Renderer, MetricsArea.x,
                           MetricsArea.y + MetricsArea.h - 1,
                           MetricsArea.x + MetricsArea.w,
                           MetricsArea.y + MetricsArea.h - 1);
        for(int Row = 1; Row < Rows; ++Row)
          SDL_RenderDrawLine(Renderer, MetricsArea.x,
                             MetricsArea.y + Row * CellHeight,
                             MetricsArea.x + MetricsArea.w,
                             MetricsArea.y + Row * CellHeight);
        for(size_t Index = 0; Index < Card.Metrics.size(); ++Index)
        {
          const int Column = int(Index) % Columns;
          const int Row = int(Index) / Columns;
          const bool LoneLastMetric = Columns == 2 && Column == 0
                                   && Index + 1 == Card.Metrics.size();
          SDL_Rect Cell = { MetricsArea.x + Column * CellWidth,
                            MetricsArea.y + Row * CellHeight,
                            LoneLastMetric ? MetricsArea.w : CellWidth,
                            CellHeight };
          if(Column == 0 && !LoneLastMetric
             && Index + 1 < Card.Metrics.size())
          {
            SDL_SetRenderDrawColor(Renderer, 68, 65, 48, 180);
            SDL_RenderDrawLine(Renderer, Cell.x + Cell.w, Cell.y + Scale,
                               Cell.x + Cell.w,
                               Cell.y + Cell.h - Scale);
          }
          CenterText(Renderer, Cell, Card.Metrics[Index].c_str(),
                     std::min(5, Scale + 1), 235, 207, 116);
        }
        Y += MetricsArea.h + Gap;
        Available = std::max(0, Area.y + Area.h - Pad - Y);
      }
    }

    if(!Card.Requirements.empty() && Available > 0)
    {
      SDL_Rect Requirements = { Area.x + Pad, Y, TextWidth, Available };
      Fill(Renderer, Requirements, Card.Missing ? 25 : 14,
           Card.Missing ? 12 : 24, Card.Missing ? 11 : 15, 242);
      const SDL_Rect RequirementText = {
        Requirements.x + Scale * 2, Requirements.y + Scale * 2,
        std::max(1, Requirements.w - Scale * 4),
        std::max(1, Requirements.h - Scale * 4) };
      PaintCardRequirements(Renderer, RequirementText, Card, Scale);
    }
  }

  void PaintMobileConditions(SDL_Renderer* Renderer, const SDL_Rect& Area,
                             const adaptiveui::HudModel& Hud)
  {
    State.ConditionViewport = Area;
    State.ConditionMaxScrollY = 0;
    if(Area.w <= 0 || Area.h <= 0 || Hud.Conditions.empty())
      return;
    const int Gap = Clamp(int(2 * State.Density), 3, 8);
    const int ChipHeight = Clamp(int(13 * State.Density), 28, 48);
    const int ContentHeight = int(Hud.Conditions.size()) * (ChipHeight + Gap)
                            - Gap;
    State.ConditionMaxScrollY = std::max(0, ContentHeight - Area.h);
    State.ConditionScrollY = Clamp(State.ConditionScrollY, 0,
                                   State.ConditionMaxScrollY);
    int Y = Area.y - State.ConditionScrollY;
    SDL_RenderSetClipRect(Renderer, &Area);
    for(size_t Index = 0; Index < Hud.Conditions.size(); ++Index)
    {
      SDL_Rect Chip = { Area.x, Y, Area.w, ChipHeight };
      const adaptiveui::StatusIndicator& Condition = Hud.Conditions[Index];
      if(Chip.y + Chip.h > Area.y && Chip.y < Area.y + Area.h)
      {
        Fill(Renderer, Chip, Condition.Red / 5, Condition.Green / 5,
             Condition.Blue / 5, 245);
        Outline(Renderer, Chip, Condition.Red, Condition.Green,
                Condition.Blue);
        CenterText(Renderer, Chip, Condition.Label.c_str(), 3,
                   240, 230, 202);
      }
      Y += ChipHeight + Gap;
    }
    SDL_RenderSetClipRect(Renderer, 0);
  }

  struct pickupactionbutton
  {
    int Code;
    const char* Label;
  };

  std::vector<pickupactionbutton> PickupItemActions(
    const adaptiveui::HudModel& Hud)
  {
    std::vector<pickupactionbutton> Result;
    if(Hud.MenuKind != adaptiveui::MENU_PICKUP_GRID
       || Hud.MenuSelected < 0
       || Hud.MenuSelected >= int(Hud.MenuItemMetrics.size()))
      return Result;

    const adaptiveui::ItemMetrics& Metrics =
      Hud.MenuItemMetrics[Hud.MenuSelected];
    if(!Metrics.Present)
      return Result;
    if(Metrics.Equippable)
      Result.push_back({ adaptiveui::ITEM_ACTION_NONE, "EQUIP" });

    const pickupactionbutton Available[] = {
      { adaptiveui::ITEM_ACTION_DRINK, "DRINK" },
      { adaptiveui::ITEM_ACTION_TASTE, "TASTE" },
      { adaptiveui::ITEM_ACTION_EAT, "EAT" },
      { adaptiveui::ITEM_ACTION_READ, "READ" },
      { adaptiveui::ITEM_ACTION_ZAP, "ZAP" },
      { adaptiveui::ITEM_ACTION_APPLY, "APPLY" }
    };
    for(size_t Index = 0;
        Index < sizeof(Available) / sizeof(Available[0])
        && Result.size() < MAX_PICKUP_ACTIONS; ++Index)
      if(Metrics.Actions & adaptiveui::ItemActionMask(
           adaptiveui::ItemAction(Available[Index].Code)))
        Result.push_back(Available[Index]);
    return Result;
  }

  void PaintAdaptiveGridMenu(SDL_Renderer* Renderer, SDL_Texture* GameTexture,
                             const adaptiveui::HudModel& Hud)
  {
    const int Count = std::min(int(Hud.MenuOptions.size()),
                               int(MAX_MENU_OPTIONS));
    const bool ShowDoll = Hud.PaperDollScreen
                       && Hud.PaperDollSource.w > 0
                       && Hud.PaperDollSource.h > 0;
    const bool ShowDetail = Count > 0;
    const bool TextButtons = Hud.MenuKind == adaptiveui::MENU_BUTTON_ROWS;
    const bool ConfirmGrid = Hud.MenuKind == adaptiveui::MENU_ITEM_GRID
                           || Hud.MenuKind == adaptiveui::MENU_PICKUP_GRID;
    const bool PickupGrid = Hud.MenuKind == adaptiveui::MENU_PICKUP_GRID;
    const std::vector<pickupactionbutton> ItemActions = PickupGrid
      ? PickupItemActions(Hud) : std::vector<pickupactionbutton>();

    std::vector<int> DisplayOrder;
    if(int(Hud.MenuDisplayOrder.size()) == Count)
      DisplayOrder = Hud.MenuDisplayOrder;
    else
      for(int Index = 0; Index < Count; ++Index)
        DisplayOrder.push_back(Index);

    int SelectedPosition = 0;
    for(int Position = 0; Position < Count; ++Position)
      if(DisplayOrder[Position] == Hud.MenuSelected)
      {
        SelectedPosition = Position;
        break;
      }

    // Adaptive icon menus occupy the complete five-column controller footprint.
    // Item-choice grids reserve the final two equal cells for Select and Back;
    // category grids reserve only the final Back cell.
    enum { MENU_COLUMNS = 5, MENU_ROWS = 3, MENU_SLOTS = 15 };
    const int EntriesPerPage = MENU_SLOTS
      - (PickupGrid ? 5 : (ConfirmGrid ? 2 : 1));
    const SDL_Rect MenuGridArea = State.Width < State.Height
      ? SDL_Rect{ State.Safe.x, State.Controls.y,
                  State.Safe.w, State.Controls.h }
      : SDL_Rect{ State.Toggle.x, State.Controls.y,
                  State.Toggle.w, State.Controls.h };
    SDL_Rect EntriesArea = MenuGridArea;
    // Landscape row menus are a side rail, not a controller-shaped grid.
    // Keep icon menus in their deliberate 5x3 button box, but let lists use
    // every safe pixel below the rail heading. Their viewport also drives
    // touch hit-testing and kinetic scrolling, so visual and input bounds stay
    // together.
    if(TextButtons && State.Width >= State.Height)
      EntriesArea.h = std::max(1,
        State.Safe.y + State.Safe.h - EntriesArea.y);
    int PageStart = 0;
    int VisibleCount = Count;
    int RowHeight = 0;
    if(TextButtons)
    {
      State.MenuViewport = EntriesArea;
      const int ControllerRowHeight = std::max(1, State.Controls.h / 5);
      const int TotalRows = std::max(1, Count + 1); // Include Back.
      const int ExpandedRowHeight = EntriesArea.h / TotalRows;
      const int MaximumRowHeight = std::max(ControllerRowHeight,
        Clamp(int(36 * State.Density), 70, 120));
      RowHeight = Clamp(ExpandedRowHeight, ControllerRowHeight,
                        MaximumRowHeight);
      State.MenuScrollStep = RowHeight;
      State.MenuMaxScrollY = std::max(0,
        (Count + 1) * RowHeight - EntriesArea.h);
      State.MenuScrollY = Clamp(State.MenuScrollY, 0, State.MenuMaxScrollY);
      if(Hud.MenuSelected != State.MenuGridSelection && Hud.MenuSelected >= 0)
      {
        const int SelectedTop = SelectedPosition * RowHeight;
        const int SelectedBottom = SelectedTop + RowHeight;
        if(SelectedTop < State.MenuScrollY)
          State.MenuScrollY = SelectedTop;
        else if(SelectedBottom > State.MenuScrollY + EntriesArea.h)
          State.MenuScrollY = SelectedBottom - EntriesArea.h;
        State.MenuScrollY = Clamp(State.MenuScrollY, 0, State.MenuMaxScrollY);
      }
    }
    else
    {
      State.MenuScrollStep = 0;
      const int PageCount = std::max(1,
        (Count + EntriesPerPage - 1) / EntriesPerPage);
      const int PageStep = std::max(1, MenuGridArea.h / MENU_ROWS);
      State.MenuViewport = MenuGridArea;
      State.MenuMaxScrollY = (PageCount - 1) * PageStep;
      State.MenuScrollY = Clamp(State.MenuScrollY, 0, State.MenuMaxScrollY);
      int CurrentPage = Clamp((State.MenuScrollY + PageStep / 2) / PageStep,
                              0, PageCount - 1);
      const int SelectedPage = SelectedPosition / EntriesPerPage;
      if(Count > EntriesPerPage
         && Hud.MenuSelected != State.MenuGridSelection
         && Hud.MenuSelected >= 0)
      {
        CurrentPage = SelectedPage;
        State.MenuScrollY = CurrentPage * PageStep;
      }
      PageStart = CurrentPage * EntriesPerPage;
      VisibleCount = std::max(0,
        std::min(EntriesPerPage, Count - PageStart));
    }
    State.MenuGridSelection = Hud.MenuSelected;
    for(int Index = 0; Index < MAX_MENU_OPTIONS; ++Index)
      State.MenuRows[Index] = { 0, 0, 0, 0 };
    State.ConditionViewport = { 0, 0, 0, 0 };
    State.ConditionMaxScrollY = 0;

    const int SummaryInset = Clamp(int(5 * State.Density), 6, 20);
    SDL_Rect Summary = { State.Game.x + SummaryInset,
                         State.Game.y + SummaryInset,
                         std::max(1, State.Game.w - SummaryInset * 2),
                         std::max(1, State.Game.h - SummaryInset * 2) };
    SDL_Rect DetailArea = Summary;
    if(TextButtons)
    {
      Fill(Renderer, State.Game, 0, 0, 0, 255);
      RenderTextureFit(Renderer, GameplaySnapshot, State.MapSource, State.Game);
      DetailArea = { 0, 0, 0, 0 };
    }
    if(ShowDoll)
    {
      const int Gap = Clamp(int(4 * State.Density), 6, 18);
      SDL_Rect DollArea;
      if(Summary.w > Summary.h * 6 / 5)
      {
        const int DollWidth = Summary.w * 42 / 100;
        DollArea = { Summary.x, Summary.y, DollWidth, Summary.h };
        DetailArea = { DollArea.x + DollArea.w + Gap, Summary.y,
                       std::max(1, Summary.w - DollArea.w - Gap), Summary.h };
      }
      else
      {
        const int DollHeight = Summary.h * 48 / 100;
        DollArea = { Summary.x, Summary.y, Summary.w, DollHeight };
        DetailArea = { Summary.x, DollArea.y + DollArea.h + Gap, Summary.w,
                       std::max(1, Summary.h - DollArea.h - Gap) };
      }
      Fill(Renderer, DollArea, 5, 7, 7, 245);
      Outline(Renderer, DollArea, 126, 102, 57);
      SDL_Rect Conditions = { DollArea.x, DollArea.y,
                              std::max(0, DollArea.w * 30 / 100), DollArea.h };
      SDL_Rect DollImage = DollArea;
      DollImage.x += Conditions.w + 4;
      DollImage.w = std::max(1, DollImage.w - Conditions.w - 4);
      RenderTextureFit(Renderer, GameTexture, Hud.PaperDollSource, DollImage);
      PaintMobileConditions(Renderer, Conditions, Hud);
    }

    SDL_RenderSetClipRect(Renderer, &State.MenuViewport);
    for(int PagePosition = 0; PagePosition < VisibleCount; ++PagePosition)
    {
      const int Position = PageStart + PagePosition;
      const int Index = DisplayOrder[Position];
      if(Index < 0 || Index >= Count)
        continue;
      const SDL_Rect Cell = TextButtons
        ? SDL_Rect{ EntriesArea.x + 3,
                    EntriesArea.y + Position * RowHeight
                      - State.MenuScrollY + 3,
                    std::max(1, EntriesArea.w - 6),
                    std::max(1, RowHeight - 6) }
        : GridCell(MenuGridArea, MENU_COLUMNS, MENU_ROWS, PagePosition, 3);
      State.MenuRows[Index] = Cell;
      const bool Selected = Index == Hud.MenuSelected;
      const bool Available = Index >= int(Hud.MenuAvailability.size())
                          || Hud.MenuAvailability[Index] != 0;
      Fill(Renderer, Cell, Selected ? 35 : 14, Selected ? 71 : 18,
           Selected ? 48 : 24, 250);
      Outline(Renderer, Cell, Selected ? 145 : (Available ? 91 : 55),
              Selected ? 190 : (Available ? 78 : 55),
              Selected ? 115 : (Available ? 59 : 55));

      if(TextButtons)
        MenuRowText(Renderer, Cell, Hud.MenuOptions[Index], 8, 4,
                    Available ? 240 : 130, Available ? 230 : 126,
                    Available ? 202 : 116);
      else
      {
        const int LabelHeight = std::max(28, Cell.h * 30 / 100);
        SDL_Rect IconArea = { Cell.x + 5, Cell.y + 5,
          std::max(1, Cell.w - 10), std::max(1, Cell.h - LabelHeight - 8) };
        if(Index < int(Hud.MenuIconSources.size())
           && Hud.MenuIconSources[Index].w > 0
           && Hud.MenuIconSources[Index].h > 0)
        {
          if(!Available)
            SDL_SetTextureColorMod(GameTexture, 100, 100, 96);
          RenderTextureFit(Renderer, GameTexture, Hud.MenuIconSources[Index],
                           IconArea);
          if(!Available)
            SDL_SetTextureColorMod(GameTexture, 255, 255, 255);
        }
        SDL_Rect Label = { Cell.x + 4, Cell.y + Cell.h - LabelHeight,
                           std::max(1, Cell.w - 8), LabelHeight };
        MenuRowText(Renderer, Label, Hud.MenuOptions[Index], 4,
                    Clamp(LabelHeight / 18, 2, 3),
                    Available ? 240 : 130, Available ? 230 : 126,
                    Available ? 202 : 116);
      }
    }
    SDL_RenderSetClipRect(Renderer, 0);

    if(ShowDetail && DetailArea.w > 0 && DetailArea.h > 0 && Count > 0)
    {
      const int Index = Clamp(Hud.MenuSelected, 0, Count - 1);
      const std::string ItemTitle = MobileItemTitle(Hud.MenuOptions[Index]);
      const adaptiveui::ItemMetrics* Candidate =
        Index < int(Hud.MenuItemMetrics.size())
          && Hud.MenuItemMetrics[Index].Present
        ? &Hud.MenuItemMetrics[Index] : 0;
      const adaptiveui::ItemMetrics* Current = 0;
      if(Hud.EquipmentComparisonActive && Hud.EquippedItemMetrics.Present)
        Current = &Hud.EquippedItemMetrics;
      else if(Index < int(Hud.MenuComparisonMetrics.size())
              && Hud.MenuComparisonMetrics[Index].Present)
        Current = &Hud.MenuComparisonMetrics[Index];
      const std::string DescriptionText =
        Index < int(Hud.MenuDetails.size()) ? Hud.MenuDetails[Index] : "";
      const std::string MetricsText = Candidate
        ? MobileItemMetrics(*Candidate) : "";
      PaintMobileItemCard(Renderer, DetailArea, ItemTitle,
                          DescriptionText, MetricsText, Candidate, Current);
    }

    State.MenuConfirm = { 0, 0, 0, 0 };
    State.MenuEquip = { 0, 0, 0, 0 };
    State.MenuItemActionCount = 0;
    for(int Index = 0; Index < MAX_PICKUP_ACTIONS; ++Index)
    {
      State.MenuItemActions[Index] = { 0, 0, 0, 0 };
      State.MenuItemActionCodes[Index] = adaptiveui::ITEM_ACTION_NONE;
    }
    if(TextButtons)
      State.MenuBack = { MenuGridArea.x + 3,
                         EntriesArea.y + Count * RowHeight
                           - State.MenuScrollY + 3,
                         std::max(1, MenuGridArea.w - 6),
                         std::max(1, RowHeight - 6) };
    else
    {
      if(ConfirmGrid)
      {
        State.MenuConfirm = GridCell(MenuGridArea, MENU_COLUMNS, MENU_ROWS,
                                     MENU_SLOTS - 2, 3);
        if(PickupGrid && !ItemActions.empty())
        {
          State.MenuItemActionCount = std::min(
            int(ItemActions.size()), int(MAX_PICKUP_ACTIONS));
          const int ActionStart = MENU_SLOTS - 2
                                - State.MenuItemActionCount;
          for(int Index = 0; Index < State.MenuItemActionCount; ++Index)
          {
            State.MenuItemActions[Index] = GridCell(
              MenuGridArea, MENU_COLUMNS, MENU_ROWS,
              ActionStart + Index, 3);
            State.MenuItemActionCodes[Index] = ItemActions[Index].Code;
            if(ItemActions[Index].Code == adaptiveui::ITEM_ACTION_NONE)
              State.MenuEquip = State.MenuItemActions[Index];
          }
        }
        State.MenuBack = GridCell(MenuGridArea, MENU_COLUMNS, MENU_ROWS,
                                  MENU_SLOTS - 1, 3);
      }
      else
        State.MenuBack = GridCell(MenuGridArea, MENU_COLUMNS, MENU_ROWS,
                                  MENU_SLOTS - 1, 3);
    }
    if(ConfirmGrid)
    {
      const bool CanConfirm = Hud.MenuSelected >= 0
                           && Hud.MenuSelected < Count;
      Fill(Renderer, State.MenuConfirm, CanConfirm ? 35 : 19,
           CanConfirm ? 71 : 23, CanConfirm ? 48 : 28, 245);
      Outline(Renderer, State.MenuConfirm, CanConfirm ? 105 : 70,
              CanConfirm ? 170 : 70, CanConfirm ? 92 : 70);
      CenterText(Renderer, State.MenuConfirm,
                 PickupGrid ? "STASH" : "SELECT", 4,
                 CanConfirm ? 235 : 130, CanConfirm ? 230 : 125,
                 CanConfirm ? 202 : 115);
      if(PickupGrid)
      {
        for(int Index = 0; Index < State.MenuItemActionCount; ++Index)
        {
          const SDL_Rect& ActionRect = State.MenuItemActions[Index];
          Fill(Renderer, ActionRect, CanConfirm ? 35 : 19,
               CanConfirm ? 58 : 23, CanConfirm ? 71 : 28, 245);
          Outline(Renderer, ActionRect, CanConfirm ? 105 : 70,
                  CanConfirm ? 145 : 70, CanConfirm ? 190 : 70);
          CenterText(Renderer, ActionRect, ItemActions[Index].Label, 4,
                     CanConfirm ? 235 : 130, CanConfirm ? 230 : 125,
                     CanConfirm ? 202 : 115);
        }
      }
    }
    if(!TextButtons || (State.MenuBack.y + State.MenuBack.h
                        > State.MenuViewport.y
                        && State.MenuBack.y < State.MenuViewport.y
                                             + State.MenuViewport.h))
    {
      if(TextButtons)
        SDL_RenderSetClipRect(Renderer, &State.MenuViewport);
      Fill(Renderer, State.MenuBack, 54, 22, 20, 245);
      Outline(Renderer, State.MenuBack, 190, 76, 61);
      CenterText(Renderer, State.MenuBack, "BACK", 4, 240, 210, 190);
      if(TextButtons)
        SDL_RenderSetClipRect(Renderer, 0);
    }
  }

  bool MobileCraftingGuidePage(const adaptiveui::HudModel& Hud)
  {
    return Hud.MenuKind == adaptiveui::MENU_GUIDE
        && State.MenuTitle.find("Crafting guide - ") == 0;
  }

  void SplitGuideSection(const std::string& Source, std::string& Heading,
                         std::string& Body)
  {
    const size_t Divider = Source.find("::");
    if(Divider == std::string::npos)
    {
      Heading.clear();
      Body = TrimCardText(Source);
      return;
    }
    Heading = TrimCardText(Source.substr(0, Divider));
    Body = TrimCardText(Source.substr(Divider + 2));
  }

  int MobileGuideContentHeight(int Width, int BodyScale, int Gap)
  {
    const int HeadingScale = std::min(5, BodyScale + 1);
    int Height = 0;
    for(int Index = 0; Index < State.MenuOptionCount; ++Index)
    {
      std::string Heading, Body;
      SplitGuideSection(State.MenuOptions[Index], Heading, Body);
      if(!Heading.empty())
        Height += HeadingScale * 8 + BodyScale * 2;
      Height += WrappedLineCount(Body, Width, BodyScale) * BodyScale * 8;
      if(Index + 1 < State.MenuOptionCount)
        Height += Gap;
    }
    return Height;
  }

  void PaintMobileCraftingGuidePage(SDL_Renderer* Renderer)
  {
    const int OuterPad = Clamp(int(7 * State.Density), 10, 24);
    const int InnerPad = Clamp(int(5 * State.Density), 8, 18);
    const int SectionGap = Clamp(int(7 * State.Density), 14, 28);
    const int FooterHeight = Clamp(int(18 * State.Density), 38, 62);
    SDL_Rect Page = { State.Game.x + OuterPad, State.Game.y + OuterPad,
                      State.Game.w - OuterPad * 2,
                      State.Game.h - OuterPad * 2 };
    Fill(Renderer, Page, 7, 10, 8, 248);
    Outline(Renderer, Page, 126, 102, 57);

    SDL_Rect Footer = { Page.x + 1, Page.y + Page.h - FooterHeight,
                        Page.w - 2, FooterHeight - 1 };
    SDL_Rect Content = { Page.x + InnerPad, Page.y + InnerPad,
                         Page.w - InnerPad * 2,
                         Page.h - FooterHeight - InnerPad * 2 };
    State.MenuViewport = Content;
    State.MenuMaxScrollY = 0;
    State.MenuScrollY = 0;
    for(int Index = 0; Index < MAX_MENU_OPTIONS; ++Index)
      State.MenuRows[Index] = { 0, 0, 0, 0 };

    const int TextIndent = Clamp(int(4 * State.Density), 7, 14);
    const int TextWidth = std::max(1, Content.w - TextIndent - InnerPad);
    int BodyScale = 5;
    while(BodyScale > 2
          && MobileGuideContentHeight(TextWidth, BodyScale, SectionGap)
             > Content.h)
      --BodyScale;
    const int HeadingScale = std::min(5, BodyScale + 1);
    int Y = Content.y;
    for(int Index = 0; Index < State.MenuOptionCount; ++Index)
    {
      std::string Heading, Body;
      SplitGuideSection(State.MenuOptions[Index], Heading, Body);
      const int HeadingHeight = Heading.empty()
        ? 0 : HeadingScale * 8 + BodyScale * 2;
      const int BodyHeight = WrappedLineCount(Body, TextWidth, BodyScale)
                           * BodyScale * 8;
      const int SectionHeight = HeadingHeight + BodyHeight;

      SDL_Rect Accent = { Content.x, Y, std::max(2, BodyScale),
                          std::max(1, SectionHeight) };
      Fill(Renderer, Accent, 126, 102, 57, 235);
      if(!Heading.empty())
      {
        SDL_Rect HeadingArea = { Content.x + TextIndent, Y, TextWidth,
                                 HeadingHeight };
        LeftTextAtScale(Renderer, HeadingArea, Heading.c_str(), HeadingScale,
                        222, 189, 91);
        Y += HeadingHeight;
      }
      SDL_Rect BodyArea = { Content.x + TextIndent, Y, TextWidth, BodyHeight };
      PaintCardParagraph(Renderer, BodyArea, Body, BodyScale,
                         240, 230, 202);
      Y += BodyHeight;
      if(Index + 1 < State.MenuOptionCount)
      {
        const int DividerY = Y + SectionGap / 2;
        SDL_SetRenderDrawColor(Renderer, 68, 61, 43, 190);
        SDL_RenderDrawLine(Renderer, Content.x + TextIndent, DividerY,
                           Content.x + Content.w, DividerY);
        Y += SectionGap;
      }
    }

    SDL_SetRenderDrawColor(Renderer, 126, 102, 57, 220);
    SDL_RenderDrawLine(Renderer, Footer.x, Footer.y,
                       Footer.x + Footer.w, Footer.y);
    char PageLabel[32];
    snprintf(PageLabel, sizeof(PageLabel), "PAGE %d OF %d",
             State.MenuPage, State.MenuPages);
    CenterText(Renderer, Footer, PageLabel, 4, 222, 189, 91);
  }

  void PaintMobileMenu(SDL_Renderer* Renderer, SDL_Texture* GameTexture)
  {
    const adaptiveui::HudModel& Hud = adaptiveui::GetHudModel();
    if(MobileCraftingGuidePage(Hud))
    {
      PaintMobileCraftingGuidePage(Renderer);
      return;
    }
    if(Hud.MenuKind == adaptiveui::MENU_CATEGORY_GRID
       || Hud.MenuKind == adaptiveui::MENU_ITEM_GRID
       || Hud.MenuKind == adaptiveui::MENU_PICKUP_GRID
       || Hud.MenuKind == adaptiveui::MENU_BUTTON_ROWS)
    {
      PaintAdaptiveGridMenu(Renderer, GameTexture, Hud);
      return;
    }
    const bool MainMenu = MainMenuPresentation();
    const bool ScrollableEquipment = State.MenuTitle == "Equipment"
                                  && State.MenuPages == 1;
    const bool MessageHistory = State.MenuTitle == "Message history";
    const bool SaveGameChooser =
      State.MenuTitle == "Choose a file and be sorry:";
    const bool RoomyWrappedRows = MessageHistory || ScrollableEquipment
                               || SaveGameChooser;
    const int Padding = Clamp(int(8 * State.Density), 10, 28);
    int SubtitleHeight = MainMenu
      ? Clamp(int(24 * State.Density), 48, 86)
      : (State.MenuSubtitle.empty() ? 0
      : Clamp(int(18 * State.Density), 36, 64));
    if(!MainMenu && State.MenuSubtitle.size() > 80)
      SubtitleHeight = Clamp(int(42 * State.Density), 96, 160);
    const int FooterHeight = MainMenu || ScrollableEquipment ? 0
      : Clamp(int(16 * State.Density), 32, 54);
    SDL_Rect Content = { State.Game.x + Padding, State.Game.y + Padding,
                         State.Game.w - Padding * 2,
                         State.Game.h - Padding * 2 };

    if(SubtitleHeight)
    {
      SDL_Rect Subtitle = { Content.x, Content.y, Content.w, SubtitleHeight };
      if(MainMenu)
        CenterText(Renderer, Subtitle, "ITER VEHEMENS AD NECEM",
                   5, 205, 48, 42);
      else if(ScrollableEquipment)
        CenteredWrappedText(Renderer, Subtitle, State.MenuSubtitle, 4);
      else
        WrappedText(Renderer, Subtitle, State.MenuSubtitle, true, 140, 4);
      Content.y += SubtitleHeight;
      Content.h -= SubtitleHeight;
    }

    SDL_Rect Footer = { Content.x, Content.y + Content.h - FooterHeight,
                        Content.w, FooterHeight };
    Content.h -= FooterHeight;
    if(MainMenu)
    {
      const int Narrowing = Content.w / 10;
      Content.x += Narrowing;
      Content.w -= Narrowing * 2;
    }
    const int Count = std::max(1, State.MenuOptionCount);
    const bool GroupedOptions = State.MenuTitle == "OPTIONS"
      && int(Hud.MenuGroups.size()) >= State.MenuOptionCount;
    int GroupHeaderCount = 0;
    if(GroupedOptions)
      for(int Index = 0; Index < State.MenuOptionCount; ++Index)
        if(Index == 0 || Hud.MenuGroups[Index] != Hud.MenuGroups[Index - 1])
          ++GroupHeaderCount;
    const int GroupHeaderHeight = GroupedOptions
      ? Clamp(int(12 * State.Density), 28, 46) : 0;
    const int GroupHeaderReserve = GroupHeaderCount * GroupHeaderHeight;
    const int MaximumRowHeight = MainMenu
      ? Clamp(int(31 * State.Density), 68, 112)
      : (RoomyWrappedRows
         ? Clamp(int(57 * State.Density), 120, 180)
         : Clamp(int(36 * State.Density), 70, 130));
    const int RowsHeight = ScrollableEquipment
      ? Count * MaximumRowHeight
      : std::min(std::max(1, Content.h - GroupHeaderReserve),
                 Count * MaximumRowHeight);
    const int ShortestRowHeight = ScrollableEquipment
      ? MaximumRowHeight - 4 : std::max(1, RowsHeight / Count - 4);
    int MenuScale = Clamp(ShortestRowHeight / 11, 1, MainMenu ? 5 : 4);
    if(MainMenu)
      for(int Index = 0; Index < State.MenuOptionCount; ++Index)
      {
        SDL_Rect FitRect = { 0, 0, Content.w - Padding * 2,
                             ShortestRowHeight };
        MenuScale = std::min(MenuScale,
          FittingTextScale(FitRect, State.MenuOptions[Index], 5));
      }
    if(MainMenu)
      Content.y += std::max(0, (Content.h - RowsHeight) / 2);
    State.MenuViewport = Content;
    State.MenuMaxScrollY = ScrollableEquipment
      ? std::max(0, RowsHeight - Content.h) : 0;
    State.MenuScrollY = Clamp(State.MenuScrollY, 0, State.MenuMaxScrollY);
    if(ScrollableEquipment && State.MenuSelected >= 0
       && State.MenuSelected < Count)
    {
      // Directional keys still select entries normally. Only move the
      // viewport when the newly selected row crosses an edge, keeping the
      // highlight visible without turning Up/Down into standalone scrolling.
      const int SelectedTop = Content.y
        + RowsHeight * State.MenuSelected / Count;
      const int SelectedBottom = Content.y
        + RowsHeight * (State.MenuSelected + 1) / Count;
      if(SelectedTop - State.MenuScrollY < Content.y)
        State.MenuScrollY = SelectedTop - Content.y;
      else if(SelectedBottom - State.MenuScrollY > Content.y + Content.h)
        State.MenuScrollY = SelectedBottom - Content.y - Content.h;
      State.MenuScrollY = Clamp(State.MenuScrollY, 0, State.MenuMaxScrollY);
    }
    if(ScrollableEquipment)
      SDL_RenderSetClipRect(Renderer, &Content);
    int HeadersBefore = 0;
    for(int Index = 0; Index < State.MenuOptionCount; ++Index)
    {
      const int ScrollY = ScrollableEquipment ? State.MenuScrollY : 0;
      const bool NewGroup = GroupedOptions
        && (Index == 0 || Hud.MenuGroups[Index] != Hud.MenuGroups[Index - 1]);
      int Y0 = Content.y + RowsHeight * Index / Count
             + HeadersBefore * GroupHeaderHeight - ScrollY;
      if(NewGroup)
      {
        SDL_Rect Header = { Content.x, Y0, Content.w, GroupHeaderHeight };
        Fill(Renderer, Header, 12, 16, 13, 248);
        SDL_SetRenderDrawColor(Renderer, 126, 102, 57, 220);
        SDL_RenderDrawLine(Renderer, Header.x,
                           Header.y + Header.h - 1,
                           Header.x + Header.w,
                           Header.y + Header.h - 1);
        LeftTextAtScale(Renderer,
          { Header.x + Padding / 2, Header.y,
            std::max(1, Header.w - Padding), Header.h },
          Hud.MenuGroups[Index].c_str(), 3, 222, 189, 91);
        ++HeadersBefore;
        Y0 += GroupHeaderHeight;
      }
      const int Y1 = Content.y + RowsHeight * (Index + 1) / Count - ScrollY;
      const int AdjustedY1 = Y1 + HeadersBefore * GroupHeaderHeight;
      // Verbose mobile rows often wrap to multiple lines. Leave a strong,
      // density-aware section break so adjacent bordered entries do not read
      // as one continuous paragraph (24 px on the Pixel's 3x density).
      const int RowGap = RoomyWrappedRows
        ? Clamp(int(8 * State.Density), 20, 32) : 4;
      SDL_Rect Row = { Content.x, Y0 + RowGap / 2, Content.w,
                       std::max(1, AdjustedY1 - Y0 - RowGap) };
      State.MenuRows[Index] = Row;
      const bool Selected = Index == State.MenuSelected;
      if(MainMenu)
      {
        Fill(Renderer, Row, Selected ? 65 : 4,
             Selected ? 20 : 5, Selected ? 18 : 7,
             Selected ? 232 : 218);
        Outline(Renderer, Row, Selected ? 202 : 112,
                Selected ? 74 : 92, Selected ? 57 : 62);
      }
      else
      {
        Fill(Renderer, Row, Selected ? 35 : (Index & 1 ? 18 : 14),
             Selected ? 71 : (Index & 1 ? 23 : 18),
             Selected ? 48 : (Index & 1 ? 28 : 24), 245);
        Outline(Renderer, Row, Selected ? 178 : 91,
                Selected ? 151 : 78, Selected ? 96 : 59);
      }

      const std::string& Label = State.MenuOptions[Index];
      if(MainMenu)
      {
        std::string Visible = Label;
        const int Columns = std::max(1,
          (Row.w - Padding * 2) / (MenuScale * 6));
        if((int)Visible.size() > Columns)
        {
          Visible.resize(std::max(1, Columns - 3));
          Visible += "...";
        }
        CenterTextAtScale(Renderer, Row, Visible.c_str(), MenuScale,
                          Selected ? 255 : 240, Selected ? 224 : 230,
                          Selected ? 205 : 202);
      }
      else
        MenuRowText(Renderer, Row, Label, Padding, MenuScale, 240, 230, 202);
    }
    if(ScrollableEquipment)
      SDL_RenderSetClipRect(Renderer, 0);
    for(int Index = State.MenuOptionCount; Index < MAX_MENU_OPTIONS; ++Index)
      State.MenuRows[Index] = { 0, 0, 0, 0 };

    if(!MainMenu && !ScrollableEquipment)
    {
      char PageLabel[32];
      snprintf(PageLabel, sizeof(PageLabel), "PAGE %d/%d",
               State.MenuPage, State.MenuPages);
      CenterText(Renderer, Footer, PageLabel, 4, 190, 180, 155);
    }
  }

  int GridIndexAt(const SDL_Rect& Grid, int Columns, int Rows, int X, int Y)
  {
    if(!Contains(Grid, X, Y))
      return -1;
    const int Column = Clamp((X - Grid.x) * Columns / Grid.w, 0, Columns - 1);
    const int Row = Clamp((Y - Grid.y) * Rows / Grid.h, 0, Rows - 1);
    return Row * Columns + Column;
  }

  void KeyLabel(int Key, char* Buffer, size_t BufferSize)
  {
    if(State.MapScreen)
    {
      if(Key == KEY_BACK_SPACE)
        snprintf(Buffer, BufferSize, "DELETE");
      else if(Key == KEY_ENTER || Key == KEY_CONTROLLER_A)
        snprintf(Buffer, BufferSize, "SAVE");
      else if(Key == 't')
        snprintf(Buffer, BufferSize, "NOTES");
      else if(Key == 'l')
        snprintf(Buffer, BufferSize, "CURSOR");
      else if(Key == 'a')
        snprintf(Buffer, BufferSize, "CREATE");
      else if(Key == 'r')
        snprintf(Buffer, BufferSize, "ROTATE");
      else if(Key == 'd')
        snprintf(Buffer, BufferSize, "DELETE");
      else if(Key == 'e')
        snprintf(Buffer, BufferSize, "EDIT");
      else if(Key == '?')
        snprintf(Buffer, BufferSize, "HELP");
      else if(Key == KEY_ESC || Key == KEY_CONTROLLER_B)
        snprintf(Buffer, BufferSize, "BACK");
      else
        snprintf(Buffer, BufferSize, "OPTION");
      return;
    }
    if(State.PromptText.find("engrave a square") != std::string::npos)
    {
      if(Key == '.')
        snprintf(Buffer, BufferSize, "SQUARE");
      else if(Key == 'i')
        snprintf(Buffer, BufferSize, "ITEM");
      else if(Key == KEY_ESC || Key == KEY_CONTROLLER_B)
        snprintf(Buffer, BufferSize, "BACK");
      else
        snprintf(Buffer, BufferSize, "OPTION");
      return;
    }
    if(Key == KEY_ESC || Key == KEY_CONTROLLER_B)
      snprintf(Buffer, BufferSize, "BACK");
    else if(Key == KEY_ENTER || Key == KEY_CONTROLLER_A)
      snprintf(Buffer, BufferSize, "SELECT");
    else if(Key == KEY_BACK_SPACE)
      snprintf(Buffer, BufferSize, "DELETE");
    else if(Key >= 'a' && Key <= 'z')
      snprintf(Buffer, BufferSize, "LOW %c", char(Key));
    else if(Key >= 'A' && Key <= 'Z')
      snprintf(Buffer, BufferSize, "CAP %c", char(Key));
    else if(Key >= 0x20 && Key < 0x7F)
      snprintf(Buffer, BufferSize, "%c", char(std::toupper(Key)));
    else
      snprintf(Buffer, BufferSize, "OPTION");
  }

  std::string CurrentHeaderTitle()
  {
    const bool SettingTextPrompt = State.PromptActive
                                && !State.PromptNumeric
                                && IsSettingTextPrompt(State.PromptText);
    return State.MenuActive ? State.MenuTitle
      : (State.ScreenTextActive ? State.ScreenTextTitle
      : (State.PromptActive && !State.PromptGameplay
         ? (State.PromptNumeric ? "SELECT QUANTITY"
            : (SettingTextPrompt ? SettingPromptTitle(State.PromptText)
                                 : "CREATE CHARACTER"))
         : "IVAN"));
  }

  SDL_Rect SegmentedHeaderTextRect(const SDL_Rect& Header,
                                   const std::string& Title)
  {
    if(State.DisplayCutoutCount <= 0)
      return Header;

    std::vector<SDL_Rect> Segments(1, Header);
    int LowestCutout = Header.y;
    for(int Index = 0; Index < State.DisplayCutoutCount; ++Index)
    {
      const SDL_Rect& Cutout = State.DisplayCutouts[Index];
      if(Cutout.y >= Header.y + Header.h
         || Cutout.y + Cutout.h <= Header.y)
        continue;
      LowestCutout = std::max(LowestCutout, Cutout.y + Cutout.h);
      std::vector<SDL_Rect> Remaining;
      for(const SDL_Rect& Segment : Segments)
      {
        const int CutoutLeft = std::max(Segment.x, Cutout.x);
        const int CutoutRight = std::min(Segment.x + Segment.w,
                                         Cutout.x + Cutout.w);
        if(CutoutLeft >= CutoutRight)
        {
          Remaining.push_back(Segment);
          continue;
        }
        if(CutoutLeft > Segment.x)
          Remaining.push_back({ Segment.x, Segment.y,
                                CutoutLeft - Segment.x, Segment.h });
        if(CutoutRight < Segment.x + Segment.w)
          Remaining.push_back({ CutoutRight, Segment.y,
                                Segment.x + Segment.w - CutoutRight,
                                Segment.h });
      }
      Segments.swap(Remaining);
    }

    const int Pad = Clamp(int(4 * State.Density), 7, 18);
    SDL_Rect Best = { Header.x, Header.y, 0, Header.h };
    for(const SDL_Rect& Segment : Segments)
      if(Segment.w > Best.w)
        Best = Segment;
    const int PreferredScale = Title.size() > 24 ? 5 : 7;
    if(Best.w >= TextWidth(Title.c_str(), PreferredScale) + Pad * 2)
      return { Best.x + Pad, Best.y, std::max(1, Best.w - Pad * 2), Best.h };

    const int Below = std::min(Header.y + Header.h, LowestCutout + Pad);
    return { Header.x + Pad, Below,
             std::max(1, Header.w - Pad * 2),
             std::max(1, Header.y + Header.h - Below) };
  }

  void PaintConsole(SDL_Renderer* Renderer)
  {
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
    Color(Renderer, 4, 6, 10, 255);
    SDL_RenderClear(Renderer);
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
    Fill(Renderer, State.Safe, 4, 6, 10, 235);
    const bool SettingTextPrompt = State.PromptActive
                                && !State.PromptNumeric
                                && IsSettingTextPrompt(State.PromptText);

    if(!State.Gameplay)
    {
      Fill(Renderer, State.Header, 18, 16, 14, 245);
      Frame(Renderer, State.Header);
      SDL_Rect GameFrame = { State.Game.x - 5, State.Game.y - 5,
                             State.Game.w + 10, State.Game.h + 10 };
      Frame(Renderer, GameFrame);
      const std::string HeaderTitle = CurrentHeaderTitle();
      const SDL_Rect HeaderText = SegmentedHeaderTextRect(State.Header,
                                                          HeaderTitle);
      if(HeaderTitle.size() > 24)
        CenteredWrappedText(Renderer, HeaderText, HeaderTitle, 5);
      else
      {
        const bool NeutralHeader = State.MenuActive || SettingTextPrompt;
        CenterText(Renderer, HeaderText, HeaderTitle.c_str(), 7,
                   MainMenuPresentation() ? 210
                                           : (NeutralHeader ? 240 : 210),
                   MainMenuPresentation() ? 55
                                           : (NeutralHeader ? 230 : 55),
                   MainMenuPresentation() ? 45
                                           : (NeutralHeader ? 202 : 45));
      }
      if(State.PromptActive && !State.PromptGameplay)
      {
        const int Pad = Clamp(int(18 * State.Density), 24, 54);
        SDL_Rect Body = { State.Game.x + Pad, State.Game.y + Pad,
                          State.Game.w - Pad * 2, State.Game.h - Pad * 2 };
        if(State.PromptNumeric)
        {
          WrappedText(Renderer, Body, State.PromptText);
        }
        else
        {
          const int TopicHeight = Clamp(Body.h * 28 / 100, 90, 240);
          SDL_Rect Topic = { Body.x, Body.y, Body.w, TopicHeight };
          if(SettingTextPrompt)
            CenteredWrappedText(Renderer, Topic,
                                SettingPromptHelp(State.PromptText));
          else
            WrappedText(Renderer, Topic, State.PromptText);

          const int FieldHeight = Clamp(int(52 * State.Density), 92, 150);
          SDL_Rect Field = { Body.x + Clamp(Body.w / 14, 18, 64),
                             Body.y + TopicHeight + Clamp(Body.h / 18, 14, 48),
                             Body.w - Clamp(Body.w / 7, 36, 128), FieldHeight };
          Fill(Renderer, Field, 10, 8, 8, 245);
          Frame(Renderer, Field);
          std::string Entry = State.PromptInput;
          Entry += '_';
          CenterText(Renderer, Field, Entry.c_str(), 7, 240, 230, 202);

          SDL_Rect Hint = { Body.x,
                            std::min(Body.y + Body.h - FieldHeight,
                                     Field.y + Field.h
                                       + Clamp(Body.h / 12, 20, 64)),
                            Body.w, FieldHeight };
          if(SettingTextPrompt)
            CenteredWrappedText(Renderer, Hint,
                                "TYPE THE NEW VALUE\nTAP ENTER TO SAVE", 4);
          else
            CenterText(Renderer, Hint, "TYPE YOUR NAME, THEN TAP ENTER", 4,
                       190, 180, 155);
          return;
        }
      }
      if(State.ScreenTextActive)
      {
        const int FooterHeight = Clamp(int(24 * State.Density), 52, 86);
        WrappedText(Renderer, { State.Game.x + 18, State.Game.y + 18,
                                State.Game.w - 36,
                                State.Game.h - FooterHeight - 36 },
                    State.ScreenText, false, 140);
        SDL_Rect Footer = { State.Game.x + 12,
                            State.Game.y + State.Game.h - FooterHeight - 8,
                            State.Game.w - 24, FooterHeight };
        CenterText(Renderer, Footer, "TAP ANYWHERE TO CONTINUE", 5,
                   190, 180, 155);
        return;
      }
    }
    else
    {
      Frame(Renderer, State.Stats);
      Frame(Renderer, State.Game);
      PaintStats(Renderer);
      if(State.PromptActive)
        PaintGameplayLog(Renderer);
      if(State.PromptActive && State.PromptShowsInput && !State.PromptNumeric)
      {
        const int Pad = Clamp(int(12 * State.Density), 16, 40);
        const int CardHeight = Clamp(State.Game.h * 42 / 100, 180, 360);
        SDL_Rect Card = { State.Game.x + Pad,
                          State.Game.y + Pad,
                          std::max(1, State.Game.w - Pad * 2),
                          std::min(CardHeight, std::max(1, State.Game.h - Pad * 2)) };
        Fill(Renderer, Card, 10, 8, 8, 248);
        Frame(Renderer, Card);
        const int TopicHeight = Card.h * 48 / 100;
        const SDL_Rect Topic = { Card.x + Pad, Card.y + Pad,
                                 Card.w - Pad * 2, TopicHeight - Pad };
        if(SettingTextPrompt)
        {
          std::string SettingText = SettingPromptTitle(State.PromptText);
          SettingText += "\n\n";
          SettingText += SettingPromptHelp(State.PromptText);
          CenteredWrappedText(Renderer, Topic, SettingText);
        }
        else
          WrappedText(Renderer, Topic, State.PromptText);
        SDL_Rect Field = { Card.x + Pad, Card.y + TopicHeight,
                           Card.w - Pad * 2,
                           Clamp(int(46 * State.Density), 64, 112) };
        Fill(Renderer, Field, 20, 17, 14, 255);
        Outline(Renderer, Field, 156, 137, 100);
        std::string Entry = State.PromptInput;
        Entry += '_';
        CenterText(Renderer, Field, Entry.c_str(), 6);
        SDL_Rect Hint = { Card.x + Pad, Field.y + Field.h,
                          Card.w - Pad * 2,
                          std::max(1, Card.y + Card.h - Field.y - Field.h) };
        if(SettingTextPrompt)
          CenteredWrappedText(Renderer, Hint,
                              "TYPE THE NEW VALUE\nTAP ENTER TO SAVE", 4);
        else
          CenterText(Renderer, Hint, "TAP FIELD TO TYPE, THEN SELECT", 4,
                     190, 180, 155);
      }
    }

    const bool AdaptiveGrid = AdaptiveGridMenuPresentation();
    const bool ShowChoices = State.QuestionChoiceCount > 0;
    const bool BinaryConfirmation = BinaryConfirmationActive();
    if(!AdaptiveGrid)
    {
      if(BinaryConfirmation && State.Width < State.Height)
      {
        SDL_Rect Buttons = BinaryConfirmationButton(0);
        Buttons.w = State.Controls.w;
        Frame(Renderer, Buttons);
      }
      else
        Frame(Renderer, State.Controls);
    }
    Frame(Renderer, State.Toggle);
    const bool ShowActions = State.Gameplay && !State.PromptActive
                          && State.ControlMode == CONTROL_ACTIONS;
    int CurrentGroup = -1;
    if(ShowActions)
    {
      int PageIndices[ACTIONS_PER_PAGE];
      BuildActionPage(State.ActionPage, PageIndices, CurrentGroup);
    }
    std::string PromptValueLabel;
    if(State.PromptNumeric)
    {
      PromptValueLabel = "VALUE: ";
      PromptValueLabel += State.PromptInput.empty() ? "_" : State.PromptInput;
    }
    const bool MapCursor = (State.MapScreen || State.PositionPrompt)
                        && State.PromptActive
                        && State.PromptGameplay && !State.PromptShowsInput
                        && !ShowChoices;
    const char* ToggleLabel = State.PromptNumeric ? PromptValueLabel.c_str()
      : (MapCursor ? "BACK"
      : (ShowChoices ? (BinaryConfirmation ? "CONFIRM"
                         : (State.MapScreen ? "MAP ACTIONS" : "CHOICES"))
       : (!State.Gameplay ? (AdaptiveGrid ? "ITEMS"
                          : (MainMenuPresentation() ? "MENU CONTROLS"
                            : (State.MenuDirectionMode ? "MENU" : "DIRECTIONS")))
                          : (ShowActions ? ActionGroupName(CurrentGroup)
                                          : "DIRECTIONS"))));
    CenterTextAtScale(Renderer, State.Toggle,
                      ToggleLabel, 5, 235, 218, 174);
    if(ShowControlSectionTabs())
      PaintControlSectionTabs(Renderer);
    if(AdaptiveGrid)
      return;

    const char* Directions[ACTION_COUNT] = {
      "NW", "N", "NE", "W", "WAIT", "E", "SW", "S", "SE"
    };
    if(State.PromptNumeric)
    {
      static const char* NumericLabels[15] = {
        "1", "2", "3", "4", "5", "6",
        "7", "8", "9", "DELETE", "0", "OK",
        "", "BACK", ""
      };
      for(int Index = 0; Index < 15; ++Index)
      {
        if(!NumericLabels[Index][0])
          continue;
        SDL_Rect Button = GridCell(State.Controls, 3, 5, Index, 3);
        const bool Confirm = Index == 11;
        const bool Delete = Index == 9;
        const bool Back = Index == 13;
        Fill(Renderer, Button, Confirm ? 35 : ((Delete || Back) ? 54 : 19),
             Confirm ? 71 : ((Delete || Back) ? 31 : 23),
             Confirm ? 48 : ((Delete || Back) ? 29 : 28), 245);
        Outline(Renderer, Button, 156, 137, 100);
        CenterTextAtScale(Renderer, Button, NumericLabels[Index], 4);
      }
    }
    else if(ShowChoices)
    {
      for(int Index = 0; Index < State.QuestionChoiceCount; ++Index)
      {
        SDL_Rect Button = BinaryConfirmation
          ? BinaryConfirmationButton(Index)
          : GridCell(State.Controls, 3, 3, Index, 3);
        const bool Back = State.QuestionChoices[Index] == KEY_ESC
                       || State.QuestionChoices[Index] == KEY_CONTROLLER_B;
        Fill(Renderer, Button, Back ? 54 : 35, Back ? 31 : 71,
             Back ? 29 : 48, 238);
        Outline(Renderer, Button, 156, 137, 100);
        char Label[16];
        if(BinaryConfirmation)
          std::snprintf(Label, sizeof(Label), "%s",
                        Index == 0 ? "YES" : "NO");
        else
          KeyLabel(State.QuestionChoices[Index], Label, sizeof(Label));
        CenterTextAtScale(Renderer, Button, Label, 5);
      }
    }
    else if(!State.Gameplay)
    {
      const actiondef* Buttons = State.MenuDirectionMode
                               ? MenuDirections : MenuNavigation;
      for(int Index = 0; Index < ACTION_COUNT; ++Index)
      {
        SDL_Rect Button = GridCell(State.Controls, 3, 3, Index, 3);
        const bool Select = !State.MenuDirectionMode && Index == 4;
        const bool Back = !State.MenuDirectionMode && Index == 8;
        const bool OriginalMenu = MainMenuPresentation();
        Fill(Renderer, Button,
             Select ? (OriginalMenu ? 65 : 35) : (Back ? 54 : 12),
             Select ? (OriginalMenu ? 20 : 71) : (Back ? 22 : 14),
             Select ? (OriginalMenu ? 18 : 48) : (Back ? 20 : 18), 238);
        Outline(Renderer, Button, OriginalMenu ? 130 : 143,
                OriginalMenu ? 102 : 124, OriginalMenu ? 69 : 91);
        CenterTextAtScale(Renderer, Button, Buttons[Index].Label, 4);
      }
    }
    else if(!ShowActions)
    {
      for(int Index = 0; Index < ACTION_COUNT; ++Index)
      {
        SDL_Rect Button = GridCell(State.Controls, 3, 3, Index, 3);
        Fill(Renderer, Button, Index == 4 ? 30 : 19, Index == 4 ? 27 : 23,
             Index == 4 ? 23 : 28, 235);
        Outline(Renderer, Button, 143, 124, 91);
        CenterTextAtScale(Renderer, Button,
                          MapCursor && Index == 4 ? "SELECT" : Directions[Index],
                          6);
      }
    }
    else
    {
      int PageIndices[ACTIONS_PER_PAGE];
      int Group = -1;
      const int PageActions = BuildActionPage(State.ActionPage,
                                              PageIndices, Group);
      const SDL_Rect Grid = ActionGridRect();
      int ActionScale = 4;
      for(int Index = 0; Index < PageActions; ++Index)
      {
        const SDL_Rect Button = GridCell(Grid, ACTION_COLUMNS, ACTION_ROWS,
                                         Index, 3);
        ActionScale = std::min(ActionScale,
          FittingTextScale(Button,
                           State.ActionLabels[PageIndices[Index]], 4));
      }

      for(int Index = 0; Index < PageActions; ++Index)
      {
        SDL_Rect Button = GridCell(Grid, ACTION_COLUMNS, ACTION_ROWS,
                                   Index, 3);
        Fill(Renderer, Button, 14, 18, 32, 238);
        Outline(Renderer, Button, 156, 137, 100);
        CenterTextAtScale(Renderer, Button,
                          State.ActionLabels[PageIndices[Index]].c_str(),
                          ActionScale);
      }

      if(ActionPagesForGroup(Group) > 1)
      {
        SDL_Rect More = GridCell(Grid, ACTION_COLUMNS, ACTION_ROWS,
                                 ACTION_COUNT - 1, 3);
        Fill(Renderer, More, 45, 38, 24, 238);
        Outline(Renderer, More, 156, 137, 100);
        CenterTextAtScale(Renderer, More, "MORE", ActionScale);
      }
    }
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
  }

}

namespace mobileui
{
  void SetControllerOnLeft(bool OnLeft)
  {
    if(State.ControllerOnLeft == OnLeft)
      return;

    State.ControllerOnLeft = OnLeft;
    ConsoleDirty = true;

    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetStatusBarHidden(bool Hidden)
  {
    JNIEnv* Env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject Activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if(!Env || !Activity)
      return;

    jclass ActivityClass = Env->GetObjectClass(Activity);
    if(ActivityClass)
    {
      jmethodID Method = Env->GetMethodID(ActivityClass,
                                          "setStatusBarHidden", "(Z)V");
      if(Method)
        Env->CallVoidMethod(Activity, Method, Hidden ? JNI_TRUE : JNI_FALSE);
      Env->DeleteLocalRef(ActivityClass);
    }
    Env->DeleteLocalRef(Activity);
  }

  void SetHapticsEnabled(bool Enabled)
  {
    State.HapticsEnabled = Enabled;
  }

  void SetHapticStrength(int Strength)
  {
    static const int StrengthPercent[] = { 50, 75, 100 };
    State.HapticStrength = StrengthPercent[Clamp(Strength, 0, 2)];
  }

  void Pulse(feedbacktype Type, int Magnitude)
  {
    if(!State.HapticsEnabled)
      return;

    JNIEnv* Env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject Activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if(!Env || !Activity)
      return;

    jclass ActivityClass = Env->GetObjectClass(Activity);
    if(ActivityClass)
    {
      jmethodID Method = Env->GetMethodID(ActivityClass,
                                          "vibrateFeedback", "(II)V");
      if(Method)
      {
        const int Strength = Clamp(State.HapticStrength
                                   * Clamp(Magnitude, 1, 100) / 100,
                                   1, 100);
        Env->CallVoidMethod(Activity, Method, int(Type), Strength);
      }
      Env->DeleteLocalRef(ActivityClass);
    }
    Env->DeleteLocalRef(Activity);
  }

  void SetSafeInsets(int Left, int Top, int Right, int Bottom,
                     const int* CutoutRects, int CutoutCount, float Density)
  {
    State.Left = std::max(0, Left);
    State.Top = std::max(0, Top);
    State.Right = std::max(0, Right);
    State.Bottom = std::max(0, Bottom);
    State.DisplayCutout = { 0, 0, 0, 0 };
    State.DisplayCutoutCount = Clamp(CutoutCount, 0, MAX_DISPLAY_CUTOUTS);
    for(int Index = 0; Index < State.DisplayCutoutCount; ++Index)
    {
      const int* Rect = CutoutRects + Index * 4;
      State.DisplayCutouts[Index] = {
        std::max(0, Rect[0]), std::max(0, Rect[1]),
        std::max(0, Rect[2] - Rect[0]), std::max(0, Rect[3] - Rect[1])
      };
      const SDL_Rect& Candidate = State.DisplayCutouts[Index];
      if(Candidate.w * Candidate.h
         > State.DisplayCutout.w * State.DisplayCutout.h)
        State.DisplayCutout = Candidate;
    }
    State.Density = std::max(1.f, Density);
    ConsoleDirty = true;

    // Insets arrive on Android's UI thread, often after the first rendered
    // frame or after the keyboard changes visibility. Ask SDL's render thread
    // for a fresh frame so the console never waits for the next game input.
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetMapFocus(int X, int Y, int PlayerX, int PlayerY)
  {
    adaptiveui::SetMapFocus(X, Y, PlayerX, PlayerY);
    const bool PlayerMoved = State.HasPlayerMapPosition
      && (State.PlayerMapX != PlayerX || State.PlayerMapY != PlayerY);
    State.MapFocusX = X;
    State.MapFocusY = Y;
    State.PlayerMapX = PlayerX;
    State.PlayerMapY = PlayerY;
    State.HasPlayerMapPosition = true;
    if(PlayerMoved)
    {
      State.CanvasPanX = 0;
      State.CanvasPanY = 0;
      State.CanvasPressActive = false;
      State.CanvasPanning = false;
    }
  }

  void SetStats(const char* Line1, const char* Line2,
                const char* Line3, const char* Line4)
  {
    adaptiveui::SetStats(Line1, Line2, Line3, Line4);
    const char* Lines[4] = { Line1, Line2, Line3, Line4 };
    bool Changed = false;
    for(int Index = 0; Index < 4; ++Index)
    {
      const std::string Value = Lines[Index] ? Lines[Index] : "";
      if(State.StatsLines[Index] != Value)
      {
        State.StatsLines[Index] = Value;
        Changed = true;
      }
    }
    if(Changed)
      ConsoleDirty = true;
  }

  void SetLog(const char* Message)
  {
    adaptiveui::SetLog(Message);
    const std::string Value = Message ? Message : "";
    State.LogMessage = Value;
    State.LogVisible = !Value.empty();
    if(State.LogHideTimer)
    {
      SDL_RemoveTimer(State.LogHideTimer);
      State.LogHideTimer = 0;
    }
    if(State.LogVisible)
    {
      State.LogHideDeadline = SDL_GetTicks() + LOG_VISIBLE_MS;
      State.LogHideTimer = SDL_AddTimer(LOG_VISIBLE_MS, QueueLogHide, 0);
    }
    ConsoleDirty = true;
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetPrompt(const char* Prompt, const char* Input, bool Numeric)
  {
    adaptiveui::SetPrompt(Prompt, Input, Numeric);
    const std::string NewPrompt = FormatPromptText(Prompt ? Prompt : "");
    const std::string NewInput = Input ? Input : "";
    const bool ShowsInput = Input != 0;
    const bool Changed = !State.PromptActive
      || State.PromptText != NewPrompt
      || State.PromptInput != NewInput
      || State.PromptShowsInput != ShowsInput
      || State.PromptNumeric != Numeric;
    if(!State.PromptActive)
    {
      State.PromptGameplay = State.Gameplay || State.MapScreen;
      State.PromptDetail.clear();
    }
    State.PromptActive = true;
    State.PromptShowsInput = ShowsInput;
    State.PromptNumeric = Numeric;
    State.PromptText = NewPrompt;
    State.PromptInput = NewInput;
    if(Changed)
    {
      ConsoleDirty = true;
      SDL_Event Event;
      SDL_zero(Event);
      Event.type = SDL_USEREVENT;
      Event.user.code = REDRAW_EVENT_CODE;
      SDL_PushEvent(&Event);
    }
  }

  void SetConfirmationPrompt(const char* Prompt)
  {
    adaptiveui::SetConfirmationPrompt(Prompt);
    const adaptiveui::HudModel& Hud = adaptiveui::GetHudModel();
    const bool Changed = !State.PromptActive
      || State.PromptText != Hud.Prompt
      || State.PromptDetail != Hud.PromptDetail
      || State.PromptShowsInput || State.PromptNumeric;
    if(!State.PromptActive)
      State.PromptGameplay = State.Gameplay || State.MapScreen;
    State.PromptActive = true;
    State.PromptShowsInput = false;
    State.PromptNumeric = false;
    State.PromptText = Hud.Prompt;
    State.PromptDetail = Hud.PromptDetail;
    State.PromptInput.clear();
    if(Changed)
    {
      ConsoleDirty = true;
      SDL_Event Event;
      SDL_zero(Event);
      Event.type = SDL_USEREVENT;
      Event.user.code = REDRAW_EVENT_CODE;
      SDL_PushEvent(&Event);
    }
  }

  void SetPromptDetail(const char* Detail)
  {
    adaptiveui::SetPromptDetail(Detail);
    const std::string Value = Detail ? Detail : "";
    if(State.PromptDetail == Value)
      return;
    State.PromptDetail = Value;
    ConsoleDirty = true;
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetPositionPrompt(bool Active)
  {
    adaptiveui::SetPositionPrompt(Active);
    if(State.PositionPrompt == Active)
      return;
    State.PositionPrompt = Active;
    ConsoleDirty = true;
  }

  void ClearPrompt()
  {
    adaptiveui::ClearPrompt();
    if(!State.PromptActive)
      return;
    State.PromptActive = false;
    State.PromptGameplay = false;
    State.PositionPrompt = false;
    State.PromptShowsInput = false;
    State.PromptNumeric = false;
    State.PromptText.clear();
    State.PromptDetail.clear();
    State.PromptInput.clear();
    ConsoleDirty = true;
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetPaperDollScreen(bool Active, int X, int Y, int Width, int Height)
  {
    adaptiveui::SetPaperDollScreen(Active, X, Y, Width, Height);
    State.PaperDollScreen = Active;
    State.PaperDollSource = Active
      ? SDL_Rect{ X, Y, Width, Height } : SDL_Rect{ 0, 0, 0, 0 };
    ConsoleDirty = true;
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetMapScreen(bool Active)
  {
    adaptiveui::SetMapScreen(Active);
    if(State.MapScreen == Active)
      return;
    State.MapScreen = Active;
    if(!Active)
    {
      State.MapNoteCount = 0;
      for(int Index = 0; Index < layoutstate::MAX_MAP_NOTES; ++Index)
        State.MapNoteLabels[Index].clear();
    }
    if(Active)
      State.ControlMode = CONTROL_MOVEMENT;
    ConsoleDirty = true;
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetMapSourceBounds(int X, int Y, int Width, int Height)
  {
    adaptiveui::SetMapSourceBounds(X, Y, Width, Height);
    SDL_Rect Bounds = { X, Y, std::max(0, Width), std::max(0, Height) };
    if(State.MapOverlaySource.x == Bounds.x
       && State.MapOverlaySource.y == Bounds.y
       && State.MapOverlaySource.w == Bounds.w
       && State.MapOverlaySource.h == Bounds.h)
      return;
    State.MapOverlaySource = Bounds;
    ConsoleDirty = true;
  }

  void SetMapNotes(const char* const* Notes, const int* X, const int* Y,
                   int Count)
  {
    adaptiveui::SetMapNotes(Notes, X, Y, Count);
    Count = Clamp(Count, 0, layoutstate::MAX_MAP_NOTES);
    bool Changed = State.MapNoteCount != Count;
    for(int Index = 0; Index < Count; ++Index)
    {
      const std::string Label = Notes && Notes[Index] ? Notes[Index] : "";
      const SDL_Point Point = { X ? X[Index] : 0, Y ? Y[Index] : 0 };
      if(State.MapNoteLabels[Index] != Label
         || State.MapNotePoints[Index].x != Point.x
         || State.MapNotePoints[Index].y != Point.y)
      {
        State.MapNoteLabels[Index] = Label;
        State.MapNotePoints[Index] = Point;
        Changed = true;
      }
    }
    for(int Index = Count; Index < State.MapNoteCount; ++Index)
      State.MapNoteLabels[Index].clear();
    State.MapNoteCount = Count;
    if(Changed)
      ConsoleDirty = true;
  }

  void SetScreenText(const char* Value)
  {
    adaptiveui::SetScreenText(Value);
    std::string Raw = Value ? Value : "";
    static const char TouchHelpHeading[] = "[Android Touch Help:]";
    static const char MapHelpHeading[] = "[Map Touch Help:]";
    if(Raw.compare(0, sizeof(TouchHelpHeading) - 1,
                   TouchHelpHeading) == 0)
    {
      State.ScreenTextTitle = "TOUCH HELP";
      const size_t Body = Raw.find('\n');
      Raw = Body == std::string::npos ? "" : Raw.substr(Body + 1);
    }
    else if(Raw.compare(0, sizeof(MapHelpHeading) - 1,
                        MapHelpHeading) == 0)
    {
      State.ScreenTextTitle = "MAP HELP";
      const size_t Body = Raw.find('\n');
      Raw = Body == std::string::npos ? "" : Raw.substr(Body + 1);
    }
    else
      State.ScreenTextTitle = "STORY";
    State.ScreenTextActive = true;
    State.ScreenText = FormatScreenText(Raw);
    State.MenuActive = false;
    ConsoleDirty = true;
  }

  void ClearScreenText()
  {
    adaptiveui::ClearScreenText();
    if(!State.ScreenTextActive)
      return;
    State.ScreenTextActive = false;
    State.ScreenText.clear();
    State.ScreenTextTitle = "STORY";
    ConsoleDirty = true;
  }

  void SetActions(const char* const* Labels, const int* Keys,
                  const int* Groups, int Count)
  {
    adaptiveui::SetActions(Labels, Keys, Groups, Count);
    Count = Clamp(Count, 0, MAX_MOBILE_ACTIONS);
    bool Changed = State.ActionCount != Count;
    for(int Index = 0; Index < Count; ++Index)
    {
      const std::string Label = Labels && Labels[Index] ? Labels[Index] : "";
      const int Key = Keys ? Keys[Index] : 0;
      const int Group = Groups ? Clamp(Groups[Index], 0, ACTION_GROUPS - 1)
                               : ACTION_SYSTEM;
      if(State.ActionLabels[Index] != Label
         || State.ActionKeys[Index] != Key
         || State.ActionGroups[Index] != Group)
        Changed = true;
      State.ActionLabels[Index] = Label;
      State.ActionKeys[Index] = Key;
      State.ActionGroups[Index] = Group;
    }
    State.ActionCount = Count;
    if(State.PinnedActionGroup >= 0
       && ActionCountForGroup(State.PinnedActionGroup) <= 0)
      State.PinnedActionGroup = -1;
    State.ActionPage = Clamp(State.ActionPage, 0, ActionPageCount() - 1);
    if(Changed)
      ConsoleDirty = true;
  }

  void SetQuestionChoices(const int* Keys, int Count)
  {
    adaptiveui::SetQuestionChoices(Keys, Count);
    Count = Clamp(Count, 0, MAX_QUESTION_CHOICES);
    bool Changed = State.QuestionChoiceCount != Count;
    for(int Index = 0; Index < Count; ++Index)
      if(State.QuestionChoices[Index] != Keys[Index])
      {
        State.QuestionChoices[Index] = Keys[Index];
        Changed = true;
      }
    State.QuestionChoiceCount = Count;
    if(Changed)
    {
      ConsoleDirty = true;
      SDL_Event Event;
      SDL_zero(Event);
      Event.type = SDL_USEREVENT;
      Event.user.code = REDRAW_EVENT_CODE;
      SDL_PushEvent(&Event);
    }
  }

  void SetMenu(const char* Title, const char* Subtitle,
               const char* const* Options, int Count, int Selected,
               int Page, int Pages)
  {
    adaptiveui::SetMenu(Title, Subtitle, Options, Count, Selected, Page, Pages);
    const std::string NewTitle = Title ? Title : "MENU";
    const bool NewMenu = !State.MenuActive || State.MenuTitle != NewTitle;
    State.MenuActive = true;
    State.MenuTitle = NewTitle;
    State.MenuSubtitle = Subtitle ? Subtitle : "";
    State.MenuOptionCount = Clamp(Count, 0, MAX_MENU_OPTIONS);
    for(int Index = 0; Index < State.MenuOptionCount; ++Index)
      State.MenuOptions[Index] = Options && Options[Index] ? Options[Index] : "";
    State.MenuSelected = Clamp(Selected, -1,
                               std::max(-1, State.MenuOptionCount - 1));
    State.MenuPage = std::max(1, Page);
    State.MenuPages = std::max(1, Pages);
    if(NewMenu)
    {
      StopMenuFling();
      State.MenuScrollY = 0;
      State.ConditionScrollY = 0;
      State.MenuGridSelection = -1;
    }
    ConsoleDirty = true;
  }

  int PageMenu(int Selected, int Direction, int Count)
  {
    adaptiveui::PageMenu(Selected, Direction, Count);
    if(State.MenuTitle != "Equipment" || !Direction || Count <= 0
       || State.MenuMaxScrollY <= 0)
      return Selected;

    const int PageStep = std::max(1, State.MenuViewport.h);
    State.MenuScrollY = Clamp(State.MenuScrollY
                                + (Direction > 0 ? PageStep : -PageStep),
                              0, State.MenuMaxScrollY);
    const int RowHeight = Clamp(int(57 * State.Density), 120, 180);
    // Select the first completely visible row in the new set. At the final
    // partial page this still exposes every remaining equipment slot.
    return Clamp((State.MenuScrollY + RowHeight - 1) / RowHeight,
                 0, Count - 1);
  }

  void ClearMenu()
  {
    adaptiveui::ClearMenu();
    State.MenuActive = false;
    State.MenuOptionCount = 0;
    State.MenuSelected = -1;
    State.MenuGridSelection = -1;
    State.MenuPressActive = false;
    State.MenuPressConditions = false;
    State.MenuScrolling = false;
    State.MenuStoppedFlingOnPress = false;
    State.MenuScrollY = 0;
    State.MenuMaxScrollY = 0;
    State.MenuScrollStep = 0;
    StopMenuFling();
    State.ConditionScrollY = 0;
    State.ConditionMaxScrollY = 0;
    State.MenuConfirm = { 0, 0, 0, 0 };
    State.MenuEquip = { 0, 0, 0, 0 };
    State.MenuItemActionCount = 0;
    for(int Index = 0; Index < MAX_PICKUP_ACTIONS; ++Index)
    {
      State.MenuItemActions[Index] = { 0, 0, 0, 0 };
      State.MenuItemActionCodes[Index] = adaptiveui::ITEM_ACTION_NONE;
    }
    State.MenuBack = { 0, 0, 0, 0 };
    ConsoleDirty = true;
  }

  void UpdateLayout(SDL_Renderer* Renderer, int GameWidth, int GameHeight)
  {
    const int PreviousWidth = State.Width;
    const int PreviousHeight = State.Height;
    const bool PreviousGameplay = State.Gameplay;
    SDL_GetRendererOutputSize(Renderer, &State.Width, &State.Height);
    State.GameWidth = GameWidth;
    State.GameHeight = GameHeight;
    State.Gameplay = GameplayPresentation();
    if(State.Gameplay && !PreviousGameplay)
    {
      if(State.PinnedActionGroup >= 0
         && ActionCountForGroup(State.PinnedActionGroup) > 0)
        SelectActionGroup(State.PinnedActionGroup);
      else
        State.ControlMode = CONTROL_MOVEMENT;
    }
    if(!State.Gameplay && PreviousGameplay)
    {
      State.MenuDirectionMode = false;
    }
    const int Gap = Clamp(int(7 * State.Density), 8, 28);
    State.Safe = { State.Left + Gap, State.Top + Gap,
                   std::max(1, State.Width - State.Left - State.Right - Gap * 2),
                   std::max(1, State.Height - State.Top - State.Bottom - Gap * 2) };
    const int BaseHeaderHeight = Clamp(int(24 * State.Density), 48, 96);
    int HeaderHeight = BaseHeaderHeight;
    if(!State.Gameplay && State.MenuActive && State.MenuTitle.size() > 24)
      HeaderHeight = std::max(HeaderHeight, 96);
    // Keep a compact header when the title fits beside every top cutout. Only
    // reserve a full row below the hardware when no unobstructed segment is
    // wide enough at the title's preferred scale.
    if(!State.Gameplay && State.Width < State.Height
       && State.DisplayCutoutCount > 0)
    {
      const SDL_Rect CompactHeader = { State.Safe.x, State.Safe.y,
                                       State.Safe.w, HeaderHeight };
      const SDL_Rect CompactText = SegmentedHeaderTextRect(
        CompactHeader, CurrentHeaderTitle());
      if(CompactText.y > CompactHeader.y)
      {
        int CutoutBottom = State.Safe.y;
        for(int Index = 0; Index < State.DisplayCutoutCount; ++Index)
          CutoutBottom = std::max(CutoutBottom,
            State.DisplayCutouts[Index].y + State.DisplayCutouts[Index].h);
        HeaderHeight = std::max(HeaderHeight,
          CutoutBottom - State.Safe.y + Gap + BaseHeaderHeight);
      }
    }

    if(State.Gameplay && State.Width < State.Height)
    {
      int StatsHeight = Clamp(int(State.Safe.w * 75.f / 384.f), 150, 230);
      if(State.DisplayCutout.w > 0 && State.DisplayCutout.h > 0
         && State.DisplayCutout.y <= State.Safe.y)
      {
        const int ExtraRow = Clamp(int(24 * State.Density), 64, 104);
        StatsHeight = std::max(StatsHeight,
          State.DisplayCutout.y + State.DisplayCutout.h - State.Safe.y
          + Gap + ExtraRow);
      }
      const int ToggleHeight = Clamp(int(28 * State.Density), 58, 100);
      const int PromptLogHeight = Clamp(int(State.Safe.w * 106.f / 678.f),
                                        110, 180);
      const int BannerHeight = Clamp(int(24 * State.Density), 50, 84);
      const int ReservedLogHeight = State.PromptActive ? PromptLogHeight : 0;
      const int MaximumControls = Clamp(int(State.Safe.h * .31f), 460, 700);
      int ControlsSize = std::min(State.Safe.w, MaximumControls);
      int MapTop = State.Safe.y + StatsHeight + Gap;
      int ControlsBottom = State.Safe.y + State.Safe.h;
      int ControlsTop = ControlsBottom - ControlsSize;
      int ToggleTop = ControlsTop - Gap - ToggleHeight;
      int LogTop = ToggleTop - Gap - ReservedLogHeight;
      int MapHeight = std::max(1, LogTop - Gap - MapTop);

      // On short portrait displays, preserve the map and shrink the controls
      // before allowing regions to overlap.
      if(MapHeight < State.Safe.w * 3 / 4)
      {
        const int Reduction = State.Safe.w * 3 / 4 - MapHeight;
        ControlsSize = std::max(360, ControlsSize - Reduction);
        ControlsTop = ControlsBottom - ControlsSize;
        ToggleTop = ControlsTop - Gap - ToggleHeight;
        LogTop = ToggleTop - Gap - ReservedLogHeight;
        MapHeight = std::max(1, LogTop - Gap - MapTop);
      }

      State.Stats = { State.Safe.x, State.Safe.y, State.Safe.w, StatsHeight };
      State.Game = { State.Safe.x, MapTop, State.Safe.w, MapHeight };
      State.Log = State.PromptActive
        ? SDL_Rect{ State.Safe.x, LogTop, State.Safe.w, PromptLogHeight }
        : SDL_Rect{ State.Game.x,
                    State.Game.y + State.Game.h - BannerHeight,
                    State.Game.w, BannerHeight };
      State.Toggle = { State.Safe.x + (State.Safe.w - ControlsSize) / 2,
                       ToggleTop, ControlsSize, ToggleHeight };
      State.Controls = { State.Safe.x + (State.Safe.w - ControlsSize) / 2,
                         ControlsTop, ControlsSize, ControlsSize };
      if(BinaryConfirmationActive())
      {
        const int ButtonHeight = std::max(1, State.Controls.h / 3);
        const int ButtonsTop = State.Controls.y + State.Controls.h
                             - ButtonHeight;
        State.Toggle.y = ButtonsTop - Gap - ToggleHeight;
        State.Log.h = std::max(State.Log.h,
          State.Toggle.y - Gap - State.Log.y);
      }
    }
    else if(State.Gameplay)
    {
      // Four-line stat groups need at least 44 pixels per row to preserve the
      // same four-pixel type used in portrait.
      const int StatsHeight = Clamp(int(State.Safe.h * .22f), 190, 240);
      const int RailWidth = Clamp(int(State.Safe.w * .38f), 560, 900);
      const int ToggleHeight = Clamp(int(22 * State.Density), 48, 82);
      const int PromptLogHeight = Clamp(int(State.Safe.h * .19f), 150, 200);
      const int BannerHeight = Clamp(int(22 * State.Density), 48, 76);
      const int ContentTop = State.Safe.y + StatsHeight + Gap;
      const int ControllerBottom = State.Safe.y + State.Safe.h;
      const int GameBottom = State.PromptActive
        ? ControllerBottom - PromptLogHeight - Gap : ControllerBottom;
      const int RailX = State.ControllerOnLeft
        ? State.Safe.x : State.Safe.x + State.Safe.w - RailWidth;
      const int GameX = State.ControllerOnLeft
        ? RailX + RailWidth + Gap : State.Safe.x;
      const int GameRight = State.ControllerOnLeft
        ? State.Safe.x + State.Safe.w : RailX - Gap;
      const int SectionTabWidth = Clamp(int(38 * State.Density), 76, 118);
      const int MaximumControlsWidth = std::max(1,
        RailWidth - (SectionTabWidth + Gap) * 2);
      const int ControlsSize = std::min(MaximumControlsWidth,
        ControllerBottom - ContentTop - ToggleHeight - Gap);
      State.Stats = { State.Safe.x, State.Safe.y, State.Safe.w, StatsHeight };
      State.Game = { GameX, ContentTop,
                     std::max(1, GameRight - GameX),
                     std::max(1, GameBottom - ContentTop) };
      State.Toggle = { RailX, ContentTop, RailWidth, ToggleHeight };
      State.Controls = { RailX + (RailWidth - ControlsSize) / 2,
                         State.Toggle.y + State.Toggle.h + Gap,
                         ControlsSize, ControlsSize };
      State.Log = State.PromptActive
        ? SDL_Rect{ GameX, GameBottom + Gap,
                    std::max(1, GameRight - GameX), PromptLogHeight }
        : SDL_Rect{ State.Game.x,
                    State.Game.y + State.Game.h - BannerHeight,
                    State.Game.w, BannerHeight };
    }
    else if(State.ScreenTextActive
            || (State.PromptActive && !State.PromptGameplay
                && !State.PromptNumeric))
    {
      State.Header = { State.Safe.x, State.Safe.y,
                       State.Safe.w, HeaderHeight };
      const int GameTop = State.Header.y + HeaderHeight + Gap;
      State.Game = { State.Safe.x, GameTop, State.Safe.w,
                     std::max(1, State.Safe.y + State.Safe.h - GameTop) };
      State.Toggle = { 0, 0, 0, 0 };
      State.Controls = { 0, 0, 0, 0 };
    }
    else if(State.Width >= State.Height)
    {
      const int Rail = Clamp(int(State.Safe.w * .27f), 260, State.Safe.w / 2);
      const int RailX = State.ControllerOnLeft
        ? State.Safe.x : State.Safe.x + State.Safe.w - Rail;
      const int ContentX = State.ControllerOnLeft
        ? State.Safe.x + Rail + Gap : State.Safe.x;
      State.Header = { ContentX, State.Safe.y,
                       State.Safe.w - Rail - Gap, HeaderHeight };
      if(State.MenuActive)
        State.Game = { State.Header.x, State.Header.y + HeaderHeight + Gap,
                       State.Header.w, State.Safe.h - HeaderHeight - Gap };
      else
        FitGameRect(State.Header.x, State.Header.y + HeaderHeight + Gap,
                    State.Header.w, State.Safe.h - HeaderHeight - Gap);
      const int ToggleHeight = Clamp(int(22 * State.Density), 48, 82);
      const int ControlsSize = std::min(Rail, State.Safe.h - ToggleHeight - Gap);
      State.Toggle = { RailX, State.Safe.y, Rail, ToggleHeight };
      State.Controls = { RailX + (Rail - ControlsSize) / 2,
                         State.Toggle.y + State.Toggle.h + Gap,
                         ControlsSize, ControlsSize };
    }
    else
    {
      State.Header = { State.Safe.x, State.Safe.y, State.Safe.w, HeaderHeight };
      const int ToggleHeight = Clamp(int(28 * State.Density), 58, 100);
      const int ControlsSize = std::min(State.Safe.w,
        Clamp(int(State.Safe.h * .34f), 460, 720));
      const int ControlsTop = State.Safe.y + State.Safe.h - ControlsSize;
      State.Toggle = { State.Safe.x + (State.Safe.w - ControlsSize) / 2,
                       ControlsTop - Gap - ToggleHeight,
                       ControlsSize, ToggleHeight };
      State.Controls = { State.Safe.x + (State.Safe.w - ControlsSize) / 2,
                         ControlsTop, ControlsSize, ControlsSize };
      const int GameTop = State.Header.y + HeaderHeight + Gap;
      if(State.MenuActive)
        State.Game = { State.Safe.x, GameTop, State.Safe.w,
                       std::max(1, State.Toggle.y - Gap - GameTop) };
      else
        FitGameRect(State.Safe.x, GameTop, State.Safe.w,
                    std::max(1, State.Toggle.y - Gap - GameTop));
    }

    if(PreviousWidth != State.Width || PreviousHeight != State.Height
       || PreviousGameplay != State.Gameplay)
    {
      ConsoleDirty = true;
      SDL_Log("IVAN mobile console: output=%dx%d gameplay=%d game=%d,%d %dx%d controls=%d,%d %dx%d",
              State.Width, State.Height, State.Gameplay ? 1 : 0,
              State.Game.x, State.Game.y, State.Game.w, State.Game.h,
              State.Controls.x, State.Controls.y,
              State.Controls.w, State.Controls.h);
      SDL_Event Event;
      SDL_zero(Event);
      Event.type = SDL_USEREVENT;
      Event.user.code = REDRAW_EVENT_CODE;
      SDL_PushEvent(&Event);
    }
  }

  const SDL_Rect& GetGameRect()
  {
    return State.Game;
  }

  void DrawBackground(SDL_Renderer* Renderer)
  {
    // Android may discard render-target texture contents during a surface
    // transition without reporting a size change.  Painting this lightweight
    // console directly keeps the controls, stats and log present on every
    // frame instead of leaving an apparently unplayable blank rail.
    PaintConsole(Renderer);
    ConsoleDirty = false;
  }

  void DrawGame(SDL_Renderer* Renderer, SDL_Texture* GameTexture)
  {
    if(State.Gameplay && !State.PaperDollScreen && !State.MapScreen
       && !State.PromptActive && !State.ScreenTextActive)
      CaptureGameplaySnapshot(Renderer, GameTexture);
    if(!State.Gameplay)
    {
      if(State.MenuActive)
      {
        if(MainMenuPresentation())
        {
          SDL_Rect Art = State.Game;
          const float SourceAspect = float(State.GameWidth) / State.GameHeight;
          const float DestinationAspect = float(Art.w) / Art.h;
          if(DestinationAspect > SourceAspect)
          {
            const int Width = std::max(1, int(Art.h * SourceAspect));
            Art.x += (Art.w - Width) / 2;
            Art.w = Width;
          }
          else
          {
            const int Height = std::max(1, int(Art.w / SourceAspect));
            Art.y += (Art.h - Height) / 2;
            Art.h = Height;
          }
          SDL_SetTextureAlphaMod(GameTexture, 205);
          SDL_RenderCopy(Renderer, GameTexture, 0, &Art);
          SDL_SetTextureAlphaMod(GameTexture, 255);
          Fill(Renderer, State.Game, 0, 0, 0, 72);
          PaintMainMenuMotifs(Renderer);
        }
        PaintMobileMenu(Renderer, GameTexture);
      }
      else if(!State.ScreenTextActive && !State.PromptActive)
        SDL_RenderCopy(Renderer, GameTexture, 0, &State.Game);
      return;
    }

    if(State.PaperDollScreen)
    {
      SDL_Rect Source = State.PaperDollSource;
      const int Left = Clamp(Source.x, 0, State.GameWidth - 1);
      const int Top = Clamp(Source.y, 0, State.GameHeight - 1);
      const int Right = Clamp(Source.x + Source.w, Left + 1, State.GameWidth);
      const int Bottom = Clamp(Source.y + Source.h, Top + 1, State.GameHeight);
      Source = { Left, Top, Right - Left, Bottom - Top };

      SDL_Rect Destination = State.Game;
      const float SourceAspect = float(Source.w) / Source.h;
      const float DestinationAspect = float(Destination.w) / Destination.h;
      if(DestinationAspect > SourceAspect)
      {
        const int Width = std::max(1, int(Destination.h * SourceAspect));
        Destination.x += (Destination.w - Width) / 2;
        Destination.w = Width;
      }
      else
      {
        const int Height = std::max(1, int(Destination.w / SourceAspect));
        Destination.y += (Destination.h - Height) / 2;
        Destination.h = Height;
      }
      SDL_RenderCopy(Renderer, GameTexture, &Source, &Destination);
      return;
    }

    if(State.MapScreen)
    {
      State.MapSource = State.MapOverlaySource;
      if(State.MapSource.w <= 0 || State.MapSource.h <= 0)
        State.MapSource = { 0, 0, State.GameWidth, State.GameHeight };
      else
      {
        // The legacy note list begins immediately outside the map bitmap.  A
        // large source padding captured only the top half of that text.  Keep
        // the crop tight and render note text separately below in mobile type.
        const int Padding = 2;
        const int Left = std::max(0, State.MapSource.x - Padding);
        const int Top = std::max(0, State.MapSource.y - Padding);
        const int Right = std::min(State.GameWidth,
          State.MapSource.x + State.MapSource.w + Padding);
        const int Bottom = std::min(State.GameHeight,
          State.MapSource.y + State.MapSource.h + Padding);
        State.MapSource = { Left, Top, std::max(1, Right - Left),
                            std::max(1, Bottom - Top) };
      }
      SDL_Rect Destination = State.Game;
      SDL_Rect NotesPanel = { 0, 0, 0, 0 };
      const int VisibleNotes = std::min(State.MapNoteCount, 6);
      const bool MoreNotes = State.MapNoteCount > VisibleNotes;
      const int NoteRowHeight = 40;
      if(VisibleNotes > 0)
      {
        const int NotesHeight = 8 + VisibleNotes * NoteRowHeight
                              + (MoreNotes ? 28 : 0);
        NotesPanel = { State.Game.x, State.Game.y + State.Game.h - NotesHeight,
                       State.Game.w, NotesHeight };
        Destination.h = std::max(1, Destination.h - NotesHeight - 8);
      }
      const float SourceAspect = float(State.MapSource.w) / State.MapSource.h;
      const float DestinationAspect = float(Destination.w) / Destination.h;
      if(DestinationAspect > SourceAspect)
      {
        const int Width = std::max(1, int(Destination.h * SourceAspect));
        Destination.x += (Destination.w - Width) / 2;
        Destination.w = Width;
      }
      else
      {
        const int Height = std::max(1, int(Destination.w / SourceAspect));
        Destination.y += (Destination.h - Height) / 2;
        Destination.h = Height;
      }
      SDL_RenderCopy(Renderer, GameTexture, &State.MapSource, &Destination);
      if(NotesPanel.h > 0)
      {
        static const Uint8 NoteColors[6][3] = {
          { 232, 194, 78 }, { 80, 196, 220 }, { 220, 104, 176 },
          { 116, 202, 105 }, { 230, 137, 65 }, { 202, 202, 202 }
        };
        Fill(Renderer, NotesPanel, 18, 16, 14, 250);
        Frame(Renderer, NotesPanel);
        for(int Index = 0; Index < VisibleNotes; ++Index)
        {
          const int RowY = NotesPanel.y + 4 + Index * NoteRowHeight;
          SDL_Rect Row = { NotesPanel.x + 4, RowY,
                           NotesPanel.w - 8, NoteRowHeight };
          if(Index)
          {
            Color(Renderer, 72, 62, 47, 180);
            SDL_RenderDrawLine(Renderer, Row.x, Row.y,
                               Row.x + Row.w, Row.y);
          }
          SDL_Rect Badge = { Row.x + 5, Row.y + 8, 24, 24 };
          Fill(Renderer, Badge, NoteColors[Index % 6][0],
               NoteColors[Index % 6][1], NoteColors[Index % 6][2], 255);
          char Number[8];
          snprintf(Number, sizeof(Number), "%d", Index + 1);
          CenterTextAtScale(Renderer, Badge, Number, 2, 12, 10, 8);
          SDL_Rect TextRow = { Row.x + 38, Row.y, Row.w - 43, Row.h };
          MenuRowText(Renderer, TextRow, State.MapNoteLabels[Index],
                      4, 4, 240, 230, 202);

          const SDL_Point SourcePoint = State.MapNotePoints[Index];
          if(SourcePoint.x >= State.MapSource.x
             && SourcePoint.x < State.MapSource.x + State.MapSource.w
             && SourcePoint.y >= State.MapSource.y
             && SourcePoint.y < State.MapSource.y + State.MapSource.h)
          {
            const int PointX = Destination.x
              + (SourcePoint.x - State.MapSource.x) * Destination.w
                / State.MapSource.w;
            const int PointY = Destination.y
              + (SourcePoint.y - State.MapSource.y) * Destination.h
                / State.MapSource.h;
            const int EndX = Badge.x + Badge.w / 2;
            const int EndY = Badge.y + Badge.h / 2;
            Color(Renderer, NoteColors[Index % 6][0],
                   NoteColors[Index % 6][1], NoteColors[Index % 6][2], 235);
            SDL_RenderDrawLine(Renderer, PointX, PointY, EndX, EndY);
            SDL_RenderDrawLine(Renderer, PointX + 1, PointY,
                               EndX + 1, EndY);
            SDL_Rect Marker = { PointX - 12, PointY - 12, 24, 24 };
            Fill(Renderer, Marker, NoteColors[Index % 6][0],
                 NoteColors[Index % 6][1], NoteColors[Index % 6][2], 245);
            CenterTextAtScale(Renderer, Marker, Number, 2, 12, 10, 8);
          }
        }
        if(MoreNotes)
        {
          char More[32];
          snprintf(More, sizeof(More), "+%d MORE NOTES",
                   State.MapNoteCount - VisibleNotes);
          SDL_Rect MoreRow = { NotesPanel.x + 8,
            NotesPanel.y + 4 + VisibleNotes * NoteRowHeight,
            NotesPanel.w - 16, 24 };
          CenterTextAtScale(Renderer, MoreRow, More, 3, 190, 180, 155);
        }
      }
      return;
    }

    // Zooming far enough out changes the source aspect ratio as each map axis
    // reaches its full extent. Clear the whole viewport before fitting that
    // source so the resulting letterbox remains part of the black map canvas
    // instead of exposing the brown console panel beneath it.
    Fill(Renderer, State.Game, 0, 0, 0, 255);

    const int MapLeft = 16;
    const int MapTop = 32;
    const int MapWidth = std::max(1, State.GameWidth - 128);
    const int MapHeight = std::max(1, State.GameHeight - 184);
    const float DestAspect = float(State.Game.w) / float(State.Game.h);
    // Above 1x, keep the familiar player-centred crop. Below 1x, expand each
    // source dimension independently until the complete rendered map is in
    // view. The destination is then aspect-fitted so the final overview does
    // not stretch the tiles.
    const int BaseSourceHeight = State.Width >= State.Height ? 240 : 320;
    int OneXHeight = std::min(MapHeight, BaseSourceHeight);
    int OneXWidth = std::max(1, int(OneXHeight * DestAspect));
    if(OneXWidth > MapWidth)
    {
      OneXWidth = MapWidth;
      OneXHeight = std::max(1, int(OneXWidth / DestAspect));
    }
    const float MinimumZoom = std::max(.05f, std::min(
      float(OneXWidth) / MapWidth, float(OneXHeight) / MapHeight));
    State.CanvasZoom = std::max(MinimumZoom, State.CanvasZoom);
    int SourceWidth = std::min(MapWidth,
      std::max(1, int(OneXWidth / State.CanvasZoom + .5f)));
    int SourceHeight = std::min(MapHeight,
      std::max(1, int(OneXHeight / State.CanvasZoom + .5f)));
    if(State.CanvasZoom <= MinimumZoom + .001f)
    {
      SourceWidth = MapWidth;
      SourceHeight = MapHeight;
    }
    State.MapSource.w = SourceWidth;
    State.MapSource.h = SourceHeight;
    State.MapSource.x = Clamp(State.MapFocusX + State.CanvasPanX
                                - SourceWidth / 2,
                              MapLeft, MapLeft + MapWidth - SourceWidth);
    State.MapSource.y = Clamp(State.MapFocusY + State.CanvasPanY
                                - SourceHeight / 2,
                              MapTop, MapTop + MapHeight - SourceHeight);
    State.CanvasPanX = State.MapSource.x + SourceWidth / 2 - State.MapFocusX;
    State.CanvasPanY = State.MapSource.y + SourceHeight / 2 - State.MapFocusY;

    State.CanvasDestination = State.Game;
    const float SourceAspect = float(SourceWidth) / SourceHeight;
    if(DestAspect > SourceAspect)
    {
      const int Width = std::max(1, int(State.Game.h * SourceAspect));
      State.CanvasDestination.x += (State.Game.w - Width) / 2;
      State.CanvasDestination.w = Width;
    }
    else if(DestAspect < SourceAspect)
    {
      const int Height = std::max(1, int(State.Game.w / SourceAspect));
      State.CanvasDestination.y += (State.Game.h - Height) / 2;
      State.CanvasDestination.h = Height;
    }
    SDL_RenderCopy(Renderer, GameTexture, &State.MapSource,
                   &State.CanvasDestination);

  }

  void Draw(SDL_Renderer* Renderer)
  {
    // Normal message banners overlay the map so appearing and disappearing
    // never resizes the viewport or controller. Prompts retain their larger,
    // reserved panel and were already painted with the console background.
    if(State.Gameplay && !State.PromptActive)
      PaintGameplayLog(Renderer);
  }

  touchresult HandleFingerDown(float NormalizedX, float NormalizedY)
  {
    touchresult Result;
    CancelDirectionPress();
    const bool InterruptedMenuFling = State.MenuFlingTimer != 0;
    StopMenuFling();
    State.MenuStoppedFlingOnPress = false;
    const int X = Clamp(int(NormalizedX * State.Width), 0, State.Width - 1);
    const int Y = Clamp(int(NormalizedY * State.Height), 0, State.Height - 1);
    if(!State.Gameplay && State.MenuActive
       && State.ConditionMaxScrollY > 0
       && Contains(State.ConditionViewport, X, Y))
    {
      State.MenuPressActive = true;
      State.MenuPressConditions = true;
      State.MenuScrolling = false;
      State.MenuStoppedFlingOnPress = InterruptedMenuFling;
      State.MenuPressY = State.MenuLastY = Y;
      State.MenuLastMotionTime = SDL_GetTicks();
      ResetMenuMotionSamples(Y, State.MenuLastMotionTime);
      return Result;
    }
    if(!State.Gameplay && State.MenuActive && State.MenuMaxScrollY > 0
       && Contains(State.MenuViewport, X, Y))
    {
      State.MenuPressActive = true;
      State.MenuPressConditions = false;
      State.MenuScrolling = false;
      State.MenuStoppedFlingOnPress = InterruptedMenuFling;
      State.MenuPressY = State.MenuLastY = Y;
      State.MenuLastMotionTime = SDL_GetTicks();
      ResetMenuMotionSamples(Y, State.MenuLastMotionTime);
      return Result;
    }
    State.LogPressActive = ShowGameplayLog() && !State.PromptActive
                        && Contains(GameplayLogRect(), X, Y);
    if(State.LogPressActive)
    {
      State.LogPressStarted = SDL_GetTicks();
      return Result;
    }

    const bool CanPanCanvas = State.Gameplay && !State.MapScreen
                           && !State.PaperDollScreen && !State.PromptActive
                           && !State.ScreenTextActive
                           && Contains(State.CanvasDestination, X, Y);
    if(CanPanCanvas)
    {
      State.CanvasPressActive = true;
      State.CanvasPanning = false;
      State.CanvasPressStarted = SDL_GetTicks();
      State.CanvasPressX = State.CanvasLastX = X;
      State.CanvasPressY = State.CanvasLastY = Y;
      return Result;
    }

    const bool CanHoldDirection = State.Gameplay
                               && State.ControlMode == CONTROL_MOVEMENT
                               && !State.QuestionChoiceCount
                               && !State.PromptNumeric
                               && !State.PromptActive
                               && !State.ScreenTextActive;
    const int ControlIndex = CanHoldDirection
                           ? GridIndexAt(State.Controls, 3, 3, X, Y) : -1;
    // The middle button is WAIT during play and SELECT while choosing a map
    // square. Neither should auto-repeat.
    if(ControlIndex >= 0 && ControlIndex != 4)
    {
      State.DirectionPressKey = KEY_CONTROLLER_DIRECTION + ControlIndex + 1;
      State.DirectionPressActive = true;
      State.DirectionRepeatTimer = SDL_AddTimer(DIRECTION_REPEAT_DELAY_MS,
                                                 QueueDirectionRepeat, 0);
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = State.DirectionPressKey;
    }
    return Result;
  }

  bool HandleFingerMotion(float NormalizedX, float NormalizedY)
  {
    if(State.MenuPressActive)
    {
      const int Y = Clamp(int(NormalizedY * State.Height), 0, State.Height - 1);
      const Uint32 Now = SDL_GetTicks();
      AddMenuMotionSample(Y, Now);
      if(!State.MenuScrolling
         && std::abs(Y - State.MenuPressY) < CANVAS_PAN_SLOP)
        return false;
      State.MenuScrolling = true;
      const int DeltaY = Y - State.MenuLastY;
      State.MenuLastY = Y;
      State.MenuLastMotionTime = Now;
      int& ScrollY = State.MenuPressConditions
        ? State.ConditionScrollY : State.MenuScrollY;
      const int Maximum = State.MenuPressConditions
        ? State.ConditionMaxScrollY : State.MenuMaxScrollY;
      const int OldScrollY = ScrollY;
      ScrollY = Clamp(ScrollY - DeltaY, 0, Maximum);
      State.MenuScrollVelocity = MenuReleaseVelocity(Now);
      if(ScrollY == OldScrollY)
      {
        // Do not carry an outward drag into a fling when the user releases at
        // an edge. A reversal starts collecting clean velocity immediately.
        ResetMenuMotionSamples(Y, Now);
        return false;
      }
      ConsoleDirty = true;
      return true;
    }
    if(!State.CanvasPressActive)
      return false;

    const int X = Clamp(int(NormalizedX * State.Width), 0, State.Width - 1);
    const int Y = Clamp(int(NormalizedY * State.Height), 0, State.Height - 1);

    const int FromStartX = X - State.CanvasPressX;
    const int FromStartY = Y - State.CanvasPressY;
    if(!State.CanvasPanning)
    {
      const bool Held = SDL_GetTicks() - State.CanvasPressStarted
                      >= CANVAS_PAN_HOLD_MS;
      const bool Moved = FromStartX * FromStartX + FromStartY * FromStartY
                       >= CANVAS_PAN_SLOP * CANVAS_PAN_SLOP;
      if(!Held || !Moved)
        return false;
      State.CanvasPanning = true;
    }

    const int DeltaX = X - State.CanvasLastX;
    const int DeltaY = Y - State.CanvasLastY;
    State.CanvasLastX = X;
    State.CanvasLastY = Y;
    if(!DeltaX && !DeltaY)
      return false;

    // Drag the map directly beneath the finger. Source-pixel scaling keeps
    // the gesture consistent at every zoom and in both orientations.
    State.CanvasPanX -= int(float(DeltaX) * State.MapSource.w
                          / std::max(1, State.CanvasDestination.w));
    State.CanvasPanY -= int(float(DeltaY) * State.MapSource.h
                          / std::max(1, State.CanvasDestination.h));
    ConsoleDirty = true;
    return true;
  }

  bool HandlePinch(float NormalizedX, float NormalizedY, float DistanceDelta,
                   int FingerCount)
  {
    if(FingerCount < 2 || !State.Gameplay || State.MapScreen
       || State.PaperDollScreen || State.PromptActive
       || State.ScreenTextActive)
      return false;

    const int X = Clamp(int(NormalizedX * State.Width), 0, State.Width - 1);
    const int Y = Clamp(int(NormalizedY * State.Height), 0, State.Height - 1);
    if(!Contains(State.Game, X, Y))
      return false;

    CancelDirectionPress();
    State.LogPressActive = false;
    State.CanvasPressActive = false;
    State.CanvasPanning = false;
    State.SuppressedFingerUps = std::max(State.SuppressedFingerUps,
                                         FingerCount);

    const float OldZoom = State.CanvasZoom;
    const int MapWidth = std::max(1, State.GameWidth - 128);
    const int MapHeight = std::max(1, State.GameHeight - 184);
    const float DestAspect = float(State.Game.w) / std::max(1, State.Game.h);
    int OneXHeight = std::min(MapHeight,
      State.Width >= State.Height ? 240 : 320);
    int OneXWidth = std::max(1, int(OneXHeight * DestAspect));
    if(OneXWidth > MapWidth)
    {
      OneXWidth = MapWidth;
      OneXHeight = std::max(1, int(OneXWidth / DestAspect));
    }
    const float MinimumZoom = std::max(.05f, std::min(
      float(OneXWidth) / MapWidth, float(OneXHeight) / MapHeight));
    State.CanvasZoom = std::max(MinimumZoom, std::min(4.f,
      State.CanvasZoom * std::exp(DistanceDelta * 4.f)));
    if((OldZoom > MinimumZoom + .001f
        && State.CanvasZoom <= MinimumZoom + .001f)
       || (OldZoom < 3.999f && State.CanvasZoom >= 3.999f))
      Pulse(FEEDBACK_ZOOM_LIMIT);
    if(std::fabs(State.CanvasZoom - OldZoom) < 0.001f)
      return false;

    ConsoleDirty = true;
    return true;
  }

  touchresult HandleDirectionRepeat()
  {
    touchresult Result;
    if(State.DirectionPressActive)
    {
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = State.DirectionPressKey;
    }
    return Result;
  }

  void HandleLogTimeout()
  {
    if(!State.LogVisible)
      return;

    const Uint32 Now = SDL_GetTicks();
    // A stale timer event from a previous message must not hide a newer one.
    if(Sint32(State.LogHideDeadline - Now) > 0)
      return;
    // Keep the latest warning visible throughout continuous movement. New
    // messages still refresh the deadline, and release resumes the timeout.
    if(State.LogPressActive || State.DirectionPressActive)
    {
      State.LogHideDeadline = Now + 750;
      State.LogHideTimer = SDL_AddTimer(750, QueueLogHide, 0);
      return;
    }

    State.LogHideTimer = 0;
    State.LogVisible = false;
    ConsoleDirty = true;
  }

  void HandleMenuFling()
  {
    if(!State.MenuFlingTimer || !State.MenuActive)
    {
      StopMenuFling();
      return;
    }

    const Uint32 Now = SDL_GetTicks();
    const Uint32 Elapsed = std::max(Uint32(1),
      std::min(Uint32(64), Now - State.MenuFlingLastTime));
    State.MenuFlingLastTime = Now;
    int& ScrollY = State.MenuFlingConditions
      ? State.ConditionScrollY : State.MenuScrollY;
    const int Maximum = State.MenuFlingConditions
      ? State.ConditionMaxScrollY : State.MenuMaxScrollY;

    State.MenuFlingPosition += State.MenuScrollVelocity * float(Elapsed);
    const float ClampedPosition = std::max(0.f,
      std::min(State.MenuFlingPosition, float(Maximum)));
    const bool HitEdge = ClampedPosition != State.MenuFlingPosition;
    State.MenuFlingPosition = ClampedPosition;
    ScrollY = int(State.MenuFlingPosition + 0.5f);

    // Time-based exponential friction avoids a visible speed discontinuity
    // near the end of a fling and behaves consistently across frame rates.
    State.MenuScrollVelocity *= std::exp(-0.0045f * float(Elapsed));

    ConsoleDirty = true;
    const float StopVelocity = std::max(0.035f, State.Density * 0.02f);
    if(HitEdge || std::fabs(State.MenuScrollVelocity) < StopVelocity)
      StopMenuFling();
  }

  touchresult HandleFinger(float NormalizedX, float NormalizedY)
  {
    touchresult Result;
    if(State.SuppressedFingerUps > 0)
    {
      --State.SuppressedFingerUps;
      CancelDirectionPress();
      State.LogPressActive = false;
      return Result;
    }
    if(State.DirectionPressActive)
    {
      // Directional taps fire on finger-down for immediate feedback. Releasing
      // only stops the timer, avoiding an extra step at the end of a hold.
      CancelDirectionPress();
      return Result;
    }
    const int X = Clamp(int(NormalizedX * State.Width), 0, State.Width - 1);
    const int Y = Clamp(int(NormalizedY * State.Height), 0, State.Height - 1);

    if(State.MenuPressActive)
    {
      const bool SuppressTap = State.MenuScrolling
                            || State.MenuStoppedFlingOnPress;
      const bool ScrolledConditions = State.MenuPressConditions;
      State.MenuPressActive = false;
      State.MenuPressConditions = false;
      State.MenuScrolling = false;
      State.MenuStoppedFlingOnPress = false;
      if(SuppressTap)
      {
        const int Maximum = ScrolledConditions
          ? State.ConditionMaxScrollY : State.MenuMaxScrollY;
        const Uint32 ReleaseTime = SDL_GetTicks();
        State.MenuScrollVelocity = MenuReleaseVelocity(ReleaseTime);
        const float MinimumFlingVelocity = std::max(0.10f,
                                                    State.Density * 0.04f);
        if(Maximum > 0
           && std::fabs(State.MenuScrollVelocity) >= MinimumFlingVelocity)
          StartMenuFling(ScrolledConditions);
        ConsoleDirty = true;
        Result.Kind = touchresult::TOUCH_REDRAW;
        return Result;
      }
    }

    if(State.CanvasPressActive)
    {
      const int DeltaX = X - State.CanvasPressX;
      const int DeltaY = Y - State.CanvasPressY;
      const bool Held = SDL_GetTicks() - State.CanvasPressStarted
                      >= CANVAS_PAN_HOLD_MS;
      const bool Moved = DeltaX * DeltaX + DeltaY * DeltaY
                       >= CANVAS_PAN_SLOP * CANVAS_PAN_SLOP;
      const bool SuppressTap = State.CanvasPanning || Held || Moved;
      State.CanvasPressActive = false;
      State.CanvasPanning = false;
      if(SuppressTap || !Contains(State.CanvasDestination, X, Y))
        return Result;
    }

    if(State.LogPressActive)
    {
      const bool LongPress = SDL_GetTicks() - State.LogPressStarted >= 500;
      State.LogPressActive = false;
      if(LongPress && Contains(GameplayLogRect(), X, Y))
      {
        Result.Kind = touchresult::TOUCH_KEY;
        Result.KeyCode = 'M';
      }
      return Result;
    }

    if(State.PromptNumeric)
    {
      static const int NumericKeys[15] = {
        '1', '2', '3', '4', '5', '6',
        '7', '8', '9', KEY_BACK_SPACE, '0', KEY_ENTER,
        0, KEY_ESC, 0
      };
      const int Index = GridIndexAt(State.Controls, 3, 5, X, Y);
      if(Index >= 0 && NumericKeys[Index])
      {
        Result.Kind = touchresult::TOUCH_KEY;
        Result.KeyCode = NumericKeys[Index];
      }
      return Result;
    }

    if(State.PromptActive && State.PromptShowsInput
       && !Contains(State.Controls, X, Y) && !Contains(State.Toggle, X, Y))
    {
      SDL_StartTextInput();
      Result.Kind = touchresult::TOUCH_REDRAW;
      return Result;
    }

    if(State.ScreenTextActive)
    {
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = KEY_ENTER;
      return Result;
    }

    const bool MapCursor = (State.MapScreen || State.PositionPrompt)
                        && State.PromptActive
                        && State.PromptGameplay && !State.PromptShowsInput
                        && !State.QuestionChoiceCount;
    if(MapCursor && Contains(State.Toggle, X, Y))
    {
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = KEY_CONTROLLER_B;
      return Result;
    }

    if(ShowControlSectionTabs())
      for(int Index = 0; Index < CONTROL_SECTION_COUNT; ++Index)
        if(Contains(ControlSectionTab(Index), X, Y))
        {
          const Uint32 Now = SDL_GetTicks();
          if(Index == 0)
          {
            State.ControlMode = CONTROL_MOVEMENT;
            State.PinnedActionGroup = -1;
          }
          else if(ActionCountForGroup(Index - 1) > 0)
          {
            const bool DoubleTap = State.LastControlSectionTap == Index
              && Now - State.LastControlSectionTapTime
                 <= CONTROL_SECTION_DOUBLE_TAP_MS;
            if(State.PinnedActionGroup != Index - 1)
              State.PinnedActionGroup = DoubleTap ? Index - 1 : -1;
            SelectActionGroup(Index - 1);
          }
          else
            return Result;
          State.LastControlSectionTap = Index;
          State.LastControlSectionTapTime = Now;
          ConsoleDirty = true;
          Result.Kind = touchresult::TOUCH_REDRAW;
          return Result;
        }

    if(!State.Gameplay && State.MenuActive)
      for(int Index = 0; Index < State.MenuOptionCount; ++Index)
        if(Contains(State.MenuViewport, X, Y)
           && Contains(State.MenuRows[Index], X, Y))
        {
          Result.Kind = touchresult::TOUCH_KEY;
          const adaptiveui::MenuPresentationKind MenuKind =
            adaptiveui::GetHudModel().MenuKind;
          const bool PreviewChoice = MenuKind == adaptiveui::MENU_ITEM_GRID
                                  || MenuKind == adaptiveui::MENU_PICKUP_GRID;
          if(PreviewChoice)
            Result.KeyCode = KEY_MOBILE_MENU_PREVIEW_BASE + Index;
          else
            Result.KeyCode = KEY_MOBILE_MENU_SELECT_BASE + Index;
          return Result;
        }

    const adaptiveui::MenuPresentationKind MenuKind =
      adaptiveui::GetHudModel().MenuKind;
    if((MenuKind == adaptiveui::MENU_ITEM_GRID
        || MenuKind == adaptiveui::MENU_PICKUP_GRID)
       && State.MenuSelected >= 0
       && Contains(State.MenuConfirm, X, Y))
    {
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = KEY_MOBILE_MENU_SELECT_BASE + State.MenuSelected;
      return Result;
    }

    if(MenuKind == adaptiveui::MENU_PICKUP_GRID
       && State.MenuSelected >= 0)
      for(int Index = 0; Index < State.MenuItemActionCount; ++Index)
        if(Contains(State.MenuItemActions[Index], X, Y))
        {
          Result.Kind = touchresult::TOUCH_KEY;
          const int Action = State.MenuItemActionCodes[Index];
          Result.KeyCode = Action == adaptiveui::ITEM_ACTION_NONE
            ? KEY_MOBILE_MENU_EQUIP_BASE + State.MenuSelected
            : KEY_MOBILE_MENU_ACTION_BASE
              + (Action - 1) * KEY_MOBILE_MENU_ACTION_STRIDE
              + State.MenuSelected;
          return Result;
        }

    if(AdaptiveGridMenuPresentation()
       && Contains(State.MenuViewport, X, Y)
       && Contains(State.MenuBack, X, Y))
    {
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = KEY_CONTROLLER_B;
      return Result;
    }

    // The icon grid owns the touch rail while active. Misses must not fall
    // through to the legacy direction/menu-navigation keypad beneath it.
    if(AdaptiveGridMenuPresentation())
      return Result;

    if(Contains(State.Toggle, X, Y) && !State.QuestionChoiceCount
       && !State.Gameplay)
    {
      State.MenuDirectionMode = !State.MenuDirectionMode;
      ConsoleDirty = true;
      Result.Kind = touchresult::TOUCH_REDRAW;
      return Result;
    }

    const bool DynamicActionGrid = State.Gameplay
                                && State.ControlMode == CONTROL_ACTIONS
                                && !State.QuestionChoiceCount;
    const SDL_Rect ControlGrid = DynamicActionGrid
                               ? ActionGridRect() : State.Controls;
    const bool BinaryConfirmation = BinaryConfirmationActive();
    const int ControlIndex = BinaryConfirmation
      ? (Y >= BinaryConfirmationButton(0).y
         && Y < BinaryConfirmationButton(0).y
                + BinaryConfirmationButton(0).h
         && X >= State.Controls.x
         && X < State.Controls.x + State.Controls.w
           ? std::min(1, (X - State.Controls.x) * 2
                         / std::max(1, State.Controls.w)) : -1)
      : GridIndexAt(ControlGrid, 3, 3, X, Y);
    if(ControlIndex >= 0 && State.QuestionChoiceCount)
    {
      if(ControlIndex < State.QuestionChoiceCount)
      {
        Result.Kind = touchresult::TOUCH_KEY;
        Result.KeyCode = State.QuestionChoices[ControlIndex];
      }
      return Result;
    }

    if(ControlIndex >= 0 && !State.Gameplay)
    {
      const actiondef* Buttons = State.MenuDirectionMode
                               ? MenuDirections : MenuNavigation;
      const int KeyCode = Buttons[ControlIndex].KeyCode;
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = KeyCode;
      return Result;
    }

    const bool ShowActions = State.ControlMode == CONTROL_ACTIONS
                          && !State.PromptActive;
    if(ControlIndex >= 0 && !ShowActions)
    {
      if(MapCursor && ControlIndex == 4)
      {
        Result.Kind = touchresult::TOUCH_KEY;
        Result.KeyCode = KEY_CONTROLLER_A;
        return Result;
      }
      const int Column = ControlIndex % 3;
      const int Row = ControlIndex / 3;
      // IVAN's controller codes follow screen coordinates: 1-3 are the top
      // row (north) and 7-9 are the bottom row (south), unlike a numpad.
      const int Direction = Row * 3 + Column + 1;
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = KEY_CONTROLLER_DIRECTION + Direction;
      return Result;
    }

    if(ControlIndex >= 0 && ShowActions)
    {
      const int CurrentGroup = CurrentActionGroup();
      if(ControlIndex == ACTION_COUNT - 1
         && ActionPagesForGroup(CurrentGroup) > 1)
      {
        AdvanceActionPageWithinGroup();
        ConsoleDirty = true;
        Result.Kind = touchresult::TOUCH_REDRAW;
      }
      else
      {
        int PageIndices[ACTIONS_PER_PAGE];
        int Group = -1;
        const int PageActions = BuildActionPage(State.ActionPage,
                                                PageIndices, Group);
        if(ControlIndex >= PageActions)
          return Result;
        Result.Kind = touchresult::TOUCH_KEY;
        Result.KeyCode = State.ActionKeys[PageIndices[ControlIndex]];
        // An action between category taps breaks the double-tap gesture. This
        // prevents a quick return to the same category from pinning it by
        // accident after an ordinary single-tap action.
        State.LastControlSectionTap = -1;
        State.LastControlSectionTapTime = 0;
        // Commands commonly ask for a direction next (open, look, throw,
        // apply, and so on). Return to movement immediately so that the
        // follow-up direction is one tap away.
        if(State.PinnedActionGroup != Group)
          State.ControlMode = CONTROL_MOVEMENT;
        ConsoleDirty = true;
      }
      return Result;
    }

    const SDL_Rect& TouchGame = State.Gameplay
                              ? State.CanvasDestination : State.Game;
    if(Contains(TouchGame, X, Y))
    {
      Result.Kind = touchresult::TOUCH_MOUSE;
      if(State.Gameplay)
      {
        Result.MouseX = State.MapSource.x
          + (X - TouchGame.x) * State.MapSource.w / TouchGame.w;
        Result.MouseY = State.MapSource.y
          + (Y - TouchGame.y) * State.MapSource.h / TouchGame.h;
      }
      else
      {
        Result.MouseX = (X - State.Game.x) * State.GameWidth / State.Game.w;
        Result.MouseY = (Y - State.Game.y) * State.GameHeight / State.Game.h;
      }
    }
    return Result;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_harminoff_ivan_IvanActivity_nativeSetSafeInsets(JNIEnv* Env, jclass, jint Left,
                                                       jint Top, jint Right,
                                                       jint Bottom,
                                                       jintArray CutoutRects,
                                                       jfloat Density)
{
  const jsize ValueCount = CutoutRects ? Env->GetArrayLength(CutoutRects) : 0;
  jint* Values = ValueCount
    ? Env->GetIntArrayElements(CutoutRects, NULL) : NULL;
  mobileui::SetSafeInsets(Left, Top, Right, Bottom,
                          Values, ValueCount / 4, Density);
  if(Values)
    Env->ReleaseIntArrayElements(CutoutRects, Values, JNI_ABORT);
}
#endif
