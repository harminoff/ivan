#define SDL_MAIN_HANDLED

#include <cassert>
#include <cmath>

#include "adaptiveui.h"
#include "felibdef.h"

namespace
{
  bool Overlaps(const SDL_Rect& A, const SDL_Rect& B)
  {
    return A.x < B.x + B.w && B.x < A.x + A.w
        && A.y < B.y + B.h && B.y < A.y + A.h;
  }

  void CheckSize(int Width, int Height)
  {
    const adaptiveui::Layout Layout = adaptiveui::CalculateLayout(
      Width, Height, 800, 600, false);
    assert(Layout.OutputWidth == Width);
    assert(Layout.OutputHeight == Height);
    assert(Layout.MapPanel.w >= 400);
    assert(Layout.MapPanel.h >= 100);
    assert(Layout.Rail.w >= 300 && Layout.Rail.w <= 420);
    if(Width == 1280)
      assert(Layout.Rail.w >= 350);
    assert(Layout.Log.h >= 52 && Layout.Log.h <= 68);
    if(Width < 1500)
    {
      assert(Layout.DashboardRows == 2);
      assert(Layout.Dashboard.h >= 92 && Layout.Dashboard.h <= 108);
    }
    else
    {
      assert(Layout.DashboardRows == 1);
      assert(Layout.Dashboard.h >= 70 && Layout.Dashboard.h <= 84);
    }
    assert(!Overlaps(Layout.Dashboard, Layout.MapPanel));
    assert(!Overlaps(Layout.Dashboard, Layout.Rail));
    assert(!Overlaps(Layout.MapPanel, Layout.Rail));
    assert(!Overlaps(Layout.Log, Layout.Rail));
    assert(!Overlaps(Layout.EquipmentPanel, Layout.RailContent));
    assert(Layout.EquipmentPanel.x >= Layout.Rail.x);
    assert(Layout.EquipmentPanel.y == Layout.Rail.y + 7);
    assert(Layout.EquipmentPanel.x + Layout.EquipmentPanel.w
           <= Layout.Rail.x + Layout.Rail.w);
    assert(Layout.RailContent.y > Layout.EquipmentPanel.y);
    assert(Layout.RailContent.y + Layout.RailContent.h
           <= Layout.Rail.y + Layout.Rail.h);
    assert(Layout.EquipmentCanvas.w > 0 && Layout.EquipmentCanvas.h > 0);
    assert(std::fabs(double(Layout.EquipmentCanvas.w)
                     / Layout.EquipmentCanvas.h - 96.0 / 112.0) < .03);
    assert(Layout.Canvas.w > 0 && Layout.Canvas.h > 0);
    assert(Layout.CanvasSource.x == 16);
    assert(Layout.CanvasSource.y == 32);
    assert(Layout.CanvasSource.w == 672);
    assert(Layout.CanvasSource.h == 416);
    const double SourceAspect = 672.0 / 416.0;
    const double CanvasAspect = double(Layout.Canvas.w) / Layout.Canvas.h;
    assert(std::fabs(SourceAspect - CanvasAspect) < .02);

    int CanvasX = -1;
    int CanvasY = -1;
    assert(adaptiveui::MapOutputToCanvas(Layout,
                                         Layout.Canvas.x + Layout.Canvas.w / 2,
                                         Layout.Canvas.y + Layout.Canvas.h / 2,
                                         800, 600, CanvasX, CanvasY));
    assert(CanvasX >= 351 && CanvasX <= 353);
    assert(CanvasY >= 239 && CanvasY <= 241);
    assert(!adaptiveui::MapOutputToCanvas(Layout,
                                          Layout.MapPanel.x,
                                          Layout.MapPanel.y,
                                          800, 600, CanvasX, CanvasY));
  }

  void CheckDynamicEquipmentPaging()
  {
    const int ItemCount = 13;
    const adaptiveui::Layout Minimum = adaptiveui::CalculateLayout(
      960, 540, 800, 600, false);
    const adaptiveui::Layout Default = adaptiveui::CalculateLayout(
      1280, 720, 800, 600, false);
    const adaptiveui::Layout Large = adaptiveui::CalculateLayout(
      1600, 900, 800, 600, false);
    const adaptiveui::Layout Tall = adaptiveui::CalculateLayout(
      1920, 1080, 800, 600, false);

    const int MinimumPage = adaptiveui::CalculateEquipmentPageSize(
      Minimum, ItemCount);
    const int DefaultPage = adaptiveui::CalculateEquipmentPageSize(
      Default, ItemCount);
    const int LargePage = adaptiveui::CalculateEquipmentPageSize(
      Large, ItemCount);
    const int TallPage = adaptiveui::CalculateEquipmentPageSize(
      Tall, ItemCount);

    assert(MinimumPage >= 1);
    assert(MinimumPage < DefaultPage);
    assert(DefaultPage < LargePage);
    assert(LargePage <= TallPage);
    assert(adaptiveui::CalculateEquipmentPageSize(Default, 1)
           >= DefaultPage);
  }

  void CheckFeeds()
  {
    const char* Labels[64];
    int Keys[64];
    int Groups[64];
    for(int Index = 0; Index < 64; ++Index)
    {
      Labels[Index] = "A deliberately long action label for scrolling";
      Keys[Index] = 1000 + Index;
      Groups[Index] = Index % adaptiveui::ACTION_GROUPS;
    }
    adaptiveui::SetActions(Labels, Keys, Groups, 64);
    assert(adaptiveui::GetHudModel().Actions.size() == 64);
    const int DisplayKeys[2] = { '>', '@' };
    adaptiveui::SetActionShortcuts(DisplayKeys, 2);
    assert(adaptiveui::GetHudModel().Actions[0].DisplayedShortcut == ">");
    assert(adaptiveui::GetHudModel().Actions[1].DisplayedShortcut == "@");
    adaptiveui::SetActions(0, 0, 0, 0);
    assert(adaptiveui::GetHudModel().Actions.empty());

    const char* Options[4] = { "A very long selectable row", "Second row",
                               "Third row", "Fourth row" };
    adaptiveui::SetMenu("TEST", "MULTI PAGE", Options, 4, 0, 2, 3);
    assert(adaptiveui::GetHudModel().MenuActive);
    assert(adaptiveui::GetHudModel().MenuPages == 3);
    assert(adaptiveui::GetHudModel().MenuOptions.size() == 4);
    adaptiveui::ClearMenu();
    assert(!adaptiveui::GetHudModel().MenuActive);

    assert(adaptiveui::TranslateDesktopShortcut(SDLK_PERIOD, KMOD_SHIFT) == '>');
    assert(adaptiveui::TranslateDesktopShortcut(SDLK_COMMA, KMOD_SHIFT) == '<');
    assert(adaptiveui::TranslateDesktopShortcut(SDLK_2, KMOD_SHIFT) == '@');
    assert(adaptiveui::TranslateDesktopShortcut(SDLK_BACKSLASH, KMOD_NONE) == '\\');
    assert(adaptiveui::TranslateDesktopShortcut(SDLK_s, KMOD_SHIFT) == 'S');
    assert(adaptiveui::TranslateDesktopShortcut(SDLK_s, KMOD_NONE) == 's');
  }

  void CheckPointerInput()
  {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_Window* Window = SDL_CreateWindow("adaptiveui-test",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          1280, 720, SDL_WINDOW_HIDDEN);
    assert(Window != 0);
    SDL_Renderer* Renderer = SDL_CreateRenderer(Window, -1,
                                                SDL_RENDERER_SOFTWARE);
    assert(Renderer != 0);

    adaptiveui::SetPlatformMode(adaptiveui::Desktop);
    const char* Labels[48];
    int Keys[48];
    int Groups[48];
    for(int Index = 0; Index < 48; ++Index)
    {
      Labels[Index] = "SCROLLABLE ACTION";
      Keys[Index] = 700 + Index;
      Groups[Index] = adaptiveui::ACTION_CONTEXT;
    }
    Labels[47] = "HISTORY";
    Keys[47] = KEY_MOBILE_COMMAND_BASE + 34;
    Groups[47] = adaptiveui::ACTION_SYSTEM;
    adaptiveui::SetActions(Labels, Keys, Groups, 48);
    adaptiveui::SetMapFocus(352, 240);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetLayout().ActionButtons.size() >= 16);
    assert(adaptiveui::SelectActionCategory(adaptiveui::ACTION_ITEMS));
    assert(adaptiveui::GetLayout().ActiveCategory
           == adaptiveui::ACTION_ITEMS);
    assert(adaptiveui::GetLayout().ActionButtons.empty());
    assert(adaptiveui::SelectActionCategory(adaptiveui::ACTION_CONTEXT));
    assert(!adaptiveui::GetLayout().ActionButtons.empty());

    const adaptiveui::Layout EquipmentLayout = adaptiveui::GetLayout();
    adaptiveui::PointerResult EquipmentClick = adaptiveui::HandlePointer(
      EquipmentLayout.EquipmentCanvas.x + EquipmentLayout.EquipmentCanvas.w / 2,
      EquipmentLayout.EquipmentCanvas.y + EquipmentLayout.EquipmentCanvas.h / 2,
      true, 0, false, 1);
    assert(EquipmentClick.Type
           == adaptiveui::PointerResult::CANVAS_MOUSE_EVENT);
    assert(EquipmentClick.CanvasX >= 744 && EquipmentClick.CanvasX <= 746);
    assert(EquipmentClick.CanvasY >= 84 && EquipmentClick.CanvasY <= 86);

    const adaptiveui::Layout BeforeZoom = adaptiveui::GetLayout();
    assert(BeforeZoom.CanvasSource.w == 336);
    assert(BeforeZoom.CanvasSource.h == 208);
    adaptiveui::PointerResult Zoom = adaptiveui::HandlePointer(
      BeforeZoom.Canvas.x + BeforeZoom.Canvas.w / 2,
      BeforeZoom.Canvas.y + BeforeZoom.Canvas.h / 2,
      false, 1, false, 0);
    assert(Zoom.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetLayout().CanvasSource.w < BeforeZoom.CanvasSource.w);
    assert(adaptiveui::GetLayout().CanvasSource.h < BeforeZoom.CanvasSource.h);
    int ZoomedCanvasX = -1;
    int ZoomedCanvasY = -1;
    assert(adaptiveui::MapOutputToCanvas(
      adaptiveui::GetLayout(),
      adaptiveui::GetLayout().Canvas.x + adaptiveui::GetLayout().Canvas.w / 2,
      adaptiveui::GetLayout().Canvas.y + adaptiveui::GetLayout().Canvas.h / 2,
      800, 600, ZoomedCanvasX, ZoomedCanvasY));
    assert(ZoomedCanvasX >= 351 && ZoomedCanvasX <= 353);
    assert(ZoomedCanvasY >= 239 && ZoomedCanvasY <= 241);
    assert(adaptiveui::AdjustMapZoom(-3));
    assert(adaptiveui::AdjustMapZoom(1));

    const adaptiveui::Layout BeforeWheel = adaptiveui::GetLayout();
    adaptiveui::PointerResult Wheel = adaptiveui::HandlePointer(
      BeforeWheel.ActionArea.x + 5, BeforeWheel.ActionArea.y + 5,
      false, -1, false, 0);
    assert(Wheel.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetLayout().ActionScroll == 2);

    const adaptiveui::Layout AfterWheel = adaptiveui::GetLayout();
    adaptiveui::PointerResult Motion = adaptiveui::HandlePointer(
      AfterWheel.ActionButtons[0].x + 2,
      AfterWheel.ActionButtons[0].y + 2,
      false, 0, true, 0);
    assert(Motion.Type == adaptiveui::PointerResult::REDRAW);
    adaptiveui::PointerResult Click = adaptiveui::HandlePointer(
      AfterWheel.ActionButtons[0].x + 2,
      AfterWheel.ActionButtons[0].y + 2,
      true, 0, false, 1);
    assert(Click.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(Click.CommandCode == 702);

    const adaptiveui::Layout BeforeEmptyCategory = adaptiveui::GetLayout();
    adaptiveui::PointerResult EmptyCategory = adaptiveui::HandlePointer(
      BeforeEmptyCategory.ActionTabs[adaptiveui::ACTION_ITEMS].x + 2,
      BeforeEmptyCategory.ActionTabs[adaptiveui::ACTION_ITEMS].y + 2,
      true, 0, false, 1);
    assert(EmptyCategory.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetLayout().ActionButtons.empty());

    SDL_Texture* GameTexture = SDL_CreateTexture(
      Renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
      800, 600);
    assert(GameTexture != 0);
    std::vector<unsigned short> GreenCanvas(800 * 600, 0x07e0);
    assert(SDL_UpdateTexture(GameTexture, 0, &GreenCanvas[0],
                             800 * sizeof(unsigned short)) == 0);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::DrawGame(Renderer, GameTexture);
    adaptiveui::Draw(Renderer);

    const char* MapNotes[12] = {
      "Stairway up", "Stairway down", "Vault entrance", "Temple",
      "Shop", "Fountain", "Dangerous corridor", "Locked door",
      "Altar", "Bridge", "Hidden cache", "Long wrapped map note label"
    };
    int MapNoteX[12];
    int MapNoteY[12];
    for(int Index = 0; Index < 12; ++Index)
    {
      MapNoteX[Index] = 120 + (Index % 4) * 80;
      MapNoteY[Index] = 120 + (Index / 4) * 80;
    }
    const int MapKeys[7] = {
      't', 'l', 'r', 'd', 'e', KEY_SPECIAL, KEY_ESC
    };
    adaptiveui::SetMapScreen(true);
    adaptiveui::SetMapSourceBounds(100, 100, 400, 300);
    adaptiveui::SetMapNotes(MapNotes, MapNoteX, MapNoteY, 12);
    adaptiveui::SetPrompt("Cartography notes action");
    adaptiveui::SetQuestionChoices(MapKeys, 7);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout MapLayout = adaptiveui::GetLayout();
    assert(MapLayout.EquipmentPanel.w == 0);
    assert(MapLayout.EquipmentCanvas.w == 0);
    assert(MapLayout.CanvasSource.x == 98);
    assert(MapLayout.CanvasSource.y == 98);
    assert(MapLayout.CanvasSource.w == 404);
    assert(MapLayout.CanvasSource.h == 304);
    assert(!MapLayout.MapNoteRows.empty());
    assert(MapLayout.MapActionButtons.size() == 6);
    assert(MapLayout.MapActionKeys.size() == 6);
    assert(!Overlaps(MapLayout.MapNotesArea,
                     MapLayout.MapActionButtons[0]));

    adaptiveui::PointerResult SelectMapNote = adaptiveui::HandlePointer(
      MapLayout.MapNoteRows[0].x + 8, MapLayout.MapNoteRows[0].y + 8,
      true, 0, false, 1);
    assert(SelectMapNote.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetSelectedMapNote() == 0);

    int EditButton = -1;
    for(size_t Index = 0; Index < MapLayout.MapActionKeys.size(); ++Index)
      if(MapLayout.MapActionKeys[Index] == 'e')
        EditButton = int(Index);
    assert(EditButton >= 0);
    adaptiveui::PointerResult EditMapNote = adaptiveui::HandlePointer(
      MapLayout.MapActionButtons[EditButton].x + 5,
      MapLayout.MapActionButtons[EditButton].y + 5,
      true, 0, false, 1);
    assert(EditMapNote.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(EditMapNote.CommandCode == 'e');

    adaptiveui::PointerResult ScrollMapNotes = adaptiveui::HandlePointer(
      MapLayout.MapNotesArea.x + 5, MapLayout.MapNotesArea.y + 5,
      false, -1, false, 0);
    assert(ScrollMapNotes.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetLayout().MapNoteIndices[0] > 0);

    const adaptiveui::Layout ScrolledMapLayout = adaptiveui::GetLayout();
    assert(ScrolledMapLayout.MapNoteMarkers.size() == 12);
    adaptiveui::PointerResult SelectMapMarker = adaptiveui::HandlePointer(
      ScrolledMapLayout.MapNoteMarkers[1].x + 4,
      ScrolledMapLayout.MapNoteMarkers[1].y + 4,
      true, 0, false, 1);
    assert(SelectMapMarker.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetSelectedMapNote() == 1);

    adaptiveui::SetPositionPrompt(true);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout CursorLayout = adaptiveui::GetLayout();
    assert(CursorLayout.MapActionKeys.size() == 3);
    assert(CursorLayout.MapActionKeys[0] == KEY_ENTER);
    assert(CursorLayout.MapActionKeys[1] == KEY_ESC);
    assert(CursorLayout.MapActionKeys[2] == KEY_SPECIAL);
    adaptiveui::PointerResult SelectNotedMapTile = adaptiveui::HandlePointer(
      CursorLayout.MapNoteMarkers[1].x + 11,
      CursorLayout.MapNoteMarkers[1].y + 11,
      true, 0, false, 1);
    assert(SelectNotedMapTile.Type
           == adaptiveui::PointerResult::CANVAS_MOUSE_EVENT);
    adaptiveui::PointerResult SelectMapTile = adaptiveui::HandlePointer(
      CursorLayout.Canvas.x + CursorLayout.Canvas.w - 3,
      CursorLayout.Canvas.y + CursorLayout.Canvas.h - 3,
      true, 0, false, 1);
    assert(SelectMapTile.Type
           == adaptiveui::PointerResult::CANVAS_MOUSE_EVENT);
    assert(SelectMapTile.CanvasX >= 498 && SelectMapTile.CanvasX <= 501);
    assert(SelectMapTile.CanvasY >= 398 && SelectMapTile.CanvasY <= 401);

    adaptiveui::DrawBackground(Renderer);
    adaptiveui::DrawGame(Renderer, GameTexture);
    adaptiveui::Draw(Renderer);
    adaptiveui::SetScreenText("MAP CURSOR HELP",
      "Move the cursor with the keyboard or mouse. Click to continue.");
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetHudModel().ScreenTextActive);
    assert(adaptiveui::GetHudModel().ScreenTextTitle == "MAP CURSOR HELP");
    adaptiveui::PointerResult CloseMapHelp = adaptiveui::HandlePointer(
      adaptiveui::GetLayout().Canvas.x + 20,
      adaptiveui::GetLayout().Canvas.y + 20,
      true, 0, false, 1);
    assert(CloseMapHelp.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(CloseMapHelp.CommandCode == KEY_ENTER);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearScreenText();
    adaptiveui::ClearPrompt();
    adaptiveui::SetQuestionChoices(0, 0);
    adaptiveui::SetMapNotes(0, 0, 0, 0);
    adaptiveui::SetMapScreen(false);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);

    const char* Options[40];
    for(int Index = 0; Index < 40; ++Index)
      Options[Index] = "A long selectable menu row";
    adaptiveui::SetMenu("TEST", "SCROLLING MENU", Options, 40, 0, 1, 2);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout MenuLayout = adaptiveui::GetLayout();
    assert(MenuLayout.MenuPrevious.y == MenuLayout.MenuBack.y);
    assert(MenuLayout.MenuNext.y == MenuLayout.MenuBack.y);
    assert(MenuLayout.MenuBack.x < MenuLayout.MenuPrevious.x);
    assert(MenuLayout.MenuPrevious.x < MenuLayout.MenuNext.x);
    assert(!Overlaps(MenuLayout.MenuBack, MenuLayout.MenuPrevious));
    assert(!Overlaps(MenuLayout.MenuPrevious, MenuLayout.MenuNext));
    assert(!adaptiveui::SelectActionCategory(adaptiveui::ACTION_ITEMS));
    adaptiveui::PointerResult MenuBack = adaptiveui::HandlePointer(
      MenuLayout.RailContent.x + 20,
      MenuLayout.RailContent.y + MenuLayout.RailContent.h - 20,
      true, 0, false, 1);
    assert(MenuBack.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(MenuBack.CommandCode == KEY_ESC);
    adaptiveui::PointerResult MenuWheel = adaptiveui::HandlePointer(
      MenuLayout.RailContent.x + 10, MenuLayout.RailContent.y + 100,
      false, -1, false, 0);
    assert(MenuWheel.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetHudModel().MenuScroll > 0);
    bool ReachedNextPage = false;
    for(int Attempt = 0; Attempt < 64; ++Attempt)
    {
      adaptiveui::PointerResult Scroll = adaptiveui::HandlePointer(
        MenuLayout.RailContent.x + 10, MenuLayout.RailContent.y + 100,
        false, -1, false, 0);
      if(Scroll.Type == adaptiveui::PointerResult::COMMAND_KEY)
      {
        assert(Scroll.CommandCode == KEY_PAGE_DOWN);
        ReachedNextPage = true;
        break;
      }
    }
    assert(ReachedNextPage);
    adaptiveui::PointerResult NextPage = adaptiveui::HandlePointer(
      MenuLayout.MenuNext.x + MenuLayout.MenuNext.w / 2,
      MenuLayout.MenuNext.y + MenuLayout.MenuNext.h / 2,
      true, 0, false, 1);
    assert(NextPage.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(NextPage.CommandCode == KEY_PAGE_DOWN);
    const adaptiveui::Layout ScrolledMenu = adaptiveui::GetLayout();
    adaptiveui::PointerResult MenuClick = adaptiveui::HandlePointer(
      ScrolledMenu.RailContent.x + 20,
      ScrolledMenu.RailContent.y + 100,
      true, 0, false, 1);
    assert(MenuClick.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(MenuClick.CommandCode >= KEY_MOBILE_MENU_SELECT_BASE);

    const char* EquipmentSlots[13] = {
      "helmet: -", "amulet: -", "cloak: -", "body armor: -",
      "belt: -", "right hand wielded: a bronze mace",
      "left hand wielded: -", "right ring: -", "left ring: -",
      "right gauntlet: -", "left gauntlet: -", "right boot: -",
      "left boot: -"
    };
    adaptiveui::SetMenu("Equipment", "", EquipmentSlots, 13,
                        0, 1, 1);
    SDL_Rect EquipmentIcons[13];
    for(int Index = 0; Index < 13; ++Index)
      EquipmentIcons[Index] = { Index * 16, 0, 16, 16 };
    adaptiveui::SetMenuPresentation(0, EquipmentIcons, 13, true);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetHudModel().MenuOptions.size() == 13);
    assert(!adaptiveui::GetHudModel().MenuIconGrid);
    assert(adaptiveui::GetHudModel().MenuPage == 1);
    assert(adaptiveui::GetHudModel().MenuPages == 3);
    const adaptiveui::Layout EquipmentMenuLayout = adaptiveui::GetLayout();
    adaptiveui::PointerResult EquipmentNext = adaptiveui::HandlePointer(
      EquipmentMenuLayout.MenuNext.x + EquipmentMenuLayout.MenuNext.w / 2,
      EquipmentMenuLayout.MenuNext.y + EquipmentMenuLayout.MenuNext.h / 2,
      true, 0, false, 1);
    assert(EquipmentNext.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(EquipmentNext.CommandCode == KEY_PAGE_DOWN);
    adaptiveui::PointerResult EquipmentWheel = adaptiveui::HandlePointer(
      EquipmentMenuLayout.RailContent.x + 20,
      EquipmentMenuLayout.RailContent.y + 100,
      false, -1, false, 0);
    assert(EquipmentWheel.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(EquipmentWheel.CommandCode == KEY_PAGE_DOWN);
    adaptiveui::SetMenu("Equipment", "", EquipmentSlots, 13,
                        5, 1, 1);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    adaptiveui::Draw(Renderer);
    assert(adaptiveui::GetHudModel().MenuPage == 2);
    assert(adaptiveui::GetHudModel().MenuPages == 3);
    assert(adaptiveui::GetHudModel().MenuScroll == 5);

    adaptiveui::DrawBackground(Renderer);
    adaptiveui::DrawGame(Renderer, GameTexture);
    adaptiveui::Draw(Renderer);
    const adaptiveui::Layout ContainedMenu = adaptiveui::GetLayout();
    Uint32 MapPixel = 0;
    SDL_Rect MapSample = { ContainedMenu.MapPanel.x
                           + ContainedMenu.MapPanel.w / 2,
                           ContainedMenu.MapPanel.y
                           + ContainedMenu.MapPanel.h / 2, 1, 1 };
    assert(SDL_RenderReadPixels(Renderer, &MapSample,
      SDL_PIXELFORMAT_ARGB8888, &MapPixel, sizeof(MapPixel)) == 0);
    SDL_PixelFormat* PixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    assert(PixelFormat != 0);
    Uint8 Red = 0;
    Uint8 Green = 0;
    Uint8 Blue = 0;
    Uint8 Alpha = 0;
    SDL_GetRGBA(MapPixel, PixelFormat, &Red, &Green, &Blue, &Alpha);
    assert(Green > 200 && Red < 40 && Blue < 40);
    SDL_FreeFormat(PixelFormat);

    adaptiveui::ClearMenu();
    const char* InventoryDetails[40];
    SDL_Rect InventoryIcons[40];
    for(int Index = 0; Index < 40; ++Index)
    {
      InventoryDetails[Index] = "A useful item description with material and condition information.";
      InventoryIcons[Index] = { 0, 0, 16, 16 };
    }
    adaptiveui::SetMenu("Your inventory (total weight: 1200g)", "",
                        Options, 40, 0, 1, 2);
    adaptiveui::SetMenuPresentation(InventoryDetails, InventoryIcons,
                                    40, true);
    adaptiveui::SetInventoryWeights(1200, 25000);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout InventoryLayout = adaptiveui::GetLayout();
    assert(!InventoryLayout.MenuCells.empty());
    assert(InventoryLayout.MenuDetail.w > 0);
    assert(InventoryLayout.MenuDetail.h > 0);
    assert(!Overlaps(InventoryLayout.MenuCells[0],
                     InventoryLayout.MenuDetail));
    adaptiveui::PointerResult InventoryHover = adaptiveui::HandlePointer(
      InventoryLayout.MenuCells[1].x + InventoryLayout.MenuCells[1].w / 2,
      InventoryLayout.MenuCells[1].y + InventoryLayout.MenuCells[1].h / 2,
      false, 0, true, 0);
    assert(InventoryHover.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetHudModel().MenuSelected == 1);
    assert(adaptiveui::GetHudModel().InventoryCurrentWeight == 1200);
    assert(adaptiveui::GetHudModel().InventoryMaximumWeight == 25000);
    assert(adaptiveui::NavigateInventoryMenu(1, KEY_LEFT, 40) == 0);
    assert(adaptiveui::NavigateInventoryMenu(1, KEY_RIGHT, 40) == 2);
    assert(adaptiveui::NavigateInventoryMenu(0, KEY_UP, 40) == 0);
    assert(adaptiveui::NavigateInventoryMenu(0, KEY_DOWN, 40) > 0);
    adaptiveui::PointerResult InventoryClick = adaptiveui::HandlePointer(
      InventoryLayout.MenuCells[1].x + InventoryLayout.MenuCells[1].w / 2,
      InventoryLayout.MenuCells[1].y + InventoryLayout.MenuCells[1].h / 2,
      true, 0, false, 1);
    assert(InventoryClick.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(InventoryClick.CommandCode == KEY_MOBILE_MENU_SELECT_BASE + 1);
    adaptiveui::PointerResult InventoryWheel = adaptiveui::HandlePointer(
      InventoryLayout.MenuCells[0].x + 2,
      InventoryLayout.MenuCells[0].y + 2,
      false, -1, false, 0);
    assert(InventoryWheel.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetHudModel().MenuScroll > 0);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::DrawGame(Renderer, GameTexture);
    adaptiveui::Draw(Renderer);
    const SDL_Rect IconSample = {
      InventoryLayout.MenuCells[0].x + InventoryLayout.MenuCells[0].w / 2,
      InventoryLayout.MenuCells[0].y + InventoryLayout.MenuCells[0].h / 2,
      1, 1 };
    Uint32 IconPixel = 0;
    assert(SDL_RenderReadPixels(Renderer, &IconSample,
      SDL_PIXELFORMAT_ARGB8888, &IconPixel, sizeof(IconPixel)) == 0);
    PixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    assert(PixelFormat != 0);
    SDL_GetRGBA(IconPixel, PixelFormat, &Red, &Green, &Blue, &Alpha);
    assert(Green > 200 && Red < 40 && Blue < 40);
    SDL_FreeFormat(PixelFormat);

    const SDL_Rect FilledIconSample = {
      InventoryLayout.MenuCells[0].x
        + InventoryLayout.MenuCells[0].w / 2 + 18,
      InventoryLayout.MenuCells[0].y
        + InventoryLayout.MenuCells[0].h / 2,
      1, 1 };
    IconPixel = 0;
    assert(SDL_RenderReadPixels(Renderer, &FilledIconSample,
      SDL_PIXELFORMAT_ARGB8888, &IconPixel, sizeof(IconPixel)) == 0);
    PixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    assert(PixelFormat != 0);
    SDL_GetRGBA(IconPixel, PixelFormat, &Red, &Green, &Blue, &Alpha);
    assert(Green > 200 && Red < 40 && Blue < 40);
    SDL_FreeFormat(PixelFormat);

    const char* ShortInventory[5] = {
      "an encrypted scroll [200g]", "a short sword [1200g]",
      "a potion [250g]", "a book [600g]", "a lantern [900g]" };
    adaptiveui::SetMenu("Your inventory (total weight: 3150g)", "",
                        ShortInventory, 5, 0, 1, 2);
    adaptiveui::SetMenuPresentation(InventoryDetails, InventoryIcons,
                                    5, true);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout ShortInventoryLayout = adaptiveui::GetLayout();
    assert(ShortInventoryLayout.MenuCells.size() == 5);
    assert(ShortInventoryLayout.MenuDetail.h > 126);
    assert(!Overlaps(ShortInventoryLayout.MenuCells[0],
                     ShortInventoryLayout.MenuDetail));

    adaptiveui::ClearMenu();
    adaptiveui::SetMenu("Choose helmet:", "", ShortInventory, 5, 0, 1, 2);
    adaptiveui::SetMenuPresentation(InventoryDetails, InventoryIcons,
                                    5, true);
    adaptiveui::ItemMetrics HelmetMetrics[5];
    for(int Index = 0; Index < 5; ++Index)
    {
      HelmetMetrics[Index].Present = true;
      HelmetMetrics[Index].ItemId = 100 + Index;
      HelmetMetrics[Index].Armor = true;
      HelmetMetrics[Index].Weight = 900 + Index * 100;
      HelmetMetrics[Index].ArmorValue = 4 + Index;
    }
    adaptiveui::SetMenuItemMetrics(HelmetMetrics, 5);
    adaptiveui::ItemMetrics EquippedHelmet;
    EquippedHelmet.Present = true;
    EquippedHelmet.ItemId = 102;
    EquippedHelmet.Armor = true;
    EquippedHelmet.Weight = 1200;
    EquippedHelmet.ArmorValue = 5;
    adaptiveui::SetEquipmentComparison("a bone helmet [1200g, AV 5]",
                                       EquippedHelmet);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout EquipmentChooserLayout = adaptiveui::GetLayout();
    assert(EquipmentChooserLayout.MenuCells.size() == 5);
    assert(EquipmentChooserLayout.MenuDetail.w > 0);
    assert(adaptiveui::GetHudModel().EquipmentComparisonActive);
    assert(adaptiveui::GetHudModel().MenuItemMetrics.size() == 5);
    assert(adaptiveui::GetHudModel().MenuDisplayOrder[0] == 2);
    assert(adaptiveui::NavigateInventoryMenu(2, KEY_RIGHT, 5) == 0);
    adaptiveui::PointerResult EquippedItemClick = adaptiveui::HandlePointer(
      EquipmentChooserLayout.MenuCells[0].x
        + EquipmentChooserLayout.MenuCells[0].w / 2,
      EquipmentChooserLayout.MenuCells[0].y
        + EquipmentChooserLayout.MenuCells[0].h / 2,
      true, 0, false, 1);
    assert(EquippedItemClick.Type
           == adaptiveui::PointerResult::COMMAND_KEY);
    assert(EquippedItemClick.CommandCode
           == KEY_MOBILE_MENU_SELECT_BASE + 2);

    SDL_SetWindowSize(Window, 960, 540);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout MinimumInventory = adaptiveui::GetLayout();
    assert(!MinimumInventory.MenuCells.empty());
    assert(MinimumInventory.MenuDetail.h >= 30);
    for(size_t Index = 0; Index < MinimumInventory.MenuCells.size(); ++Index)
      assert(!Overlaps(MinimumInventory.MenuCells[Index],
                       MinimumInventory.MenuDetail));
    assert(!Overlaps(MinimumInventory.MenuDetail,
                     MinimumInventory.MenuPrevious));
    assert(!Overlaps(MinimumInventory.MenuDetail,
                     MinimumInventory.MenuNext));
    SDL_SetWindowSize(Window, 1280, 720);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);

    adaptiveui::ClearMenu();
    adaptiveui::PointerResult OpenHistory = adaptiveui::HandlePointer(
      MenuLayout.Log.x + MenuLayout.Log.w / 2,
      MenuLayout.Log.y + MenuLayout.Log.h / 2,
      true, 0, false, 1);
    assert(OpenHistory.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(OpenHistory.CommandCode == KEY_MOBILE_COMMAND_BASE + 34);

    adaptiveui::SetMenu("Message history", "", Options, 40, 39, 1, 2);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout HistoryLayout = adaptiveui::GetLayout();
    assert(HistoryLayout.Menu.x == HistoryLayout.MapPanel.x);
    assert(HistoryLayout.Menu.y == HistoryLayout.MapPanel.y);
    assert(HistoryLayout.Menu.w == HistoryLayout.MapPanel.w);
    assert(HistoryLayout.Menu.h == HistoryLayout.MapPanel.h);
    assert(!Overlaps(HistoryLayout.Menu, HistoryLayout.Log));
    assert(!Overlaps(HistoryLayout.Menu, HistoryLayout.Rail));
    adaptiveui::PointerResult CloseHistory = adaptiveui::HandlePointer(
      HistoryLayout.Log.x + HistoryLayout.Log.w / 2,
      HistoryLayout.Log.y + HistoryLayout.Log.h / 2,
      true, 0, false, 1);
    assert(CloseHistory.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(CloseHistory.CommandCode == KEY_ESC);

    adaptiveui::ClearMenu();
    adaptiveui::SetActions(0, 0, 0, 0);
    adaptiveui::SetLog("YOU HAVE MUCH TROUBLE USING THE BRONZE MACE.");
    adaptiveui::SetPrompt("CONTINUE ANYWAY? Y/N");
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetLayout().Log.h >= 78);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearPrompt();

    adaptiveui::SetPrompt("WHAT IS YOUR NAME?", "RAKTAS L'YEAI", false);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::IsTextEntryPromptActive());
    const adaptiveui::Layout PromptLayout = adaptiveui::GetLayout();
    adaptiveui::PointerResult FocusInput = adaptiveui::HandlePointer(
      PromptLayout.PromptInput.x + PromptLayout.PromptInput.w / 2,
      PromptLayout.PromptInput.y + PromptLayout.PromptInput.h / 2,
      true, 0, false, 1);
    assert(FocusInput.Type == adaptiveui::PointerResult::REDRAW);
    adaptiveui::PointerResult ContinuePrompt = adaptiveui::HandlePointer(
      PromptLayout.PromptContinue.x + PromptLayout.PromptContinue.w / 2,
      PromptLayout.PromptContinue.y + PromptLayout.PromptContinue.h / 2,
      true, 0, false, 1);
    assert(ContinuePrompt.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(ContinuePrompt.CommandCode == KEY_ENTER);
    adaptiveui::PointerResult CancelPrompt = adaptiveui::HandlePointer(
      PromptLayout.PromptCancel.x + PromptLayout.PromptCancel.w / 2,
      PromptLayout.PromptCancel.y + PromptLayout.PromptCancel.h / 2,
      true, 0, false, 1);
    assert(CancelPrompt.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(CancelPrompt.CommandCode == KEY_ESC);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearPrompt();
    assert(!adaptiveui::IsTextEntryPromptActive());

    adaptiveui::SetConfirmationPrompt(
      "Your quest is not yet completed! Really quit? [y/N]");
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetHudModel().PromptConfirmsChoice);
    assert(adaptiveui::GetHudModel().Prompt.find("[y/N]")
           == std::string::npos);
    const adaptiveui::Layout ConfirmationLayout = adaptiveui::GetLayout();
    adaptiveui::PointerResult ConfirmYes = adaptiveui::HandlePointer(
      ConfirmationLayout.PromptContinue.x
        + ConfirmationLayout.PromptContinue.w / 2,
      ConfirmationLayout.PromptContinue.y
        + ConfirmationLayout.PromptContinue.h / 2,
      true, 0, false, 1);
    assert(ConfirmYes.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(ConfirmYes.CommandCode == 'y');
    adaptiveui::PointerResult ConfirmNo = adaptiveui::HandlePointer(
      ConfirmationLayout.PromptCancel.x
        + ConfirmationLayout.PromptCancel.w / 2,
      ConfirmationLayout.PromptCancel.y
        + ConfirmationLayout.PromptCancel.h / 2,
      true, 0, false, 1);
    assert(ConfirmNo.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(ConfirmNo.CommandCode == 'n');
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearPrompt();

    adaptiveui::SetQuitPrompt(
      "Do you want to save your game before quitting?");
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetHudModel().PromptOffersQuitChoices);
    const adaptiveui::Layout QuitLayout = adaptiveui::GetLayout();
    adaptiveui::PointerResult QuitYes = adaptiveui::HandlePointer(
      QuitLayout.PromptContinue.x + QuitLayout.PromptContinue.w / 2,
      QuitLayout.PromptContinue.y + QuitLayout.PromptContinue.h / 2,
      true, 0, false, 1);
    assert(QuitYes.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(QuitYes.CommandCode == 'y');
    adaptiveui::PointerResult QuitNo = adaptiveui::HandlePointer(
      QuitLayout.PromptDecline.x + QuitLayout.PromptDecline.w / 2,
      QuitLayout.PromptDecline.y + QuitLayout.PromptDecline.h / 2,
      true, 0, false, 1);
    assert(QuitNo.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(QuitNo.CommandCode == 'n');
    adaptiveui::PointerResult QuitCancel = adaptiveui::HandlePointer(
      QuitLayout.PromptCancel.x + QuitLayout.PromptCancel.w / 2,
      QuitLayout.PromptCancel.y + QuitLayout.PromptCancel.h / 2,
      true, 0, false, 1);
    assert(QuitCancel.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(QuitCancel.CommandCode == KEY_ESC);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearPrompt();

    adaptiveui::SetKeyCapturePrompt(
      "Press a new key for wait a turn (currently '.').");
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetHudModel().PromptCapturesKey);
    const adaptiveui::Layout KeyCaptureLayout = adaptiveui::GetLayout();
    adaptiveui::PointerResult IgnoreCaptureContinue = adaptiveui::HandlePointer(
      KeyCaptureLayout.PromptContinue.x
        + KeyCaptureLayout.PromptContinue.w / 2,
      KeyCaptureLayout.PromptContinue.y
        + KeyCaptureLayout.PromptContinue.h / 2,
      true, 0, false, 1);
    assert(IgnoreCaptureContinue.Type == adaptiveui::PointerResult::CONSUMED);
    adaptiveui::PointerResult CancelKeyCapture = adaptiveui::HandlePointer(
      KeyCaptureLayout.PromptCancel.x + KeyCaptureLayout.PromptCancel.w / 2,
      KeyCaptureLayout.PromptCancel.y + KeyCaptureLayout.PromptCancel.h / 2,
      true, 0, false, 1);
    assert(CancelKeyCapture.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(CancelKeyCapture.CommandCode == KEY_ESC);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearPrompt();

    adaptiveui::SetKeyTransferPrompt(
      "The key 'k' is used by kick. Take it and leave kick unbound?");
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    assert(adaptiveui::GetHudModel().PromptConfirmsKeyTransfer);
    const adaptiveui::Layout KeyTransferLayout = adaptiveui::GetLayout();
    adaptiveui::PointerResult TakeKey = adaptiveui::HandlePointer(
      KeyTransferLayout.PromptContinue.x
        + KeyTransferLayout.PromptContinue.w / 2,
      KeyTransferLayout.PromptContinue.y
        + KeyTransferLayout.PromptContinue.h / 2,
      true, 0, false, 1);
    assert(TakeKey.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(TakeKey.CommandCode == KEY_ENTER);
    adaptiveui::PointerResult CancelKeyTransfer = adaptiveui::HandlePointer(
      KeyTransferLayout.PromptCancel.x
        + KeyTransferLayout.PromptCancel.w / 2,
      KeyTransferLayout.PromptCancel.y
        + KeyTransferLayout.PromptCancel.h / 2,
      true, 0, false, 1);
    assert(CancelKeyTransfer.Type == adaptiveui::PointerResult::COMMAND_KEY);
    assert(CancelKeyTransfer.CommandCode == KEY_ESC);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearPrompt();

    adaptiveui::ClearMenu();
    adaptiveui::SetActions(0, 0, 0, 0);
    adaptiveui::SetStats("", "", "", "");
    adaptiveui::SetLocationTime("", "");
    const char* ConfigurationOptions[] = {
      "Player's default name  -",
      "Autosave interval  100 turns",
      "Be nice to pets  yes"
    };
    const char* ConfigurationGroups[] = {
      "General Setup", "General Setup", "Gameplay Options"
    };
    adaptiveui::SetMenu(
      "Which setting do you wish to configure? (* requires restart)",
      "Setting Value", ConfigurationOptions, 3, 0, 1, 1);
    adaptiveui::SetMenuGroups(ConfigurationGroups, 3);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout ConfigurationLayout = adaptiveui::GetLayout();
    assert(ConfigurationLayout.Menu.x == ConfigurationLayout.Gutter);
    assert(ConfigurationLayout.Menu.y == ConfigurationLayout.Gutter);
    assert(ConfigurationLayout.Menu.w
           == ConfigurationLayout.OutputWidth - ConfigurationLayout.Gutter * 2);
    assert(ConfigurationLayout.MenuDetail.w > 0);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearMenu();

    std::vector<std::string> AutoPickupStrings;
    std::vector<const char*> AutoPickupOptions;
    std::vector<const char*> AutoPickupGroups;
    for(int Index = 0; Index < 22; ++Index)
      AutoPickupStrings.push_back(Index < 5 ? "USE PRESET"
        : (Index < 19 ? "(X) ITEM TYPE" : "SAVE CHANGES"));
    for(int Index = 0; Index < 22; ++Index)
    {
      AutoPickupOptions.push_back(AutoPickupStrings[Index].c_str());
      AutoPickupGroups.push_back(Index == 0 ? "AUTO PICKUP"
        : (Index < 5 ? "PRESETS"
          : (Index < 19 ? "ITEM TYPES" : "ACTIONS")));
    }
    adaptiveui::SetMenu("AUTO PICK UP ITEMS", "",
                        &AutoPickupOptions[0], 22, 0, 1, 1);
    adaptiveui::SetMenuGroups(&AutoPickupGroups[0], 22);
    adaptiveui::UpdateLayout(Renderer, 800, 600, false);
    const adaptiveui::Layout AutoPickupLayout = adaptiveui::GetLayout();
    assert(AutoPickupLayout.Menu.x == AutoPickupLayout.Gutter);
    assert(AutoPickupLayout.MenuDetail.w > 0);
    adaptiveui::PointerResult ScrollAutoPickup = adaptiveui::HandlePointer(
      AutoPickupLayout.Menu.x + 20, AutoPickupLayout.Menu.y + 120,
      false, -1, false, 0);
    assert(ScrollAutoPickup.Type == adaptiveui::PointerResult::REDRAW);
    assert(adaptiveui::GetHudModel().MenuScroll == 1);
    adaptiveui::DrawBackground(Renderer);
    adaptiveui::Draw(Renderer);
    adaptiveui::ClearMenu();

    SDL_DestroyTexture(GameTexture);
    SDL_DestroyRenderer(Renderer);
    SDL_DestroyWindow(Window);
    SDL_Quit();
  }
}

int main()
{
  CheckSize(960, 540);
  CheckSize(1280, 720);
  CheckSize(1440, 810);
  CheckSize(1600, 900);
  CheckSize(1920, 1080);
  CheckSize(3440, 1440);
  CheckSize(2560, 1440);
  CheckDynamicEquipmentPaging();
  assert(adaptiveui::CalculateLayout(1280, 720, 800, 600, true).Fullscreen);
  CheckFeeds();
  CheckPointerInput();
  return 0;
}
