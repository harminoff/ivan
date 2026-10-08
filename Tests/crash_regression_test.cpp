#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "char.h"
#include "adaptiveui.h"
#include "bitmap.h"
#include "command.h"
#include "config.h"
#include "feio.h"
#include "felist.h"
#include "graphics.h"
#include "game.h"
#include "god.h"
#include "iconf.h"
#include "message.h"
#include "whandler.h"

struct EquipmentCrashTestAccess
{
  static truth EquipItemInSlot(character* Char, item* Item, int Slot)
  { return commandsystem::EquipItemInSlot(Char, Item, Slot); }
  static truth EquipPickedItem(character* Char, item* Item)
  { return commandsystem::EquipPickedItem(Char, Item); }
};

namespace
{
  void Check(bool Result, const char* Message)
  {
    if(!Result)
      throw std::runtime_error(Message);
  }

  class PrayerInfoTestGod : public god
  {
   public:
    virtual cchar* GetName() const { return "Test deity"; }
    virtual cchar* GetDescription() const { return "god of test descriptions"; }
    virtual int GetAlignment() const { return 0; }
    virtual col16 GetColor() const { return 0; }
    virtual col16 GetEliteColor() const { return 0; }
    virtual const prototype* GetProtoType() const { return 0; }
    virtual int GetSex() const { return MALE; }
    void RememberResponse(const char* Response) { fsLastKnownRelation = Response; }
   protected:
    virtual void PrayGoodEffect() { }
    virtual void PrayBadEffect() { }
  };

  void CheckPrayerInformation()
  {
    PrayerInfoTestGod God;
    God.SetRelation(-999);
    God.RememberResponse("Test deity was pleased.");
    const std::string Basic = God.GetPrayerInfo(false).CStr();
    const std::string Extra = God.GetPrayerInfo(true).CStr();
    Check(Basic.find("god of test descriptions") != std::string::npos
          && Basic.find("never prayed") != std::string::npos,
          "Prayer information lost the description or prayer history");
    Check(Basic.find("was pleased") == std::string::npos
          && Extra.find("Last known response:\nTest deity was pleased.")
             != std::string::npos,
          "ShowGodInfo did not control the remembered prayer response");
    Check(Basic.find("\n\nAlignment: ") != std::string::npos
          && Basic.find("\n\nPrayer history:\nYou have ") != std::string::npos
          && Basic.find("Test deity", Basic.find("Test deity") + 1) == std::string::npos,
          "God information did not use readable sections without a repeated table row");
    Check(Extra.find("-999") == std::string::npos && God.GetRelation() == -999,
          "Previewing prayer information exposed or changed hidden relation");
    God.RememberResponse("");
    Check(std::string(God.GetPrayerInfo(true).CStr()).find("No response remembered yet.")
          != std::string::npos, "Unknown prayer response was not explained");
  }

  class TestCharacter : public character
  {
   public:
    TestCharacter() : Database(), SetCalls(0)
    {
      Database.CanUseEquipment = true;
      DataBase = &Database;
      BodyParts = 0;
      BodyPartSlot = 0;
      OriginalBodyPartID = 0;
      CWeaponSkill = 0;
      ID = game::CreateNewCharacterID(this);
      for(int I = 0; I < MAX_EQUIPMENT_SLOTS; ++I)
      {
        Present[I] = true;
        Allowed[I] = true;
        Equipment[I] = 0;
      }
    }

    virtual bodypart* GetBodyPartOfEquipment(int I) const
    {
      // Only presence is queried: the equipment setters are mocked below.
      return Present[I] ? reinterpret_cast<bodypart*>(
        const_cast<TestCharacter*>(this)) : 0;
    }
    virtual truth EquipmentIsAllowed(int I) const { return Allowed[I]; }
    virtual int GetEquipments() const { return MAX_EQUIPMENT_SLOTS; }
    virtual item* GetEquipment(int I) const { return Equipment[I]; }
    virtual void SetEquipment(int I, item* What)
    {
      Check(Present[I] && Allowed[I], "Equipment setter reached an unavailable slot");
      ++SetCalls;
      Equipment[I] = What;
    }
    virtual truth Hit(character*, v2, int, int) { return false; }
    virtual void Kick(lsquare*, int, truth) { }
    virtual int DrawStats(truth) const { return 0; }
    virtual int GetCarryingStrength() const { return 1; }
    virtual void CalculateBattleInfo() { }
    virtual double GetTimeToKill(ccharacter*, truth) const { return 0; }
    virtual truth UseMaterialAttributes() const { return false; }
    virtual void AddSpecialStethoscopeInfo(felist&) const { }
    virtual void AddAttackInfo(felist&) const { }

    database Database;
    bool Present[MAX_EQUIPMENT_SLOTS];
    bool Allowed[MAX_EQUIPMENT_SLOTS];
    item* Equipment[MAX_EQUIPMENT_SLOTS];
    int SetCalls;
  };

  class TestItem : public item
  {
   public:
    enum kind { Weapon, Ring, Gauntlet, Boot, Helmet };
    explicit TestItem(kind Kind = Weapon)
      : Database(), Kind(Kind), RemoveCalls(0), Equippable(true), Removable(true)
    {
      DataBase = &Database;
      ID = 0;
      SquaresUnder = 1;
      Slot = new slot*[1]();
      Size = 1;
      Volume = Weight = 1;
    }
    virtual truth IsWeapon(ccharacter*) const { return Kind == Weapon; }
    virtual truth IsRing(ccharacter*) const { return Kind == Ring; }
    virtual truth IsGauntlet(ccharacter*) const { return Kind == Gauntlet; }
    virtual truth IsBoot(ccharacter*) const { return Kind == Boot; }
    virtual truth IsHelmet(ccharacter*) const { return Kind == Helmet; }
    virtual void RemoveFromSlot() { ++RemoveCalls; }
    virtual truth CanBeEquipped(int) const { return Equippable; }
    virtual truth CanBeUnEquipped(int) const { return Removable; }
    virtual void AddName(festring& Name, int) const { Name << "test item"; }

    database Database;
    kind Kind;
    int RemoveCalls;
    bool Equippable, Removable;
  };

  void CheckEquipment()
  {
    msgsystem::DisableMessages();
    TestCharacter Char;
    TestItem Incoming, Old;
    Char.Present[RIGHT_WIELDED_INDEX] = false;
    Char.Equipment[RIGHT_WIELDED_INDEX] = &Old;
    Check(!EquipmentCrashTestAccess::EquipItemInSlot(&Char, &Incoming, RIGHT_WIELDED_INDEX),
          "Inventory quick-equip accepted a missing arm");
    Check(Incoming.RemoveCalls == 0 && Old.RemoveCalls == 0 && Char.SetCalls == 0,
          "Failed quick-equip changed items");
    Check(!EquipmentCrashTestAccess::EquipItemInSlot(&Char, &Incoming, -1)
          && !EquipmentCrashTestAccess::EquipItemInSlot(&Char, &Incoming, MAX_EQUIPMENT_SLOTS),
          "Quick-equip accepted an invalid index");
    Char.Allowed[LEFT_WIELDED_INDEX] = false;
    Check(!EquipmentCrashTestAccess::EquipItemInSlot(&Char, &Incoming, LEFT_WIELDED_INDEX)
          && Incoming.RemoveCalls == 0, "Quick-equip accepted a forbidden slot");

    const TestItem::kind Kinds[] =
      { TestItem::Weapon, TestItem::Ring, TestItem::Gauntlet, TestItem::Boot };
    const int Right[] = { RIGHT_WIELDED_INDEX, RIGHT_RING_INDEX,
                         RIGHT_GAUNTLET_INDEX, RIGHT_BOOT_INDEX };
    const int Left[] = { LEFT_WIELDED_INDEX, LEFT_RING_INDEX,
                        LEFT_GAUNTLET_INDEX, LEFT_BOOT_INDEX };
    for(int I = 0; I < 4; ++I)
      for(int Missing = 0; Missing < 3; ++Missing)
      {
        TestCharacter Player;
        TestItem Picked(Kinds[I]);
        Player.Present[Right[I]] = Missing == 1;
        Player.Present[Left[I]] = Missing == 0;
        const bool Expected = Missing != 2;
        Check(EquipmentCrashTestAccess::EquipPickedItem(&Player, &Picked) == Expected,
              "Pickup quick-equip did not handle missing limbs");
        Check(Picked.RemoveCalls == (Expected ? 1 : 0)
              && Player.SetCalls == (Expected ? 1 : 0),
              "Pickup quick-equip lost or duplicated an item");
        if(Expected)
          Check(Player.GetEquipment(Missing == 0 ? Left[I] : Right[I]) == &Picked,
                "Pickup quick-equip selected the missing limb");
      }

    TestCharacter Intact;
    TestItem Picked, Helmet(TestItem::Helmet);
    Check(EquipmentCrashTestAccess::EquipPickedItem(&Intact, &Picked)
          && Intact.GetEquipment(RIGHT_WIELDED_INDEX) == &Picked,
          "Normal quick-equip no longer prefers the empty right slot");
    Intact.Present[HELMET_INDEX] = false;
    Check(!EquipmentCrashTestAccess::EquipPickedItem(&Intact, &Helmet) && Helmet.RemoveCalls == 0,
          "Quick-equip accepted a helmet without a head");

    TestCharacter Restricted;
    TestItem Rejected, Existing;
    Restricted.Equipment[RIGHT_WIELDED_INDEX] = &Existing;
    Rejected.Equippable = false;
    Check(!EquipmentCrashTestAccess::EquipItemInSlot(&Restricted, &Rejected, RIGHT_WIELDED_INDEX)
          && Existing.RemoveCalls == 0 && Rejected.RemoveCalls == 0,
          "Rejected equipment displaced the current item");
    Rejected.Equippable = true;
    Existing.Removable = false;
    Check(!EquipmentCrashTestAccess::EquipItemInSlot(&Restricted, &Rejected, RIGHT_WIELDED_INDEX)
          && Rejected.RemoveCalls == 0 && Restricted.SetCalls == 0,
          "Quick-equip bypassed cursed equipment");
  }

  std::string SavedConfiguration()
  {
    Check(configsystem::Save(), "Could not save test configuration");
    std::ifstream File("crash-regression.cfg");
    Check(bool(File), "Could not read saved test configuration");
    return std::string(std::istreambuf_iterator<char>(File),
                       std::istreambuf_iterator<char>());
  }

  void CheckRepeatedInitialization()
  {
    numberoption Option("CrashTestOption", "Crash test", "", 17);
    configsystem::SetConfigFileName(CONST_S("crash-regression.cfg"));
    configsystem::ResetOptions();
    configsystem::AddOption(CONST_S("Test"), &Option);
    Option.ChangeValue(42);
    const std::string First = SavedConfiguration();
    Check(!First.empty(), "Test configuration was empty");
    for(int Pass = 0; Pass < 3; ++Pass)
    {
      configsystem::ResetOptions();
      configsystem::AddOption(CONST_S("Test"), &Option);
      Check(Option.Value == 42, "Rebuilding registration reset the option value");
      Check(SavedConfiguration() == First,
            "Repeated configuration initialization changed or duplicated options");
    }
    int Region = -1;
    for(int Pass = 0; Pass < 3; ++Pass)
    {
      graphics::Init();
      SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
      graphics::SetMode("Crash regression", 0, v2(800, 600), v2(960, 540),
                        1, 0, false, graphics::PRESENTATION_CLASSIC);
      Check(graphics::GetWindow() && DOUBLE_BUFFER, "Graphics did not initialize");
      DOUBLE_BUFFER->ClearToColor(0);
      if(Region == -1)
      {
        blitdata Data = DEFAULT_BLITDATA;
        Data.Dest = v2(8, 8);
        Data.Border = v2(16, 16);
        Data.Stretch = 2;
        Region = graphics::AddStretchRegion(Data, "Crash reentry test");
      }
      graphics::SetAllowStretchedBlit();
      Check(graphics::PrepareBuffer() != DOUBLE_BUFFER,
            "Cached scaled region was not rebound after graphics reentry");
      SDL_Renderer* Renderer = SDL_GetRenderer(graphics::GetWindow());
      adaptiveui::SetStats("Test player", "", "", "");
      adaptiveui::UpdateLayout(Renderer, 800, 600);
      SDL_Texture* TestTexture = SDL_CreateTexture(Renderer,
        SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 800, 600);
      Check(TestTexture != 0, "Could not create gameplay texture");
      adaptiveui::DrawGame(Renderer, TestTexture);
      SDL_DestroyTexture(TestTexture);
      globalwindowhandler::AddKeyToBuffer('q');
      graphics::DeInit();
      graphics::DeInit();
      Check(!graphics::GetWindow() && !DOUBLE_BUFFER && !FONT,
            "Graphics cleanup retained stale pointers");
      Check(adaptiveui::GetHudModel().Stats[0].empty(),
            "Graphics cleanup retained a previous session's HUD");
      Check(!globalwindowhandler::HasKeysOnBuffer(),
            "Graphics cleanup retained a previous session's input");
      Check(!iosystem::IsOnMenu() && !iosystem::IsInUse()
            && !felist::isAnyFelistCurrentlyDrawn(),
            "Graphics cleanup retained an interrupted native menu");
    }
  }
}

int RunCrashRegressionTests()
{
  try
  {
    std::cout << "Testing equipment mutations and missing limbs..." << std::endl;
    CheckEquipment();
    CheckPrayerInformation();
    game::DeInit(); // OS shutdown may arrive before a game was started.
    game::DeInit(); // Cleanup must also be safe after a completed session.
    Check(!game::IsRunning() && !game::GetPlayer() && !game::IsQuestionMode()
          && !game::IsInGetCommand(), "Session cleanup retained interaction state");
    std::cout << "Testing config reentry and graphics cleanup..." << std::endl;
    CheckRepeatedInitialization();
    std::cout << "Crash regression tests passed: missing limbs, equipment restrictions, "
                 "configuration reentry and repeated graphics cleanup." << std::endl;
    return 0;
  }
  catch(const std::exception& Error)
  {
    std::cerr << "Crash regression test failed: " << Error.what() << std::endl;
    graphics::DeInit();
    return 1;
  }
}
