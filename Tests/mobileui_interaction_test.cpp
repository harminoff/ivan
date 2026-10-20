#define SDL_MAIN_HANDLED
#define IVAN_UI_TEST_LABELS
#include <cassert>
#include <cstdlib>
#include <iostream>

extern "C" void* SDL_AndroidGetJNIEnv() { return 0; }
extern "C" void* SDL_AndroidGetActivity() { return 0; }
#include "../FeLib/Source/mobileui.cpp"

truth iosystem::IsOnMenu() { return false; }
bool iosystem::IsInUse() { return false; }
truth felist::isAnyFelistCurrentlyDrawn() { return State.MenuActive; }

#undef assert
#define assert(Condition) do { if(!(Condition)) { \
  std::cerr << "Assertion failed: " << #Condition << ':' << __LINE__ << '\n'; \
  std::exit(EXIT_FAILURE); } } while(false)

void Check(int Width, int Height, const char* Prefix, bool Baseline)
{
  mobileui::DeInit();
  SDL_Surface* Surface = SDL_CreateRGBSurfaceWithFormat(0, Width, Height,
    32, SDL_PIXELFORMAT_ARGB8888);
  assert(Surface);
  SDL_Renderer* Renderer = SDL_CreateSoftwareRenderer(Surface);
  assert(Renderer);
  mobileui::SetSafeInsets(0, 24, 0, 24, 0, 0, 1.75f);
  mobileui::SetStats("HP 29/37  MANA 10  GOLD 49",
    "ARM 10  LEG 10  DEX 11  AGI 10", "END 10  PER 10  INT 9  WIS 10",
    "WILL 10  CHA 10  HT 175  WT 95");
  const auto Draw = [&](const char* Name)
  {
    mobileui::UpdateLayout(Renderer, 800, 600);
    mobileui::DrawBackground(Renderer);
    mobileui::Draw(Renderer);
    SDL_RenderPresent(Renderer);
    if(Prefix)
    {
      const std::string Path = std::string(Prefix) + "-" + Name
        + (Width < Height ? "-portrait.bmp" : "-landscape.bmp");
      assert(SDL_SaveBMP(Surface, Path.c_str()) == 0);
    }
  };
  Draw("hud");
  mobileui::SetPrompt("Do you want to (t)ake something from or "
                     "(p)ut something in this container? [t,p]");
  const int Keys[] = { 't', 'p', 'T', 'P', KEY_ESC };
  mobileui::SetQuestionChoices(Keys, 5);
  Draw("chest");
  if(!Baseline)
  {
    assert(State.QuestionChoiceCount == 3);
    const int Expected[] = { 't', 'p', KEY_ESC };
    const char* Labels[] = { "TAKE", "PUT", "CANCEL" };
    for(int I = 0; I < 3; ++I)
    {
      char Label[32];
      KeyLabel(State.QuestionChoices[I], Label, sizeof(Label));
      assert(std::string(Label) == Labels[I]);
      const SDL_Rect Button = GridCell(State.Controls, 3, 3, I, 3);
      const auto Tap = mobileui::HandleFinger(
        float(Button.x + Button.w / 2) / Width,
        float(Button.y + Button.h / 2) / Height);
      assert(Tap.Kind == mobileui::touchresult::TOUCH_KEY);
      assert(Tap.KeyCode == Expected[I]);
    }
  }
  mobileui::SetQuestionChoices(0, 0);
  mobileui::ClearPrompt();
  mobileui::SetLog("\"Ah! That iron short sword costs 23 gold pieces. No haggling, please.\"");
  mobileui::SetConfirmationPrompt(Baseline ? "Do you accept this deal? [y/N]"
    : "Buy an iron short sword for 23 gold pieces total? [y/N]");
  const int YesNo[] = { 'y', 'n' };
  mobileui::SetQuestionChoices(YesNo, 2);
  Draw("merchant");
  if(!Baseline)
  {
    assert(State.PromptText.find("23 gold pieces total") != std::string::npos);
    assert(!GameplayPromptShowsLogContext());
    for(int I = 0; I < 2; ++I)
    {
      const SDL_Rect Button = BinaryConfirmationButton(I);
      const auto Tap = mobileui::HandleFinger(
        float(Button.x + Button.w / 2) / Width,
        float(Button.y + Button.h / 2) / Height);
      assert(Tap.Kind == mobileui::touchresult::TOUCH_KEY && Tap.KeyCode == YesNo[I]);
    }
  }
  mobileui::ClearPrompt();
  mobileui::SetQuestionChoices(0, 0);
  mobileui::SetPrompt("Case-sensitive choices");
  mobileui::SetQuestionChoices(Keys, 5);
  assert(State.QuestionChoiceCount == 5);
  mobileui::ClearPrompt();
  mobileui::DeInit();
  SDL_DestroyRenderer(Renderer);
  SDL_FreeSurface(Surface);
}

void CheckShopBrowser(int Width, int Height, const char* Prefix)
{
  mobileui::DeInit();
  SDL_Surface* Surface = SDL_CreateRGBSurfaceWithFormat(0, Width, Height,
    32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* Renderer = SDL_CreateSoftwareRenderer(Surface);
  assert(Surface && Renderer);
  mobileui::SetSafeInsets(0, 24, 0, 24, 0, 0, 1.75f);
  const char* Options[] = { "an iron short sword [1200g]",
    "a leather armor [2500g]", "3 scrolls [30g]", "a dropped sword [1200g]" };
  const char* Details[] = { "An iron short sword.", "A suit of leather armor.",
    "A stack of three scrolls.", "Your own dropped sword." };
  const SDL_Rect Icons[4] = {};
  adaptiveui::ItemMetrics Metrics[4];
  for(int I = 0; I < 4; ++I) Metrics[I].Present = true;
  Metrics[0].PurchasePrice = 23; Metrics[0].Weight = 1200;
  Metrics[0].Equippable = true;
  Metrics[1].PurchasePrice = 67; Metrics[1].Weight = 2500;
  Metrics[2].PurchasePrice = 12; Metrics[2].Weight = 30;
  Metrics[2].PurchaseCount = 3;
  Metrics[3].Weight = 1200; Metrics[3].Equippable = true;
  const auto Draw = [&](int Selected, int Count)
  {
    mobileui::SetMenu("What do you want to pick up?", "Your gold: 49", Options, Count, Selected, 1, 1);
    adaptiveui::SetMenuPresentation(Details, Icons, Count, adaptiveui::MENU_PICKUP_GRID);
    adaptiveui::SetMenuItemMetrics(Metrics, Count);
    mobileui::UpdateLayout(Renderer, 800, 600);
    mobileui::DrawBackground(Renderer);
    mobileui::DrawGame(Renderer, 0);
    mobileui::Draw(Renderer);
    SDL_RenderPresent(Renderer);
  };
  const auto HasLabel = [&](const char* Value)
  {
    for(const auto& Label : DrawnLabels)
      if(Label.Text == Value) return true;
    return false;
  };
  Draw(0, 3);
  assert(CurrentHeaderTitle() == "What do you want to buy?");
  assert(HasLabel("23 GOLD") && HasLabel("67 GOLD") && HasLabel("12 GOLD"));
  assert(HasLabel("PRICE: 23 GOLD | HAVE: 49") && HasLabel("BUY") && !HasLabel("STASH"));
  assert(!HasLabel("Your gold: 49"));
  assert(State.MenuItemActionCount == 0); // Pay before using merchant goods.
  if(Prefix)
  {
    const std::string Name = std::string(Prefix) + "-shop-browser"
      + (Width < Height ? "-portrait.bmp" : "-landscape.bmp");
    assert(SDL_SaveBMP(Surface, Name.c_str()) == 0);
  }
  const auto Buy = mobileui::HandleFinger(
    float(State.MenuConfirm.x + State.MenuConfirm.w / 2) / Width,
    float(State.MenuConfirm.y + State.MenuConfirm.h / 2) / Height);
  assert(Buy.Kind == mobileui::touchresult::TOUCH_KEY
    && Buy.KeyCode == KEY_MOBILE_MENU_SELECT_BASE);
  Draw(2, 3);
  assert(HasLabel("PRICE: 12 GOLD TOTAL / 3 ITEMS | HAVE: 49"));
  Draw(3, 4);
  assert(CurrentHeaderTitle() == "What do you want to pick up?");
  assert(HasLabel("STASH") && !HasLabel("BUY") && HasLabel("EQUIP"));
  // Free and large offers fit without hiding the price on narrow screens.
  Metrics[0].PurchasePrice = 0;
  Metrics[1].PurchasePrice = 1234567890;
  Draw(0, 3);
  assert(HasLabel("0 GOLD")
    && (HasLabel("1234567890 GOLD") || HasLabel("1234567890")));
  for(const auto& Label : DrawnLabels)
    if(Label.Text == "1234567890 GOLD" || Label.Text == "1234567890")
      assert(Label.Rect.x >= State.MenuRows[1].x
        && Label.Rect.x + Label.Rect.w <= State.MenuRows[1].x + State.MenuRows[1].w);
  mobileui::ClearMenu();
  mobileui::DeInit();
  SDL_DestroyRenderer(Renderer);
  SDL_FreeSurface(Surface);
}


void CheckAuditScreens(int Width, int Height, const char* Prefix)
{
  mobileui::DeInit();
  SDL_Surface* Surface = SDL_CreateRGBSurfaceWithFormat(0, Width, Height, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* Renderer = SDL_CreateSoftwareRenderer(Surface);
  assert(Surface && Renderer);
  mobileui::SetSafeInsets(0, 24, 0, 24, 0, 0, 1.75f);
  const auto Draw = [&](const char* Name) {
    mobileui::UpdateLayout(Renderer, 800, 600);
    mobileui::DrawBackground(Renderer);
    mobileui::DrawGame(Renderer, 0);
    mobileui::Draw(Renderer);
    SDL_RenderPresent(Renderer);
    if(Prefix) {
      std::string Path = std::string(Prefix) + "-" + Name
        + (Width < Height ? "-portrait.bmp" : "-landscape.bmp");
      assert(SDL_SaveBMP(Surface, Path.c_str()) == 0);
    }
  };
  const auto Named = [&](const char* Name, const char* Prompt, const int* Keys,
                         const char* const* Labels, int Count) {
    mobileui::SetPrompt(Prompt);
    mobileui::SetQuestionChoices(Keys, Count, Labels);
    Draw(Name);
    assert(State.QuestionChoiceCount == Count);
    for(int I = 0; I < Count; ++I) {
      char Label[96]; KeyLabel(Keys[I], Label, sizeof(Label));
      assert(std::string(Label) == Labels[I]);
      SDL_Rect Button = GridCell(State.Controls, 3, 3, I, 3);
      auto Tap = mobileui::HandleFinger(float(Button.x + Button.w / 2) / Width,
        float(Button.y + Button.h / 2) / Height);
      assert(Tap.Kind == mobileui::touchresult::TOUCH_KEY && Tap.KeyCode == Keys[I]);
    }
    mobileui::SetQuestionChoices(0, 0); mobileui::ClearPrompt();
  };
  const int Sides[] = {'r','l',KEY_ESC};
  const char* SideLabels[] = {"RIGHT","LEFT","CANCEL"};
  Named("equipment", "Equip on which side? [r] Right, [l] Left, [Esc] Cancel.", Sides, SideLabels, 3);
  const int Craft[] = {'r','c',KEY_ESC};
  const char* CraftLabels[] = {"RESUME","DISCARD","BACK"};
  Named("crafting", "Suspended crafting: [r] Resume, [c] Discard, [Esc] Back. Discarding permanently loses the selected crafting action.", Craft, CraftLabels, 3);
  const int Golem[] = {'m','s',KEY_ESC};
  const char* GolemLabels[] = {"USE MAIN","USE OTHER","BACK"};
  Named("golem", "The selected item will be consumed. Choose material: [m] Main: iron (1200 cm3); [s] Other: wood (400 cm3); [Esc] Back.", Golem, GolemLabels, 3);
  const int Limbs[] = {'l','r','L','R','h',KEY_ESC};
  const char* LimbLabels[] = {"LEFT ARM","RIGHT ARM","LEFT LEG","RIGHT LEG","HEAD","CANCEL"};
  Named("wizard-limbs", "Detach which body part?", Limbs, LimbLabels, 6);
  const auto Confirm = [&](const char* Name, const char* Prompt, const char* Accept, const char* Reject) {
    mobileui::SetConfirmationPrompt(Prompt, Accept, Reject);
    const int Keys[] = {'y','n'}; mobileui::SetQuestionChoices(Keys, 2);
    Draw(Name);
    assert(adaptiveui::GetHudModel().ConfirmAction == Accept);
    assert(adaptiveui::GetHudModel().CancelAction == Reject);
    mobileui::SetQuestionChoices(0, 0); mobileui::ClearPrompt();
  };
  Confirm("priest", "Reattach your left arm for 75 gold? The limb returns with 1 HP. [y/N]", "REATTACH", "CANCEL");
  Confirm("companion", "Buy the slave for 50 gold? They will join your team. [y/N]", "BUY", "CANCEL");
  Confirm("abandonment", "These companions cannot cross this terrain. Abandoning them permanently removes them from the game:\nYour dog\nYour golem\nAbandon them? [y/N]", "ABANDON", "STAY");
  Confirm("ownership", "Eating the property of the Cathedral is forbidden.\n\nProceed with this action? [y/N]", "EAT", "CANCEL");
  const char* Options[] = {"an iron short sword [1200g]", "a leather armor [2500g]", "3 scrolls [30g]"};
  const char* Details[] = {"An iron short sword.", "A suit of leather armor.", "Three scrolls."};
  const char* RepairOptions[] = {"a rusty iron sword [1200g]", "a broken iron helmet [850g]", "a rusty iron axe [1800g]"};
  const char* RepairDetails[] = {"A rusty iron sword.", "A broken iron helmet.", "A rusty iron axe."};
  const SDL_Rect Icons[3] = {};
  adaptiveui::ItemMetrics Metrics[3];
  for(int I=0;I<3;++I) { Metrics[I].Present=true; Metrics[I].PurchasePrice=12+I*20; Metrics[I].Weight=1200; }
  for(int Mode=0;Mode<3;++Mode) {
    for(auto& M:Metrics) M.PriceAction=Mode==0?"BUY":Mode==1?"SELL":"REPAIR";
    mobileui::SetMenu("Items", "Your gold: 49", Mode==2?RepairOptions:Options, 3, 0, 1, 1);
    adaptiveui::SetMenuPresentation(Mode==2?RepairDetails:Details, Icons, 3, adaptiveui::MENU_ITEM_GRID);
    adaptiveui::SetMenuItemMetrics(Metrics, 3);
    Draw(Mode==0?"shop-container":Mode==1?"selling":"repairs");
    bool Found=false; for(auto& Label:DrawnLabels) if(Label.Text==Metrics[0].PriceAction) Found=true;
    assert(Found);
    auto Tap=mobileui::HandleFinger(float(State.MenuConfirm.x+State.MenuConfirm.w/2)/Width,
      float(State.MenuConfirm.y+State.MenuConfirm.h/2)/Height);
    assert(Tap.Kind==mobileui::touchresult::TOUCH_KEY && Tap.KeyCode==KEY_MOBILE_MENU_SELECT_BASE);
  }
  const char* UnavailableOptions[] = {"a burning leather armor [2500g]", "a torn silk shirt [100g]", "a torn leather cloak [800g]"};
  const char* UnavailableDetails[] = {"A burning suit of leather armor.", "A torn silk shirt.", "A torn leather cloak."};
  mobileui::SetMenu("Tailoring", "Your gold: 49", UnavailableOptions, 3, 0, 1, 1);
  adaptiveui::SetMenuPresentation(UnavailableDetails, Icons, 3, adaptiveui::MENU_ITEM_GRID);
  Metrics[0].PurchasePrice=-1; Metrics[0].PriceNote="Extinguish this item before repair.";
  adaptiveui::SetMenuItemMetrics(Metrics, 3);
  Draw("repair-unavailable");
  auto Tap=mobileui::HandleFinger(float(State.MenuConfirm.x+State.MenuConfirm.w/2)/Width,
    float(State.MenuConfirm.y+State.MenuConfirm.h/2)/Height);
  assert(Tap.Kind!=mobileui::touchresult::TOUCH_KEY);
  mobileui::ClearMenu(); mobileui::DeInit(); SDL_DestroyRenderer(Renderer); SDL_FreeSurface(Surface);
}

int main(int Argc, char** Argv)
{
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  const bool Baseline = Argc > 2 && std::string(Argv[2]) == "--baseline";
  Check(720, 1600, Argc > 1 ? Argv[1] : 0, Baseline);
  Check(1600, 720, Argc > 1 ? Argv[1] : 0, Baseline);
  Check(480, 800, 0, Baseline);
  if(!Baseline)
  {
    CheckShopBrowser(720, 1600, Argc > 1 ? Argv[1] : 0);
    CheckShopBrowser(1600, 720, Argc > 1 ? Argv[1] : 0);
    CheckShopBrowser(480, 800, 0);
    CheckAuditScreens(720, 1600, Argc > 1 ? Argv[1] : 0);
    CheckAuditScreens(1600, 720, Argc > 1 ? Argv[1] : 0);
    CheckAuditScreens(480, 800, 0);
  }
  std::cout << "Chest and trade renderer/input checks passed.\n";
  SDL_Quit();
}
