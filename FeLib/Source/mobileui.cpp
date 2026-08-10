#include "mobileui.h"

#ifdef ANDROID
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <jni.h>
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
         MAX_QUESTION_CHOICES = 9, MAX_MENU_OPTIONS = 26,
         CONTROL_MOVEMENT = 0, CONTROL_ACTIONS = 1,
         MODE_CHOOSER_COUNT = mobileui::ACTION_GROUPS + 1 };

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
    float Density = 1.f;
    int ActionPage = 0;
    int ControlMode = CONTROL_MOVEMENT;
    bool ModeChooser = false;
    bool TogglePressActive = false;
    Uint32 TogglePressStarted = 0;
    bool DirectionPressActive = false;
    int DirectionPressKey = 0;
    SDL_TimerID DirectionRepeatTimer = 0;
    bool MenuDirectionMode = false;
    bool Gameplay = false;
    bool ControllerOnLeft = false;
    int QuestionChoiceCount = 0;
    int QuestionChoices[MAX_QUESTION_CHOICES] = { 0 };
    int MapFocusX = 352;
    int MapFocusY = 240;
    SDL_Rect Safe = { 0, 0, 1, 1 };
    SDL_Rect Header = { 0, 0, 1, 1 };
    SDL_Rect Game = { 0, 0, 1, 1 };
    SDL_Rect Stats = { 0, 0, 1, 1 };
    SDL_Rect Log = { 0, 0, 1, 1 };
    SDL_Rect Controls = { 0, 0, 1, 1 };
    SDL_Rect Toggle = { 0, 0, 1, 1 };
    SDL_Rect MapSource = { 0, 0, 1, 1 };
    std::string StatsLines[4];
    std::string LogMessage;
    bool PromptActive = false;
    bool PromptGameplay = false;
    bool PromptShowsInput = false;
    bool PromptNumeric = false;
    std::string PromptText;
    std::string PromptInput;
    bool ScreenTextActive = false;
    std::string ScreenText;
    bool MapScreen = false;
    SDL_Rect MapOverlaySource = { 0, 0, 0, 0 };
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
  } State;

  bool ConsoleDirty = true;

  enum { DIRECTION_REPEAT_DELAY_MS = 350,
         DIRECTION_REPEAT_INTERVAL_MS = 110 };

  Uint32 QueueDirectionRepeat(Uint32, void*)
  {
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = mobileui::DIRECTION_REPEAT_EVENT_CODE;
    SDL_PushEvent(&Event);
    return DIRECTION_REPEAT_INTERVAL_MS;
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

  void AdvanceControlSection()
  {
    if(State.ControlMode == CONTROL_MOVEMENT)
    {
      for(int Group = 0; Group < mobileui::ACTION_GROUPS; ++Group)
        if(FirstActionPageForGroup(Group) >= 0)
        {
          SelectActionGroup(Group);
          return;
        }
      return;
    }

    const int CurrentGroup = CurrentActionGroup();
    for(int Group = CurrentGroup + 1; Group < mobileui::ACTION_GROUPS; ++Group)
      if(FirstActionPageForGroup(Group) >= 0)
      {
        SelectActionGroup(Group);
        return;
      }
    State.ControlMode = CONTROL_MOVEMENT;
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

  SDL_Rect ModeChooserRow(int Index)
  {
    const int Inset = Clamp(int(5 * State.Density), 5, 14);
    return GridCell(State.Controls, 1, MODE_CHOOSER_COUNT, Index, Inset);
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
    static const unsigned char LeftParen[7] = {2,4,8,8,8,4,2};
    static const unsigned char RightParen[7] = {8,4,2,2,2,4,8};
    static const unsigned char Percent[7] = {17,2,4,8,16,17,0};
    static const unsigned char Blank[7] = {0,0,0,0,0,0,0};
    if(Character >= 'a' && Character <= 'z')
      Character = char(Character - 'a' + 'A');
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
     case '(': return LeftParen;
     case ')': return RightParen;
     case '%': return Percent;
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
    return Result;
  }

  void WrappedText(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                   const std::string& Value, bool CenterVertically = true,
                   int LineSpacingPercent = 0)
  {
    if(Value.empty())
      return;
    int Scale = 1;
    std::vector<std::string> Lines;
    int LinesFit = 1;
    for(int Candidate = 6; Candidate >= 1; --Candidate)
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

  void MenuRowText(SDL_Renderer* Renderer, const SDL_Rect& Row,
                   const std::string& Value, int Padding,
                   Uint8 R, Uint8 G, Uint8 B)
  {
    const int AvailableWidth = std::max(1, Row.w - Padding * 2);
    const int MaximumScale = Clamp(Row.h / 11, 1, 5);
    // Three-pixel glyphs remain comfortably readable on a phone.  Long
    // desktop list entries should wrap before they are reduced below that.
    const int MinimumScale = std::min(3, MaximumScale);
    int Scale = MaximumScale;
    int LinesFit = 1;
    std::vector<std::string> Lines;

    for(int Candidate = MaximumScale; Candidate >= MinimumScale; --Candidate)
    {
      const int Columns = std::max(1, AvailableWidth / (Candidate * 6));
      std::vector<std::string> CandidateLines = WrapText(Value, Columns);
      const int LineAdvance = Candidate * 8;
      const int CandidateLinesFit = Row.h >= Candidate * 7
        ? 1 + std::max(0, Row.h - Candidate * 7) / LineAdvance : 1;
      if((int)CandidateLines.size() <= CandidateLinesFit
         || Candidate == MinimumScale)
      {
        Scale = Candidate;
        LinesFit = CandidateLinesFit;
        Lines.swap(CandidateLines);
        break;
      }
    }

    if(Lines.empty())
      Lines.push_back("");
    if((int)Lines.size() > LinesFit)
    {
      Lines.resize(LinesFit);
      const int Columns = std::max(1, AvailableWidth / (Scale * 6));
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

    const int LineAdvance = Scale * 8;
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

  void PaintStats(SDL_Renderer* Renderer)
  {
    std::vector<std::string> Values;
    for(int Line = 0; Line < 4; ++Line)
    {
      const std::vector<std::string> LineValues = StatGroups(State.StatsLines[Line]);
      Values.insert(Values.end(), LineValues.begin(), LineValues.end());
    }
    if(Values.size() < 16)
      return;

    // Source order is HP/MANA/GOLD, ARM/LEG/DEX/AGI,
    // END/PER/INT/WIS, WILL/CHA/DAY/TIME/TURN.  Present it as semantic
    // vertical groups so related values remain together in either rotation.
    static const int Groups[5][4] = {
      { 0, 1, 2, -1 },       // resources: HP, mana, gold
      { 3, 4, 7, -1 },       // physical: arm, leg, endurance
      { 5, 6, 8, -1 },       // mobility/senses: dex, agility, perception
      { 9, 10, 11, 12 },     // mental/social: int, wis, will, charisma
      { 13, 14, 15, -1 }     // world: day, time, turn
    };
    static const int GroupSizes[5] = { 3, 3, 3, 4, 3 };
    const int Left = State.Stats.x + 8;
    const int Top = State.Stats.y + 5;
    const int Width = State.Stats.w - 16;
    const int Height = State.Stats.h - 10;
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
        CenterText(Renderer, Cell, Values[Groups[Column][Row]].c_str(), 6);
      }
    }
  }

  void PaintMobileMenu(SDL_Renderer* Renderer)
  {
    const bool MainMenu = MainMenuPresentation();
    const int Padding = Clamp(int(8 * State.Density), 10, 28);
    const int SubtitleHeight = MainMenu
      ? Clamp(int(24 * State.Density), 48, 86)
      : (State.MenuSubtitle.empty() ? 0
      : Clamp(int(18 * State.Density), 36, 64));
    const int FooterHeight = MainMenu ? 0
      : Clamp(int(16 * State.Density), 32, 54);
    SDL_Rect Content = { State.Game.x + Padding, State.Game.y + Padding,
                         State.Game.w - Padding * 2,
                         State.Game.h - Padding * 2 };

    if(SubtitleHeight)
    {
      SDL_Rect Subtitle = { Content.x, Content.y, Content.w, SubtitleHeight };
      CenterText(Renderer, Subtitle,
                 MainMenu ? "ITER VEHEMENS AD NECEM"
                          : State.MenuSubtitle.c_str(),
                 5, MainMenu ? 205 : 190,
                 MainMenu ? 48 : 180, MainMenu ? 42 : 155);
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
    const int MaximumRowHeight = MainMenu
      ? Clamp(int(31 * State.Density), 68, 112)
      : Clamp(int(36 * State.Density), 70, 130);
    const int RowsHeight = std::min(Content.h, Count * MaximumRowHeight);
    if(MainMenu)
      Content.y += std::max(0, (Content.h - RowsHeight) / 2);
    for(int Index = 0; Index < State.MenuOptionCount; ++Index)
    {
      const int Y0 = Content.y + RowsHeight * Index / Count;
      const int Y1 = Content.y + RowsHeight * (Index + 1) / Count;
      SDL_Rect Row = { Content.x, Y0 + 2, Content.w,
                       std::max(1, Y1 - Y0 - 4) };
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
        int Scale = Clamp(Row.h / 11, 1, 5);
        while(Scale > 1
              && TextWidth(Label.c_str(), Scale) > Row.w - Padding * 2)
          --Scale;
        std::string Visible = Label;
        const int Columns = std::max(1,
          (Row.w - Padding * 2) / (Scale * 6));
        if((int)Visible.size() > Columns)
        {
          Visible.resize(std::max(1, Columns - 3));
          Visible += "...";
        }
        Text(Renderer,
             Row.x + (Row.w - TextWidth(Visible.c_str(), Scale)) / 2,
             Row.y + (Row.h - Scale * 7) / 2,
             Visible.c_str(), Scale,
             Selected ? 255 : 240, Selected ? 224 : 230,
             Selected ? 205 : 202);
      }
      else
        MenuRowText(Renderer, Row, Label, Padding, 240, 230, 202);
    }
    for(int Index = State.MenuOptionCount; Index < MAX_MENU_OPTIONS; ++Index)
      State.MenuRows[Index] = { 0, 0, 0, 0 };

    if(!MainMenu)
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
      if(Key == 't')
        snprintf(Buffer, BufferSize, "NOTES");
      else if(Key == 'l')
        snprintf(Buffer, BufferSize, "LOOK");
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
    if(Key == KEY_ESC || Key == KEY_CONTROLLER_B)
      snprintf(Buffer, BufferSize, "BACK");
    else if(Key == KEY_ENTER || Key == KEY_CONTROLLER_A)
      snprintf(Buffer, BufferSize, "SELECT");
    else if(Key == KEY_BACK_SPACE)
      snprintf(Buffer, BufferSize, "DELETE");
    else if(Key >= 'a' && Key <= 'z')
      snprintf(Buffer, BufferSize, "LOW %c", char(Key - 'a' + 'A'));
    else if(Key >= 'A' && Key <= 'Z')
      snprintf(Buffer, BufferSize, "CAP %c", char(Key));
    else if(Key >= 0x20 && Key < 0x7F)
      snprintf(Buffer, BufferSize, "%c", char(std::toupper(Key)));
    else
      snprintf(Buffer, BufferSize, "OPTION");
  }

  void PaintConsole(SDL_Renderer* Renderer)
  {
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
    Color(Renderer, 0, 0, 0, 255);
    SDL_RenderClear(Renderer);
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
    Fill(Renderer, State.Safe, 4, 6, 10, 235);

    if(!State.Gameplay)
    {
      Frame(Renderer, State.Header);
      SDL_Rect GameFrame = { State.Game.x - 5, State.Game.y - 5,
                             State.Game.w + 10, State.Game.h + 10 };
      Frame(Renderer, GameFrame);
      CenterText(Renderer, State.Header,
                  State.MenuActive ? State.MenuTitle.c_str()
                                   : (State.ScreenTextActive ? "STORY"
                                      : (State.PromptActive
                                         && !State.PromptGameplay
                                           ? "CREATE CHARACTER" : "IVAN")),
                 7, MainMenuPresentation() ? 210
                                            : (State.MenuActive ? 240 : 210),
                 MainMenuPresentation() ? 55
                                            : (State.MenuActive ? 230 : 55),
                 MainMenuPresentation() ? 45
                                            : (State.MenuActive ? 202 : 45));
      if(State.PromptActive && !State.PromptGameplay)
      {
        const int Pad = Clamp(int(18 * State.Density), 24, 54);
        SDL_Rect Body = { State.Game.x + Pad, State.Game.y + Pad,
                          State.Game.w - Pad * 2, State.Game.h - Pad * 2 };
        const int TopicHeight = Clamp(Body.h * 28 / 100, 90, 240);
        SDL_Rect Topic = { Body.x, Body.y, Body.w, TopicHeight };
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
                                   Field.y + Field.h + Clamp(Body.h / 12, 20, 64)),
                          Body.w, FieldHeight };
        CenterText(Renderer, Hint, "TYPE YOUR NAME - PRESS ENTER", 4,
                   190, 180, 155);
        return;
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
      Frame(Renderer, State.Log);
      PaintStats(Renderer);
      std::string VisibleLog = State.LogMessage;
      if(State.PromptActive)
      {
        VisibleLog = State.PromptText;
        if(State.PromptShowsInput)
        {
          VisibleLog += "\n> ";
          VisibleLog += State.PromptInput;
          VisibleLog += "_";
        }
      }
      WrappedText(Renderer, { State.Log.x + 5, State.Log.y + 5,
                              State.Log.w - 10, State.Log.h - 10 },
                  VisibleLog);
    }

    Frame(Renderer, State.Controls);
    Frame(Renderer, State.Toggle);
    const bool ShowChoices = State.QuestionChoiceCount > 0;
    const bool ShowActions = State.Gameplay
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
    const bool MapCursor = State.MapScreen && State.PromptActive
                        && State.PromptGameplay && !State.PromptShowsInput
                        && !ShowChoices;
    const char* ToggleLabel = State.PromptNumeric ? PromptValueLabel.c_str()
      : (MapCursor ? "BACK"
      : (ShowChoices ? (State.MapScreen ? "MAP ACTIONS" : "CHOICES")
      : (State.ModeChooser ? "SELECT CONTROLS"
      : (!State.Gameplay ? (MainMenuPresentation() ? "MENU CONTROLS"
                           : (State.MenuDirectionMode ? "MENU" : "DIRECTIONS"))
                         : (ShowActions ? ActionGroupName(CurrentGroup)
                                         : "DIRECTIONS")))));
    CenterText(Renderer, State.Toggle,
               ToggleLabel, 7, 235, 218, 174);

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
        CenterText(Renderer, Button, NumericLabels[Index], 5);
      }
    }
    else if(State.ModeChooser && State.Gameplay && !ShowChoices)
    {
      static const char* ModeLabels[MODE_CHOOSER_COUNT] = {
        "DIRECTIONS", "CONTEXT", "ITEMS", "CHARACTER", "MOVE", "SYSTEM"
      };
      const int SelectedMode = State.ControlMode == CONTROL_MOVEMENT
        ? 0 : CurrentActionGroup() + 1;
      for(int Index = 0; Index < MODE_CHOOSER_COUNT; ++Index)
      {
        SDL_Rect Button = ModeChooserRow(Index);
        const bool Selected = Index == SelectedMode;
        Fill(Renderer, Button, Selected ? 35 : 19,
             Selected ? 71 : 23, Selected ? 48 : 28, 246);
        Outline(Renderer, Button, 156, 137, 100);
        CenterText(Renderer, Button, ModeLabels[Index], 7);
      }
    }
    else if(ShowChoices)
    {
      for(int Index = 0; Index < State.QuestionChoiceCount; ++Index)
      {
        SDL_Rect Button = GridCell(State.Controls, 3, 3, Index, 3);
        const bool Back = State.QuestionChoices[Index] == KEY_ESC
                       || State.QuestionChoices[Index] == KEY_CONTROLLER_B;
        Fill(Renderer, Button, Back ? 54 : 35, Back ? 31 : 71,
             Back ? 29 : 48, 238);
        Outline(Renderer, Button, 156, 137, 100);
        char Label[16];
        KeyLabel(State.QuestionChoices[Index], Label, sizeof(Label));
        CenterText(Renderer, Button, Label, 7);
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
        CenterText(Renderer, Button, Buttons[Index].Label, 6);
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
        CenterText(Renderer, Button,
                   MapCursor && Index == 4 ? "SELECT" : Directions[Index], 7);
      }
    }
    else
    {
      int PageIndices[ACTIONS_PER_PAGE];
      int Group = -1;
      const int PageActions = BuildActionPage(State.ActionPage,
                                              PageIndices, Group);
      const SDL_Rect Grid = ActionGridRect();

      for(int Index = 0; Index < PageActions; ++Index)
      {
        SDL_Rect Button = GridCell(Grid, ACTION_COLUMNS, ACTION_ROWS,
                                   Index, 3);
        Fill(Renderer, Button, 14, 18, 32, 238);
        Outline(Renderer, Button, 156, 137, 100);
        CenterText(Renderer, Button,
                   State.ActionLabels[PageIndices[Index]].c_str(),
                   4);
      }

      if(ActionPagesForGroup(Group) > 1)
      {
        SDL_Rect More = GridCell(Grid, ACTION_COLUMNS, ACTION_ROWS,
                                 ACTION_COUNT - 1, 3);
        Fill(Renderer, More, 45, 38, 24, 238);
        Outline(Renderer, More, 156, 137, 100);
        CenterText(Renderer, More, "MORE", 4);
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

  void SetSafeInsets(int Left, int Top, int Right, int Bottom, float Density)
  {
    State.Left = std::max(0, Left);
    State.Top = std::max(0, Top);
    State.Right = std::max(0, Right);
    State.Bottom = std::max(0, Bottom);
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

  void SetMapFocus(int X, int Y)
  {
    State.MapFocusX = X;
    State.MapFocusY = Y;
  }

  void SetStats(const char* Line1, const char* Line2,
                const char* Line3, const char* Line4)
  {
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
    const std::string Value = Message ? Message : "";
    if(State.LogMessage != Value)
    {
      State.LogMessage = Value;
      ConsoleDirty = true;
    }
  }

  void SetPrompt(const char* Prompt, const char* Input, bool Numeric)
  {
    const std::string NewPrompt = Prompt ? Prompt : "";
    const std::string NewInput = Input ? Input : "";
    const bool ShowsInput = Input != 0;
    const bool Changed = !State.PromptActive
      || State.PromptText != NewPrompt
      || State.PromptInput != NewInput
      || State.PromptShowsInput != ShowsInput
      || State.PromptNumeric != Numeric;
    if(!State.PromptActive)
      State.PromptGameplay = State.Gameplay;
    State.PromptActive = true;
    State.PromptShowsInput = ShowsInput;
    State.PromptNumeric = Numeric;
    State.PromptText = NewPrompt;
    State.PromptInput = NewInput;
    State.ModeChooser = false;
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

  void ClearPrompt()
  {
    if(!State.PromptActive)
      return;
    State.PromptActive = false;
    State.PromptGameplay = false;
    State.PromptShowsInput = false;
    State.PromptNumeric = false;
    State.PromptText.clear();
    State.PromptInput.clear();
    ConsoleDirty = true;
    SDL_Event Event;
    SDL_zero(Event);
    Event.type = SDL_USEREVENT;
    Event.user.code = REDRAW_EVENT_CODE;
    SDL_PushEvent(&Event);
  }

  void SetMapScreen(bool Active)
  {
    if(State.MapScreen == Active)
      return;
    State.MapScreen = Active;
    State.ModeChooser = false;
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
    SDL_Rect Bounds = { X, Y, std::max(0, Width), std::max(0, Height) };
    if(State.MapOverlaySource.x == Bounds.x
       && State.MapOverlaySource.y == Bounds.y
       && State.MapOverlaySource.w == Bounds.w
       && State.MapOverlaySource.h == Bounds.h)
      return;
    State.MapOverlaySource = Bounds;
    ConsoleDirty = true;
  }

  void SetScreenText(const char* Value)
  {
    State.ScreenTextActive = true;
    State.ScreenText = FormatScreenText(Value ? Value : "");
    State.MenuActive = false;
    ConsoleDirty = true;
  }

  void ClearScreenText()
  {
    if(!State.ScreenTextActive)
      return;
    State.ScreenTextActive = false;
    State.ScreenText.clear();
    ConsoleDirty = true;
  }

  void SetActions(const char* const* Labels, const int* Keys,
                  const int* Groups, int Count)
  {
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
    State.ActionPage = Clamp(State.ActionPage, 0, ActionPageCount() - 1);
    if(Changed)
      ConsoleDirty = true;
  }

  void SetQuestionChoices(const int* Keys, int Count)
  {
    Count = Clamp(Count, 0, MAX_QUESTION_CHOICES);
    bool Changed = State.QuestionChoiceCount != Count;
    for(int Index = 0; Index < Count; ++Index)
      if(State.QuestionChoices[Index] != Keys[Index])
      {
        State.QuestionChoices[Index] = Keys[Index];
        Changed = true;
      }
    State.QuestionChoiceCount = Count;
    if(Count)
      State.ModeChooser = false;
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
    State.MenuActive = true;
    State.ModeChooser = false;
    State.MenuTitle = Title ? Title : "MENU";
    State.MenuSubtitle = Subtitle ? Subtitle : "";
    State.MenuOptionCount = Clamp(Count, 0, MAX_MENU_OPTIONS);
    for(int Index = 0; Index < State.MenuOptionCount; ++Index)
      State.MenuOptions[Index] = Options && Options[Index] ? Options[Index] : "";
    State.MenuSelected = Clamp(Selected, -1,
                               std::max(-1, State.MenuOptionCount - 1));
    State.MenuPage = std::max(1, Page);
    State.MenuPages = std::max(1, Pages);
    ConsoleDirty = true;
  }

  void ClearMenu()
  {
    State.MenuActive = false;
    State.MenuOptionCount = 0;
    State.MenuSelected = -1;
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
      State.ControlMode = CONTROL_MOVEMENT;
      State.ModeChooser = false;
    }
    if(!State.Gameplay && PreviousGameplay)
    {
      State.MenuDirectionMode = false;
      State.ModeChooser = false;
    }
    const int Gap = Clamp(int(7 * State.Density), 8, 28);
    State.Safe = { State.Left + Gap, State.Top + Gap,
                   std::max(1, State.Width - State.Left - State.Right - Gap * 2),
                   std::max(1, State.Height - State.Top - State.Bottom - Gap * 2) };
    const int HeaderHeight = Clamp(int(18 * State.Density), 34, 72);

    if(State.Gameplay && State.Width < State.Height)
    {
      const int StatsHeight = Clamp(int(State.Safe.w * 75.f / 384.f), 150, 230);
      const int ToggleHeight = Clamp(int(28 * State.Density), 58, 100);
      const int LogHeight = Clamp(int(State.Safe.w * 106.f / 678.f), 110, 180);
      const int MaximumControls = Clamp(int(State.Safe.h * .31f), 460, 700);
      int ControlsSize = std::min(State.Safe.w, MaximumControls);
      int MapTop = State.Safe.y + StatsHeight + Gap;
      int ControlsBottom = State.Safe.y + State.Safe.h;
      int ControlsTop = ControlsBottom - ControlsSize;
      int ToggleTop = ControlsTop - Gap - ToggleHeight;
      int LogTop = ToggleTop - Gap - LogHeight;
      int MapHeight = std::max(1, LogTop - Gap - MapTop);

      // On short portrait displays, preserve the map and shrink the controls
      // before allowing regions to overlap.
      if(MapHeight < State.Safe.w * 3 / 4)
      {
        const int Reduction = State.Safe.w * 3 / 4 - MapHeight;
        ControlsSize = std::max(360, ControlsSize - Reduction);
        ControlsTop = ControlsBottom - ControlsSize;
        ToggleTop = ControlsTop - Gap - ToggleHeight;
        LogTop = ToggleTop - Gap - LogHeight;
        MapHeight = std::max(1, LogTop - Gap - MapTop);
      }

      State.Stats = { State.Safe.x, State.Safe.y, State.Safe.w, StatsHeight };
      State.Game = { State.Safe.x, MapTop, State.Safe.w, MapHeight };
      State.Log = { State.Safe.x, LogTop, State.Safe.w, LogHeight };
      State.Toggle = { State.Safe.x + (State.Safe.w - ControlsSize) / 2,
                       ToggleTop, ControlsSize, ToggleHeight };
      State.Controls = { State.Safe.x + (State.Safe.w - ControlsSize) / 2,
                         ControlsTop, ControlsSize, ControlsSize };
    }
    else if(State.Gameplay)
    {
      const int StatsHeight = Clamp(int(State.Safe.h * .20f), 150, 220);
      const int RailWidth = Clamp(int(State.Safe.w * .31f), 440, 720);
      const int ToggleHeight = Clamp(int(22 * State.Density), 48, 82);
      const int LogHeight = Clamp(int(State.Safe.h * .19f), 150, 200);
      const int ContentTop = State.Safe.y + StatsHeight + Gap;
      const int ContentBottom = State.Safe.y + State.Safe.h - LogHeight - Gap;
      const int RailX = State.ControllerOnLeft
        ? State.Safe.x : State.Safe.x + State.Safe.w - RailWidth;
      const int GameX = State.ControllerOnLeft
        ? RailX + RailWidth + Gap : State.Safe.x;
      const int GameRight = State.ControllerOnLeft
        ? State.Safe.x + State.Safe.w : RailX - Gap;
      const int ControlsSize = std::min(RailWidth,
        ContentBottom - ContentTop - ToggleHeight - Gap);
      State.Stats = { State.Safe.x, State.Safe.y, State.Safe.w, StatsHeight };
      State.Game = { GameX, ContentTop,
                     std::max(1, GameRight - GameX),
                     std::max(1, ContentBottom - ContentTop) };
      State.Toggle = { RailX, ContentTop, RailWidth, ToggleHeight };
      State.Controls = { RailX + (RailWidth - ControlsSize) / 2,
                         State.Toggle.y + State.Toggle.h + Gap,
                         ControlsSize, ControlsSize };
      State.Log = { State.Safe.x, ContentBottom + Gap,
                    State.Safe.w, LogHeight };
    }
    else if(State.ScreenTextActive
            || (State.PromptActive && !State.PromptGameplay))
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
        PaintMobileMenu(Renderer);
      }
      else if(!State.ScreenTextActive && !State.PromptActive)
        SDL_RenderCopy(Renderer, GameTexture, 0, &State.Game);
      return;
    }

    if(State.MapScreen)
    {
      State.MapSource = State.MapOverlaySource;
      if(State.MapSource.w <= 0 || State.MapSource.h <= 0)
        State.MapSource = { 0, 0, State.GameWidth, State.GameHeight };
      else
      {
        const int Padding = std::max(4,
          std::min(State.MapSource.w, State.MapSource.h) / 20);
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
      return;
    }

    const int MapLeft = 16;
    const int MapTop = 32;
    const int MapWidth = std::max(1, State.GameWidth - 128);
    const int MapHeight = std::max(1, State.GameHeight - 184);
    const float DestAspect = float(State.Game.w) / float(State.Game.h);
    // Half the old source dimensions gives the gameplay viewport a true 2x
    // zoom while retaining the player-centred camera and aspect-ratio crop.
    const int MaximumSourceHeight = State.Width >= State.Height ? 120 : 160;
    int SourceHeight = std::min(MapHeight, MaximumSourceHeight);
    int SourceWidth = int(SourceHeight * DestAspect);
    if(SourceWidth > MapWidth)
    {
      SourceWidth = MapWidth;
      SourceHeight = std::max(1, int(SourceWidth / DestAspect));
    }
    State.MapSource.w = SourceWidth;
    State.MapSource.h = SourceHeight;
    State.MapSource.x = Clamp(State.MapFocusX - SourceWidth / 2,
                              MapLeft, MapLeft + MapWidth - SourceWidth);
    State.MapSource.y = Clamp(State.MapFocusY - SourceHeight / 2,
                              MapTop, MapTop + MapHeight - SourceHeight);
    SDL_RenderCopy(Renderer, GameTexture, &State.MapSource, &State.Game);

  }

  void Draw(SDL_Renderer* Renderer)
  {
    (void)Renderer;
  }

  touchresult HandleFingerDown(float NormalizedX, float NormalizedY)
  {
    touchresult Result;
    CancelDirectionPress();
    const int X = Clamp(int(NormalizedX * State.Width), 0, State.Width - 1);
    const int Y = Clamp(int(NormalizedY * State.Height), 0, State.Height - 1);
    State.TogglePressActive = Contains(State.Toggle, X, Y)
                           && !State.QuestionChoiceCount
                           && !State.PromptActive;
    State.TogglePressStarted = SDL_GetTicks();

    const bool CanHoldDirection = State.Gameplay
                               && State.ControlMode == CONTROL_MOVEMENT
                               && !State.ModeChooser
                               && !State.QuestionChoiceCount
                               && !State.PromptNumeric
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

  touchresult HandleFinger(float NormalizedX, float NormalizedY)
  {
    touchresult Result;
    if(State.DirectionPressActive)
    {
      // Directional taps fire on finger-down for immediate feedback. Releasing
      // only stops the timer, avoiding an extra step at the end of a hold.
      CancelDirectionPress();
      State.TogglePressActive = false;
      return Result;
    }
    const int X = Clamp(int(NormalizedX * State.Width), 0, State.Width - 1);
    const int Y = Clamp(int(NormalizedY * State.Height), 0, State.Height - 1);

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

    if(State.PromptActive && !State.PromptGameplay)
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

    if(State.ModeChooser)
    {
      for(int Index = 0; Index < MODE_CHOOSER_COUNT; ++Index)
        if(Contains(ModeChooserRow(Index), X, Y))
        {
          if(Index == 0)
            State.ControlMode = CONTROL_MOVEMENT;
          else
            SelectActionGroup(Index - 1);
          State.ModeChooser = false;
          State.TogglePressActive = false;
          ConsoleDirty = true;
          Result.Kind = touchresult::TOUCH_REDRAW;
          return Result;
        }
      State.ModeChooser = false;
      State.TogglePressActive = false;
      ConsoleDirty = true;
      Result.Kind = touchresult::TOUCH_REDRAW;
      return Result;
    }

    const bool MapCursor = State.MapScreen && State.PromptActive
                        && State.PromptGameplay && !State.PromptShowsInput
                        && !State.QuestionChoiceCount;
    if(MapCursor && Contains(State.Toggle, X, Y))
    {
      State.TogglePressActive = false;
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = KEY_CONTROLLER_B;
      return Result;
    }

    if(!State.Gameplay && State.MenuActive)
      for(int Index = 0; Index < State.MenuOptionCount; ++Index)
        if(Contains(State.MenuRows[Index], X, Y))
        {
          Result.Kind = touchresult::TOUCH_KEY;
          Result.KeyCode = KEY_MOBILE_MENU_SELECT_BASE + Index;
          return Result;
        }

    if(Contains(State.Toggle, X, Y) && !State.QuestionChoiceCount)
    {
      const bool LongPress = State.TogglePressActive
                          && SDL_GetTicks() - State.TogglePressStarted >= 500;
      State.TogglePressActive = false;
      if(State.Gameplay && LongPress)
        State.ModeChooser = true;
      else if(State.Gameplay)
        AdvanceControlSection();
      else
        State.MenuDirectionMode = !State.MenuDirectionMode;
      ConsoleDirty = true;
      Result.Kind = touchresult::TOUCH_REDRAW;
      return Result;
    }

    State.TogglePressActive = false;

    const bool DynamicActionGrid = State.Gameplay
                                && State.ControlMode == CONTROL_ACTIONS
                                && !State.QuestionChoiceCount;
    const SDL_Rect ControlGrid = DynamicActionGrid
                               ? ActionGridRect() : State.Controls;
    const int ControlIndex = GridIndexAt(ControlGrid, 3, 3, X, Y);
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
      Result.Kind = touchresult::TOUCH_KEY;
      Result.KeyCode = Buttons[ControlIndex].KeyCode;
      return Result;
    }

    const bool ShowActions = State.ControlMode == CONTROL_ACTIONS;
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
        // Commands commonly ask for a direction next (open, look, throw,
        // apply, and so on). Return to movement immediately so that the
        // follow-up direction is one tap away.
        State.ControlMode = CONTROL_MOVEMENT;
        ConsoleDirty = true;
      }
      return Result;
    }

    if(Contains(State.Game, X, Y))
    {
      Result.Kind = touchresult::TOUCH_MOUSE;
      if(State.Gameplay)
      {
        Result.MouseX = State.MapSource.x
          + (X - State.Game.x) * State.MapSource.w / State.Game.w;
        Result.MouseY = State.MapSource.y
          + (Y - State.Game.y) * State.MapSource.h / State.Game.h;
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
Java_io_github_harminoff_ivan_IvanActivity_nativeSetSafeInsets(JNIEnv*, jclass, jint Left,
                                                       jint Top, jint Right,
                                                       jint Bottom, jfloat Density)
{
  mobileui::SetSafeInsets(Left, Top, Right, Bottom, Density);
}
#endif
