#include "adaptiveui.h"

#ifdef USE_SDL
#include "SDL.h"
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

#include "felibdef.h"

namespace
{
  adaptiveui::HudModel Hud;
  adaptiveui::Layout CurrentLayout;
  adaptiveui::PlatformMode CurrentPlatform = adaptiveui::Desktop;
  bool Dirty = true;
  int DesktopMapZoom = 2;
  int DesktopMapNoteScroll = 0;
  int DesktopSelectedMapNote = -1;
  int DesktopHoverMapNote = -1;
  int DesktopHoverMapAction = -1;

#ifdef USE_SDL
  SDL_Renderer* SnapshotRenderer = 0;
  SDL_Texture* GameplaySnapshot = 0;
  SDL_Texture* CurrentMenuTexture = 0;
  int SnapshotWidth = 0;
  int SnapshotHeight = 0;
  bool SnapshotValid = false;
#endif

  const int DesktopTileSize = 16;
  const int DesktopSidebarFontScale = 2;
  const int DesktopSidebarLineHeight = 16;
  const int DesktopSidebarRowHeight = 54;
  const int DesktopActionFooterReserve = 38;
  const int DesktopEquipmentMinimumRowHeight = 48;

  int Clamp(int Value, int Low, int High)
  {
    return std::max(Low, std::min(Value, High));
  }

  void RebuildMenuDisplayOrder()
  {
    Hud.MenuDisplayOrder.clear();
    for(size_t Index = 0; Index < Hud.MenuOptions.size(); ++Index)
      Hud.MenuDisplayOrder.push_back(int(Index));

    if(!Hud.EquipmentComparisonActive || Hud.MenuDisplayOrder.empty())
      return;

    int EquippedIndex = -1;
    for(size_t Index = 0; Index < Hud.MenuItemMetrics.size(); ++Index)
    {
      const adaptiveui::ItemMetrics& Candidate = Hud.MenuItemMetrics[Index];
      if((Hud.EquippedItemMetrics.Present
          && Hud.EquippedItemMetrics.ItemId
          && Candidate.ItemId == Hud.EquippedItemMetrics.ItemId)
         || (!Hud.EquippedItemMetrics.Present && !Candidate.Present))
      {
        EquippedIndex = int(Index);
        break;
      }
    }
    if(EquippedIndex > 0)
    {
      Hud.MenuDisplayOrder.erase(Hud.MenuDisplayOrder.begin()
                                 + EquippedIndex);
      Hud.MenuDisplayOrder.insert(Hud.MenuDisplayOrder.begin(),
                                  EquippedIndex);
    }
  }

  int DisplayPositionForMenuIndex(int MenuIndex)
  {
    const std::vector<int>::const_iterator Found = std::find(
      Hud.MenuDisplayOrder.begin(), Hud.MenuDisplayOrder.end(), MenuIndex);
    return Found == Hud.MenuDisplayOrder.end()
      ? MenuIndex : int(Found - Hud.MenuDisplayOrder.begin());
  }

  int MenuIndexForDisplayPosition(int DisplayPosition)
  {
    return DisplayPosition >= 0
        && DisplayPosition < int(Hud.MenuDisplayOrder.size())
      ? Hud.MenuDisplayOrder[DisplayPosition] : DisplayPosition;
  }

  std::string Safe(const char* Value)
  {
    return Value ? Value : "";
  }

  bool IsEquipmentMenu()
  {
    return Hud.MenuActive && Hud.MenuTitle == "Equipment";
  }

  bool PromptShowsLogContext()
  {
    if(!Hud.PromptActive || Hud.PromptShowsInput || Hud.PromptNumeric
       || Hud.PositionPrompt || !Hud.PromptDetail.empty()
       || Hud.LogMessage.empty())
      return false;
    std::string Prompt = Hud.Prompt;
    std::transform(Prompt.begin(), Prompt.end(), Prompt.begin(),
      [](unsigned char Character) { return char(std::tolower(Character)); });
    return Prompt.find("continue anyway") != std::string::npos
        || Prompt.find("still continue") != std::string::npos
        || Prompt == "continue? [y/n]";
  }

  struct EquipmentRow
  {
    std::string Slot;
    std::string Item;
  };

  EquipmentRow ParseEquipmentRow(const std::string& Value)
  {
    EquipmentRow Result;
    const size_t Colon = Value.find(':');
    Result.Slot = Colon == std::string::npos
      ? Value : Value.substr(0, Colon);
    size_t ItemStart = Colon == std::string::npos ? Value.size() : Colon + 1;
    while(ItemStart < Value.size()
          && std::isspace(static_cast<unsigned char>(Value[ItemStart])))
      ++ItemStart;
    Result.Item = ItemStart < Value.size() ? Value.substr(ItemStart) : "-";
    const size_t Metadata = Result.Item.find(" [");
    if(Metadata != std::string::npos)
      Result.Item.erase(Metadata);
    if(Result.Slot == "right hand wielded")
      Result.Slot = "right hand";
    else if(Result.Slot == "left hand wielded")
      Result.Slot = "left hand";
    return Result;
  }

  bool Contains(const SDL_Rect& Rect, int X, int Y)
  {
    return X >= Rect.x && Y >= Rect.y
        && X < Rect.x + Rect.w && Y < Rect.y + Rect.h;
  }

  SDL_Rect FitRect(const SDL_Rect& Area, int Width, int Height)
  {
    const float Aspect = float(std::max(1, Width))
                       / float(std::max(1, Height));
    int ResultWidth = Area.w;
    int ResultHeight = int(ResultWidth / Aspect);
    if(ResultHeight > Area.h)
    {
      ResultHeight = Area.h;
      ResultWidth = int(ResultHeight * Aspect);
    }
    ResultWidth = std::max(1, ResultWidth);
    ResultHeight = std::max(1, ResultHeight);
    return { Area.x + (Area.w - ResultWidth) / 2,
             Area.y + (Area.h - ResultHeight) / 2,
             ResultWidth, ResultHeight };
  }

  bool HasGameplayContext()
  {
    if(!Hud.Actions.empty() || !Hud.Location.empty() || !Hud.Clock.empty())
      return true;
    for(int Index = 0; Index < 4; ++Index)
      if(!Hud.Stats[Index].empty())
        return true;
    return false;
  }

  SDL_Rect ClipSource(SDL_Rect Source, int Width, int Height)
  {
    Source.x = Clamp(Source.x, 0, std::max(0, Width - 1));
    Source.y = Clamp(Source.y, 0, std::max(0, Height - 1));
    Source.w = Clamp(Source.w, 1, std::max(1, Width - Source.x));
    Source.h = Clamp(Source.h, 1, std::max(1, Height - Source.y));
    return Source;
  }

  SDL_Rect GameplaySource(int Width, int Height)
  {
    const int Columns = std::max(1, Width / DesktopTileSize - 8);
    const int Rows = std::max(1, Height / DesktopTileSize - 11);
    return ClipSource({ 16, 32, Columns * DesktopTileSize,
                        Rows * DesktopTileSize }, Width, Height);
  }

  SDL_Rect EquipmentSource(int Width, int Height)
  {
    // Matches humanoid::DrawSilhouette and game::PrepareStretchRegionsLazy:
    // the 48x64 body plus the 23px equipment slots around it.
    return ClipSource({ Width - 48 - 39 - 16, 53 - 24, 96, 112 },
                      Width, Height);
  }

  bool CanZoomGameplay()
  {
    return CurrentPlatform == adaptiveui::Desktop && HasGameplayContext()
        && !Hud.MenuActive && !Hud.ScreenTextActive && !Hud.PromptActive
        && !Hud.MapScreen && !Hud.PaperDollScreen;
  }

  SDL_Rect ZoomedGameplaySource(int Width, int Height)
  {
    const SDL_Rect Base = GameplaySource(Width, Height);
    const int SourceWidth = std::max(1, Base.w / DesktopMapZoom);
    const int SourceHeight = std::max(1, Base.h / DesktopMapZoom);
    return {
      Clamp(Hud.MapFocusX - SourceWidth / 2,
            Base.x, Base.x + Base.w - SourceWidth),
      Clamp(Hud.MapFocusY - SourceHeight / 2,
            Base.y, Base.y + Base.h - SourceHeight),
      SourceWidth, SourceHeight
    };
  }

  SDL_Rect ActiveCanvasSource(int Width, int Height)
  {
    if(Hud.PaperDollScreen && Hud.PaperDollSource.w > 0
       && Hud.PaperDollSource.h > 0)
      return ClipSource(Hud.PaperDollSource, Width, Height);
    if(Hud.MapScreen && Hud.HasMapSource)
    {
      const int Padding = 2;
      return ClipSource({ Hud.MapSource.x - Padding,
                          Hud.MapSource.y - Padding,
                          Hud.MapSource.w + Padding * 2,
                          Hud.MapSource.h + Padding * 2 }, Width, Height);
    }
    if(HasGameplayContext() && !Hud.MenuActive && !Hud.ScreenTextActive)
      return ZoomedGameplaySource(Width, Height);
    return { 0, 0, std::max(1, Width), std::max(1, Height) };
  }

#ifdef USE_SDL
  void Color(SDL_Renderer* Renderer, unsigned char Red,
             unsigned char Green, unsigned char Blue,
             unsigned char Alpha = 255)
  {
    SDL_SetRenderDrawColor(Renderer, Red, Green, Blue, Alpha);
  }

  void Fill(SDL_Renderer* Renderer, const SDL_Rect& Rect,
            unsigned char Red, unsigned char Green, unsigned char Blue,
            unsigned char Alpha = 255)
  {
    Color(Renderer, Red, Green, Blue, Alpha);
    SDL_RenderFillRect(Renderer, &Rect);
  }

  void Outline(SDL_Renderer* Renderer, const SDL_Rect& Rect,
               unsigned char Red, unsigned char Green, unsigned char Blue)
  {
    Color(Renderer, Red, Green, Blue);
    SDL_RenderDrawRect(Renderer, &Rect);
  }

  void Frame(SDL_Renderer* Renderer, const SDL_Rect& Rect)
  {
    Fill(Renderer, Rect, 16, 15, 14, 255);
    Outline(Renderer, Rect, 104, 82, 49);
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
    static const unsigned char Blank[7] = {0,0,0,0,0,0,0};
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

  int TextWidth(const std::string& Value, int Scale)
  {
    return Value.empty() ? 0 : int(Value.size()) * Scale * 6 - Scale;
  }

  int CenteredTextX(int X, int Width, const std::string& Value, int Scale)
  {
    int InkLeft = 0;
    int InkRight = 0;
    bool HasInk = false;
    for(size_t Index = 0; Index < Value.size(); ++Index)
    {
      const unsigned char* Rows = Glyph(Value[Index]);
      for(int Row = 0; Row < 7; ++Row)
        for(int Column = 0; Column < 5; ++Column)
          if(Rows[Row] & (1 << (4 - Column)))
          {
            const int Pixel = int(Index) * 6 + Column;
            if(!HasInk)
            {
              InkLeft = Pixel;
              InkRight = Pixel + 1;
              HasInk = true;
            }
            else
            {
              InkLeft = std::min(InkLeft, Pixel);
              InkRight = std::max(InkRight, Pixel + 1);
            }
          }
    }
    if(!HasInk)
      return X + Width / 2;
    const int InkWidth = (InkRight - InkLeft) * Scale;
    return X + (Width - InkWidth) / 2 - InkLeft * Scale;
  }

  std::string Elide(const std::string& Value, int MaximumCharacters)
  {
    MaximumCharacters = std::max(1, MaximumCharacters);
    if(int(Value.size()) <= MaximumCharacters)
      return Value;
    if(MaximumCharacters <= 3)
      return Value.substr(0, MaximumCharacters);
    return Value.substr(0, MaximumCharacters - 3) + "...";
  }

  void Text(SDL_Renderer* Renderer, int X, int Y, const std::string& Value,
            int Scale, unsigned char Red = 240, unsigned char Green = 230,
            unsigned char Blue = 202)
  {
    Color(Renderer, Red, Green, Blue);
    SDL_Rect Pixels[256];
    int PixelCount = 0;
    for(size_t Index = 0; Index < Value.size(); ++Index, X += Scale * 6)
    {
      const unsigned char* Rows = Glyph(Value[Index]);
      for(int Row = 0; Row < 7; ++Row)
        for(int Column = 0; Column < 5; ++Column)
          if(Rows[Row] & (1 << (4 - Column)))
          {
            if(PixelCount == 256)
            {
              SDL_RenderFillRects(Renderer, Pixels, PixelCount);
              PixelCount = 0;
            }
            Pixels[PixelCount++] = { X + Column * Scale,
                                     Y + Row * Scale, Scale, Scale };
          }
    }
    if(PixelCount)
      SDL_RenderFillRects(Renderer, Pixels, PixelCount);
  }

  const unsigned char* DashboardHeaderGlyph(char Character)
  {
    static const unsigned char Letters[26][9] = {
      {28,34,65,65,127,65,65,65,65},
      {126,65,65,65,126,65,65,65,126},
      {62,65,64,64,64,64,64,65,62},
      {124,66,65,65,65,65,65,66,124},
      {127,64,64,64,126,64,64,64,127},
      {127,64,64,64,126,64,64,64,64},
      {62,65,64,64,79,65,65,65,63},
      {65,65,65,65,127,65,65,65,65},
      {127,8,8,8,8,8,8,8,127},
      {15,2,2,2,2,2,2,66,60},
      {65,66,68,72,112,72,68,66,65},
      {64,64,64,64,64,64,64,64,127},
      {65,99,85,73,65,65,65,65,65},
      {65,97,81,73,69,67,65,65,65},
      {62,65,65,65,65,65,65,65,62},
      {126,65,65,65,126,64,64,64,64},
      {62,65,65,65,65,73,69,66,61},
      {126,65,65,65,126,72,68,66,65},
      {63,64,64,64,62,1,1,1,126},
      {127,8,8,8,8,8,8,8,8},
      {65,65,65,65,65,65,65,65,62},
      {65,65,65,65,65,34,34,20,8},
      {65,65,65,65,65,73,85,99,65},
      {65,34,20,8,8,8,20,34,65},
      {65,34,20,8,8,8,8,8,8},
      {127,1,2,4,8,16,32,64,127}
    };
    static const unsigned char Blank[9] = {0,0,0,0,0,0,0,0,0};
    static const unsigned char Period[9] = {0,0,0,0,0,0,0,24,24};
    if(Character >= 'a' && Character <= 'z')
      Character = char(Character - 'a' + 'A');
    if(Character >= 'A' && Character <= 'Z')
      return Letters[Character - 'A'];
    if(Character == '.')
      return Period;
    return Blank;
  }

  int DashboardHeaderTextWidth(const std::string& Value)
  {
    return Value.empty() ? 0 : int(Value.size()) * 8 - 1;
  }

  void DashboardHeaderText(SDL_Renderer* Renderer, int X, int Y,
                           const std::string& Value,
                           unsigned char Red, unsigned char Green,
                           unsigned char Blue)
  {
    Color(Renderer, Red, Green, Blue);
    SDL_Rect Pixels[256];
    int PixelCount = 0;
    for(size_t Index = 0; Index < Value.size(); ++Index)
    {
      const unsigned char* Rows = DashboardHeaderGlyph(Value[Index]);
      for(int Row = 0; Row < 9; ++Row)
        for(int Column = 0; Column < 7; ++Column)
          if(Rows[Row] & (1 << (6 - Column)))
          {
            if(PixelCount == 256)
            {
              SDL_RenderFillRects(Renderer, Pixels, PixelCount);
              PixelCount = 0;
            }
            Pixels[PixelCount++] = {
              X + int(Index) * 8 + Column, Y + Row, 1, 1
            };
          }
    }
    if(PixelCount)
      SDL_RenderFillRects(Renderer, Pixels, PixelCount);
  }

  std::vector<std::string> Wrap(const std::string& Value, int Columns)
  {
    std::vector<std::string> Lines;
    std::string Line;
    size_t Position = 0;
    Columns = std::max(1, Columns);
    while(Position < Value.size())
    {
      if(Value[Position] == '\n')
      {
        Lines.push_back(Line);
        Line.clear();
        ++Position;
        continue;
      }
      while(Position < Value.size()
            && std::isspace((unsigned char)Value[Position])
            && Value[Position] != '\n')
        ++Position;
      if(Position >= Value.size())
        break;
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

  std::string ReflowScreenText(const std::string& Value)
  {
    // Legacy text screens use single newlines to fit the old fixed-width
    // framebuffer. Preserve blank lines as paragraph breaks, but let desktop
    // presentation wrap prose to its actual output-space width.
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

  void Wrapped(SDL_Renderer* Renderer, const SDL_Rect& Rect,
               const std::string& Value, int MaximumScale = 4,
               unsigned char Red = 240, unsigned char Green = 230,
               unsigned char Blue = 202)
  {
    if(Value.empty())
      return;
    int Scale = 1;
    std::vector<std::string> Lines;
    for(int Candidate = MaximumScale; Candidate >= 1; --Candidate)
    {
      const int Columns = std::max(1, (Rect.w - 20) / (Candidate * 6));
      std::vector<std::string> CandidateLines = Wrap(Value, Columns);
      if((int)CandidateLines.size() * Candidate * 8 <= Rect.h - 12
         || Candidate == 1)
      {
        Scale = Candidate;
        Lines.swap(CandidateLines);
        break;
      }
    }
    const int Advance = Scale * 8;
    const int TotalHeight = std::max(1, int(Lines.size()) * Advance - 1);
    int Y = Rect.y + std::max(4, (Rect.h - TotalHeight) / 2);
    for(size_t Index = 0; Index < Lines.size(); ++Index, Y += Advance)
      Text(Renderer, Rect.x + 8, Y, Lines[Index], Scale, Red, Green, Blue);
  }

  void TopWrapped(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                  const std::string& Value, int MaximumScale,
                  unsigned char Red, unsigned char Green,
                  unsigned char Blue)
  {
    if(Value.empty())
      return;
    int Scale = 1;
    int Advance = 10;
    std::vector<std::string> Lines;
    for(int Candidate = MaximumScale; Candidate >= 1; --Candidate)
    {
      const int CandidateAdvance = Candidate * 10;
      const int Columns = std::max(1, Rect.w / (Candidate * 6));
      std::vector<std::string> CandidateLines = Wrap(Value, Columns);
      if(int(CandidateLines.size()) * CandidateAdvance <= Rect.h
         || Candidate == 1)
      {
        Scale = Candidate;
        Advance = CandidateAdvance;
        Lines.swap(CandidateLines);
        break;
      }
    }
    int Y = Rect.y;
    for(size_t Index = 0; Index < Lines.size() && Y + Scale * 7 <= Rect.y + Rect.h;
        ++Index, Y += Advance)
      Text(Renderer, Rect.x, Y, Lines[Index], Scale, Red, Green, Blue);
  }

  void Centered(SDL_Renderer* Renderer, const SDL_Rect& Rect,
                const std::string& Value, int MaximumScale = 5,
                unsigned char Red = 240, unsigned char Green = 230,
                unsigned char Blue = 202)
  {
    const int Length = std::max(1, int(Value.size()));
    const int Scale = Clamp(std::min(Rect.h / 11,
                                     Rect.w / (Length * 6 + 2)),
                            1, MaximumScale);
    Text(Renderer, Rect.x + (Rect.w - TextWidth(Value, Scale)) / 2,
         Rect.y + (Rect.h - Scale * 7) / 2, Value, Scale,
         Red, Green, Blue);
  }

  std::string ShortcutLabel(int Key)
  {
    if(Key == -1) return "NONE";
    if(Key == KEY_CONTROLLER_A) return "ENTER";
    if(Key == KEY_CONTROLLER_B) return "ESC";
    if(Key == KEY_CONTROLLER_X) return "X";
    if(Key == KEY_CONTROLLER_Y) return "Y";
    if(Key == KEY_ENTER) return "ENTER";
    if(Key == KEY_ESC) return "ESC";
    if(Key == KEY_PAGE_UP) return "PGUP";
    if(Key == KEY_PAGE_DOWN) return "PGDN";
    if(Key == KEY_UP) return "UP";
    if(Key == KEY_DOWN) return "DOWN";
    if(Key == KEY_LEFT) return "LEFT";
    if(Key == KEY_RIGHT) return "RIGHT";
    if(Key >= 0x20 && Key < 0x7f)
    {
      std::string Result(1, char(Key));
      return Result;
    }
    return "KEY";
  }

  std::string MapActionLabel(int Key)
  {
    if(Key == 'l') return "MOVE CURSOR";
    if(Key == 'e') return "ADD / EDIT";
    if(Key == 'd') return "DELETE NOTE";
    if(Key == 't') return "TOGGLE NOTES";
    if(Key == 'r') return "ROTATE LABELS";
    if(Key == KEY_SPECIAL) return "HELP";
    if(Key == KEY_ENTER || Key == KEY_CONTROLLER_A) return "SELECT TILE";
    if(Key == KEY_ESC || Key == KEY_CONTROLLER_B) return "BACK";
    return "MAP ACTION";
  }

  std::string MapActionShortcut(int Key)
  {
    if(Key == KEY_SPECIAL) return "F1";
    if(Key == KEY_ENTER || Key == KEY_CONTROLLER_A) return "ENTER";
    if(Key == KEY_ESC || Key == KEY_CONTROLLER_B) return "ESC";
    if(Key >= 0x20 && Key < 0x7f)
      return std::string(1, char(Key));
    return ShortcutLabel(Key);
  }

  bool HasQuestionChoice(int Key)
  {
    for(int Index = 0; Index < Hud.QuestionChoiceCount; ++Index)
      if(Hud.QuestionChoices[Index] == Key)
        return true;
    return false;
  }

  std::vector<int> MapActionKeys()
  {
    std::vector<int> Keys;
    if(Hud.PositionPrompt)
    {
      Keys.push_back(KEY_ENTER);
      Keys.push_back(KEY_ESC);
      Keys.push_back(KEY_SPECIAL);
      return Keys;
    }

    const int Preferred[] = { 'l', 'e', 'd', 't', KEY_SPECIAL, KEY_ESC };
    for(size_t Index = 0; Index < sizeof(Preferred) / sizeof(Preferred[0]);
        ++Index)
      if(HasQuestionChoice(Preferred[Index]))
        Keys.push_back(Preferred[Index]);
    if(Keys.empty())
      Keys.push_back(KEY_ESC);
    return Keys;
  }

  void RebuildMapRects()
  {
    CurrentLayout.MapNoteRows.clear();
    CurrentLayout.MapNoteIndices.clear();
    CurrentLayout.MapNoteMarkers.clear();
    CurrentLayout.MapActionButtons.clear();
    CurrentLayout.MapActionKeys = MapActionKeys();
    if(!Hud.MapScreen)
      return;

    const SDL_Rect Panel = CurrentLayout.RailContent;
    const int HeaderBottom = Panel.y + 76;
    const int Columns = 2;
    const int Gap = 6;
    const int ButtonHeight = 46;
    const int ActionRows = std::max(1,
      (int(CurrentLayout.MapActionKeys.size()) + Columns - 1) / Columns);
    const int ActionsHeight = 24 + ActionRows * ButtonHeight
                            + (ActionRows - 1) * Gap;
    const int ActionsTop = Panel.y + Panel.h - ActionsHeight - 8;
    CurrentLayout.MapNotesArea = {
      Panel.x + 6, HeaderBottom,
      std::max(1, Panel.w - 12),
      std::max(1, ActionsTop - HeaderBottom - 8)
    };

    const int RowHeight = 52;
    const int VisibleRows = std::max(1,
      CurrentLayout.MapNotesArea.h / RowHeight);
    const int MaximumScroll = std::max(0,
      int(Hud.MapNotes.size()) - VisibleRows);
    DesktopMapNoteScroll = Clamp(DesktopMapNoteScroll, 0, MaximumScroll);
    for(int Visible = 0; Visible < VisibleRows; ++Visible)
    {
      const int NoteIndex = DesktopMapNoteScroll + Visible;
      if(NoteIndex >= int(Hud.MapNotes.size()))
        break;
      CurrentLayout.MapNoteRows.push_back({
        CurrentLayout.MapNotesArea.x,
        CurrentLayout.MapNotesArea.y + Visible * RowHeight,
        CurrentLayout.MapNotesArea.w, RowHeight - 4
      });
      CurrentLayout.MapNoteIndices.push_back(NoteIndex);
    }

    const int ButtonWidth = std::max(1,
      (Panel.w - 12 - Gap) / Columns);
    for(int Index = 0; Index < int(CurrentLayout.MapActionKeys.size()); ++Index)
    {
      const int Row = Index / Columns;
      const int Column = Index % Columns;
      CurrentLayout.MapActionButtons.push_back({
        Panel.x + 6 + Column * (ButtonWidth + Gap),
        ActionsTop + 24 + Row * (ButtonHeight + Gap),
        ButtonWidth, ButtonHeight
      });
    }

    const SDL_Rect Source = CurrentLayout.CanvasSource;
    for(size_t Index = 0; Index < Hud.MapNotes.size(); ++Index)
    {
      const adaptiveui::MapNote& Note = Hud.MapNotes[Index];
      if(Note.X < Source.x || Note.X >= Source.x + Source.w
         || Note.Y < Source.y || Note.Y >= Source.y + Source.h)
      {
        CurrentLayout.MapNoteMarkers.push_back({ 0, 0, 0, 0 });
        continue;
      }
      const int X = CurrentLayout.Canvas.x
        + (Note.X - Source.x) * CurrentLayout.Canvas.w / Source.w;
      const int Y = CurrentLayout.Canvas.y
        + (Note.Y - Source.y) * CurrentLayout.Canvas.h / Source.h;
      CurrentLayout.MapNoteMarkers.push_back({ X - 11, Y - 11, 22, 22 });
    }
  }

  struct DashboardMetric
  {
    std::string Label;
    std::string Value;
  };

  std::vector<std::string> StatGroups(const std::string& Line)
  {
    std::vector<std::string> Result;
    size_t Begin = 0;
    while(Begin < Line.size())
    {
      const size_t End = Line.find("  ", Begin);
      Result.push_back(Line.substr(Begin, End == std::string::npos
        ? std::string::npos : End - Begin));
      if(End == std::string::npos)
        break;
      Begin = End + 2;
      while(Begin < Line.size() && Line[Begin] == ' ')
        ++Begin;
    }
    return Result;
  }

  DashboardMetric ExpandDesktopMetric(const std::string& Group)
  {
    const size_t Separator = Group.find(' ');
    const std::string Key = Group.substr(0, Separator);
    std::string Value = Separator == std::string::npos
      ? "" : Group.substr(Separator + 1);
    while(!Value.empty() && Value[0] == ' ')
      Value.erase(0, 1);

    const char* Label = Key.c_str();
    if(Key == "HP") Label = "HEALTH";
    else if(Key == "MANA") Label = "MANA";
    else if(Key == "GOLD") Label = "GOLD";
    else if(Key == "ARM") Label = "ARM STRENGTH";
    else if(Key == "LEG") Label = "LEG STRENGTH";
    else if(Key == "DEX") Label = "DEXTERITY";
    else if(Key == "AGI") Label = "AGILITY";
    else if(Key == "END") Label = "ENDURANCE";
    else if(Key == "PER") Label = "PERCEPTION";
    else if(Key == "INT") Label = "INTELLIGENCE";
    else if(Key == "WIS") Label = "WISDOM";
    else if(Key == "WILL") Label = "WILLPOWER";
    else if(Key == "CHA") Label = "CHARISMA";
    else if(Key == "HT")
    {
      Label = "HEIGHT";
      Value += " CM";
    }
    else if(Key == "WT")
    {
      Label = "WEIGHT";
      Value += " KG";
    }
    return { Label, Value };
  }

  std::vector<DashboardMetric> DashboardMetrics()
  {
    std::vector<DashboardMetric> Parsed;
    for(int Line = 0; Line < 4; ++Line)
    {
      const std::vector<std::string> Groups = StatGroups(Hud.Stats[Line]);
      for(size_t Index = 0; Index < Groups.size(); ++Index)
        if(!Groups[Index].empty())
          Parsed.push_back(ExpandDesktopMetric(Groups[Index]));
    }

    // Read left-to-right as resources, physical capability, awareness and
    // mind/social traits, then stable body profile information.
    static const char* Order[] = {
      "HEALTH", "MANA", "GOLD", "ENDURANCE",
      "ARM STRENGTH", "LEG STRENGTH", "DEXTERITY", "AGILITY",
      "PERCEPTION", "INTELLIGENCE", "WISDOM", "WILLPOWER",
      "CHARISMA", "HEIGHT", "WEIGHT"
    };
    std::vector<DashboardMetric> Result;
    std::vector<bool> Used(Parsed.size(), false);
    for(size_t OrderIndex = 0;
        OrderIndex < sizeof(Order) / sizeof(Order[0]); ++OrderIndex)
      for(size_t MetricIndex = 0; MetricIndex < Parsed.size(); ++MetricIndex)
        if(!Used[MetricIndex] && Parsed[MetricIndex].Label == Order[OrderIndex])
        {
          Result.push_back(Parsed[MetricIndex]);
          Used[MetricIndex] = true;
          break;
        }
    for(size_t Index = 0; Index < Parsed.size(); ++Index)
      if(!Used[Index])
        Result.push_back(Parsed[Index]);
    return Result;
  }

  void DrawDashboard(SDL_Renderer* Renderer)
  {
    Frame(Renderer, CurrentLayout.Dashboard);
    const int HeaderHeight = 4;
    const int ChipHeight = 4;
    const int ContentTop = CurrentLayout.Dashboard.y + HeaderHeight;
    const int ContentBottom = CurrentLayout.Dashboard.y
                            + CurrentLayout.Dashboard.h - ChipHeight;
    const std::vector<DashboardMetric> Metrics = DashboardMetrics();
    if(!Metrics.empty())
    {
      const int ContentHeight = std::max(1, ContentBottom - ContentTop);
      const int Rows = std::max(1, CurrentLayout.DashboardRows);
      const int FirstRowCount = Rows == 1 ? int(Metrics.size())
        : (int(Metrics.size()) + 1) / 2;
      for(int Row = 0; Row < Rows; ++Row)
      {
        const int Begin = Row == 0 ? 0 : FirstRowCount;
        const int End = Row == 0 ? FirstRowCount : int(Metrics.size());
        const int Count = std::max(0, End - Begin);
        if(!Count)
          continue;
        for(int Column = 0; Column < Count; ++Column)
        {
          const int Index = Begin + Column;
          const int X0 = CurrentLayout.Dashboard.x
                       + CurrentLayout.Dashboard.w * Column / Count;
          const int X1 = CurrentLayout.Dashboard.x
                       + CurrentLayout.Dashboard.w * (Column + 1) / Count;
          const int Y0 = ContentTop + ContentHeight * Row / Rows;
          const int Y1 = ContentTop + ContentHeight * (Row + 1) / Rows;
          const int Width = std::max(1, X1 - X0 - 8);
          const int Height = std::max(1, Y1 - Y0);
          const std::string Label = Elide(Metrics[Index].Label,
                                          std::max(1, Width / 8));
          const int ValueScale = 1;
          const int TotalHeight = 9 + 3 + ValueScale * 7;
          const int TextY = Y0 + std::max(2, (Height - TotalHeight) / 2);
          DashboardHeaderText(Renderer,
            X0 + (X1 - X0 - DashboardHeaderTextWidth(Label)) / 2,
            TextY, Label, 205, 181, 126);
          Text(Renderer,
               CenteredTextX(X0, X1 - X0, Metrics[Index].Value, ValueScale),
               TextY + 12,
               Metrics[Index].Value, ValueScale,
               222, 215, 194);
          if(Column)
          {
            const bool GroupBoundary = (Row == 0 && Column == 3)
                                    || (Row == 1 && Column == 5);
            Fill(Renderer, { X0, Y0 + 5, 1, std::max(1, Height - 10) },
                 GroupBoundary ? 90 : 58,
                 GroupBoundary ? 69 : 47,
                 GroupBoundary ? 43 : 34);
          }
        }
      }
      if(Rows > 1)
        Fill(Renderer, { CurrentLayout.Dashboard.x + 7,
                         ContentTop + ContentHeight / Rows,
                         CurrentLayout.Dashboard.w - 14, 1 },
             58, 47, 34);
    }
    else
    {
      const int Columns = CurrentLayout.DashboardRows == 2 ? 2 : 4;
      for(int Index = 0; Index < 4; ++Index)
      {
        const int Column = Index % Columns;
        const int Row = Index / Columns;
        const int X0 = CurrentLayout.Dashboard.x
                     + CurrentLayout.Dashboard.w * Column / Columns;
        const int X1 = CurrentLayout.Dashboard.x
                     + CurrentLayout.Dashboard.w * (Column + 1) / Columns;
        const int ContentHeight = std::max(1, ContentBottom - ContentTop);
        const int Y0 = ContentTop + ContentHeight * Row
                      / CurrentLayout.DashboardRows;
        const int Y1 = ContentTop + ContentHeight * (Row + 1)
                      / CurrentLayout.DashboardRows;
        const int AvailableWidth = std::max(1, X1 - X0 - 20);
        const int Scale = TextWidth(Hud.Stats[Index], 2) <= AvailableWidth
                        && Y1 - Y0 >= 18 ? 2 : 1;
        const int Characters = std::max(1, AvailableWidth / (Scale * 6));
        const std::string Value = Elide(Hud.Stats[Index], Characters);
        Text(Renderer, X0 + 10,
             Y0 + std::max(2, (Y1 - Y0 - Scale * 7) / 2),
             Value, Scale, 236, 226, 198);
      }
    }
  }

  std::string ConditionLabel(const adaptiveui::StatusIndicator& Indicator)
  {
    return Indicator.Label + (Indicator.Value.empty()
      ? "" : ":" + Indicator.Value);
  }

  void DrawEquipmentConditions(SDL_Renderer* Renderer)
  {
    const SDL_Rect Area = CurrentLayout.EquipmentConditions;
    if(Hud.Conditions.empty() || Area.w <= 0 || Area.h <= 0)
      return;

    int X = Area.x;
    int Y = Area.y + 2;
    for(size_t Index = 0; Index < Hud.Conditions.size(); ++Index)
    {
      const adaptiveui::StatusIndicator& Indicator = Hud.Conditions[Index];
      const std::string Label = ConditionLabel(Indicator);
      const int Width = std::min(Area.w,
        std::max(30, TextWidth(Label, 1) + 12));
      if(X != Area.x && X + Width > Area.x + Area.w)
      {
        X = Area.x;
        Y += 18;
      }
      if(Y + 14 > Area.y + Area.h)
        break;
      Fill(Renderer, { X, Y, Width, 14 }, 24, 23, 20, 255);
      Outline(Renderer, { X, Y, Width, 14 },
              Indicator.Red, Indicator.Green, Indicator.Blue);
      Text(Renderer, X + 6, Y + 3, Label, 1,
           Indicator.Red, Indicator.Green, Indicator.Blue);
      X += Width + 5;
    }
  }

  void DrawActions(SDL_Renderer* Renderer)
  {
    Frame(Renderer, CurrentLayout.RailContent);
    Text(Renderer, CurrentLayout.RailContent.x + 9,
         CurrentLayout.RailContent.y + 8,
         "ACTIONS", 1, 221, 190, 112);
    const std::string CategoryHint = "ALT+1..5";
    Text(Renderer, CurrentLayout.RailContent.x
                   + CurrentLayout.RailContent.w
                   - TextWidth(CategoryHint, 1) - 9,
         CurrentLayout.RailContent.y + 8, CategoryHint, 1, 115, 107, 91);
    for(int Category = 0; Category < adaptiveui::ACTION_GROUPS; ++Category)
    {
      const bool Selected = Category == CurrentLayout.ActiveCategory;
      const SDL_Rect& Tab = CurrentLayout.ActionTabs[Category];
      Fill(Renderer, Tab, Selected ? 29 : 18, Selected ? 58 : 20,
           Selected ? 38 : 20, 255);
      Outline(Renderer, Tab, Selected ? 184 : 72,
              Selected ? 152 : 59, Selected ? 82 : 43);
       const std::string Label =
         (Category == adaptiveui::ACTION_CONTEXT ? "CONTEXT" :
          Category == adaptiveui::ACTION_ITEMS ? "ITEMS" :
          Category == adaptiveui::ACTION_CHARACTER ? "CHARACTER" :
          Category == adaptiveui::ACTION_MOVE ? "MOVE" : "SYSTEM");
      Text(Renderer, Tab.x + std::max(2, (Tab.w - TextWidth(Label, 1)) / 2),
           Tab.y + std::max(3, (Tab.h - 7) / 2), Label, 1,
           Selected ? 244 : 174, Selected ? 226 : 165,
           Selected ? 171 : 139);
    }
    SDL_RenderSetClipRect(Renderer, &CurrentLayout.ActionArea);
    for(size_t Index = 0; Index < CurrentLayout.ActionButtons.size(); ++Index)
    {
      const SDL_Rect& Button = CurrentLayout.ActionButtons[Index];
      const int ActionIndex = CurrentLayout.ActionButtonIndices[Index];
      const adaptiveui::ActionEntry& Action = Hud.Actions[ActionIndex];
      const bool Hovered = int(Index) == CurrentLayout.HoverAction;
      const bool Pressed = int(Index) == CurrentLayout.PressedAction;
      Fill(Renderer, Button, Pressed ? 55 : (Hovered ? 34 : 22),
           Pressed ? 83 : (Hovered ? 55 : 27),
           Pressed ? 51 : (Hovered ? 36 : 27), 255);
      Outline(Renderer, Button, Pressed ? 219 : (Hovered ? 166 : 68),
              Pressed ? 181 : (Hovered ? 137 : 57),
              Pressed ? 99 : (Hovered ? 76 : 43));
      const std::string Shortcut = Action.DisplayedShortcut.empty()
        ? ShortcutLabel(Action.DispatchCode) : Action.DisplayedShortcut;
      const int ShortcutWidth = std::max(
        22, TextWidth(Shortcut, DesktopSidebarFontScale) + 10);
      SDL_Rect Key = { Button.x + Button.w - ShortcutWidth - 5,
                       Button.y + 5, ShortcutWidth,
                       std::max(1, Button.h - 10) };
      Fill(Renderer, Key, 14, 14, 13, 255);
      Outline(Renderer, Key, 91, 75, 50);
      Text(Renderer,
           Key.x + (Key.w - TextWidth(Shortcut,
                                      DesktopSidebarFontScale)) / 2,
           Key.y + std::max(2, (Key.h
                                - 7 * DesktopSidebarFontScale) / 2),
           Shortcut, DesktopSidebarFontScale, 222, 190, 111);
      const int LabelWidth = std::max(1, Key.x - Button.x - 12);
      std::vector<std::string> Lines = Wrap(
        Action.Label,
        std::max(1, LabelWidth / (6 * DesktopSidebarFontScale)));
      if(Lines.empty())
        Lines.push_back("");
      const int TextHeight = int(Lines.size()) * DesktopSidebarLineHeight - 2;
      int TextY = Button.y + std::max(2, (Button.h - TextHeight) / 2);
      for(size_t Line = 0; Line < Lines.size(); ++Line,
          TextY += DesktopSidebarLineHeight)
        Text(Renderer, Button.x + 7, TextY, Lines[Line],
             DesktopSidebarFontScale,
             Action.Available ? 232 : 117,
             Action.Available ? 223 : 117,
             Action.Available ? 198 : 117);
    }
    SDL_RenderSetClipRect(Renderer, 0);
    if(CurrentLayout.ActionButtons.empty())
      Centered(Renderer, CurrentLayout.ActionArea, "NO ACTIONS", 2,
               150, 140, 110);
    const int FooterY = CurrentLayout.RailContent.y
                      + CurrentLayout.RailContent.h - 14;
    if(!Hud.Location.empty())
      Text(Renderer, CurrentLayout.RailContent.x + 9, FooterY,
           Hud.Location, 1, 160, 151, 130);
    if(!Hud.Clock.empty())
      Text(Renderer,
           CurrentLayout.RailContent.x + CurrentLayout.RailContent.w - 9
             - TextWidth(Hud.Clock, 1),
           FooterY, Hud.Clock, 1, 160, 151, 130);
    CurrentLayout.PressedAction = -1;
  }

  void DrawMapMarkers(SDL_Renderer* Renderer)
  {
    static const unsigned char Colors[6][3] = {
      { 232, 194, 78 }, { 80, 196, 220 }, { 220, 104, 176 },
      { 116, 202, 105 }, { 230, 137, 65 }, { 202, 202, 202 }
    };
    Outline(Renderer, CurrentLayout.Canvas, 116, 91, 49);
    for(size_t Index = 0; Index < CurrentLayout.MapNoteMarkers.size(); ++Index)
    {
      const SDL_Rect Marker = CurrentLayout.MapNoteMarkers[Index];
      if(Marker.w <= 0 || Marker.h <= 0)
        continue;
      const bool Selected = int(Index) == DesktopSelectedMapNote;
      const unsigned char* ColorValue = Colors[Index % 6];
      Fill(Renderer, Marker, Selected ? 34 : 16,
           Selected ? 65 : 18, Selected ? 39 : 18, 245);
      Outline(Renderer, Marker, ColorValue[0], ColorValue[1], ColorValue[2]);
      char Number[12];
      snprintf(Number, sizeof(Number), "%d", int(Index) + 1);
      Centered(Renderer, Marker, Number, 1,
               ColorValue[0], ColorValue[1], ColorValue[2]);
    }
  }

  void DrawMapSidebar(SDL_Renderer* Renderer)
  {
    static const unsigned char Colors[6][3] = {
      { 232, 194, 78 }, { 80, 196, 220 }, { 220, 104, 176 },
      { 116, 202, 105 }, { 230, 137, 65 }, { 202, 202, 202 }
    };
    const SDL_Rect Panel = CurrentLayout.RailContent;
    Frame(Renderer, Panel);
    Text(Renderer, Panel.x + 9, Panel.y + 8,
         "CARTOGRAPHY", 2, 221, 190, 112);
    const std::string Mode = Hud.PositionPrompt
      ? "SELECT A TILE ON THE MAP" : "SELECT A NOTE OR ACTION";
    Text(Renderer, Panel.x + 9, Panel.y + 32,
         Elide(Mode, std::max(1, (Panel.w - 18) / 6)),
         1, 168, 157, 132);
    Fill(Renderer, { Panel.x + 8, Panel.y + 54,
                     std::max(1, Panel.w - 16), 1 }, 67, 53, 38);

    std::string NoteTitle = "MAP NOTES";
    if(!Hud.MapNotes.empty())
    {
      char Count[24];
      snprintf(Count, sizeof(Count), "  %d", int(Hud.MapNotes.size()));
      NoteTitle += Count;
    }
    Text(Renderer, CurrentLayout.MapNotesArea.x,
         CurrentLayout.MapNotesArea.y - 16,
         NoteTitle, 1, 176, 164, 137);

    if(Hud.MapNotes.empty())
      Centered(Renderer, CurrentLayout.MapNotesArea,
               "NO MAP NOTES", 2, 125, 116, 96);
    for(size_t Visible = 0; Visible < CurrentLayout.MapNoteRows.size(); ++Visible)
    {
      const SDL_Rect Row = CurrentLayout.MapNoteRows[Visible];
      const int NoteIndex = CurrentLayout.MapNoteIndices[Visible];
      const bool Selected = NoteIndex == DesktopSelectedMapNote;
      const bool Hovered = NoteIndex == DesktopHoverMapNote;
      Fill(Renderer, Row, Selected ? 28 : (Hovered ? 28 : 18),
           Selected ? 66 : (Hovered ? 42 : 22),
           Selected ? 39 : (Hovered ? 29 : 22), 255);
      Outline(Renderer, Row,
              Selected ? 105 : (Hovered ? 89 : 54),
              Selected ? 164 : (Hovered ? 111 : 48),
              Selected ? 89 : (Hovered ? 69 : 39));
      const unsigned char* ColorValue = Colors[NoteIndex % 6];
      const SDL_Rect Badge = { Row.x + 5, Row.y + 10, 28, 28 };
      Fill(Renderer, Badge, 14, 14, 13, 255);
      Outline(Renderer, Badge, ColorValue[0], ColorValue[1], ColorValue[2]);
      char Number[12];
      snprintf(Number, sizeof(Number), "%d", NoteIndex + 1);
      Centered(Renderer, Badge, Number, 1,
               ColorValue[0], ColorValue[1], ColorValue[2]);
      std::vector<std::string> Lines = Wrap(
        Hud.MapNotes[NoteIndex].Label,
        std::max(1, (Row.w - 45) / 12));
      const int VisibleLines = std::min(2, int(Lines.size()));
      int Y = Row.y + std::max(4, (Row.h - VisibleLines * 16) / 2);
      for(int Line = 0; Line < VisibleLines; ++Line, Y += 16)
        Text(Renderer, Row.x + 40, Y, Lines[Line], 2,
             235, 226, 204);
    }

    const int ActionsTitleY = CurrentLayout.MapActionButtons.empty()
      ? Panel.y + Panel.h - 24
      : CurrentLayout.MapActionButtons[0].y - 18;
    Text(Renderer, Panel.x + 7, ActionsTitleY,
         Hud.PositionPrompt ? "MAP CURSOR" : "MAP ACTIONS",
         1, 176, 164, 137);
    for(size_t Index = 0; Index < CurrentLayout.MapActionButtons.size(); ++Index)
    {
      const SDL_Rect Button = CurrentLayout.MapActionButtons[Index];
      const int Key = CurrentLayout.MapActionKeys[Index];
      const bool Hovered = int(Index) == DesktopHoverMapAction;
      Fill(Renderer, Button, Hovered ? 34 : 22,
           Hovered ? 55 : 27, Hovered ? 36 : 27, 255);
      Outline(Renderer, Button, Hovered ? 166 : 68,
              Hovered ? 137 : 57, Hovered ? 76 : 43);
      std::vector<std::string> Lines = Wrap(
        MapActionLabel(Key), std::max(1, (Button.w - 12) / 12));
      const int VisibleLines = std::min(2, int(Lines.size()));
      int Y = Button.y + 6;
      for(int Line = 0; Line < VisibleLines; ++Line, Y += 16)
        Text(Renderer, Button.x + 6, Y, Lines[Line], 2,
             232, 223, 198);
      const std::string Shortcut = MapActionShortcut(Key);
      Text(Renderer, Button.x + Button.w - TextWidth(Shortcut, 1) - 5,
           Button.y + Button.h - 11, Shortcut, 1, 222, 190, 111);
    }
  }

  void DrawLog(SDL_Renderer* Renderer)
  {
    Frame(Renderer, CurrentLayout.Log);
    if(PromptShowsLogContext())
    {
      const SDL_Rect Inner = { CurrentLayout.Log.x + 4,
        CurrentLayout.Log.y + 2, std::max(1, CurrentLayout.Log.w - 8),
        std::max(1, CurrentLayout.Log.h - 4) };
      const int PromptHeight = Clamp(Inner.h / 3, 24, 32);
      const SDL_Rect Message = { Inner.x, Inner.y, Inner.w,
        std::max(1, Inner.h - PromptHeight - 1) };
      const SDL_Rect Prompt = { Inner.x,
        Message.y + Message.h + 1, Inner.w, PromptHeight };
      Wrapped(Renderer, Message, Hud.LogMessage, 2, 240, 230, 202);
      Fill(Renderer, { Inner.x + 8, Prompt.y - 1,
                       std::max(1, Inner.w - 16), 1 }, 67, 53, 38);
      Wrapped(Renderer, Prompt, Hud.Prompt, 2, 248, 224, 154);
      return;
    }
    std::string Value = Hud.PromptActive
      ? (Hud.PromptDetail.empty() ? Hud.Prompt : Hud.PromptDetail)
      : Hud.LogMessage;
    if(Hud.PromptActive && Hud.PromptInput.size())
      Value += "\n> " + Hud.PromptInput;
    Wrapped(Renderer, { CurrentLayout.Log.x + 4, CurrentLayout.Log.y + 2,
                        std::max(1, CurrentLayout.Log.w - 8),
                        std::max(1, CurrentLayout.Log.h - 4) },
             Value, 2, 240, 230, 202);
  }

  bool IsExpandedHistoryMenu()
  {
    return Hud.MenuActive && HasGameplayContext()
        && Hud.MenuTitle == "Message history";
  }

  bool IsOptionsMenu()
  {
    return Hud.MenuActive
        && (Hud.MenuTitle == "OPTIONS"
            || Hud.MenuTitle == "AUTO PICK UP ITEMS"
            || Hud.MenuTitle.find("setting do you wish to configure")
               != std::string::npos);
  }

  bool IsHelpMenu()
  {
    return Hud.MenuActive && HasGameplayContext()
        && Hud.MenuTitle == "Keyboard Layout";
  }

  bool IsCraftingGuideTextPage()
  {
    return Hud.MenuActive && HasGameplayContext()
        && Hud.MenuTitle.find("Crafting guide - ") == 0;
  }

  bool IsHallOfFameMenu()
  {
    return Hud.MenuActive && HasGameplayContext()
        && Hud.MenuTitle == "Adventurers' Hall of Fame";
  }

  bool IsExpandedMainMenu()
  {
    return IsExpandedHistoryMenu() || IsOptionsMenu() || IsHelpMenu()
        || IsHallOfFameMenu();
  }

  std::string DisplayMenuTitle()
  {
    return Hud.MenuTitle == "AUTO PICK UP ITEMS" ? Hud.MenuTitle
      : IsOptionsMenu() ? "OPTIONS"
      : (Hud.MenuTitle.empty() ? "MENU" : Hud.MenuTitle);
  }

std::string DisplayMenuSubtitle()
{
  std::string Subtitle = Hud.MenuSubtitle;
  if(IsHelpMenu())
  {
    while(!Subtitle.empty()
          && std::isspace((unsigned char)Subtitle.back()))
      Subtitle.pop_back();

    const std::string Description = "Description";
    const size_t DescriptionAt = Subtitle.rfind(Description);
    if(DescriptionAt != std::string::npos
       && DescriptionAt + Description.size() == Subtitle.size())
    {
      size_t KeyEnd = DescriptionAt;
      while(KeyEnd && std::isspace((unsigned char)Subtitle[KeyEnd - 1]))
        --KeyEnd;

      const std::string Key = "Key";
      if(KeyEnd >= Key.size()
         && Subtitle.compare(KeyEnd - Key.size(), Key.size(), Key) == 0)
        Subtitle.erase(KeyEnd - Key.size());
    }

    while(!Subtitle.empty()
          && std::isspace((unsigned char)Subtitle.back()))
      Subtitle.pop_back();
  }
  return Subtitle;
}

  struct OptionLabel
  {
    std::string Name;
    std::string Value;
  };

  OptionLabel ParseOptionLabel(const std::string& Text)
  {
    OptionLabel Result;
    Result.Name = Text;
    size_t End = Text.size();
    while(End && std::isspace((unsigned char)Text[End - 1]))
      --End;
    size_t Split = std::string::npos;
    for(size_t Index = 0; Index + 1 < End; ++Index)
      if(std::isspace((unsigned char)Text[Index])
         && std::isspace((unsigned char)Text[Index + 1]))
      {
        Split = Index;
        break;
      }
    if(Split == std::string::npos)
    {
      Result.Name = Text.substr(0, End);
      return Result;
    }
    Result.Name = Text.substr(0, Split);
    size_t ValueStart = Split;
    while(ValueStart < End
          && std::isspace((unsigned char)Text[ValueStart]))
      ++ValueStart;
    Result.Value = Text.substr(ValueStart, End - ValueStart);
    return Result;
  }

  bool IsInventoryGridMenu()
  {
    return Hud.MenuActive && HasGameplayContext() && Hud.MenuIconGrid;
  }

  bool IsInventoryCategoryMenu()
  {
    return IsInventoryGridMenu()
      && (Hud.MenuTitle == "Inventory categories"
          || Hud.MenuTitle.find(" - categories") != std::string::npos
          || Hud.MenuTitle.find(" - material routes") != std::string::npos);
  }

  int HistoryDispatchCode()
  {
    for(size_t Index = 0; Index < Hud.Actions.size(); ++Index)
      if(Hud.Actions[Index].Label == "HISTORY"
         && Hud.Actions[Index].Available)
        return Hud.Actions[Index].DispatchCode;
    return KEY_MOBILE_COMMAND_BASE + 34;
  }

  struct PromptGeometry
  {
    SDL_Rect Dialog;
    SDL_Rect Input;
    SDL_Rect Continue;
    SDL_Rect Decline;
    SDL_Rect Cancel;
  };

  bool HasBlockingPromptDialog()
  {
    return Hud.PromptActive && !Hud.PositionPrompt
        && (Hud.PromptShowsInput || Hud.PromptNumeric
            || Hud.PromptCapturesKey
            || Hud.PromptConfirmsKeyTransfer
            || Hud.PromptConfirmsChoice
            || Hud.PromptOffersQuitChoices);
  }

  PromptGeometry GetPromptGeometry()
  {
    const int Width = Clamp(CurrentLayout.OutputWidth * 9 / 20, 440, 620);
    const int Height = Hud.PromptConfirmsChoice
                       && !Hud.PromptDetail.empty() ? 240
                     : Hud.PromptCapturesKey
                    || Hud.PromptConfirmsKeyTransfer
                    || Hud.PromptConfirmsChoice
                    || Hud.PromptOffersQuitChoices ? 190 : 156;
    SDL_Rect Host = CurrentLayout.MapPanel;
    if(!HasGameplayContext())
      Host = { 0, 0, CurrentLayout.OutputWidth, CurrentLayout.OutputHeight };
    PromptGeometry Geometry;
    Geometry.Dialog = { Host.x + (Host.w - Width) / 2,
                        Host.y + (Host.h - Height) / 2,
                        std::min(Width, Host.w - 20),
                        std::min(Height, Host.h - 20) };
    Geometry.Input = { Geometry.Dialog.x + 16, Geometry.Dialog.y + 45,
                       std::max(1, Geometry.Dialog.w - 32), 38 };
    Geometry.Continue = { Geometry.Dialog.x + 16,
                          Geometry.Dialog.y + Geometry.Dialog.h - 36,
                          132, 25 };
    Geometry.Decline = { 0, 0, 0, 0 };
    Geometry.Cancel = { Geometry.Dialog.x + Geometry.Dialog.w - 116,
                        Geometry.Dialog.y + Geometry.Dialog.h - 36,
                        100, 25 };
    if(Hud.PromptOffersQuitChoices)
    {
      const int Gap = 8;
      const int ButtonWidth = std::max(1,
        (Geometry.Dialog.w - 32 - Gap * 2) / 3);
      Geometry.Continue = { Geometry.Dialog.x + 16,
                            Geometry.Dialog.y + Geometry.Dialog.h - 36,
                            ButtonWidth, 25 };
      Geometry.Decline = { Geometry.Continue.x + ButtonWidth + Gap,
                           Geometry.Continue.y, ButtonWidth, 25 };
      Geometry.Cancel = { Geometry.Decline.x + ButtonWidth + Gap,
                          Geometry.Continue.y, ButtonWidth, 25 };
    }
    return Geometry;
  }

  void DrawPromptDialog(SDL_Renderer* Renderer)
  {
    if(!HasBlockingPromptDialog())
      return;
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
    Fill(Renderer, { 0, 0, CurrentLayout.OutputWidth,
                     CurrentLayout.OutputHeight }, 0, 0, 0, 166);
    const PromptGeometry Geometry = GetPromptGeometry();
    const SDL_Rect Dialog = Geometry.Dialog;
    Frame(Renderer, Dialog);
    const std::string Title = Hud.PromptCapturesKey ? "REMAP CONTROL"
      : Hud.PromptConfirmsKeyTransfer ? "KEY ALREADY IN USE"
      : Hud.PromptConfirmsChoice ? "CONFIRM"
      : Hud.PromptOffersQuitChoices ? "QUIT GAME"
      : Hud.Prompt;
    Text(Renderer, Dialog.x + 16, Dialog.y + 14,
         Elide(Title, std::max(1, (Dialog.w - 32) / 12)), 2,
         248, 224, 154);
    const SDL_Rect Input = Geometry.Input;
    if(Hud.PromptCapturesKey || Hud.PromptConfirmsKeyTransfer
       || Hud.PromptConfirmsChoice || Hud.PromptOffersQuitChoices)
    {
      const std::string PromptText = Hud.PromptConfirmsChoice
        && !Hud.PromptDetail.empty()
          ? Hud.PromptDetail + "\n\n" + Hud.Prompt : Hud.Prompt;
      TopWrapped(Renderer, { Input.x, Input.y, Input.w,
                             Hud.PromptCapturesKey ? 58
                               : std::max(76, Dialog.h - 96) },
                 PromptText, 2, 240, 230, 202);
      if(Hud.PromptCapturesKey)
      {
        SDL_Rect Capture = { Dialog.x + 16, Dialog.y + 112,
                             std::max(1, Dialog.w - 32), 28 };
        Fill(Renderer, Capture, 10, 12, 11, 255);
        Outline(Renderer, Capture, 100, 157, 96);
        Centered(Renderer, Capture, "PRESS ANY KEY", 1,
                 190, 226, 157);
      }
    }
    else
    {
      Fill(Renderer, Input, 10, 12, 11, 255);
      Outline(Renderer, Input, 100, 157, 96);
      const std::string Value = "> " + Hud.PromptInput;
      const std::string VisibleValue = Elide(
        Value, std::max(1, (Input.w - 20) / 12));
      Text(Renderer, Input.x + 10, Input.y + 12,
           VisibleValue, 2, 240, 230, 202);
      if((SDL_GetTicks() / 500) % 2 == 0)
      {
        const int CaretX = std::min(Input.x + Input.w - 4,
                                    Input.x + 12 + TextWidth(VisibleValue, 2));
        Fill(Renderer, { CaretX, Input.y + 10, 2, 18 },
             240, 230, 202, 255);
      }
    }
    if(!Hud.PromptCapturesKey)
    {
      Fill(Renderer, Geometry.Continue, 24, 55, 35, 255);
      Outline(Renderer, Geometry.Continue, 102, 169, 92);
      Centered(Renderer, Geometry.Continue,
               Hud.PromptConfirmsKeyTransfer ? "TAKE KEY"
                 : (Hud.PromptConfirmsChoice || Hud.PromptOffersQuitChoices)
                   ? "YES" : "CONTINUE", 1,
               190, 226, 157);
    }
    if(Hud.PromptOffersQuitChoices)
    {
      Fill(Renderer, Geometry.Decline, 48, 39, 20, 255);
      Outline(Renderer, Geometry.Decline, 169, 128, 57);
      Centered(Renderer, Geometry.Decline, "NO", 1, 236, 204, 126);
    }
    Fill(Renderer, Geometry.Cancel, 49, 20, 18, 255);
    Outline(Renderer, Geometry.Cancel, 168, 71, 58);
    Centered(Renderer, Geometry.Cancel,
             Hud.PromptConfirmsChoice ? "NO" : "CANCEL", 1,
             228, 168, 139);
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
  }

  struct MenuGeometry
  {
    SDL_Rect Area;
    int Top;
    int Bottom;
    int RowHeight;
    int VisibleCount;
    int MaximumScroll;
    bool FrontEnd;
    SDL_Rect Back;
    SDL_Rect Confirm;
    SDL_Rect Previous;
    SDL_Rect Next;
    std::vector<SDL_Rect> ItemActions;
    std::vector<int> ItemActionCodes;
    int GridColumns;
    int GridCellWidth;
    int GridCellHeight;
    int GridGap;
    SDL_Rect Detail;
  };

  const int OptionCategoryHeight = 24;

  bool StartsOptionCategory(int Index, int First)
  {
    return Index == First || Index == 0
      || (Index > 0 && Index < int(Hud.MenuGroups.size())
          && Hud.MenuGroups[Index] != Hud.MenuGroups[Index - 1]);
  }

  SDL_Rect OptionRowRect(const MenuGeometry& Geometry, int Index, int First)
  {
    int Y = Geometry.Top;
    for(int Current = First; Current <= Index; ++Current)
    {
      if(StartsOptionCategory(Current, First))
        Y += OptionCategoryHeight;
      if(Current == Index)
        return { 0, Y, 0, Geometry.RowHeight };
      Y += Geometry.RowHeight;
    }
    return { 0, Geometry.Bottom, 0, Geometry.RowHeight };
  }

  int VisibleOptionCount(const MenuGeometry& Geometry, int First)
  {
    int Count = 0;
    for(int Index = First; Index < int(Hud.MenuOptions.size()); ++Index)
    {
      const SDL_Rect Row = OptionRowRect(Geometry, Index, First);
      if(Row.y + Row.h > Geometry.Bottom)
        break;
      ++Count;
    }
    return std::max(1, Count);
  }

  int MaximumOptionScroll(const MenuGeometry& Geometry)
  {
    for(int First = 0; First < int(Hud.MenuOptions.size()); ++First)
      if(First + VisibleOptionCount(Geometry, First)
         >= int(Hud.MenuOptions.size()))
        return First;
    return std::max(0, int(Hud.MenuOptions.size()) - 1);
  }

  struct DesktopItemAction
  {
    int Code;
    const char* Label;
  };

  std::vector<DesktopItemAction> DesktopPickupActions()
  {
    std::vector<DesktopItemAction> Result;
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

    const DesktopItemAction Available[] = {
      { adaptiveui::ITEM_ACTION_DRINK, "DRINK" },
      { adaptiveui::ITEM_ACTION_TASTE, "TASTE" },
      { adaptiveui::ITEM_ACTION_EAT, "EAT" },
      { adaptiveui::ITEM_ACTION_READ, "READ" },
      { adaptiveui::ITEM_ACTION_ZAP, "ZAP" },
      { adaptiveui::ITEM_ACTION_APPLY, "APPLY" }
    };
    for(size_t Index = 0;
        Index < sizeof(Available) / sizeof(Available[0]); ++Index)
      if(Metrics.Actions & adaptiveui::ItemActionMask(
           adaptiveui::ItemAction(Available[Index].Code)))
        Result.push_back(Available[Index]);
    return Result;
  }

  SDL_Rect DesktopMenuButton(const SDL_Rect& Area, int Index, int Count,
                             int Top, int RowHeight, int Gap)
  {
    const int Columns = std::min(4, std::max(1, Count));
    const int Column = Index % Columns;
    const int Row = Index / Columns;
    const int Width = std::max(1,
      (Area.w - 20 - Gap * (Columns - 1)) / Columns);
    return { Area.x + 10 + Column * (Width + Gap),
             Top + Row * (RowHeight + Gap), Width, RowHeight };
  }

  MenuGeometry GetMenuGeometry()
  {
    MenuGeometry Geometry;
    Geometry.FrontEnd = !HasGameplayContext() && !IsOptionsMenu();
    if(IsOptionsMenu() && !HasGameplayContext())
    {
      const int Gutter = CurrentLayout.Gutter;
      Geometry.Area = { Gutter, Gutter,
                        std::max(1, CurrentLayout.OutputWidth - Gutter * 2),
                        std::max(1, CurrentLayout.OutputHeight - Gutter * 2) };
    }
    else if(Geometry.FrontEnd)
    {
      const int Width = Clamp(CurrentLayout.OutputWidth * 9 / 20, 460, 620);
      const int RowCount = Clamp(int(Hud.MenuOptions.size()), 1, 11);
      const int Height = std::min(CurrentLayout.OutputHeight
                                  - CurrentLayout.Gutter * 2,
                                  100 + RowCount * 34);
      Geometry.Area = { (CurrentLayout.OutputWidth - Width) / 2,
                        (CurrentLayout.OutputHeight - Height) / 2,
                        Width, Height };
    }
    else if(IsExpandedMainMenu())
      Geometry.Area = CurrentLayout.MapPanel;
    else
      Geometry.Area = CurrentLayout.RailContent;
    Geometry.RowHeight = Geometry.FrontEnd ? 34
      : (IsOptionsMenu() ? 34 : (IsHelpMenu() ? 34
                                               : DesktopSidebarRowHeight));
    if(Geometry.FrontEnd)
      Geometry.Top = Geometry.Area.y + 78;
    else
    {
      if(IsInventoryGridMenu())
        Geometry.Top = Geometry.Area.y + 10;
      else
      {
        const int HeaderWidth = std::max(1, Geometry.Area.w - 20);
        const std::string Title = DisplayMenuTitle();
        const int Columns = std::max(
          1, HeaderWidth / (6 * DesktopSidebarFontScale));
        const int TitleLines = std::max(1, int(Wrap(Title, Columns).size()));
        const std::string Subtitle = DisplayMenuSubtitle();
        const int SubtitleLines = (Subtitle.empty()
                                   || IsOptionsMenu()) ? 0
          : std::max(1, int(Wrap(Subtitle, Columns).size()));
        Geometry.Top = Geometry.Area.y + 10
                     + TitleLines * DesktopSidebarLineHeight
                     + (SubtitleLines ? 5 + SubtitleLines
                                        * DesktopSidebarLineHeight : 0)
                     + 10;
      }
    }
    Geometry.Confirm = { 0, 0, 0, 0 };
    Geometry.Previous = { 0, 0, 0, 0 };
    Geometry.Next = { 0, 0, 0, 0 };
    Geometry.ItemActions.clear();
    Geometry.ItemActionCodes.clear();
    Geometry.Back = Geometry.FrontEnd
      ? SDL_Rect{ 0, 0, 0, 0 }
      : SDL_Rect{ Geometry.Area.x + 10,
                  Geometry.Area.y + Geometry.Area.h - 42, 116, 32 };
    const std::vector<DesktopItemAction> PickupActions =
      DesktopPickupActions();
    const bool PickupButtons = Hud.MenuKind == adaptiveui::MENU_PICKUP_GRID;
    if(PickupButtons)
    {
      const int ButtonCount = int(PickupActions.size()) + 2;
      const int Columns = std::min(4, std::max(1, ButtonCount));
      const int Rows = (ButtonCount + Columns - 1) / Columns;
      const int ButtonHeight = 32;
      const int ButtonGap = 6;
      const int ButtonTop = Geometry.Area.y + Geometry.Area.h - 10
        - Rows * ButtonHeight - (Rows - 1) * ButtonGap;
      for(size_t Index = 0; Index < PickupActions.size(); ++Index)
      {
        Geometry.ItemActions.push_back(DesktopMenuButton(
          Geometry.Area, int(Index), ButtonCount, ButtonTop,
          ButtonHeight, ButtonGap));
        Geometry.ItemActionCodes.push_back(PickupActions[Index].Code);
      }
      Geometry.Confirm = DesktopMenuButton(
        Geometry.Area, int(PickupActions.size()), ButtonCount,
        ButtonTop, ButtonHeight, ButtonGap);
      Geometry.Back = DesktopMenuButton(
        Geometry.Area, int(PickupActions.size()) + 1, ButtonCount,
        ButtonTop, ButtonHeight, ButtonGap);
      Geometry.Bottom = ButtonTop - 8;
    }
    else if(Geometry.FrontEnd)
    {
      const int PageButtonWidth = 92;
      Geometry.Next = { Geometry.Area.x + Geometry.Area.w - 10
                          - PageButtonWidth,
                        Geometry.Area.y + Geometry.Area.h - 30,
                        PageButtonWidth, 22 };
      Geometry.Previous = { Geometry.Next.x - 6 - PageButtonWidth,
                            Geometry.Next.y, PageButtonWidth, 22 };
      Geometry.Bottom = Geometry.Area.y + Geometry.Area.h - 22;
    }
    else
    {
      if(Hud.MenuPages > 1)
      {
        const int ButtonWidth = std::max(1, (Geometry.Area.w - 32) / 3);
        const int ButtonY = Geometry.Area.y + Geometry.Area.h - 42;
        Geometry.Back = { Geometry.Area.x + 10, ButtonY,
                          ButtonWidth, 32 };
        Geometry.Previous = { Geometry.Back.x + ButtonWidth + 6, ButtonY,
                              ButtonWidth, 32 };
        Geometry.Next = { Geometry.Previous.x + ButtonWidth + 6, ButtonY,
                          ButtonWidth, 32 };
        Geometry.Bottom = Geometry.Area.y + Geometry.Area.h - 48;
      }
      else
      {
        Geometry.Bottom = Geometry.Area.y + Geometry.Area.h - 48;
      }
    }
    Geometry.GridColumns = 0;
    Geometry.GridCellWidth = 0;
    Geometry.GridCellHeight = 0;
    Geometry.GridGap = 5;
    Geometry.Detail = { 0, 0, 0, 0 };
    if(IsOptionsMenu())
    {
      const int DetailX = Geometry.Area.x + Geometry.Area.w * 61 / 100;
      Geometry.Detail = { DetailX, Geometry.Top,
                          Geometry.Area.x + Geometry.Area.w - 10 - DetailX,
                          std::max(1, Geometry.Bottom - Geometry.Top) };
    }
    if(IsInventoryGridMenu())
    {
      const int ContentWidth = std::max(1, Geometry.Area.w - 20);
      Geometry.GridColumns = Clamp(ContentWidth / 54, 4, 6);
      Geometry.GridCellWidth = std::max(
        40, (ContentWidth - Geometry.GridGap
          * (Geometry.GridColumns - 1)) / Geometry.GridColumns);
      Geometry.GridCellHeight = 50;
      const int AvailableHeight = std::max(1, Geometry.Bottom - Geometry.Top);
      const int DetailTarget = Clamp(AvailableHeight / 3, 64, 126);
      const int DetailHeight = std::min(DetailTarget,
        std::max(30, AvailableHeight - Geometry.GridCellHeight - 8));
      Geometry.Detail = { Geometry.Area.x + 10,
                          Geometry.Bottom - DetailHeight,
                          ContentWidth, DetailHeight };
      const int GridHeight = std::max(Geometry.GridCellHeight,
        Geometry.Detail.y - Geometry.Top - 8);
      const int GridRows = std::max(1, (GridHeight + Geometry.GridGap)
        / (Geometry.GridCellHeight + Geometry.GridGap));
      Geometry.VisibleCount = GridRows * Geometry.GridColumns;
      const int UsedRows = std::max(1,
        (std::min(int(Hud.MenuOptions.size()), Geometry.VisibleCount)
          + Geometry.GridColumns - 1) / Geometry.GridColumns);
      const int UsedGridBottom = Geometry.Top
        + UsedRows * (Geometry.GridCellHeight + Geometry.GridGap) + 3;
      if(UsedGridBottom < Geometry.Detail.y)
      {
        Geometry.Detail.y = UsedGridBottom;
        const int RemainingHeight = std::max(
          30, Geometry.Bottom - UsedGridBottom);
        Geometry.Detail.h = IsInventoryCategoryMenu()
          ? std::min(92, RemainingHeight) : RemainingHeight;
      }
    }
    else
    {
      if(IsEquipmentMenu())
      {
        const int PageSize = adaptiveui::CalculateEquipmentPageSize(
          CurrentLayout, int(Hud.MenuOptions.size()));
        Geometry.RowHeight = std::min(DesktopSidebarRowHeight,
          std::max(1, (Geometry.Bottom - Geometry.Top) / PageSize));
      }
      Geometry.VisibleCount = std::max(1, (Geometry.Bottom - Geometry.Top)
                                        / Geometry.RowHeight);
      if(IsEquipmentMenu())
        Geometry.VisibleCount = std::min(
          adaptiveui::CalculateEquipmentPageSize(
            CurrentLayout, int(Hud.MenuOptions.size())),
                                          Geometry.VisibleCount);
    }
    Geometry.MaximumScroll = std::max(0, int(Hud.MenuOptions.size())
                                      - Geometry.VisibleCount);
    if(IsOptionsMenu())
    {
      Geometry.VisibleCount = VisibleOptionCount(
        Geometry, Clamp(Hud.MenuScroll, 0,
                        std::max(0, int(Hud.MenuOptions.size()) - 1)));
      Geometry.MaximumScroll = MaximumOptionScroll(Geometry);
    }
    if(IsEquipmentMenu() && !Hud.MenuOptions.empty())
      Geometry.MaximumScroll = (int(Hud.MenuOptions.size()) - 1)
                             / Geometry.VisibleCount
                             * Geometry.VisibleCount;
    if(IsInventoryGridMenu() && Geometry.GridColumns > 0)
      Geometry.MaximumScroll = ((Geometry.MaximumScroll
        + Geometry.GridColumns - 1) / Geometry.GridColumns)
        * Geometry.GridColumns;
    return Geometry;
  }

  SDL_Rect InventoryCell(const MenuGeometry& Geometry, int VisibleIndex)
  {
    const int Column = VisibleIndex % Geometry.GridColumns;
    const int Row = VisibleIndex / Geometry.GridColumns;
    return { Geometry.Area.x + 10
               + Column * (Geometry.GridCellWidth + Geometry.GridGap),
             Geometry.Top
               + Row * (Geometry.GridCellHeight + Geometry.GridGap),
             Geometry.GridCellWidth, Geometry.GridCellHeight };
  }

  struct InventoryLabel
  {
    std::string Name;
    std::string Weight;
  };

  InventoryLabel ParseInventoryLabel(const std::string& Value)
  {
    InventoryLabel Result;
    Result.Name = Value;
    const size_t Open = Value.rfind(" [");
    const size_t Close = Value.rfind(']');
    if(Open == std::string::npos || Close == std::string::npos
       || Close <= Open + 2)
      return Result;

    Result.Name = Value.substr(0, Open);
    const std::string Metadata = Value.substr(Open + 2,
                                               Close - Open - 2);
    size_t Position = 0;
    while(Position < Metadata.size())
    {
      while(Position < Metadata.size()
            && std::isspace((unsigned char)Metadata[Position]))
        ++Position;
      size_t End = Position;
      while(End < Metadata.size()
            && !std::isspace((unsigned char)Metadata[End]))
        ++End;
      const std::string Token = Metadata.substr(Position, End - Position);
      if(Token.size() > 1 && (Token[Token.size() - 1] == 'g'
                              || Token[Token.size() - 1] == 'G'))
      {
        Result.Weight = Token;
        break;
      }
      Position = End;
    }
    return Result;
  }

  struct DesktopComparisonRow
  {
    std::string Label;
    std::string Equipped;
    std::string Selected;
    int Advantage;
  };

  std::string DesktopNumber(int Value)
  {
    return std::to_string(Value);
  }

  std::string DesktopDamage(const adaptiveui::ItemMetrics& Metrics)
  {
    return std::to_string(Metrics.MinimumDamage) + "-"
      + std::to_string(Metrics.MaximumDamage);
  }

  std::string DesktopSkill(const adaptiveui::ItemMetrics& Metrics)
  {
    return std::to_string(Metrics.CategorySkill) + "/"
      + std::to_string(Metrics.SpecificSkill);
  }

  void AddDesktopComparison(std::vector<DesktopComparisonRow>& Rows,
                            const char* Label,
                            const std::string& Equipped,
                            const std::string& Selected, int Advantage)
  {
    DesktopComparisonRow Row;
    Row.Label = Label;
    Row.Equipped = Equipped;
    Row.Selected = Selected;
    Row.Advantage = Advantage;
    Rows.push_back(Row);
  }

  std::vector<DesktopComparisonRow> DesktopComparisonRows(
    const adaptiveui::ItemMetrics& Candidate,
    const adaptiveui::ItemMetrics& Current)
  {
    std::vector<DesktopComparisonRow> Rows;
    if(Candidate.Weapon && Current.Weapon
       && !Candidate.Shield && !Current.Shield)
    {
      AddDesktopComparison(Rows, "DAMAGE", DesktopDamage(Current),
        DesktopDamage(Candidate), Candidate.MinimumDamage
          + Candidate.MaximumDamage - Current.MinimumDamage
          - Current.MaximumDamage);
      AddDesktopComparison(Rows, "HIT", DesktopNumber(Current.ToHit),
        DesktopNumber(Candidate.ToHit), Candidate.ToHit - Current.ToHit);
      if(!Candidate.Accuracy.empty() || !Current.Accuracy.empty())
        AddDesktopComparison(Rows, "ACCURACY", Current.Accuracy,
          Candidate.Accuracy, Candidate.ToHit - Current.ToHit);
      if(!Candidate.Durability.empty() || !Current.Durability.empty())
        AddDesktopComparison(Rows, "DURABILITY", Current.Durability,
          Candidate.Durability,
          Candidate.ArmorValue - Current.ArmorValue);
      if(Candidate.CategorySkill || Candidate.SpecificSkill
         || Current.CategorySkill || Current.SpecificSkill)
        AddDesktopComparison(Rows, "SKILL", DesktopSkill(Current),
          DesktopSkill(Candidate), Candidate.CategorySkill
            + Candidate.SpecificSkill - Current.CategorySkill
            - Current.SpecificSkill);
    }
    if(Candidate.Armor && Current.Armor)
      AddDesktopComparison(Rows, "ARMOR", DesktopNumber(Current.ArmorValue),
        DesktopNumber(Candidate.ArmorValue),
        Candidate.ArmorValue - Current.ArmorValue);
    if(Candidate.Shield && Current.Shield)
    {
      AddDesktopComparison(Rows, "BLOCK", DesktopNumber(Current.Block),
        DesktopNumber(Candidate.Block), Candidate.Block - Current.Block);
      if(!Candidate.BlockQuality.empty() || !Current.BlockQuality.empty())
        AddDesktopComparison(Rows, "BLOCK QUALITY", Current.BlockQuality,
          Candidate.BlockQuality, Candidate.Block - Current.Block);
    }
    if(Candidate.Enchantment || Current.Enchantment)
      AddDesktopComparison(Rows, "ENCHANTMENT",
        DesktopNumber(Current.Enchantment),
        DesktopNumber(Candidate.Enchantment),
        Candidate.Enchantment - Current.Enchantment);
    AddDesktopComparison(Rows, "WEIGHT", DesktopNumber(int(Current.Weight)),
      DesktopNumber(int(Candidate.Weight)),
      int(Current.Weight - Candidate.Weight));
    return Rows;
  }

  void DesktopCenteredText(SDL_Renderer* Renderer, const SDL_Rect& Area,
                           const std::string& Value, int Scale,
                           Uint8 R, Uint8 G, Uint8 B)
  {
    const int X = Area.x + std::max(0,
      (Area.w - TextWidth(Value, Scale)) / 2);
    const int Y = Area.y + std::max(0, (Area.h - Scale * 7) / 2);
    Text(Renderer, X, Y, Elide(Value,
      std::max(1, Area.w / (Scale * 6))), Scale, R, G, B);
  }

  void DesktopCenteredWrappedText(SDL_Renderer* Renderer,
                                  const SDL_Rect& Area,
                                  const std::string& Value, int Scale,
                                  Uint8 R, Uint8 G, Uint8 B)
  {
    const int LineHeight = Scale * 7 + 3;
    const int Columns = std::max(1, Area.w / (Scale * 6));
    std::vector<std::string> Lines = Wrap(Value, Columns);
    if(Lines.size() > 2)
    {
      Lines.resize(2);
      Lines[1] = Elide(Lines[1], Columns);
    }
    int Y = Area.y + std::max(0,
      (Area.h - int(Lines.size()) * LineHeight) / 2);
    for(size_t Index = 0; Index < Lines.size(); ++Index, Y += LineHeight)
    {
      const int X = Area.x + std::max(0,
        (Area.w - TextWidth(Lines[Index], Scale)) / 2);
      Text(Renderer, X, Y, Lines[Index], Scale, R, G, B);
    }
  }

  int DesktopComparisonHeight(const adaptiveui::ItemMetrics& Candidate,
                              const adaptiveui::ItemMetrics& Current)
  {
    return 58 + int(DesktopComparisonRows(Candidate, Current).size()) * 18;
  }

  int DrawDesktopComparison(SDL_Renderer* Renderer, const SDL_Rect& Area,
                            const adaptiveui::ItemMetrics& Candidate,
                            const adaptiveui::ItemMetrics& Current,
                            const std::string& SelectedLabel)
  {
    const std::vector<DesktopComparisonRow> Rows =
      DesktopComparisonRows(Candidate, Current);
    if(Rows.empty() || Area.h <= 0)
      return 0;

    const int HeaderHeight = 58;
    const int RowHeight = 18;
    const int Height = std::min(Area.h,
      HeaderHeight + int(Rows.size()) * RowHeight);
    const SDL_Rect Panel = { Area.x, Area.y, Area.w, Height };
    Fill(Renderer, Panel, 12, 17, 13, 255);
    Outline(Renderer, Panel, 74, 61, 41);
    const int LabelWidth = Clamp(Area.w * 27 / 100, 82, 116);
    const int ValueWidth = std::max(1, (Area.w - LabelWidth) / 2);
    const int SelectedX = Area.x + ValueWidth + LabelWidth;

    DesktopCenteredText(Renderer,
      { Area.x, Area.y + 3, ValueWidth, 14 }, "EQUIPPED", 1,
      181, 169, 143);
    DesktopCenteredText(Renderer,
      { SelectedX, Area.y + 3, ValueWidth, 14 }, "SELECTED", 1,
      154, 220, 119);
    DesktopCenteredWrappedText(Renderer,
      { Area.x + 3, Area.y + 18, ValueWidth - 6, 34 },
      Current.Label.empty() ? "equipped item" : Current.Label,
      1, 225, 211, 176);
    DesktopCenteredWrappedText(Renderer,
      { SelectedX + 3, Area.y + 18, ValueWidth - 6, 34 },
      SelectedLabel, 1, 232, 226, 194);
    Fill(Renderer, { Area.x, Area.y + HeaderHeight - 1,
                     Area.w, 1 }, 92, 83, 55);

    for(size_t Index = 0; Index < Rows.size(); ++Index)
    {
      const int Y = Area.y + HeaderHeight + int(Index) * RowHeight;
      if(Y + RowHeight > Area.y + Height)
        break;
      if(Index % 2)
        Fill(Renderer, { Area.x + 1, Y, Area.w - 2, RowHeight },
             15, 21, 17, 255);
      DesktopCenteredText(Renderer,
        { Area.x + 3, Y, ValueWidth - 6, RowHeight },
        Rows[Index].Equipped, 1, 224, 216, 190);
      DesktopCenteredText(Renderer,
        { Area.x + ValueWidth, Y, LabelWidth, RowHeight },
        Rows[Index].Label, 1, 222, 189, 91);
      const bool Better = Rows[Index].Advantage > 0;
      const bool Worse = Rows[Index].Advantage < 0;
      DesktopCenteredText(Renderer,
        { SelectedX + 3, Y, ValueWidth - 6, RowHeight },
        Rows[Index].Selected, 1,
        Better ? 154 : Worse ? 239 : 224,
        Better ? 220 : Worse ? 109 : 216,
        Better ? 119 : Worse ? 91 : 190);
      Fill(Renderer, { Area.x, Y + RowHeight - 1, Area.w, 1 },
           43, 38, 31);
    }
    return Height;
  }

  void DrawItemMetric(SDL_Renderer* Renderer, int X, int& Y,
                      const std::string& Label,
                      const std::string& Value)
  {
    if(Value.empty())
      return;
    Text(Renderer, X, Y, Label + " " + Value,
         1, 224, 216, 190);
    Y += 11;
  }

  void DrawStandaloneItemMetrics(SDL_Renderer* Renderer, int X, int& Y,
                                 const adaptiveui::ItemMetrics& Metrics)
  {
    if(Metrics.Shield)
    {
      DrawItemMetric(Renderer, X, Y, "ARMOR",
                     std::to_string(Metrics.ArmorValue));
      DrawItemMetric(Renderer, X, Y, "BLOCK", Metrics.BlockQuality);
    }
    else
    {
      if(Metrics.Armor)
        DrawItemMetric(Renderer, X, Y, "ARMOR",
                       std::to_string(Metrics.ArmorValue));
      if(Metrics.Weapon)
      {
        DrawItemMetric(Renderer, X, Y, "DAMAGE",
          std::to_string(Metrics.MinimumDamage) + "-"
            + std::to_string(Metrics.MaximumDamage));
        DrawItemMetric(Renderer, X, Y, "HIT", Metrics.Accuracy);
        DrawItemMetric(Renderer, X, Y, "DURABILITY", Metrics.Durability);
      }
    }
    if(Metrics.Enchantment)
      DrawItemMetric(Renderer, X, Y, "ENCHANT",
                     std::to_string(Metrics.Enchantment));
    if(Metrics.CategorySkill || Metrics.SpecificSkill)
      DrawItemMetric(Renderer, X, Y, "SKILL C/S",
        std::to_string(Metrics.CategorySkill) + "/"
          + std::to_string(Metrics.SpecificSkill));
    DrawItemMetric(Renderer, X, Y, "WEIGHT",
                   std::to_string(Metrics.Weight) + "G");
  }

  void DrawDesktopDetailText(SDL_Renderer* Renderer, const SDL_Rect& Area,
                             const std::string& Detail)
  {
    if(Area.w <= 0 || Area.h <= 0)
      return;
    SDL_RenderSetClipRect(Renderer, &Area);
    const std::vector<std::string> Lines = Wrap(
      Detail.empty() ? "NO DESCRIPTION AVAILABLE" : Detail,
      std::max(1, Area.w / 6));
    int Y = Area.y;
    for(size_t Line = 0; Line < Lines.size(); ++Line, Y += 11)
    {
      const std::string& Value = Lines[Line];
      const bool Heading = Value == "STATUS" || Value == "MAIN MATERIAL"
        || Value == "SECONDARY MATERIAL" || Value == "TOOLS"
        || Value == "FACILITIES" || Value == "DESCRIPTION";
      const bool Ready = Value.find("READY") != std::string::npos
        || Value == "Ready to choose ingredients";
      const bool Missing = Value.find("MISSING") != std::string::npos
        || Value == "Cannot craft with current supplies";
      Text(Renderer, Area.x, Y, Value, 1,
           Heading ? 236 : Ready ? 137 : Missing ? 214 : 224,
           Heading ? 204 : Ready ? 186 : Missing ? 112 : 216,
           Heading ? 126 : Ready ? 106 : Missing ? 92 : 190);
    }
    SDL_RenderSetClipRect(Renderer, 0);
  }

  void DrawInventoryGrid(SDL_Renderer* Renderer,
                         const MenuGeometry& Geometry)
  {
    if(Hud.MenuSelected >= 0)
    {
      const int SelectedPosition = DisplayPositionForMenuIndex(
        Hud.MenuSelected);
      if(SelectedPosition < Hud.MenuScroll)
        Hud.MenuScroll = (SelectedPosition / Geometry.GridColumns)
                       * Geometry.GridColumns;
      else if(SelectedPosition >= Hud.MenuScroll + Geometry.VisibleCount)
        Hud.MenuScroll = ((SelectedPosition / Geometry.GridColumns)
          - Geometry.VisibleCount / Geometry.GridColumns + 1)
          * Geometry.GridColumns;
    }
    Hud.MenuScroll = Clamp(Hud.MenuScroll, 0, Geometry.MaximumScroll);
    const int First = Hud.MenuScroll;
    const int Last = std::min(int(Hud.MenuOptions.size()),
                              First + Geometry.VisibleCount);
    for(int DisplayIndex = First; DisplayIndex < Last; ++DisplayIndex)
    {
      const int Index = MenuIndexForDisplayPosition(DisplayIndex);
      const SDL_Rect Cell = InventoryCell(Geometry, DisplayIndex - First);
      const bool Selected = Index == Hud.MenuSelected;
      const bool Available = Index >= int(Hud.MenuAvailability.size())
                          || Hud.MenuAvailability[Index] != 0;
      const bool Equipped = Hud.EquipmentComparisonActive
                         && DisplayIndex == 0;
      Fill(Renderer, Cell,
           Available ? (Selected ? 31 : 18) : (Selected ? 31 : 17),
           Available ? (Selected ? 62 : 22) : (Selected ? 37 : 18),
           Available ? (Selected ? 39 : 21) : (Selected ? 33 : 18), 255);
      Outline(Renderer, Cell,
              Available ? (Selected ? 119 : 62) : (Selected ? 104 : 51),
              Available ? (Selected ? 185 : 53) : (Selected ? 103 : 49),
              Available ? (Selected ? 103 : 39) : (Selected ? 92 : 47));
      if(Equipped)
      {
        const SDL_Rect EquippedOutline = { Cell.x + 2, Cell.y + 2,
          std::max(1, Cell.w - 4), std::max(1, Cell.h - 4) };
        Outline(Renderer, EquippedOutline, 221, 190, 112);
      }
      if(Index < int(Hud.MenuIconSources.size())
         && Hud.MenuIconSources[Index].w > 0 && CurrentMenuTexture)
      {
        const int IconSize = std::max(1, std::min(Cell.w - 6, Cell.h - 6));
        const SDL_Rect Icon = { Cell.x + (Cell.w - IconSize) / 2,
                                Cell.y + (Cell.h - IconSize) / 2,
                                IconSize, IconSize };
        if(!Available)
          SDL_SetTextureColorMod(CurrentMenuTexture, 105, 105, 100);
        SDL_RenderCopy(Renderer, CurrentMenuTexture,
                       &Hud.MenuIconSources[Index], &Icon);
        if(!Available)
          SDL_SetTextureColorMod(CurrentMenuTexture, 255, 255, 255);
      }
      else
        Centered(Renderer, Cell, "?", 2,
                 Available ? 181 : 105, Available ? 169 : 101,
                 Available ? 143 : 94);
    }

    Fill(Renderer, Geometry.Detail, 12, 14, 13, 255);
    Outline(Renderer, Geometry.Detail, 74, 61, 41);
    if(Hud.MenuSelected >= 0
       && Hud.MenuSelected < int(Hud.MenuOptions.size()))
    {
      const int Index = Hud.MenuSelected;
      const InventoryLabel Label = ParseInventoryLabel(
        Hud.MenuOptions[Index]);
      std::string Detail = Index < int(Hud.MenuDetails.size())
        ? Hud.MenuDetails[Index] : "";
      std::string ItemDescription;
      const bool CraftItemMenu = Hud.MenuTitle.find("Craft an item - ") == 0
        && Hud.MenuTitle.find("categories") == std::string::npos
        && Hud.MenuTitle.find("material routes") == std::string::npos;
      const std::string DescriptionMarker = "@ITEM_DESCRIPTION@\n";
      if(CraftItemMenu && Detail.find(DescriptionMarker) == 0)
      {
        const size_t DescriptionEnd = Detail.find("\n\n");
        if(DescriptionEnd != std::string::npos)
        {
          ItemDescription = Detail.substr(DescriptionMarker.size(),
            DescriptionEnd - DescriptionMarker.size());
          Detail = Detail.substr(DescriptionEnd + 2);
        }
      }
      const int NameScale = 2;
      const std::vector<std::string> NameLines = Wrap(
        Label.Name,
        std::max(1, (Geometry.Detail.w - 16)
          / (6 * NameScale)));
      const int Left = Geometry.Detail.x + 8;
      const int Right = Geometry.Detail.x + Geometry.Detail.w - 8;
      int Y = Geometry.Detail.y + 8;
      const int MaximumNameLines = std::min(2, int(NameLines.size()));
      for(int Line = 0; Line < MaximumNameLines; ++Line,
          Y += 17)
        Text(Renderer, Left, Y, NameLines[Line],
             NameScale, 248, 224, 154);
      if(!ItemDescription.empty())
      {
        Y += 2;
        const std::vector<std::string> DescriptionLines = Wrap(
          ItemDescription, std::max(1, (Right - Left) / 6));
        for(size_t Line = 0; Line < DescriptionLines.size(); ++Line,
            Y += 11)
          Text(Renderer, Left, Y, DescriptionLines[Line],
               1, 224, 216, 190);
      }
      Y += 5;
      Fill(Renderer, { Left, Y, std::max(1, Right - Left), 1 },
           67, 53, 38);
      Y += 7;
      const adaptiveui::ItemMetrics* Candidate =
        Index < int(Hud.MenuItemMetrics.size())
          && Hud.MenuItemMetrics[Index].Present
        ? &Hud.MenuItemMetrics[Index] : 0;
      const adaptiveui::ItemMetrics* Current = 0;
      if(Hud.EquipmentComparisonActive)
        Current = &Hud.EquippedItemMetrics;
      else if(Index < int(Hud.MenuComparisonMetrics.size())
              && Hud.MenuComparisonMetrics[Index].Present)
        Current = &Hud.MenuComparisonMetrics[Index];
      bool DetailDrawn = false;
      if(Candidate)
      {
        if(Current && Current->Present)
        {
          const int DetailBottom = Geometry.Detail.y + Geometry.Detail.h - 6;
          const int DesiredComparisonHeight = DesktopComparisonHeight(
            *Candidate, *Current);
          const int ReservedComparisonHeight = std::min(
            std::max(1, DetailBottom - Y), DesiredComparisonHeight);
          const int ComparisonY = DetailBottom - ReservedComparisonHeight;
          const SDL_Rect DescriptionArea = { Left, Y,
            std::max(1, Right - Left),
            std::max(0, ComparisonY - Y - 7) };
          DrawDesktopDetailText(Renderer, DescriptionArea, Detail);
          DetailDrawn = true;
          const int PaintedComparisonHeight = DrawDesktopComparison(
            Renderer, { Left, ComparisonY, std::max(1, Right - Left),
                        ReservedComparisonHeight },
            *Candidate, *Current, Label.Name);
          Y = ComparisonY + PaintedComparisonHeight;
        }
        else
        {
          if(Hud.EquipmentComparisonActive)
          {
            Text(Renderer, Left, Y, "CURRENT SLOT EMPTY",
                 1, 181, 169, 143);
            Y += 11;
          }
          DrawStandaloneItemMetrics(Renderer, Left, Y, *Candidate);
        }
        Y += 4;
      }
      else if(Hud.EquipmentComparisonActive)
      {
        Text(Renderer, Left, Y, "SELECT NONE TO UNEQUIP",
             1, 181, 169, 143);
        Y += 15;
      }
      else if(!IsInventoryCategoryMenu())
      {
        Text(Renderer, Left, Y, "WEIGHT", 2, 206, 188, 145);
        Text(Renderer, Left + 84, Y + 4,
             Label.Weight.empty() ? "UNKNOWN" : Label.Weight,
             1, 240, 230, 202);
        Y += 22;
      }
      else
      {
        Text(Renderer, Left, Y, "SELECT TO OPEN", 1, 181, 169, 143);
        Y += 18;
      }
      Fill(Renderer, { Left, Y, std::max(1, Right - Left), 1 },
           67, 53, 38);
      Y += 7;
      if(!DetailDrawn)
        DrawDesktopDetailText(Renderer,
          { Left, Y, std::max(1, Right - Left),
            std::max(1, Geometry.Detail.y + Geometry.Detail.h - Y - 6) },
          Detail);
    }
    else
      Centered(Renderer, Geometry.Detail, "HIGHLIGHT AN ITEM", 1,
               181, 169, 143);
  }

  void DrawMenu(SDL_Renderer* Renderer)
  {
    const MenuGeometry Geometry = GetMenuGeometry();
    const SDL_Rect Area = Geometry.Area;
    Frame(Renderer, Area);
    const int Padding = Geometry.FrontEnd ? 22 : 10;
    const std::string Title = DisplayMenuTitle();
    const std::string Subtitle = DisplayMenuSubtitle();
    if(Geometry.FrontEnd)
      Centered(Renderer, { Area.x + Padding, Area.y + 13,
                           std::max(1, Area.w - Padding * 2), 27 },
               Title, 3, 248, 224, 154);
    else if(!IsInventoryGridMenu())
    {
      const int Columns = std::max(
        1, (Area.w - Padding * 2) / (6 * DesktopSidebarFontScale));
      const std::vector<std::string> TitleLines = Wrap(Title, Columns);
      int HeaderY = Area.y + 10;
      for(size_t Line = 0; Line < TitleLines.size(); ++Line,
          HeaderY += DesktopSidebarLineHeight)
        Text(Renderer, Area.x + Padding, HeaderY, TitleLines[Line],
             DesktopSidebarFontScale, 248, 224, 154);
      if(!Subtitle.empty() && !IsOptionsMenu())
      {
        HeaderY += 5;
        const std::vector<std::string> SubtitleLines =
          Wrap(Subtitle, Columns);
        for(size_t Line = 0; Line < SubtitleLines.size(); ++Line,
            HeaderY += DesktopSidebarLineHeight)
          Text(Renderer, Area.x + Padding, HeaderY, SubtitleLines[Line],
               DesktopSidebarFontScale, 181, 169, 143);
      }
    }
    if(Geometry.FrontEnd)
      Text(Renderer, Area.x + Padding, Area.y + 50,
           Elide(Subtitle,
                 std::max(1, (Area.w - Padding * 2) / 6)),
           1, 181, 169, 143);
    if(!IsInventoryGridMenu())
      Fill(Renderer, { Area.x + Padding, Geometry.Top - 9,
                       std::max(1, Area.w - Padding * 2), 1 },
           67, 53, 38);
    if(IsInventoryGridMenu())
      DrawInventoryGrid(Renderer, Geometry);
    else if(IsCraftingGuideTextPage())
    {
      const SDL_Rect Content = {
        Area.x + Padding, Geometry.Top,
        std::max(1, Area.w - Padding * 2),
        std::max(1, Geometry.Bottom - Geometry.Top)
      };
      SDL_RenderSetClipRect(Renderer, &Content);
      int Y = Content.y;
      const int Columns = std::max(1, Content.w / 12);
      for(size_t Index = 0; Index < Hud.MenuOptions.size(); ++Index)
      {
        const std::string Section = Hud.MenuOptions[Index];
        const std::string Separator = " :: ";
        const size_t Break = Section.find(Separator);
        const std::string Heading = Break == std::string::npos
          ? Section : Section.substr(0, Break);
        const std::string Body = Break == std::string::npos
          ? "" : Section.substr(Break + Separator.size());

        Text(Renderer, Content.x, Y, Heading, 2, 236, 204, 126);
        Y += 18;
        Fill(Renderer, { Content.x, Y, Content.w, 1 }, 67, 53, 38);
        Y += 7;
        const std::vector<std::string> Lines = Wrap(Body, Columns);
        for(size_t Line = 0; Line < Lines.size(); ++Line, Y += 16)
          Text(Renderer, Content.x, Y, Lines[Line], 2, 224, 216, 190);
        Y += 12;
      }
      SDL_RenderSetClipRect(Renderer, 0);
    }
    else
    {
      if(IsEquipmentMenu() && Hud.MenuSelected >= 0)
        Hud.MenuScroll = Hud.MenuSelected / Geometry.VisibleCount
                       * Geometry.VisibleCount;
      if(IsOptionsMenu() && Hud.MenuSelected >= 0)
      {
        if(Hud.MenuSelected < Hud.MenuScroll)
          Hud.MenuScroll = Hud.MenuSelected;
        while(Hud.MenuSelected >= Hud.MenuScroll
              + VisibleOptionCount(Geometry, Hud.MenuScroll)
              && Hud.MenuScroll < Geometry.MaximumScroll)
          ++Hud.MenuScroll;
      }
      Hud.MenuScroll = Clamp(Hud.MenuScroll, 0, Geometry.MaximumScroll);
      const int First = Hud.MenuScroll;
      const int Last = std::min(int(Hud.MenuOptions.size()), First
        + (IsOptionsMenu() ? VisibleOptionCount(Geometry, First)
                           : Geometry.VisibleCount));
      const int RowScale = Geometry.FrontEnd ? 2 : DesktopSidebarFontScale;
      for(int Index = First; Index < Last; ++Index)
      {
        const int Y = IsOptionsMenu()
          ? OptionRowRect(Geometry, Index, First).y
          : Geometry.Top + (Index - First) * Geometry.RowHeight;
        SDL_Rect Row = { Area.x + Padding, Y,
                         IsOptionsMenu()
                           ? std::max(1, Geometry.Detail.x
                               - (Area.x + Padding) - 8)
                           : std::max(1, Area.w - Padding * 2),
                         Geometry.RowHeight - 3 };
        const bool Selected = Index == Hud.MenuSelected;
        Fill(Renderer, Row, Selected ? 31 : 19, Selected ? 62 : 23,
             Selected ? 39 : 23, 255);
        if(Selected)
          Fill(Renderer, { Row.x, Row.y, 3, Row.h }, 119, 185, 103);
        else
          Fill(Renderer, { Row.x, Row.y + Row.h - 1, Row.w, 1 },
               43, 38, 31);
        if(Geometry.FrontEnd)
        {
          const std::string Value = Elide(Hud.MenuOptions[Index],
            std::max(1, (Row.w - 12) / (RowScale * 6)));
          Text(Renderer, Row.x + 7,
               Row.y + std::max(3, (Row.h - RowScale * 7) / 2),
               Value, RowScale, Selected ? 244 : 224,
               Selected ? 236 : 216, Selected ? 206 : 190);
        }
        else
        {
          if(IsOptionsMenu())
          {
            const OptionLabel Setting = ParseOptionLabel(
              Hud.MenuOptions[Index]);
            const std::string Group = Index < int(Hud.MenuGroups.size())
              ? Hud.MenuGroups[Index] : "";
            const bool NewGroup = StartsOptionCategory(Index, First);
            if(NewGroup)
            {
              const SDL_Rect Category = { Row.x,
                Row.y - OptionCategoryHeight, Row.w, OptionCategoryHeight - 2 };
              Fill(Renderer, Category, 25, 23, 18, 255);
              Fill(Renderer, { Category.x, Category.y + Category.h - 1,
                               Category.w, 1 }, 111, 88, 45);
              Text(Renderer, Category.x + 8, Category.y + 4,
                   Elide(Group, std::max(1, (Row.w - 16) / 12)),
                   2, 236, 204, 126);
            }
            const int ValueWidth = Setting.Value.empty()
              ? 0 : TextWidth(Setting.Value, 2) + 12;
            Text(Renderer, Row.x + 8, Row.y + 9,
                 Elide(Setting.Name,
                   std::max(1, (Row.w - 16 - ValueWidth) / 12)),
                 2, Selected ? 244 : 224, Selected ? 236 : 216,
                 Selected ? 206 : 190);
            if(!Setting.Value.empty())
              Text(Renderer,
                   Row.x + Row.w - 8 - TextWidth(Setting.Value, 2),
                    Row.y + 9, Setting.Value, 2,
                   Selected ? 176 : 137, Selected ? 214 : 186,
                   Selected ? 123 : 106);
          }
          else if(IsEquipmentMenu())
          {
            const EquipmentRow Equipment = ParseEquipmentRow(
              Hud.MenuOptions[Index]);
            const int HeaderHeight = 21;
            const SDL_Rect Header = { Row.x, Row.y, Row.w, HeaderHeight };
            Fill(Renderer, Header, Selected ? 35 : 16,
                 Selected ? 69 : 20, Selected ? 42 : 19, 255);
            Fill(Renderer, { Header.x, Header.y + Header.h - 1,
                             Header.w, 1 },
                 Selected ? 88 : 67, Selected ? 113 : 53,
                 Selected ? 72 : 38);
            const int TextX = Row.x + 7;
            Text(Renderer, TextX, Row.y + 3, Equipment.Slot + ":",
                 2, Selected ? 244 : 224, Selected ? 236 : 216,
                 Selected ? 206 : 190);
            std::vector<std::string> ItemLines = Wrap(
              Equipment.Item.empty() ? "-" : Equipment.Item,
              std::max(1, (Row.w - 14) / 6));
            const int ItemLineCount = std::min(2, int(ItemLines.size()));
            int ItemY = Header.y + Header.h + 4;
            for(int Line = 0; Line < ItemLineCount; ++Line, ItemY += 11)
              Text(Renderer, TextX, ItemY, ItemLines[Line], 1,
                   Selected ? 226 : 190, Selected ? 220 : 184,
                   Selected ? 195 : 164);
          }
          else
          {
            std::vector<std::string> Lines = Wrap(
              Hud.MenuOptions[Index],
              std::max(1, (Row.w - 14) / (RowScale * 6)));
            if(Lines.empty())
              Lines.push_back("");
            const int TextHeight = int(Lines.size())
                                 * DesktopSidebarLineHeight - 2;
            int TextY = Row.y + std::max(3, (Row.h - TextHeight) / 2);
            for(size_t Line = 0; Line < Lines.size(); ++Line,
                TextY += DesktopSidebarLineHeight)
              Text(Renderer, Row.x + 7, TextY, Lines[Line], RowScale,
                   Selected ? 244 : 224, Selected ? 236 : 216,
                   Selected ? 206 : 190);
          }
        }
      }
    }
    if(IsOptionsMenu())
    {
      Fill(Renderer, Geometry.Detail, 12, 14, 13, 255);
      Outline(Renderer, Geometry.Detail, 74, 61, 41);
      if(Hud.MenuSelected >= 0
         && Hud.MenuSelected < int(Hud.MenuOptions.size()))
      {
        const int Index = Hud.MenuSelected;
        const OptionLabel Setting = ParseOptionLabel(Hud.MenuOptions[Index]);
        const std::string Group = Index < int(Hud.MenuGroups.size())
          ? Hud.MenuGroups[Index] : "SETTING";
        int Y = Geometry.Detail.y + 14;
        Text(Renderer, Geometry.Detail.x + 14, Y,
             Elide(Group, std::max(1, (Geometry.Detail.w - 28) / 12)),
             2, 236, 204, 126);
        Y += 22;
        const std::vector<std::string> NameLines = Wrap(Setting.Name,
          std::max(1, (Geometry.Detail.w - 28) / 12));
        for(size_t Line = 0; Line < NameLines.size() && Line < 3;
            ++Line, Y += 17)
          Text(Renderer, Geometry.Detail.x + 14, Y,
               NameLines[Line], 2, 248, 224, 154);
        if(!Setting.Value.empty())
        {
          Y += 5;
          Text(Renderer, Geometry.Detail.x + 14, Y,
               "CURRENT VALUE", 1, 158, 145, 98);
          Y += 14;
          Text(Renderer, Geometry.Detail.x + 14, Y,
               Elide(Setting.Value,
                 std::max(1, (Geometry.Detail.w - 28) / 12)),
               2, 176, 214, 123);
          Y += 22;
        }
        Fill(Renderer, { Geometry.Detail.x + 14, Y,
                         std::max(1, Geometry.Detail.w - 28), 1 },
             67, 53, 38);
        Y += 10;
        const std::string Detail = Index < int(Hud.MenuDetails.size())
          ? Hud.MenuDetails[Index] : "";
        const SDL_Rect DetailText = {
          Geometry.Detail.x + 14, Y,
          std::max(1, Geometry.Detail.w - 28),
          std::max(1, Geometry.Detail.y + Geometry.Detail.h - Y - 12)
        };
        SDL_RenderSetClipRect(Renderer, &DetailText);
        TopWrapped(Renderer, DetailText,
                   Detail.empty() ? "No additional information." : Detail,
                   2, 224, 216, 190);
        SDL_RenderSetClipRect(Renderer, 0);
      }
    }
    if(Geometry.FrontEnd)
      Text(Renderer, Area.x + Padding, Area.y + Area.h - 15,
           "SELECT AN OPTION", 1, 125, 153, 105);
    else
    {
      const bool PickupButtons = Hud.MenuKind == adaptiveui::MENU_PICKUP_GRID;
      const bool CanConfirm = Hud.MenuSelected >= 0
                           && Hud.MenuSelected < int(Hud.MenuOptions.size());
      if(PickupButtons)
      {
        const std::vector<DesktopItemAction> Actions =
          DesktopPickupActions();
        for(size_t Index = 0;
            Index < Actions.size() && Index < Geometry.ItemActions.size();
            ++Index)
        {
          Fill(Renderer, Geometry.ItemActions[Index],
               CanConfirm ? 25 : 19, CanConfirm ? 43 : 23,
               CanConfirm ? 58 : 28, 255);
          Outline(Renderer, Geometry.ItemActions[Index],
                  CanConfirm ? 86 : 70, CanConfirm ? 126 : 70,
                  CanConfirm ? 169 : 70);
          Centered(Renderer, Geometry.ItemActions[Index],
                   Actions[Index].Label, 2,
                   CanConfirm ? 224 : 130, CanConfirm ? 216 : 125,
                   CanConfirm ? 190 : 115);
        }
        Fill(Renderer, Geometry.Confirm,
             CanConfirm ? 24 : 19, CanConfirm ? 55 : 23,
             CanConfirm ? 35 : 28, 255);
        Outline(Renderer, Geometry.Confirm,
                CanConfirm ? 102 : 70, CanConfirm ? 169 : 70,
                CanConfirm ? 92 : 70);
        Centered(Renderer, Geometry.Confirm, "STASH", 2,
                 CanConfirm ? 190 : 130, CanConfirm ? 226 : 125,
                 CanConfirm ? 157 : 115);
      }
      Fill(Renderer, Geometry.Back, 49, 20, 18, 255);
      Outline(Renderer, Geometry.Back, 168, 71, 58);
      Centered(Renderer, Geometry.Back, "BACK", 2, 228, 168, 139);
      if(IsInventoryGridMenu()
         && !PickupButtons
         && Hud.InventoryCurrentWeight >= 0
         && Hud.InventoryMaximumWeight >= 0)
      {
        const std::string Weight = std::to_string(Hud.InventoryCurrentWeight)
          + "G / " + std::to_string(Hud.InventoryMaximumWeight) + "G";
        const int WeightY = Hud.MenuPages > 1
          ? Area.y + Area.h - 62 : Area.y + Area.h - 24;
        Text(Renderer, Area.x + Area.w - Padding - TextWidth(Weight, 1),
             WeightY, Weight, 1, 224, 216, 190);
      }
    }
    if(Hud.MenuPages > 1
       && Hud.MenuKind != adaptiveui::MENU_PICKUP_GRID)
    {
      const std::string Page = std::string("PAGE ")
        + std::to_string(Hud.MenuPage) + "/" + std::to_string(Hud.MenuPages);
      Text(Renderer, Geometry.FrontEnd
            ? Area.x + Area.w - Padding - TextWidth(Page, 1)
            : Area.x + (Area.w - TextWidth(Page, 1)) / 2,
            Geometry.FrontEnd ? Area.y + 50 : Area.y + Area.h - 57,
            Page, 1, 168, 157, 132);
      const bool HasPrevious = Hud.MenuPage > 1;
      const bool HasNext = Hud.MenuPage < Hud.MenuPages;
      Fill(Renderer, Geometry.Previous, HasPrevious ? 22 : 16,
           HasPrevious ? 39 : 19, HasPrevious ? 26 : 19, 255);
      Outline(Renderer, Geometry.Previous, HasPrevious ? 104 : 56,
              HasPrevious ? 91 : 49, HasPrevious ? 58 : 39);
      Centered(Renderer, Geometry.Previous, "PREV PAGE", 2,
               HasPrevious ? 224 : 102, HasPrevious ? 216 : 98,
               HasPrevious ? 190 : 90);
      Fill(Renderer, Geometry.Next, HasNext ? 22 : 16,
           HasNext ? 39 : 19, HasNext ? 26 : 19, 255);
      Outline(Renderer, Geometry.Next, HasNext ? 104 : 56,
              HasNext ? 91 : 49, HasNext ? 58 : 39);
      Centered(Renderer, Geometry.Next, "NEXT PAGE", 2,
               HasNext ? 224 : 102, HasNext ? 216 : 98,
               HasNext ? 190 : 90);
    }
  }

  void DrawScreenText(SDL_Renderer* Renderer)
  {
    SDL_Rect Area = CurrentLayout.MapPanel;
    if(!HasGameplayContext())
    {
      const int Width = Clamp(CurrentLayout.OutputWidth - 64, 720, 1120);
      const int Height = Clamp(CurrentLayout.OutputHeight - 64, 440, 680);
      Area = { (CurrentLayout.OutputWidth - Width) / 2,
               (CurrentLayout.OutputHeight - Height) / 2,
               Width, Height };
    }
    Frame(Renderer, Area);
    Text(Renderer, Area.x + 18, Area.y + 16,
         Elide(Hud.ScreenTextTitle.empty() ? "STORY" : Hud.ScreenTextTitle,
               std::max(1, (Area.w - 36) / 12)),
         2, 248, 224, 154);
    Fill(Renderer, { Area.x + 18, Area.y + 41,
                     std::max(1, Area.w - 36), 1 }, 67, 53, 38);
    TopWrapped(Renderer, { Area.x + 26, Area.y + 58,
                           std::max(1, Area.w - 52),
                           std::max(1, Area.h - 96) },
               Hud.ScreenText, 2, 240, 230, 202);
    Text(Renderer, Area.x + 18, Area.y + Area.h - 18,
         "CLICK OR ENTER TO CONTINUE", 1, 176, 214, 123);
  }

  int ActionCountForCategory(int Category)
  {
    int Count = 0;
    for(size_t Index = 0; Index < Hud.Actions.size(); ++Index)
      if(Hud.Actions[Index].Category == Category)
        ++Count;
    return Count;
  }

  int MaximumActionScroll(int Category)
  {
    const int Rows = std::max(1, CurrentLayout.ActionArea.h
                              / DesktopSidebarRowHeight);
    return std::max(0, ActionCountForCategory(Category) - Rows * 2);
  }

  void RebuildActionRects()
  {
    CurrentLayout.ActionButtons.clear();
    CurrentLayout.ActionButtonIndices.clear();
    int Seen = 0;
    const int Category = Clamp(CurrentLayout.ActiveCategory, 0,
                               adaptiveui::ACTION_GROUPS - 1);
    const int Columns = 2;
    const int RowHeight = DesktopSidebarRowHeight;
    const int VisibleRows = std::max(1, CurrentLayout.ActionArea.h
                                     / RowHeight);
    const int Capacity = VisibleRows * Columns;
    for(int Index = 0; Index < int(Hud.Actions.size()); ++Index)
      if(Hud.Actions[Index].Category == Category)
      {
        if(Seen++ < CurrentLayout.ActionScroll)
          continue;
        if(int(CurrentLayout.ActionButtonIndices.size()) >= Capacity)
          break;
        CurrentLayout.ActionButtonIndices.push_back(Index);
      }
    for(int Index = 0; Index < int(CurrentLayout.ActionButtonIndices.size());
        ++Index)
    {
      const int Column = Index % Columns;
      const int Row = Index / Columns;
      const int X0 = CurrentLayout.ActionArea.x
                   + CurrentLayout.ActionArea.w * Column / Columns;
      const int X1 = CurrentLayout.ActionArea.x
                   + CurrentLayout.ActionArea.w * (Column + 1) / Columns;
      const int Y0 = CurrentLayout.ActionArea.y + Row * RowHeight;
      CurrentLayout.ActionButtons.push_back({ X0 + 2, Y0 + 2,
                                              std::max(1, X1 - X0 - 4),
                                              RowHeight - 4 });
    }
  }

  bool UpdateGameplaySnapshot(SDL_Renderer* Renderer,
                              SDL_Texture* GameTexture)
  {
    if(!Renderer || !GameTexture)
      return false;
    Uint32 Format = SDL_PIXELFORMAT_RGB565;
    int Access = 0;
    int Width = 0;
    int Height = 0;
    if(SDL_QueryTexture(GameTexture, &Format, &Access, &Width, &Height) != 0)
      return false;
    if(SnapshotRenderer != Renderer)
    {
      SnapshotRenderer = Renderer;
      GameplaySnapshot = 0;
      SnapshotWidth = 0;
      SnapshotHeight = 0;
      SnapshotValid = false;
    }
    if(!GameplaySnapshot || SnapshotWidth != Width
       || SnapshotHeight != Height)
    {
      if(GameplaySnapshot)
        SDL_DestroyTexture(GameplaySnapshot);
      GameplaySnapshot = SDL_CreateTexture(Renderer, Format,
        SDL_TEXTUREACCESS_TARGET, Width, Height);
      if(!GameplaySnapshot)
        GameplaySnapshot = SDL_CreateTexture(Renderer,
          SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
          Width, Height);
      SnapshotWidth = Width;
      SnapshotHeight = Height;
      SnapshotValid = false;
    }
    if(!GameplaySnapshot)
      return false;
    SDL_Texture* PreviousTarget = SDL_GetRenderTarget(Renderer);
    if(SDL_SetRenderTarget(Renderer, GameplaySnapshot) != 0)
      return false;
    Fill(Renderer, { 0, 0, Width, Height }, 0, 0, 0, 255);
    const bool Copied = SDL_RenderCopy(Renderer, GameTexture, 0, 0) == 0;
    SDL_SetRenderTarget(Renderer, PreviousTarget);
    SnapshotValid = Copied;
    return SnapshotValid;
  }
#endif
}

adaptiveui::ItemMetrics::ItemMetrics()
  : ItemId(0), Present(false), Armor(false), Weapon(false), Shield(false),
    Equippable(false), Actions(0), Weight(0),
    ArmorValue(0), MinimumDamage(0), MaximumDamage(0), ToHit(0), Block(0),
    Enchantment(0), CategorySkill(0), SpecificSkill(0)
{
}

adaptiveui::HudModel::HudModel()
  : PromptActive(false), PromptShowsInput(false), PromptNumeric(false),
    PromptCapturesKey(false), PromptConfirmsKeyTransfer(false),
    PromptConfirmsChoice(false), PromptOffersQuitChoices(false),
    PositionPrompt(false),
    PaperDollScreen(false), PaperDollSource({ 0, 0, 0, 0 }),
    ScreenTextActive(false), ScreenTextTitle("STORY"), MenuActive(false),
    MenuKind(MENU_ROWS), MenuIconGrid(false), EquipmentComparisonActive(false),
    InventoryCurrentWeight(-1),
    InventoryMaximumWeight(-1),
    MenuSelected(-1), MenuScroll(0), MenuPage(1), MenuPages(1), QuestionChoiceCount(0),
    MapScreen(false), MapSource({ 0, 0, 0, 0 }), HasMapSource(false),
    MapFocusX(0), MapFocusY(0), PlayerFocusX(0), PlayerFocusY(0)
{
  for(int Index = 0; Index < 9; ++Index)
    QuestionChoices[Index] = 0;
}

adaptiveui::MobileMenuLayout::MobileMenuLayout()
  : Area({ 0, 0, 0, 0 }), PaperDoll({ 0, 0, 0, 0 }),
    Conditions({ 0, 0, 0, 0 }), GridViewport({ 0, 0, 0, 0 }),
    Detail({ 0, 0, 0, 0 }), Footer({ 0, 0, 0, 0 }),
    Columns(1), CellSize(1), ContentHeight(0), MaximumScrollY(0),
    Landscape(false)
{
}

adaptiveui::Layout::Layout()
  : OutputWidth(1), OutputHeight(1), CanvasWidth(1), CanvasHeight(1),
    Gutter(8), Gap(8), DashboardRows(1), Fullscreen(false),
    Dashboard({ 0, 0, 1, 1 }), MapPanel({ 0, 0, 1, 1 }),
    CanvasSource({ 0, 0, 1, 1 }), Canvas({ 0, 0, 1, 1 }),
    Rail({ 0, 0, 1, 1 }), EquipmentPanel({ 0, 0, 1, 1 }),
    EquipmentCanvas({ 0, 0, 1, 1 }),
    EquipmentConditions({ 0, 0, 0, 0 }),
    RailContent({ 0, 0, 1, 1 }),
    Log({ 0, 0, 1, 1 }),
    Menu({ 0, 0, 1, 1 }), Prompt({ 0, 0, 1, 1 }),
    PromptDialog({ 0, 0, 1, 1 }), PromptInput({ 0, 0, 1, 1 }),
    PromptContinue({ 0, 0, 1, 1 }), PromptDecline({ 0, 0, 0, 0 }),
    PromptCancel({ 0, 0, 1, 1 }),
    MenuBack({ 0, 0, 0, 0 }),
    MenuConfirm({ 0, 0, 0, 0 }),
    MenuPrevious({ 0, 0, 0, 0 }), MenuNext({ 0, 0, 0, 0 }),
    MenuDetail({ 0, 0, 0, 0 }),
    ActionArea({ 0, 0, 1, 1 }), MapNotesArea({ 0, 0, 0, 0 }),
    ActiveCategory(0), ActionScroll(0), HoverAction(-1), PressedAction(-1)
{
  for(int Index = 0; Index < adaptiveui::ACTION_GROUPS; ++Index)
    ActionTabs[Index] = { 0, 0, 0, 0 };
}

namespace adaptiveui
{
  Layout CalculateLayout(int OutputWidth, int OutputHeight,
                         int CanvasWidth, int CanvasHeight,
                         bool Fullscreen)
  {
    Layout Result;
    Result.OutputWidth = std::max(1, OutputWidth);
    Result.OutputHeight = std::max(1, OutputHeight);
    Result.CanvasWidth = std::max(1, CanvasWidth);
    Result.CanvasHeight = std::max(1, CanvasHeight);
    Result.Fullscreen = Fullscreen;
    const float Scale = std::min(float(Result.OutputWidth) / 1280.f,
                                 float(Result.OutputHeight) / 720.f);
    Result.Gutter = Clamp(int(8.f * Scale + .5f), 6, 14);
    Result.Gap = Clamp(int(6.f * Scale + .5f), 5, 10);
    Result.DashboardRows = Result.OutputWidth < 1500 ? 2 : 1;
    const int DashboardHeight = Result.DashboardRows == 2
      ? Clamp(int(100.f * Scale + .5f), 92, 108)
      : Clamp(int(76.f * Scale + .5f), 70, 84);
    const int LogHeight = PromptShowsLogContext()
      ? Clamp(int(88.f * Scale + .5f), 78, 96)
      : Clamp(int(56.f * Scale + .5f), 52, 68);
    const int RailWidth = Clamp(int(Result.OutputWidth * .28f), 300, 420);
    Result.Dashboard = { Result.Gutter, Result.Gutter,
                         std::max(1, Result.OutputWidth - Result.Gutter * 2),
                         DashboardHeight };
    const int ContentTop = Result.Dashboard.y + Result.Dashboard.h
                         + Result.Gap;
    const int Bottom = Result.OutputHeight - Result.Gutter;
    const int MapWidth = std::max(1, Result.OutputWidth - Result.Gutter * 2
                                  - Result.Gap - RailWidth);
    const int MapHeight = std::max(1, Bottom - ContentTop
                                   - Result.Gap - LogHeight);
    Result.MapPanel = { Result.Gutter, ContentTop, MapWidth, MapHeight };
    Result.Log = { Result.Gutter, Result.MapPanel.y + Result.MapPanel.h
                             + Result.Gap, MapWidth, LogHeight };
    Result.Rail = { Result.MapPanel.x + Result.MapPanel.w + Result.Gap,
                    ContentTop, RailWidth,
                    std::max(1, Bottom - ContentTop) };
    const int EquipmentHeight = Clamp(Result.Rail.h * 3 / 10, 126, 180);
    Result.EquipmentPanel = { Result.Rail.x + 7, Result.Rail.y + 7,
                              std::max(1, Result.Rail.w - 14),
                              EquipmentHeight };
    Result.EquipmentCanvas = FitRect(
      { Result.EquipmentPanel.x + 5, Result.EquipmentPanel.y + 5,
        std::max(1, Result.EquipmentPanel.w - 10),
        std::max(1, Result.EquipmentPanel.h - 10) }, 96, 112);
    Result.EquipmentConditions = { 0, 0, 0, 0 };
    const int RailContentTop = Result.EquipmentPanel.y
                             + Result.EquipmentPanel.h + Result.Gap;
    Result.RailContent = { Result.Rail.x + 5, RailContentTop,
                           std::max(1, Result.Rail.w - 10),
                           std::max(1, Result.Rail.y + Result.Rail.h
                                         - RailContentTop - 5) };
    Result.CanvasSource = GameplaySource(Result.CanvasWidth,
                                         Result.CanvasHeight);
    Result.Canvas = FitRect(Result.MapPanel, Result.CanvasSource.w,
                            Result.CanvasSource.h);
    Result.Menu = Result.MapPanel;
    Result.Prompt = Result.Log;
    const int TabHeight = 24;
    const int TabWeights[ACTION_GROUPS] = { 9, 7, 11, 6, 8 };
    int WeightTotal = 0;
    for(int Index = 0; Index < ACTION_GROUPS; ++Index)
      WeightTotal += TabWeights[Index];
    int WeightBefore = 0;
    for(int Index = 0; Index < ACTION_GROUPS; ++Index)
    {
      const int X0 = Result.RailContent.x + 2
                   + (Result.RailContent.w - 4)
                   * WeightBefore / WeightTotal;
      WeightBefore += TabWeights[Index];
      const int X1 = Result.RailContent.x + 2
                   + (Result.RailContent.w - 4)
                   * WeightBefore / WeightTotal;
      Result.ActionTabs[Index] = { X0 + 1, Result.RailContent.y + 24,
                                   std::max(1, X1 - X0 - 2), TabHeight };
    }
    const int TabsBottom = Result.ActionTabs[ACTION_GROUPS - 1].y
                         + Result.ActionTabs[ACTION_GROUPS - 1].h;
    Result.ActionArea = { Result.RailContent.x + 2, TabsBottom + 5,
                          std::max(1, Result.RailContent.w - 4),
                          std::max(1, Result.RailContent.y
                                        + Result.RailContent.h
                                        - TabsBottom
                                        - DesktopActionFooterReserve) };
    return Result;
  }

  int CalculateEquipmentPageSize(const Layout& Current, int ItemCount)
  {
    // Equipment has one title line and always retains the Back button.  Only
    // reserve the taller paging footer when all rows cannot fit at once.
    const int Top = Current.RailContent.y + 10
                  + DesktopSidebarLineHeight + 10;
    const int SinglePageBottom = Current.RailContent.y
                               + Current.RailContent.h - 48;
    const int SinglePageCapacity = std::max(1,
      (SinglePageBottom - Top) / DesktopEquipmentMinimumRowHeight);
    if(ItemCount <= SinglePageCapacity)
      return SinglePageCapacity;

    const int PagedBottom = Current.RailContent.y
                          + Current.RailContent.h - 86;
    return std::max(1,
      (PagedBottom - Top) / DesktopEquipmentMinimumRowHeight);
  }

  bool MapOutputToCanvas(const Layout& Current, int OutputX, int OutputY,
                         int CanvasWidth, int CanvasHeight,
                         int& CanvasX, int& CanvasY)
  {
    if(!Contains(Current.Canvas, OutputX, OutputY))
      return false;
    const SDL_Rect Source = ClipSource(Current.CanvasSource,
                                       std::max(1, CanvasWidth),
                                       std::max(1, CanvasHeight));
    CanvasX = Source.x + Clamp((OutputX - Current.Canvas.x) * Source.w
                               / std::max(1, Current.Canvas.w),
                               0, Source.w - 1);
    CanvasY = Source.y + Clamp((OutputY - Current.Canvas.y) * Source.h
                               / std::max(1, Current.Canvas.h),
                               0, Source.h - 1);
    return true;
  }

  MobileMenuLayout CalculateMobileMenuLayout(const SDL_Rect& Area,
                                               float Density,
                                               int ItemCount,
                                               bool ShowPaperDoll,
                                               bool ShowDetail,
                                               int ScrollY)
  {
    MobileMenuLayout Result;
    Result.Area = Area;
    Result.Landscape = Area.w > Area.h * 6 / 5;
    Density = std::max(.75f, Density);
    const int Padding = Clamp(int(5.f * Density + .5f), 6, 20);
    const int Gap = Clamp(int(4.f * Density + .5f), 6, 18);
    const int FooterHeight = Clamp(int(18.f * Density + .5f), 42, 72);
    SDL_Rect Content = { Area.x + Padding, Area.y + Padding,
      std::max(1, Area.w - Padding * 2),
      std::max(1, Area.h - Padding * 2 - FooterHeight - Gap) };
    Result.Footer = { Content.x, Content.y + Content.h + Gap,
                      Content.w, FooterHeight };

    if(Result.Landscape)
    {
      const int MaximumRight = std::max(1, Content.w / 2);
      const int MinimumRight = std::min(MaximumRight,
        std::max(1, int(150 * Density)));
      const int RightWidth = ShowDetail || ShowPaperDoll
        ? Clamp(Content.w * 38 / 100, MinimumRight, MaximumRight)
        : 0;
      Result.GridViewport = { Content.x, Content.y,
        std::max(1, Content.w - (RightWidth ? RightWidth + Gap : 0)),
        Content.h };
      if(RightWidth)
      {
        SDL_Rect Right = { Result.GridViewport.x + Result.GridViewport.w + Gap,
                           Content.y, RightWidth, Content.h };
        if(ShowPaperDoll)
        {
          const int DollHeight = ShowDetail ? Right.h * 42 / 100 : Right.h;
          Result.PaperDoll = { Right.x + Gap, Right.y,
            std::max(1, Right.w - Gap * 2), std::max(1, DollHeight) };
          Result.Conditions = { Right.x, Right.y, std::max(0, Right.w / 3),
                                std::max(1, DollHeight) };
        }
        if(ShowDetail)
        {
          const int DetailY = ShowPaperDoll
            ? Result.PaperDoll.y + Result.PaperDoll.h + Gap : Right.y;
          Result.Detail = { Right.x, DetailY, Right.w,
            std::max(1, Right.y + Right.h - DetailY) };
        }
      }
    }
    else
    {
      int Top = Content.y;
      if(ShowPaperDoll)
      {
        const int DollHeight = Clamp(Content.h * 27 / 100,
          int(84 * Density), Content.h * 2 / 5);
        Result.PaperDoll = { Content.x, Top, Content.w, DollHeight };
        Result.Conditions = { Content.x, Top,
                              std::max(0, Content.w * 30 / 100), DollHeight };
        Top += DollHeight + Gap;
      }
      int DetailHeight = 0;
      if(ShowDetail)
        DetailHeight = Clamp(Content.h * 28 / 100,
          int(78 * Density), Content.h * 2 / 5);
      Result.GridViewport = { Content.x, Top, Content.w,
        std::max(1, Content.y + Content.h - Top
                       - (DetailHeight ? DetailHeight + Gap : 0)) };
      if(DetailHeight)
        Result.Detail = { Content.x,
          Result.GridViewport.y + Result.GridViewport.h + Gap,
          Content.w, DetailHeight };
    }

    const int MinimumCell = Clamp(int(48.f * Density + .5f), 48, 168);
    Result.Columns = std::max(1, Result.GridViewport.w / MinimumCell);
    Result.Columns = std::min(Result.Columns, std::max(1, ItemCount));
    const int CellWidth = std::max(1, Result.GridViewport.w / Result.Columns);
    Result.CellSize = std::max(MinimumCell, CellWidth);
    const int Rows = ItemCount > 0
      ? (ItemCount + Result.Columns - 1) / Result.Columns : 0;
    Result.ContentHeight = Rows * Result.CellSize;
    Result.MaximumScrollY = std::max(0,
      Result.ContentHeight - Result.GridViewport.h);
    ScrollY = Clamp(ScrollY, 0, Result.MaximumScrollY);
    Result.Cells.reserve(std::max(0, ItemCount));
    for(int Index = 0; Index < ItemCount; ++Index)
    {
      const int Column = Index % Result.Columns;
      const int Row = Index / Result.Columns;
      const int X0 = Result.GridViewport.x
                   + Result.GridViewport.w * Column / Result.Columns;
      const int X1 = Result.GridViewport.x
                   + Result.GridViewport.w * (Column + 1) / Result.Columns;
      Result.Cells.push_back({ X0 + 2,
        Result.GridViewport.y + Row * Result.CellSize - ScrollY + 2,
        std::max(1, X1 - X0 - 4), std::max(1, Result.CellSize - 4) });
    }
    return Result;
  }

  int MobileMenuIndexAt(const MobileMenuLayout& Current, int X, int Y)
  {
    if(!Contains(Current.GridViewport, X, Y))
      return -1;
    for(size_t Index = 0; Index < Current.Cells.size(); ++Index)
      if(Contains(Current.Cells[Index], X, Y))
        return int(Index);
    return -1;
  }

  void SetPlatformMode(PlatformMode Mode)
  {
    CurrentPlatform = Mode;
  }

  PlatformMode GetPlatformMode()
  {
    return CurrentPlatform;
  }

  bool IsDesktopPresentationEnabled()
  {
    return CurrentPlatform == Desktop;
  }

  const HudModel& GetHudModel()
  {
    return Hud;
  }

  void SetStats(const char* Line1, const char* Line2,
                const char* Line3, const char* Line4)
  {
    const char* Lines[4] = { Line1, Line2, Line3, Line4 };
    for(int Index = 0; Index < 4; ++Index)
      Hud.Stats[Index] = Safe(Lines[Index]);
    Dirty = true;
  }

  void SetLocationTime(const char* Location, const char* Clock)
  {
    Hud.Location = Safe(Location);
    Hud.Clock = Safe(Clock);
    Dirty = true;
  }

  void SetConditions(const char* const* Labels, int Count)
  {
    Hud.Conditions.clear();
    const int SafeCount = Clamp(Count, 0, 32);
    for(int Index = 0; Index < SafeCount; ++Index)
    {
      StatusIndicator Indicator;
      Indicator.Label = Safe(Labels ? Labels[Index] : 0);
      std::string Lower = Indicator.Label;
      for(size_t Character = 0; Character < Lower.size(); ++Character)
        Lower[Character] = char(std::tolower((unsigned char)Lower[Character]));
      if(Lower.find("starv") != std::string::npos
         || Lower.find("faint") != std::string::npos
         || Lower.find("overload") != std::string::npos
         || Lower.find("exhaust") != std::string::npos)
      {
        Indicator.Red = 235; Indicator.Green = 80; Indicator.Blue = 72;
      }
      else if(Lower.find("hungry") != std::string::npos
              || Lower.find("burden") != std::string::npos
              || Lower.find("stress") != std::string::npos)
      {
        Indicator.Red = 224; Indicator.Green = 181; Indicator.Blue = 65;
      }
      else if(Lower.find("ship") != std::string::npos
              || Lower.find("running") != std::string::npos
              || Lower.find("fast") != std::string::npos)
      {
        Indicator.Red = 88; Indicator.Green = 191; Indicator.Blue = 121;
      }
      Hud.Conditions.push_back(Indicator);
    }
    Dirty = true;
  }

  void SetLog(const char* Message)
  {
    Hud.LogMessage = Safe(Message);
    Dirty = true;
  }

  void SetPrompt(const char* Prompt, const char* Input, bool Numeric)
  {
    Hud.Prompt = Safe(Prompt);
    Hud.PromptInput = Safe(Input);
    Hud.PromptShowsInput = Input != 0;
    Hud.PromptNumeric = Numeric;
    Hud.PromptCapturesKey = false;
    Hud.PromptConfirmsKeyTransfer = false;
    Hud.PromptConfirmsChoice = false;
    Hud.PromptOffersQuitChoices = false;
    Hud.PromptActive = true;
#ifdef USE_SDL
    if(CurrentPlatform == Desktop && Input != 0)
      SDL_StartTextInput();
#endif
    Dirty = true;
  }

  void SetKeyCapturePrompt(const char* Prompt)
  {
    Hud.Prompt = Safe(Prompt);
    Hud.PromptInput.clear();
    Hud.PromptShowsInput = false;
    Hud.PromptNumeric = false;
    Hud.PromptCapturesKey = true;
    Hud.PromptConfirmsKeyTransfer = false;
    Hud.PromptConfirmsChoice = false;
    Hud.PromptOffersQuitChoices = false;
    Hud.PromptActive = true;
    Dirty = true;
  }

  void SetKeyTransferPrompt(const char* Prompt)
  {
    Hud.Prompt = Safe(Prompt);
    Hud.PromptInput.clear();
    Hud.PromptShowsInput = false;
    Hud.PromptNumeric = false;
    Hud.PromptCapturesKey = false;
    Hud.PromptConfirmsKeyTransfer = true;
    Hud.PromptConfirmsChoice = false;
    Hud.PromptOffersQuitChoices = false;
    Hud.PromptActive = true;
    Dirty = true;
  }

  void SetConfirmationPrompt(const char* Prompt)
  {
    Hud.Prompt = Safe(Prompt);
    const char* Hints[] = { " [y/N]", " [y/n]", " [Y/n]", " [Y/N]" };
    for(size_t Index = 0; Index < sizeof(Hints) / sizeof(Hints[0]); ++Index)
    {
      const size_t At = Hud.Prompt.find(Hints[Index]);
      if(At != std::string::npos)
        Hud.Prompt.erase(At, std::strlen(Hints[Index]));
    }
    while(!Hud.Prompt.empty()
          && (Hud.Prompt.back() == '\r' || Hud.Prompt.back() == '\n'
              || Hud.Prompt.back() == ' '))
      Hud.Prompt.pop_back();
    std::string LowerPrompt = Hud.Prompt;
    std::transform(LowerPrompt.begin(), LowerPrompt.end(),
      LowerPrompt.begin(), [](unsigned char Character)
      { return char(std::tolower(Character)); });
    const bool ContinueConfirmation =
      LowerPrompt.find("continue anyway") != std::string::npos
      || LowerPrompt.find("still continue") != std::string::npos
      || LowerPrompt == "continue?";
    Hud.PromptDetail = ContinueConfirmation ? Hud.LogMessage : "";
    Hud.PromptInput.clear();
    Hud.PromptShowsInput = false;
    Hud.PromptNumeric = false;
    Hud.PromptCapturesKey = false;
    Hud.PromptConfirmsKeyTransfer = false;
    Hud.PromptConfirmsChoice = true;
    Hud.PromptOffersQuitChoices = false;
    Hud.PromptActive = true;
    Dirty = true;
  }

  void SetQuitPrompt(const char* Prompt)
  {
    Hud.Prompt = Safe(Prompt);
    while(!Hud.Prompt.empty()
          && (Hud.Prompt.back() == '\r' || Hud.Prompt.back() == '\n'
              || Hud.Prompt.back() == ' '))
      Hud.Prompt.pop_back();
    Hud.PromptInput.clear();
    Hud.PromptShowsInput = false;
    Hud.PromptNumeric = false;
    Hud.PromptCapturesKey = false;
    Hud.PromptConfirmsKeyTransfer = false;
    Hud.PromptConfirmsChoice = false;
    Hud.PromptOffersQuitChoices = true;
    Hud.PromptActive = true;
    Dirty = true;
  }

  void SetPromptDetail(const char* Detail)
  {
    Hud.PromptDetail = Safe(Detail);
    Dirty = true;
  }

  void SetPositionPrompt(bool Active)
  {
    Hud.PositionPrompt = Active;
#ifdef USE_SDL
    if(CurrentPlatform == Desktop && Hud.MapScreen)
      RebuildMapRects();
#endif
    Dirty = true;
  }

  void ClearPrompt()
  {
#ifdef USE_SDL
    if(CurrentPlatform == Desktop && Hud.PromptShowsInput)
      SDL_StopTextInput();
#endif
    Hud.PromptActive = false;
    Hud.PromptShowsInput = false;
    Hud.PromptNumeric = false;
    Hud.PromptCapturesKey = false;
    Hud.PromptConfirmsKeyTransfer = false;
    Hud.PromptConfirmsChoice = false;
    Hud.PromptOffersQuitChoices = false;
    Hud.PositionPrompt = false;
    Hud.Prompt.clear();
    Hud.PromptDetail.clear();
    Hud.PromptInput.clear();
    Dirty = true;
  }

  void SetPaperDollScreen(bool Active, int X, int Y, int Width, int Height)
  {
    Hud.PaperDollScreen = Active;
    Hud.PaperDollSource = { X, Y, std::max(0, Width), std::max(0, Height) };
    Dirty = true;
  }

  void SetScreenText(const char* Title, const char* Text)
  {
    Hud.ScreenTextTitle = Safe(Title);
    Hud.ScreenText = ReflowScreenText(Safe(Text));
    Hud.ScreenTextActive = true;
    Hud.MenuActive = false;
    Dirty = true;
  }

  void SetScreenText(const char* Text)
  {
    SetScreenText("STORY", Text);
  }

  void ClearScreenText()
  {
    Hud.ScreenTextActive = false;
    Hud.ScreenText.clear();
    Dirty = true;
  }

  void SetMenu(const char* Title, const char* Subtitle,
               const char* const* Options, int Count, int Selected,
               int Page, int Pages)
  {
    const std::string NewTitle = Safe(Title);
    Count = Clamp(Count, 0, 64);
    const bool DesktopEquipment = CurrentPlatform == Desktop
                               && NewTitle == "Equipment";
    const int EquipmentPageSize = DesktopEquipment
      ? CalculateEquipmentPageSize(CurrentLayout, Count) : 1;
    const int TargetPage = DesktopEquipment && Count > 0
      ? Clamp(Selected, 0, Count - 1) / EquipmentPageSize + 1
      : std::max(1, Page);
    const int TargetPages = DesktopEquipment
      ? std::max(1, (Count + EquipmentPageSize - 1)
                      / EquipmentPageSize)
      : std::max(1, Pages);
    const bool SamePage = Hud.MenuActive && Hud.MenuTitle == NewTitle
                       && Hud.MenuPage == TargetPage;
    Hud.MenuActive = true;
    Hud.MenuTitle = NewTitle;
    Hud.MenuSubtitle = Safe(Subtitle);
    Hud.MenuOptions.clear();
    Hud.MenuDetails.clear();
    Hud.MenuGroups.clear();
    Hud.MenuIconSources.clear();
    Hud.MenuAvailability.clear();
    Hud.MenuItemMetrics.clear();
    Hud.MenuComparisonMetrics.clear();
    Hud.MenuDisplayOrder.clear();
    Hud.MenuKind = MENU_ROWS;
    Hud.MenuIconGrid = false;
    for(int Index = 0; Index < Count; ++Index)
      Hud.MenuOptions.push_back(Options && Options[Index]
                                ? Options[Index] : "");
    Hud.MenuSelected = Clamp(Selected, -1,
                             std::max(-1, int(Hud.MenuOptions.size()) - 1));
    Hud.MenuPage = TargetPage;
    Hud.MenuPages = TargetPages;
    if(!SamePage)
      Hud.MenuScroll = 0;
    Dirty = true;
  }

  void SetMenuPresentation(const char* const* Details,
                           const SDL_Rect* IconSources, int Count,
                           MenuPresentationKind Kind)
  {
    Hud.MenuDetails.clear();
    Hud.MenuIconSources.clear();
    Count = Clamp(Count, 0, int(Hud.MenuOptions.size()));
    for(int Index = 0; Index < Count; ++Index)
    {
      Hud.MenuDetails.push_back(Details && Details[Index]
                                ? Details[Index] : "");
      Hud.MenuIconSources.push_back(IconSources ? IconSources[Index]
                                                : SDL_Rect{ 0, 0, 0, 0 });
    }
    while(Hud.MenuDetails.size() < Hud.MenuOptions.size())
    {
      Hud.MenuDetails.push_back("");
      Hud.MenuIconSources.push_back({ 0, 0, 0, 0 });
    }
    Hud.MenuKind = Kind;
    Hud.MenuIconGrid = Kind == MENU_CATEGORY_GRID || Kind == MENU_ITEM_GRID
                    || Kind == MENU_PICKUP_GRID;
    Dirty = true;
  }

  void SetMenuGroups(const char* const* Groups, int Count)
  {
    Hud.MenuGroups.clear();
    Count = Clamp(Count, 0, int(Hud.MenuOptions.size()));
    for(int Index = 0; Index < Count; ++Index)
      Hud.MenuGroups.push_back(Groups && Groups[Index]
                               ? Groups[Index] : "");
    while(Hud.MenuGroups.size() < Hud.MenuOptions.size())
      Hud.MenuGroups.push_back("");
    Dirty = true;
  }

  void SetMenuItemMetrics(const ItemMetrics* Metrics, int Count)
  {
    Hud.MenuItemMetrics.clear();
    Count = Clamp(Count, 0, int(Hud.MenuOptions.size()));
    for(int Index = 0; Index < Count; ++Index)
      Hud.MenuItemMetrics.push_back(Metrics ? Metrics[Index] : ItemMetrics());
    while(Hud.MenuItemMetrics.size() < Hud.MenuOptions.size())
      Hud.MenuItemMetrics.push_back(ItemMetrics());
    RebuildMenuDisplayOrder();
    Dirty = true;
  }

  void SetMenuAvailability(const unsigned char* Available, int Count)
  {
    Hud.MenuAvailability.clear();
    Count = Clamp(Count, 0, int(Hud.MenuOptions.size()));
    for(int Index = 0; Index < Count; ++Index)
      Hud.MenuAvailability.push_back(!Available || Available[Index] ? 1 : 0);
    while(Hud.MenuAvailability.size() < Hud.MenuOptions.size())
      Hud.MenuAvailability.push_back(1);
    Dirty = true;
  }

  void SetMenuComparisonMetrics(const ItemMetrics* Metrics, int Count)
  {
    Hud.MenuComparisonMetrics.clear();
    Count = Clamp(Count, 0, int(Hud.MenuOptions.size()));
    for(int Index = 0; Index < Count; ++Index)
      Hud.MenuComparisonMetrics.push_back(
        Metrics ? Metrics[Index] : ItemMetrics());
    while(Hud.MenuComparisonMetrics.size() < Hud.MenuOptions.size())
      Hud.MenuComparisonMetrics.push_back(ItemMetrics());
    Dirty = true;
  }

  void SetEquipmentComparison(const char* Label,
                              const ItemMetrics& Metrics)
  {
    Hud.EquipmentComparisonActive = true;
    Hud.EquippedItemLabel = Safe(Label);
    Hud.EquippedItemMetrics = Metrics;
    RebuildMenuDisplayOrder();
    Dirty = true;
  }

  void SetInventoryWeights(long Current, long Maximum)
  {
    Hud.InventoryCurrentWeight = std::max(0L, Current);
    Hud.InventoryMaximumWeight = std::max(0L, Maximum);
    Dirty = true;
  }

  void ClearMenu()
  {
    Hud.MenuActive = false;
    Hud.MenuOptions.clear();
    Hud.MenuDetails.clear();
    Hud.MenuGroups.clear();
    Hud.MenuIconSources.clear();
    Hud.MenuAvailability.clear();
    Hud.MenuItemMetrics.clear();
    Hud.MenuComparisonMetrics.clear();
    Hud.MenuDisplayOrder.clear();
    Hud.MenuKind = MENU_ROWS;
    Hud.MenuIconGrid = false;
    Hud.EquipmentComparisonActive = false;
    Hud.EquippedItemLabel.clear();
    Hud.EquippedItemMetrics = ItemMetrics();
    Hud.InventoryCurrentWeight = -1;
    Hud.InventoryMaximumWeight = -1;
    Hud.MenuSelected = -1;
    Hud.MenuScroll = 0;
    Dirty = true;
  }

  void SetActions(const char* const* Labels, const int* Keys,
                  const int* Groups, int Count)
  {
    Hud.Actions.clear();
    Count = Clamp(Count, 0, 64);
    for(int Index = 0; Index < Count; ++Index)
    {
      ActionEntry Entry;
      Entry.Label = Labels && Labels[Index] ? Labels[Index] : "";
      Entry.DispatchCode = Keys ? Keys[Index] : 0;
      Entry.Category = Groups ? Clamp(Groups[Index], 0, ACTION_GROUPS - 1)
                              : ACTION_SYSTEM;
      Entry.Available = true;
      Hud.Actions.push_back(Entry);
    }
    Dirty = true;
  }

  void SetActionShortcuts(const int* Keys, int Count)
  {
    Count = Clamp(Count, 0, int(Hud.Actions.size()));
    for(int Index = 0; Index < Count; ++Index)
      Hud.Actions[Index].DisplayedShortcut = ShortcutLabel(Keys ? Keys[Index] : 0);
    Dirty = true;
  }

  int PageMenu(int Selected, int Direction, int Count)
  {
    if(Count <= 0 || Direction == 0)
      return Selected;
    const int Step = 8;
    if(Direction > 0)
      return std::min(Count - 1, Selected + Step);
    return std::max(0, Selected - Step);
  }

  int NavigateInventoryMenu(int Selected, int Key, int Count)
  {
    if(Count <= 0)
      return -1;
    Selected = Clamp(Selected, 0, Count - 1);
    int Position = DisplayPositionForMenuIndex(Selected);
    const int Columns = std::max(1, GetMenuGeometry().GridColumns);
    if(Key == KEY_LEFT)
      Position = std::max(0, Position - 1);
    if(Key == KEY_RIGHT)
      Position = std::min(Count - 1, Position + 1);
    if(Key == KEY_UP)
      Position = std::max(0, Position - Columns);
    if(Key == KEY_DOWN)
      Position = std::min(Count - 1, Position + Columns);
    return MenuIndexForDisplayPosition(Position);
  }

  void SetQuestionChoices(const int* Keys, int Count)
  {
    Hud.QuestionChoiceCount = Clamp(Count, 0, 9);
    for(int Index = 0; Index < Hud.QuestionChoiceCount; ++Index)
      Hud.QuestionChoices[Index] = Keys ? Keys[Index] : 0;
    Dirty = true;
  }

  void SetMapScreen(bool Active)
  {
    Hud.MapScreen = Active;
    DesktopMapNoteScroll = 0;
    DesktopSelectedMapNote = -1;
    DesktopHoverMapNote = -1;
    DesktopHoverMapAction = -1;
#ifdef USE_SDL
    if(CurrentPlatform == Desktop)
      RebuildMapRects();
#endif
    Dirty = true;
  }

  void SetMapSourceBounds(int X, int Y, int Width, int Height)
  {
    Hud.MapSource = { X, Y, std::max(0, Width), std::max(0, Height) };
    Hud.HasMapSource = Width > 0 && Height > 0;
    Dirty = true;
  }

  void SetMapFocus(int X, int Y)
  {
    Hud.MapFocusX = X;
    Hud.MapFocusY = Y;
    Dirty = true;
  }

  void SetMapFocus(int X, int Y, int PlayerX, int PlayerY)
  {
    Hud.MapFocusX = X;
    Hud.MapFocusY = Y;
    Hud.PlayerFocusX = PlayerX;
    Hud.PlayerFocusY = PlayerY;
    Dirty = true;
  }

  void SetMapNotes(const char* const* Notes, const int* X, const int* Y,
                   int Count)
  {
    Hud.MapNotes.clear();
    const int SafeCount = Clamp(Count, 0, 32);
    for(int Index = 0; Index < SafeCount; ++Index)
    {
      MapNote Note;
      Note.Label = Safe(Notes ? Notes[Index] : 0);
      Note.X = X ? X[Index] : 0;
      Note.Y = Y ? Y[Index] : 0;
      Hud.MapNotes.push_back(Note);
    }
    if(DesktopSelectedMapNote >= SafeCount)
      DesktopSelectedMapNote = -1;
#ifdef USE_SDL
    if(CurrentPlatform == Desktop && Hud.MapScreen)
      RebuildMapRects();
#endif
    Dirty = true;
  }

  int GetSelectedMapNote()
  {
    return Hud.MapScreen ? DesktopSelectedMapNote : -1;
  }

  bool AdjustMapZoom(int Steps)
  {
    if(!CanZoomGameplay() || Steps == 0)
      return false;
    DesktopMapZoom = Clamp(DesktopMapZoom + Steps, 1, 4);
    CurrentLayout.CanvasSource = ZoomedGameplaySource(
      CurrentLayout.CanvasWidth, CurrentLayout.CanvasHeight);
    CurrentLayout.Canvas = FitRect(CurrentLayout.MapPanel,
                                   CurrentLayout.CanvasSource.w,
                                   CurrentLayout.CanvasSource.h);
    Dirty = true;
    return true;
  }

#ifdef USE_SDL
  int TranslateDesktopShortcut(SDL_Keycode Key, SDL_Keymod Modifiers)
  {
    const bool Shift = (Modifiers & KMOD_SHIFT) != 0;
    const bool Caps = (Modifiers & KMOD_CAPS) != 0;
    if(Key >= SDLK_a && Key <= SDLK_z)
      return (Shift != Caps) ? int('A' + Key - SDLK_a) : int(Key);
    if(!Shift)
      return Key >= 0x20 && Key < 0x7f ? int(Key) : 0;
    switch(Key)
    {
     case SDLK_1: return '!';
     case SDLK_2: return '@';
     case SDLK_3: return '#';
     case SDLK_4: return '$';
     case SDLK_5: return '%';
     case SDLK_6: return '^';
     case SDLK_7: return '&';
     case SDLK_8: return '*';
     case SDLK_9: return '(';
     case SDLK_0: return ')';
     case SDLK_MINUS: return '_';
     case SDLK_EQUALS: return '+';
     case SDLK_LEFTBRACKET: return '{';
     case SDLK_RIGHTBRACKET: return '}';
     case SDLK_BACKSLASH: return '|';
     case SDLK_SEMICOLON: return ':';
     case SDLK_QUOTE: return '"';
     case SDLK_COMMA: return '<';
     case SDLK_PERIOD: return '>';
     case SDLK_SLASH: return '?';
     case SDLK_BACKQUOTE: return '~';
     default: return 0;
    }
  }

  bool SelectActionCategory(int Category)
  {
    if(CurrentPlatform != Desktop || Hud.MenuActive
       || Hud.ScreenTextActive || Hud.PromptActive
       || Category < 0 || Category >= ACTION_GROUPS)
      return false;
    CurrentLayout.ActiveCategory = Category;
    CurrentLayout.ActionScroll = 0;
    CurrentLayout.HoverAction = -1;
    RebuildActionRects();
    Dirty = true;
    return true;
  }

  void UpdateLayout(SDL_Renderer* Renderer, int CanvasWidth, int CanvasHeight,
                    bool Fullscreen)
  {
    int OutputWidth = 1;
    int OutputHeight = 1;
    SDL_GetRendererOutputSize(Renderer, &OutputWidth, &OutputHeight);
    const int OldCategory = CurrentLayout.ActiveCategory;
    const int OldScroll = CurrentLayout.ActionScroll;
    const int OldHover = CurrentLayout.HoverAction;
    CurrentLayout = CalculateLayout(OutputWidth, OutputHeight,
                                    CanvasWidth, CanvasHeight, Fullscreen);
    if(Hud.MapScreen)
    {
      CurrentLayout.EquipmentPanel = { 0, 0, 0, 0 };
      CurrentLayout.EquipmentCanvas = { 0, 0, 0, 0 };
      CurrentLayout.EquipmentConditions = { 0, 0, 0, 0 };
      CurrentLayout.RailContent = {
        CurrentLayout.Rail.x + 5, CurrentLayout.Rail.y + 5,
        std::max(1, CurrentLayout.Rail.w - 10),
        std::max(1, CurrentLayout.Rail.h - 10)
      };
    }
    else if(!Hud.Conditions.empty())
    {
      const int InnerHeight = std::max(1,
        CurrentLayout.EquipmentPanel.h - 10);
      const int Gap = 5;
      const int ConditionsX = CurrentLayout.EquipmentPanel.x + 5;
      const int ConditionWidth = std::max(0,
        CurrentLayout.EquipmentCanvas.x - Gap - ConditionsX);
      CurrentLayout.EquipmentConditions = {
        ConditionsX,
        CurrentLayout.EquipmentPanel.y + 5,
        ConditionWidth,
        InnerHeight
      };
    }
    CurrentLayout.CanvasSource = ActiveCanvasSource(CanvasWidth,
                                                    CanvasHeight);
    CurrentLayout.Canvas = FitRect(CurrentLayout.MapPanel,
                                   CurrentLayout.CanvasSource.w,
                                   CurrentLayout.CanvasSource.h);
    CurrentLayout.ActiveCategory = Clamp(OldCategory, 0, ACTION_GROUPS - 1);
    CurrentLayout.ActionScroll = Clamp(OldScroll, 0,
                                       MaximumActionScroll(CurrentLayout.ActiveCategory));
    CurrentLayout.HoverAction = OldHover;
    RebuildActionRects();
    const PromptGeometry PromptRects = GetPromptGeometry();
    CurrentLayout.PromptDialog = PromptRects.Dialog;
    CurrentLayout.PromptInput = PromptRects.Input;
    CurrentLayout.PromptContinue = PromptRects.Continue;
    CurrentLayout.PromptDecline = PromptRects.Decline;
    CurrentLayout.PromptCancel = PromptRects.Cancel;
    const MenuGeometry MenuRects = GetMenuGeometry();
    CurrentLayout.Menu = MenuRects.Area;
    CurrentLayout.MenuDetail = MenuRects.Detail;
    CurrentLayout.MenuBack = MenuRects.Back;
    CurrentLayout.MenuConfirm = MenuRects.Confirm;
    CurrentLayout.MenuCells.clear();
    if(IsInventoryGridMenu())
      for(int Index = 0; Index < std::min(MenuRects.VisibleCount,
          int(Hud.MenuOptions.size())); ++Index)
        CurrentLayout.MenuCells.push_back(InventoryCell(MenuRects, Index));
    CurrentLayout.MenuPrevious = MenuRects.Previous;
    CurrentLayout.MenuNext = MenuRects.Next;
    CurrentLayout.MenuItemActions = MenuRects.ItemActions;
    CurrentLayout.MenuItemActionCodes = MenuRects.ItemActionCodes;
    RebuildMapRects();
    Dirty = false;
  }

  const Layout& GetLayout()
  {
    return CurrentLayout;
  }

  const SDL_Rect& GetCanvasRect()
  {
    return CurrentLayout.Canvas;
  }

  bool IsTextEntryPromptActive()
  {
    return CurrentPlatform == Desktop && Hud.PromptActive
      && !Hud.PositionPrompt && (Hud.PromptShowsInput || Hud.PromptNumeric);
  }

  void DrawBackground(SDL_Renderer* Renderer)
  {
    Fill(Renderer, { 0, 0, CurrentLayout.OutputWidth,
                     CurrentLayout.OutputHeight }, 0, 0, 0, 255);
    if(CurrentPlatform != Desktop)
      return;
    if((Hud.MenuActive || Hud.ScreenTextActive || Hud.PromptActive)
       && !HasGameplayContext())
      return;
    DrawDashboard(Renderer);
    Fill(Renderer, CurrentLayout.MapPanel, 0, 0, 0, 255);
    Frame(Renderer, CurrentLayout.Rail);
    if(Hud.MapScreen)
    {
      Frame(Renderer, CurrentLayout.Log);
      return;
    }
    Frame(Renderer, CurrentLayout.EquipmentPanel);
    Frame(Renderer, CurrentLayout.Log);
  }

  void DrawGame(SDL_Renderer* Renderer, SDL_Texture* GameTexture)
  {
    if(CurrentPlatform != Desktop || !GameTexture)
      return;
    CurrentMenuTexture = GameTexture;
    const bool Gameplay = HasGameplayContext();
    const bool GameplayMenu = Gameplay && Hud.MenuActive;
    if((Hud.MenuActive && !Gameplay) || Hud.ScreenTextActive
       || (Hud.PromptActive && !Gameplay))
      return;

    if(Gameplay && !Hud.MenuActive && !Hud.PaperDollScreen
       && !Hud.MapScreen)
      UpdateGameplaySnapshot(Renderer, GameTexture);

    SDL_Texture* PresentationTexture = GameplayMenu && SnapshotValid
      ? GameplaySnapshot : GameTexture;
    SDL_Rect Source = GameplayMenu
      ? ZoomedGameplaySource(CurrentLayout.CanvasWidth,
                             CurrentLayout.CanvasHeight)
      : CurrentLayout.CanvasSource;
    SDL_RenderCopy(Renderer, PresentationTexture, &Source,
                   &CurrentLayout.Canvas);

    if(Gameplay && !Hud.MapScreen)
    {
      SDL_Rect Equipment = EquipmentSource(CurrentLayout.CanvasWidth,
                                            CurrentLayout.CanvasHeight);
      SDL_RenderCopy(Renderer, PresentationTexture, &Equipment,
                     &CurrentLayout.EquipmentCanvas);
      Outline(Renderer, CurrentLayout.EquipmentCanvas, 74, 61, 41);
    }
  }

  void Draw(SDL_Renderer* Renderer)
  {
    if(CurrentPlatform != Desktop)
      return;
    if(Hud.MenuActive)
    {
      DrawMenu(Renderer);
      if(IsExpandedHistoryMenu())
        DrawLog(Renderer);
    }
    else if(Hud.ScreenTextActive)
      DrawScreenText(Renderer);
    else
    {
      if(HasGameplayContext())
      {
        if(Hud.MapScreen)
        {
          DrawMapMarkers(Renderer);
          DrawMapSidebar(Renderer);
        }
        else
        {
          DrawEquipmentConditions(Renderer);
          DrawActions(Renderer);
        }
        DrawLog(Renderer);
      }
    }
    // Confirmation, text-entry, and key-capture prompts are true modals.
    // Draw them last so menus, story/help screens, map views, and gameplay
    // chrome can never cover a window-close confirmation.
    DrawPromptDialog(Renderer);
  }

  PointerResult HandlePointer(int OutputX, int OutputY, bool Pressed,
                              int WheelY, bool Motion, int Button)
  {
    PointerResult Result;
    if(CurrentPlatform != Desktop)
      return Result;

    if(Hud.ScreenTextActive && !HasBlockingPromptDialog())
    {
      if(Pressed)
      {
        Result.Type = PointerResult::COMMAND_KEY;
        Result.CommandCode = KEY_ENTER;
      }
      else if(Motion || WheelY)
        Result.Type = PointerResult::CONSUMED;
      return Result;
    }

    if(Hud.MapScreen && !HasBlockingPromptDialog())
    {
      if(WheelY && Contains(CurrentLayout.MapNotesArea, OutputX, OutputY))
      {
        const int VisibleRows = std::max(1,
          int(CurrentLayout.MapNoteRows.size()));
        const int MaximumScroll = std::max(0,
          int(Hud.MapNotes.size()) - VisibleRows);
        const int NewScroll = Clamp(DesktopMapNoteScroll
          + (WheelY < 0 ? 1 : -1), 0, MaximumScroll);
        if(NewScroll != DesktopMapNoteScroll)
        {
          DesktopMapNoteScroll = NewScroll;
          RebuildMapRects();
          Result.Type = PointerResult::REDRAW;
        }
        else
          Result.Type = PointerResult::CONSUMED;
        return Result;
      }

      int HoverNote = -1;
      int HoverAction = -1;
      for(size_t Index = 0; Index < CurrentLayout.MapNoteRows.size(); ++Index)
        if(Contains(CurrentLayout.MapNoteRows[Index], OutputX, OutputY))
          HoverNote = CurrentLayout.MapNoteIndices[Index];
      if(!Hud.PositionPrompt)
        for(size_t Index = 0;
            Index < CurrentLayout.MapNoteMarkers.size(); ++Index)
          if(Contains(CurrentLayout.MapNoteMarkers[Index], OutputX, OutputY))
            HoverNote = int(Index);
      for(size_t Index = 0; Index < CurrentLayout.MapActionButtons.size(); ++Index)
        if(Contains(CurrentLayout.MapActionButtons[Index], OutputX, OutputY))
          HoverAction = int(Index);

      if(Motion && (HoverNote != DesktopHoverMapNote
                    || HoverAction != DesktopHoverMapAction))
      {
        DesktopHoverMapNote = HoverNote;
        DesktopHoverMapAction = HoverAction;
        Result.Type = PointerResult::REDRAW;
        return Result;
      }

      if(Pressed && HoverNote >= 0)
      {
        DesktopSelectedMapNote = HoverNote;
        Result.Type = PointerResult::REDRAW;
        return Result;
      }
      if(Pressed && HoverAction >= 0)
      {
        Result.Type = PointerResult::COMMAND_KEY;
        Result.CommandCode = CurrentLayout.MapActionKeys[HoverAction];
        return Result;
      }

      int CanvasX = 0;
      int CanvasY = 0;
      if(Hud.PositionPrompt
         && MapOutputToCanvas(CurrentLayout, OutputX, OutputY,
                              CurrentLayout.CanvasWidth,
                              CurrentLayout.CanvasHeight,
                              CanvasX, CanvasY))
      {
        Result.Type = PointerResult::CANVAS_MOUSE_EVENT;
        Result.CanvasX = CanvasX;
        Result.CanvasY = CanvasY;
        Result.Button = Pressed ? Button : 0;
        Result.Motion = Motion;
        return Result;
      }

      if(Contains(CurrentLayout.Rail, OutputX, OutputY)
         || Contains(CurrentLayout.MapPanel, OutputX, OutputY))
        Result.Type = PointerResult::CONSUMED;
      return Result;
    }

    if(HasBlockingPromptDialog())
    {
      if(Pressed && (Hud.PromptShowsInput || Hud.PromptNumeric
                      || Hud.PromptCapturesKey
                      || Hud.PromptConfirmsKeyTransfer
                      || Hud.PromptConfirmsChoice
                      || Hud.PromptOffersQuitChoices))
      {
        const PromptGeometry Geometry = GetPromptGeometry();
        if((Hud.PromptShowsInput || Hud.PromptNumeric)
           && Contains(Geometry.Input, OutputX, OutputY))
        {
          SDL_StartTextInput();
          Result.Type = PointerResult::REDRAW;
        }
        else if(!Hud.PromptCapturesKey
                && Contains(Geometry.Continue, OutputX, OutputY))
        {
          Result.Type = PointerResult::COMMAND_KEY;
          Result.CommandCode = Hud.PromptConfirmsChoice
                            || Hud.PromptOffersQuitChoices ? 'y' : KEY_ENTER;
        }
        else if(Hud.PromptOffersQuitChoices
                && Contains(Geometry.Decline, OutputX, OutputY))
        {
          Result.Type = PointerResult::COMMAND_KEY;
          Result.CommandCode = 'n';
        }
        else if(Contains(Geometry.Cancel, OutputX, OutputY))
        {
          Result.Type = PointerResult::COMMAND_KEY;
          Result.CommandCode = Hud.PromptConfirmsChoice ? 'n' : KEY_ESC;
        }
        else
          Result.Type = PointerResult::CONSUMED;
      }
      else
        Result.Type = PointerResult::CONSUMED;
      return Result;
    }

    if(Hud.MenuActive)
    {
      const MenuGeometry Geometry = GetMenuGeometry();
      const bool InMenu = Contains(Geometry.Area, OutputX, OutputY);

      if(IsExpandedHistoryMenu()
         && Pressed && Contains(CurrentLayout.Log, OutputX, OutputY))
      {
        Result.Type = PointerResult::COMMAND_KEY;
        Result.CommandCode = KEY_ESC;
        return Result;
      }

      if(Hud.MenuKind == adaptiveui::MENU_PICKUP_GRID && Pressed)
      {
        const bool HasSelection = Hud.MenuSelected >= 0
          && Hud.MenuSelected < int(Hud.MenuOptions.size());
        for(size_t Index = 0; Index < Geometry.ItemActions.size()
            && Index < Geometry.ItemActionCodes.size(); ++Index)
          if(Contains(Geometry.ItemActions[Index], OutputX, OutputY))
          {
            Result.Type = HasSelection ? PointerResult::COMMAND_KEY
                                       : PointerResult::CONSUMED;
            if(HasSelection)
            {
              const int Action = Geometry.ItemActionCodes[Index];
              Result.CommandCode = Action == adaptiveui::ITEM_ACTION_NONE
                ? KEY_MOBILE_MENU_EQUIP_BASE + Hud.MenuSelected
                : KEY_MOBILE_MENU_ACTION_BASE
                  + (Action - 1) * KEY_MOBILE_MENU_ACTION_STRIDE
                  + Hud.MenuSelected;
            }
            return Result;
          }
        if(Contains(Geometry.Confirm, OutputX, OutputY))
        {
          Result.Type = HasSelection ? PointerResult::COMMAND_KEY
                                     : PointerResult::CONSUMED;
          Result.CommandCode = HasSelection
            ? KEY_MOBILE_MENU_SELECT_BASE + Hud.MenuSelected : 0;
          return Result;
        }
      }

      if(!Geometry.FrontEnd && Pressed
         && Contains(Geometry.Back, OutputX, OutputY))
      {
        Result.Type = PointerResult::COMMAND_KEY;
        Result.CommandCode = KEY_ESC;
        return Result;
      }

      if(Hud.MenuPages > 1 && Pressed
         && Contains(Geometry.Previous, OutputX, OutputY))
      {
        Result.Type = Hud.MenuPage > 1 ? PointerResult::COMMAND_KEY
                                      : PointerResult::CONSUMED;
        Result.CommandCode = Hud.MenuPage > 1 ? KEY_PAGE_UP : 0;
        return Result;
      }

      if(Hud.MenuPages > 1 && Pressed
         && Contains(Geometry.Next, OutputX, OutputY))
      {
        Result.Type = Hud.MenuPage < Hud.MenuPages ? PointerResult::COMMAND_KEY
                                                  : PointerResult::CONSUMED;
        Result.CommandCode = Hud.MenuPage < Hud.MenuPages ? KEY_PAGE_DOWN : 0;
        return Result;
      }

      if(WheelY && InMenu)
      {
        if(IsEquipmentMenu())
        {
          const int Direction = WheelY < 0 ? 1 : -1;
          const bool CanMove = Direction > 0
            ? Hud.MenuPage < Hud.MenuPages : Hud.MenuPage > 1;
          Result.Type = CanMove ? PointerResult::COMMAND_KEY
                                : PointerResult::CONSUMED;
          Result.CommandCode = CanMove
            ? (Direction > 0 ? KEY_PAGE_DOWN : KEY_PAGE_UP) : 0;
          return Result;
        }
        const int Direction = WheelY < 0
          ? (IsInventoryGridMenu() ? Geometry.GridColumns : 1)
          : (IsInventoryGridMenu() ? -Geometry.GridColumns : -1);
        const int NewScroll = Clamp(Hud.MenuScroll + Direction,
                                    0, Geometry.MaximumScroll);
        if(NewScroll != Hud.MenuScroll)
        {
          Hud.MenuScroll = NewScroll;
          Result.Type = PointerResult::REDRAW;
        }
        else if((Direction > 0 && Hud.MenuPage < Hud.MenuPages)
                || (Direction < 0 && Hud.MenuPage > 1))
        {
          Result.Type = PointerResult::COMMAND_KEY;
          Result.CommandCode = Direction > 0 ? KEY_PAGE_DOWN : KEY_PAGE_UP;
        }
        else
          Result.Type = PointerResult::CONSUMED;
        return Result;
      }

      if(IsInventoryGridMenu() && InMenu
         && OutputY >= Geometry.Top && OutputY < Geometry.Detail.y)
      {
        const int LocalX = OutputX - (Geometry.Area.x + 10);
        const int LocalY = OutputY - Geometry.Top;
        const int Column = LocalX
          / (Geometry.GridCellWidth + Geometry.GridGap);
        const int Row = LocalY
          / (Geometry.GridCellHeight + Geometry.GridGap);
        const int VisibleIndex = Row * Geometry.GridColumns + Column;
        const SDL_Rect Cell = Column >= 0 && Column < Geometry.GridColumns
          && VisibleIndex >= 0 && VisibleIndex < Geometry.VisibleCount
          ? InventoryCell(Geometry, VisibleIndex)
          : SDL_Rect{ 0, 0, 0, 0 };
        const int Index = MenuIndexForDisplayPosition(
          Hud.MenuScroll + VisibleIndex);
        if(Contains(Cell, OutputX, OutputY)
           && Index >= 0 && Index < int(Hud.MenuOptions.size()))
        {
          if(Motion)
          {
            if(Hud.MenuSelected != Index)
            {
              Hud.MenuSelected = Index;
              Result.Type = PointerResult::REDRAW;
            }
            else
              Result.Type = PointerResult::CONSUMED;
            return Result;
          }
          if(Pressed)
          {
            Result.Type = PointerResult::COMMAND_KEY;
            Result.CommandCode = KEY_MOBILE_MENU_SELECT_BASE + Index;
            return Result;
          }
        }
      }
      else if(IsCraftingGuideTextPage() && InMenu)
      {
        Result.Type = PointerResult::CONSUMED;
        return Result;
      }
      else if(InMenu && OutputY >= Geometry.Top
              && OutputY < Geometry.Bottom
              && (!IsOptionsMenu() || OutputX < Geometry.Detail.x - 4))
      {
        int Index = -1;
        if(IsOptionsMenu())
        {
          const int First = Hud.MenuScroll;
          const int Last = std::min(int(Hud.MenuOptions.size()), First
            + VisibleOptionCount(Geometry, First));
          for(int Candidate = First; Candidate < Last; ++Candidate)
          {
            SDL_Rect Row = OptionRowRect(Geometry, Candidate, First);
            Row.x = Geometry.Area.x + 10;
            Row.w = std::max(1, Geometry.Detail.x - Row.x - 8);
            if(Contains(Row, OutputX, OutputY))
            {
              Index = Candidate;
              break;
            }
          }
        }
        else
          Index = Hud.MenuScroll
                + (OutputY - Geometry.Top) / Geometry.RowHeight;
        if(Index >= 0 && Index < int(Hud.MenuOptions.size()))
        {
          if(Motion)
          {
            if(Hud.MenuSelected != Index)
            {
              Hud.MenuSelected = Index;
              Result.Type = PointerResult::REDRAW;
            }
            else
              Result.Type = PointerResult::CONSUMED;
            return Result;
          }
          if(Pressed)
          {
            Result.Type = PointerResult::COMMAND_KEY;
            Result.CommandCode = KEY_MOBILE_MENU_SELECT_BASE + Index;
            return Result;
          }
        }
      }
      if(InMenu)
        Result.Type = PointerResult::CONSUMED;
      return Result;
    }

    if(Contains(CurrentLayout.EquipmentCanvas, OutputX, OutputY))
    {
      if(WheelY)
      {
        Result.Type = PointerResult::CONSUMED;
        return Result;
      }
      if(Motion || Pressed)
      {
        const SDL_Rect Source = EquipmentSource(CurrentLayout.CanvasWidth,
                                                 CurrentLayout.CanvasHeight);
        Result.Type = PointerResult::CANVAS_MOUSE_EVENT;
        Result.CanvasX = Source.x + Clamp(
          (OutputX - CurrentLayout.EquipmentCanvas.x) * Source.w
            / std::max(1, CurrentLayout.EquipmentCanvas.w),
          0, Source.w - 1);
        Result.CanvasY = Source.y + Clamp(
          (OutputY - CurrentLayout.EquipmentCanvas.y) * Source.h
            / std::max(1, CurrentLayout.EquipmentCanvas.h),
          0, Source.h - 1);
        Result.Button = Pressed ? Button : 0;
        Result.Motion = Motion;
        return Result;
      }
      Result.Type = PointerResult::CONSUMED;
      return Result;
    }

    if(WheelY && Contains(CurrentLayout.Rail, OutputX, OutputY))
    {
      CurrentLayout.ActionScroll = Clamp(
          CurrentLayout.ActionScroll + (WheelY < 0 ? 2 : -2), 0,
          MaximumActionScroll(CurrentLayout.ActiveCategory));
      RebuildActionRects();
      Result.Type = PointerResult::REDRAW;
      Result.WheelY = WheelY;
      return Result;
    }

    if(WheelY && Contains(CurrentLayout.MapPanel, OutputX, OutputY)
       && AdjustMapZoom(WheelY > 0 ? 1 : -1))
    {
      Result.Type = PointerResult::REDRAW;
      Result.WheelY = WheelY;
      return Result;
    }

    if(Motion)
    {
      int NewHover = -1;
      for(int Index = 0; Index < int(CurrentLayout.ActionButtons.size());
          ++Index)
        if(Contains(CurrentLayout.ActionButtons[Index], OutputX, OutputY))
          NewHover = Index;
      if(NewHover != CurrentLayout.HoverAction)
      {
        CurrentLayout.HoverAction = NewHover;
        Result.Type = PointerResult::REDRAW;
        return Result;
      }
      int CanvasX = 0;
      int CanvasY = 0;
      if(MapOutputToCanvas(CurrentLayout, OutputX, OutputY,
                           CurrentLayout.CanvasWidth,
                           CurrentLayout.CanvasHeight, CanvasX, CanvasY))
      {
        Result.Type = PointerResult::CANVAS_MOUSE_EVENT;
        Result.CanvasX = CanvasX;
        Result.CanvasY = CanvasY;
        Result.Motion = true;
        Result.Button = 0;
        return Result;
      }
      return Result;
    }

    if(!Pressed)
      return Result;

    if(HasGameplayContext()
       && Contains(CurrentLayout.Log, OutputX, OutputY))
    {
      Result.Type = PointerResult::COMMAND_KEY;
      Result.CommandCode = HistoryDispatchCode();
      return Result;
    }

    for(int Category = 0; Category < ACTION_GROUPS; ++Category)
      if(Contains(CurrentLayout.ActionTabs[Category], OutputX, OutputY))
      {
        CurrentLayout.ActiveCategory = Category;
        CurrentLayout.ActionScroll = 0;
        RebuildActionRects();
        Result.Type = PointerResult::REDRAW;
        return Result;
      }

    for(int Index = 0; Index < int(CurrentLayout.ActionButtons.size());
        ++Index)
      if(Contains(CurrentLayout.ActionButtons[Index], OutputX, OutputY))
      {
        const int ActionIndex = CurrentLayout.ActionButtonIndices[Index];
        if(ActionIndex >= 0 && ActionIndex < int(Hud.Actions.size()))
        {
          Result.Type = PointerResult::COMMAND_KEY;
          Result.CommandCode = Hud.Actions[ActionIndex].DispatchCode;
          CurrentLayout.HoverAction = Index;
          CurrentLayout.PressedAction = Index;
          return Result;
        }
      }

    int CanvasX = 0;
    int CanvasY = 0;
    if(MapOutputToCanvas(CurrentLayout, OutputX, OutputY,
                         CurrentLayout.CanvasWidth,
                         CurrentLayout.CanvasHeight, CanvasX, CanvasY))
    {
        Result.Type = PointerResult::CANVAS_MOUSE_EVENT;
      Result.CanvasX = CanvasX;
      Result.CanvasY = CanvasY;
      Result.Button = Button;
      return Result;
    }

    return Result;
  }
#endif
}
