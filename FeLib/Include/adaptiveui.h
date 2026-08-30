#ifndef __ADAPTIVEUI_H__
#define __ADAPTIVEUI_H__

#ifdef USE_SDL
#include "SDL.h"
#endif

#include <string>
#include <vector>

namespace adaptiveui
{
  enum PlatformMode
  {
    Desktop,
    Android
  };

  enum ActionCategory
  {
    ACTION_CONTEXT = 0,
    ACTION_ITEMS = 1,
    ACTION_CHARACTER = 2,
    ACTION_MOVE = 3,
    ACTION_SYSTEM = 4,
    ACTION_GROUPS = 5
  };

  enum MenuPresentationKind
  {
    MENU_ROWS = 0,
    MENU_CATEGORY_GRID,
    MENU_ITEM_GRID,
    MENU_PICKUP_GRID,
    MENU_BUTTON_ROWS,
    MENU_GUIDE,
    MENU_DETAIL
  };

  struct StatusIndicator
  {
    std::string Label;
    std::string Value;
    unsigned char Red;
    unsigned char Green;
    unsigned char Blue;

    StatusIndicator()
      : Red(190), Green(170), Blue(110) { }
  };

  struct ActionEntry
  {
    std::string Label;
    int DispatchCode;
    std::string DisplayedShortcut;
    int Category;
    bool Available;

    ActionEntry()
      : DispatchCode(0), Category(ACTION_SYSTEM), Available(false) { }
  };

  struct MapNote
  {
    std::string Label;
    int X;
    int Y;

    MapNote() : X(0), Y(0) { }
  };

  struct ItemMetrics
  {
    unsigned long ItemId;
    bool Present;
    bool Armor;
    bool Weapon;
    bool Shield;
    bool Equippable;
    unsigned int Actions;
    long Weight;
    int ArmorValue;
    int MinimumDamage;
    int MaximumDamage;
    int ToHit;
    int Block;
    int Enchantment;
    std::string Label;
    std::string Accuracy;
    std::string Durability;
    std::string BlockQuality;
    int CategorySkill;
    int SpecificSkill;

    ItemMetrics();
  };

  enum ItemAction
  {
    ITEM_ACTION_NONE = 0,
    ITEM_ACTION_DRINK = 1,
    ITEM_ACTION_TASTE = 2,
    ITEM_ACTION_EAT = 3,
    ITEM_ACTION_READ = 4,
    ITEM_ACTION_ZAP = 5,
    ITEM_ACTION_APPLY = 6
  };

  inline unsigned int ItemActionMask(ItemAction Action)
  {
    return Action == ITEM_ACTION_NONE ? 0U : 1U << unsigned(Action);
  }

  struct HudModel
  {
    std::string Stats[4];
    std::string Location;
    std::string Clock;
    std::vector<StatusIndicator> Conditions;
    std::string LogMessage;
    std::string Prompt;
    std::string PromptDetail;
    std::string PromptInput;
    bool PromptActive;
    bool PromptShowsInput;
    bool PromptNumeric;
    bool PromptCapturesKey;
    bool PromptConfirmsKeyTransfer;
    bool PromptConfirmsChoice;
    bool PromptOffersQuitChoices;
    bool PositionPrompt;
    bool PaperDollScreen;
    SDL_Rect PaperDollSource;
    bool ScreenTextActive;
    std::string ScreenTextTitle;
    std::string ScreenText;
    bool MenuActive;
    std::string MenuTitle;
    std::string MenuSubtitle;
    std::vector<std::string> MenuOptions;
    std::vector<std::string> MenuDetails;
    std::vector<std::string> MenuGroups;
    std::vector<SDL_Rect> MenuIconSources;
    std::vector<unsigned char> MenuAvailability;
    std::vector<ItemMetrics> MenuItemMetrics;
    std::vector<ItemMetrics> MenuComparisonMetrics;
    std::vector<int> MenuDisplayOrder;
    MenuPresentationKind MenuKind;
    bool MenuIconGrid;
    bool EquipmentComparisonActive;
    std::string EquippedItemLabel;
    ItemMetrics EquippedItemMetrics;
    long InventoryCurrentWeight;
    long InventoryMaximumWeight;
    int MenuSelected;
    int MenuScroll;
    int MenuPage;
    int MenuPages;
    std::vector<ActionEntry> Actions;
    int QuestionChoices[9];
    int QuestionChoiceCount;
    bool MapScreen;
    SDL_Rect MapSource;
    bool HasMapSource;
    int MapFocusX;
    int MapFocusY;
    int PlayerFocusX;
    int PlayerFocusY;
    std::vector<MapNote> MapNotes;

    HudModel();
  };

  struct MobileMenuLayout
  {
    SDL_Rect Area;
    SDL_Rect PaperDoll;
    SDL_Rect Conditions;
    SDL_Rect GridViewport;
    SDL_Rect Detail;
    SDL_Rect Footer;
    std::vector<SDL_Rect> Cells;
    int Columns;
    int CellSize;
    int ContentHeight;
    int MaximumScrollY;
    bool Landscape;

    MobileMenuLayout();
  };

  struct Layout
  {
    int OutputWidth;
    int OutputHeight;
    int CanvasWidth;
    int CanvasHeight;
    int Gutter;
    int Gap;
    int DashboardRows;
    bool Fullscreen;
    SDL_Rect Dashboard;
    SDL_Rect MapPanel;
    SDL_Rect CanvasSource;
    SDL_Rect Canvas;
    SDL_Rect Rail;
    SDL_Rect EquipmentPanel;
    SDL_Rect EquipmentCanvas;
    SDL_Rect EquipmentConditions;
    SDL_Rect RailContent;
    SDL_Rect Log;
    SDL_Rect Menu;
    SDL_Rect Prompt;
    SDL_Rect PromptDialog;
    SDL_Rect PromptInput;
    SDL_Rect PromptContinue;
    SDL_Rect PromptDecline;
    SDL_Rect PromptCancel;
    SDL_Rect MenuBack;
    SDL_Rect MenuConfirm;
    SDL_Rect MenuPrevious;
    SDL_Rect MenuNext;
    SDL_Rect MenuDetail;
    std::vector<SDL_Rect> MenuCells;
    std::vector<SDL_Rect> MenuItemActions;
    std::vector<int> MenuItemActionCodes;
    SDL_Rect ActionArea;
    SDL_Rect ActionTabs[ACTION_GROUPS];
    std::vector<SDL_Rect> ActionButtons;
    std::vector<int> ActionButtonIndices;
    SDL_Rect MapNotesArea;
    std::vector<SDL_Rect> MapNoteRows;
    std::vector<int> MapNoteIndices;
    std::vector<SDL_Rect> MapNoteMarkers;
    std::vector<SDL_Rect> MapActionButtons;
    std::vector<int> MapActionKeys;
    int ActiveCategory;
    int ActionScroll;
    int HoverAction;
    int PressedAction;

    Layout();
  };

  struct PointerResult
  {
    enum Kind
    {
      POINTER_NONE,
      CONSUMED,
      COMMAND_KEY,
      CANVAS_MOUSE_EVENT,
      REDRAW
    };

    Kind Type;
    int CommandCode;
    int CanvasX;
    int CanvasY;
    int Button;
    int WheelY;
    bool Motion;

    PointerResult()
      : Type(POINTER_NONE), CommandCode(0), CanvasX(0), CanvasY(0), Button(0),
        WheelY(0), Motion(false) { }
  };

  Layout CalculateLayout(int OutputWidth, int OutputHeight,
                         int CanvasWidth, int CanvasHeight,
                         bool Fullscreen = false);
  int CalculateEquipmentPageSize(const Layout& Current, int ItemCount);
  bool MapOutputToCanvas(const Layout& Current, int OutputX, int OutputY,
                         int CanvasWidth, int CanvasHeight,
                          int& CanvasX, int& CanvasY);
  MobileMenuLayout CalculateMobileMenuLayout(const SDL_Rect& Area,
                                               float Density,
                                               int ItemCount,
                                               bool ShowPaperDoll,
                                               bool ShowDetail,
                                               int ScrollY = 0);
  int MobileMenuIndexAt(const MobileMenuLayout& Current, int X, int Y);

  void SetPlatformMode(PlatformMode Mode);
  PlatformMode GetPlatformMode();
  bool IsDesktopPresentationEnabled();
  const HudModel& GetHudModel();

  void SetStats(const char* Line1, const char* Line2,
                const char* Line3, const char* Line4);
  void SetLocationTime(const char* Location, const char* Clock);
  void SetConditions(const char* const* Labels, int Count);
  void SetLog(const char* Message);
  void SetPrompt(const char* Prompt, const char* Input = 0,
                 bool Numeric = false);
  void SetKeyCapturePrompt(const char* Prompt);
  void SetKeyTransferPrompt(const char* Prompt);
  void SetConfirmationPrompt(const char* Prompt);
  void SetQuitPrompt(const char* Prompt);
  void SetPromptDetail(const char* Detail);
  void SetPositionPrompt(bool Active);
  void ClearPrompt();
  void SetPaperDollScreen(bool Active, int X = 0, int Y = 0,
                          int Width = 0, int Height = 0);
  void SetScreenText(const char* Text);
  void SetScreenText(const char* Title, const char* Text);
  void ClearScreenText();
  void SetMenu(const char* Title, const char* Subtitle,
               const char* const* Options, int Count, int Selected,
               int Page, int Pages);
  void SetMenuPresentation(const char* const* Details,
                           const SDL_Rect* IconSources, int Count,
                           MenuPresentationKind Kind);
  void SetMenuGroups(const char* const* Groups, int Count);
  void SetMenuAvailability(const unsigned char* Available, int Count);
  void SetMenuItemMetrics(const ItemMetrics* Metrics, int Count);
  void SetMenuComparisonMetrics(const ItemMetrics* Metrics, int Count);
  void SetEquipmentComparison(const char* Label,
                              const ItemMetrics& Metrics);
  void SetInventoryWeights(long Current, long Maximum);
  void ClearMenu();
  void SetActions(const char* const* Labels, const int* Keys,
                  const int* Groups, int Count);
  void SetActionShortcuts(const int* Keys, int Count);
  int PageMenu(int Selected, int Direction, int Count);
  int NavigateInventoryMenu(int Selected, int Key, int Count);
  void SetQuestionChoices(const int* Keys, int Count);
  void SetMapScreen(bool Active);
  void SetMapSourceBounds(int X, int Y, int Width, int Height);
  void SetMapFocus(int X, int Y);
  void SetMapFocus(int X, int Y, int PlayerX, int PlayerY);
  void SetMapNotes(const char* const* Notes, const int* X, const int* Y,
                   int Count);
  int GetSelectedMapNote();
  bool AdjustMapZoom(int Steps);

#ifdef USE_SDL
  int TranslateDesktopShortcut(SDL_Keycode Key, SDL_Keymod Modifiers);
  bool SelectActionCategory(int Category);
  void UpdateLayout(SDL_Renderer* Renderer, int CanvasWidth, int CanvasHeight,
                    bool Fullscreen = false);
  const Layout& GetLayout();
  const SDL_Rect& GetCanvasRect();
  bool IsTextEntryPromptActive();
  void DrawBackground(SDL_Renderer* Renderer);
  void DrawGame(SDL_Renderer* Renderer, SDL_Texture* GameTexture);
  void Draw(SDL_Renderer* Renderer);
  PointerResult HandlePointer(int OutputX, int OutputY, bool Pressed,
                              int WheelY = 0, bool Motion = false,
                              int Button = 1);
#endif
}

#endif
