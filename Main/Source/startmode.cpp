/*
 * Shared new-game character starts for IVAN.
 */

#include <algorithm>

#include "startmode.h"

#include "char.h"
#include "confdef.h"
#include "felist.h"
#include "game.h"
#include "gear.h"
#include "iconf.h"
#include "materias.h"
#include "miscitem.h"
#include "nonhuman.h"
#include "save.h"
#include "stack.h"
#include "team.h"
#include "worldmap.h"

namespace
{
const int START_ATTRIBUTE_TOTAL = 110;
const int START_ATTRIBUTE_MIN = 7;
const int START_ATTRIBUTE_MAX = 13;
const int CUSTOM_ITEM_POINTS = 6;
const ulong STARTMODE_SAVE_MAGIC_V1 = 0x53545254UL; /* STRT, draft indices */
const ulong STARTMODE_SAVE_MAGIC_V2 = 0x53545232UL; /* STR2, stable IDs */
const ulong STARTMODE_SAVE_MAGIC = 0x53545233UL; /* STR3, custom item pool */
const ulong ORIGIN_STATS_MAGIC = 0x4F535431UL; /* OST1 */
const char* const ORIGIN_STATS_FILENAME = "OriginStats.dat";

const char* const AttributeNames[ATTRIBUTES] =
{
  "Endurance", "Perception", "Intelligence", "Wisdom", "Willpower",
  "Charisma", "Mana", "Arm strength", "Leg strength", "Dexterity",
  "Agility"
};

struct CustomStarterOption
{
  const char* Name;
  const char* Description;
  int Cost;
};

const CustomStarterOption CustomStarterOptions[] =
{
  { "Two bananas", "A small food reserve for the road to Attnam.", 1 },
  { "30 gold", "A little flexibility for early shops and services.", 2 },
  { "Mundane belt", "A simple wearable belt.", 1 },
  { "Lantern", "Portable light for dark places.", 1 },
  { "Whistle", "A light utility item used with animals.", 1 },
  { "Ordinary book", "Reading material with possible practical value.", 2 },
  { "Pickaxe", "A mining tool that can also serve as a weapon.", 2 },
  { "Hammer", "A blunt weapon and useful workshop tool.", 2 },
  { "Bronze dagger", "A compact, accurate starting weapon.", 2 },
  { "Balsa spear", "A light pole arm with extra reach.", 2 },
  { "Balsa shield", "A modest defensive option for either hand.", 2 }
};

const StarterKit Kits[] =
{
  { "porter", "Plantation Porter's kit",
    "Pickaxe, mundane belt, two bananas, 30 gold, and modest tool training.",
    "Pickaxe, mundane belt, two bananas", "Modest axe and pickaxe training",
    30, AXES, 50, -1, 0, false },
  { "scout", "Canopy Scout's kit",
    "Bronze dagger, lantern, one banana, 35 gold, and small-sword training.",
    "Bronze dagger, lantern, one banana", "Modest small-sword and dagger training",
    35, SMALL_SWORDS, 100, -1, 0, false },
  { "scribe", "Viceroy's Scribe's kit",
    "Ordinary book, lantern, 60 gold, and modest crafting familiarity.",
    "Ordinary book, lantern", "Modest crafting familiarity",
    60, CRAFTING, 50, -1, 0, false },
  { "guard", "Banana Guard's kit",
    "Balsa spear, low-tier shield, 35 gold, and pole-arm and shield training.",
    "Balsa spear, low-tier shield", "Modest pole-arm and shield training",
    35, POLE_ARMS, 75, SHIELDS, 75, false },
  { "workshop", "Workshop Hand's kit",
    "Hammer, pickaxe, 30 gold, and blunt-weapon and crafting training.",
    "Hammer, pickaxe", "Modest blunt-weapon and crafting training",
    30, BLUNT_WEAPONS, 50, CRAFTING, 100, false },
  { "kennel", "Kennel Keeper's kit",
    "Whistle, two bananas, 40 gold, and a dog even when pets are disabled.",
    "Whistle, two bananas", "No weapon training",
    40, -1, 0, -1, 0, true }
};

const StartDefinition Definitions[] =
{
  { "plantation_porter", "Plantation Porter",
    "A powerful carrier shaped by endless days hauling the colony's banana crop.",
    "Before the viceroy summoned you, the plantation knew you as the worker who could carry one more load when everyone else had stopped.",
    START_STANDARD, { 11, 10, 9, 9, 10, 10, 9, 12, 12, 9, 9 }, 0, false },
  { "canopy_scout", "Canopy Scout",
    "A quick-eyed climber accustomed to dangerous branches and stolen fruit.",
    "You spent your days above the plantation floor, watching the canopy for monkeys, predators, and fruit worth the climb.",
    START_STANDARD, { 9, 12, 10, 10, 10, 9, 9, 8, 9, 12, 12 }, 1, false },
  { "viceroys_scribe", "Viceroy's Scribe",
    "A literate colonial assistant with a sharper mind than sword arm.",
    "Unlike most workers, you had seen the inside of the viceroy's offices before, recording banana tallies and carefully ignoring their inconsistencies.",
    START_STANDARD, { 9, 10, 12, 12, 11, 10, 11, 8, 8, 10, 9 }, 2, false },
  { "banana_guard", "Banana Guard",
    "A stubborn plantation defender trained to keep beasts and thieves away.",
    "You guarded the colony's banana stores with more courage than equipment, which is still more courage than most guards could claim.",
    START_STANDARD, { 12, 10, 8, 9, 11, 9, 9, 12, 11, 9, 10 }, 3, false },
  { "workshop_hand", "Workshop Hand",
    "A practical builder who knows tools, materials, and improvised weapons.",
    "The plantation workshop taught you that almost anything can be repaired, dismantled, or turned into a tool if struck at the correct angle.",
    START_STANDARD, { 11, 11, 11, 9, 9, 9, 9, 11, 9, 12, 9 }, 4, false },
  { "kennel_keeper", "Kennel Keeper",
    "A perceptive animal handler accompanied by a loyal colony dog.",
    "You cared for the colony dogs and learned that a loyal animal is often wiser company than a colonial official.",
    START_STANDARD, { 10, 11, 9, 12, 11, 12, 9, 8, 9, 9, 10 }, 5, false },
  { "barefoot_courier", "Barefoot Courier",
    "Challenge: exceptionally quick and observant, but frail, poor, and completely unequipped.",
    "The viceroy chose you because you were quick, available, and already accustomed to traveling without shoes, provisions, or pay.",
    START_DIFFICULT, { 8, 11, 8, 8, 8, 8, 8, 8, 8, 11, 12 }, -1, true },
  { "overworked_picker", "Overworked Picker",
    "Challenge: strong from plantation labor, but deficient everywhere else.",
    "Years of picking and hauling bananas left you strong enough for the journey and too exhausted to object to it.",
    START_DIFFICULT, { 11, 8, 8, 8, 8, 8, 8, 11, 11, 8, 8 }, -2, true }
};

const char* ChallengeKitId(int OriginIndex)
{
  return OriginIndex == 6 ? "barefoot_challenge"
       : OriginIndex == 7 ? "picker_challenge" : "";
}

int FindDefinition(cfestring& Id)
{
  for(int c = 0; c < int(sizeof(Definitions) / sizeof(Definitions[0])); ++c)
    if(Id == Definitions[c].Id)
      return c;
  return -1;
}

int FindKit(cfestring& Id)
{
  for(int c = 0; c < int(sizeof(Kits) / sizeof(Kits[0])); ++c)
    if(Id == Kits[c].Id)
      return c;
  return -1;
}

unsigned long CustomMaskForLegacyKit(int KitIndex)
{
  switch(KitIndex)
  {
   case 0: return (1UL << 0) | (1UL << 1) | (1UL << 2) | (1UL << 6);
   case 1: return (1UL << 0) | (1UL << 1) | (1UL << 3) | (1UL << 8);
   case 2: return (1UL << 1) | (1UL << 3) | (1UL << 5);
   case 3: return (1UL << 1) | (1UL << 9) | (1UL << 10);
   case 4: return (1UL << 1) | (1UL << 6) | (1UL << 7);
   case 5: return (1UL << 0) | (1UL << 1) | (1UL << 4);
   default: return 0;
  }
}

StartSelection CurrentSelection;
festring CurrentOriginName("Classic");
festring CurrentModeName("Classic");
truth CurrentVictoryRecorded = false;

void LoadOriginStatistics(std::vector<festring>& Ids,
                          std::vector<long>& Runs,
                          std::vector<long>& Wins)
{
  inputfile File(GetUserDataDir() + ORIGIN_STATS_FILENAME,
                 0, false);
  if(!File.IsOpen())
    return;
  ulong Magic = 0;
  File >> Magic;
  if(Magic != ORIGIN_STATS_MAGIC)
    return;
  File >> Ids >> Runs >> Wins;
  if(Ids.size() != Runs.size() || Ids.size() != Wins.size())
  {
    Ids.clear();
    Runs.clear();
    Wins.clear();
  }
}

void SaveOriginStatistics(const std::vector<festring>& Ids,
                          const std::vector<long>& Runs,
                          const std::vector<long>& Wins)
{
  outputfile File(GetUserDataDir() + ORIGIN_STATS_FILENAME);
  File << ORIGIN_STATS_MAGIC << Ids << Runs << Wins;
}

festring CurrentStatisticsId()
{
  if(CurrentSelection.Mode == START_CLASSIC)
    return festring("classic");
  if(CurrentSelection.Mode == START_CUSTOM)
    return festring("custom");
  if(CurrentSelection.OriginIndex >= 0
     && CurrentSelection.OriginIndex < int(sizeof(Definitions)
                                           / sizeof(Definitions[0])))
    return festring(Definitions[CurrentSelection.OriginIndex].Id);
  return festring();
}

void EditOriginStatistics(cfestring& Id, long RunDelta, long WinDelta)
{
  if(Id.IsEmpty())
    return;
  std::vector<festring> Ids;
  std::vector<long> Runs;
  std::vector<long> Wins;
  LoadOriginStatistics(Ids, Runs, Wins);
  size_t Index = 0;
  for(; Index < Ids.size(); ++Index)
    if(Ids[Index] == Id)
      break;
  if(Index == Ids.size())
  {
    Ids.push_back(Id);
    Runs.push_back(0);
    Wins.push_back(0);
  }
  Runs[Index] += RunDelta;
  Wins[Index] += WinDelta;
  SaveOriginStatistics(Ids, Runs, Wins);
}

void AddItem(character* Player, item* Item)
{
  if(Item)
    Player->GetStack()->AddItem(Item);
}

void AddBananas(character* Player, int Count)
{
  for(int c = 0; c < Count; ++c)
    AddItem(Player, banana::Spawn());
}

void AddKitItems(character* Player, int KitIndex)
{
  switch(KitIndex)
  {
   case 0:
    AddItem(Player, pickaxe::Spawn());
    AddItem(Player, belt::Spawn());
    AddBananas(Player, 2);
    break;
   case 1:
    {
      meleeweapon* Dagger = meleeweapon::Spawn(DAGGER, NO_MATERIALS);
      Dagger->InitMaterials(MAKE_MATERIAL(BRONZE), MAKE_MATERIAL(BALSA_WOOD), true);
      AddItem(Player, Dagger);
      AddItem(Player, lantern::Spawn());
      AddBananas(Player, 1);
      break;
    }
   case 2:
    AddItem(Player, holybook::Spawn());
    AddItem(Player, lantern::Spawn());
    break;
   case 3:
    {
      meleeweapon* Spear = meleeweapon::Spawn(SPEAR, NO_MATERIALS);
      Spear->InitMaterials(MAKE_MATERIAL(BALSA_WOOD), MAKE_MATERIAL(BALSA_WOOD), true);
      AddItem(Player, Spear);
      shield* GuardShield = shield::Spawn(0, NO_MATERIALS);
      GuardShield->InitMaterials(MAKE_MATERIAL(BALSA_WOOD));
      AddItem(Player, GuardShield);
      break;
    }
   case 4:
    AddItem(Player, meleeweapon::Spawn(HAMMER));
    AddItem(Player, pickaxe::Spawn());
    break;
   case 5:
    AddItem(Player, whistle::Spawn());
    AddBananas(Player, 2);
    break;
   case -2:
    AddItem(Player, pickaxe::Spawn());
    AddBananas(Player, 4);
    break;
  }
}

int CustomStarterOptionCount()
{
  return int(sizeof(CustomStarterOptions) / sizeof(CustomStarterOptions[0]));
}

int CustomItemPointsUsed(unsigned long Mask)
{
  int Used = 0;
  for(int Index = 0; Index < CustomStarterOptionCount(); ++Index)
    if(Mask & (1UL << Index))
      Used += CustomStarterOptions[Index].Cost;
  return Used;
}

truth ValidCustomItemMask(unsigned long Mask)
{
  const unsigned long ValidBits = CustomStarterOptionCount()
    >= int(sizeof(unsigned long) * 8) ? ~0UL
    : (1UL << CustomStarterOptionCount()) - 1UL;
  return !(Mask & ~ValidBits) && CustomItemPointsUsed(Mask) <= CUSTOM_ITEM_POINTS;
}

void AddCustomStarterItems(character* Player, unsigned long Mask)
{
  if(Mask & (1UL << 0))
    AddBananas(Player, 2);
  if(Mask & (1UL << 2))
    AddItem(Player, belt::Spawn());
  if(Mask & (1UL << 3))
    AddItem(Player, lantern::Spawn());
  if(Mask & (1UL << 4))
    AddItem(Player, whistle::Spawn());
  if(Mask & (1UL << 5))
    AddItem(Player, holybook::Spawn());
  if(Mask & (1UL << 6))
    AddItem(Player, pickaxe::Spawn());
  if(Mask & (1UL << 7))
    AddItem(Player, meleeweapon::Spawn(HAMMER));
  if(Mask & (1UL << 8))
  {
    meleeweapon* Dagger = meleeweapon::Spawn(DAGGER, NO_MATERIALS);
    Dagger->InitMaterials(MAKE_MATERIAL(BRONZE),
                          MAKE_MATERIAL(BALSA_WOOD), true);
    AddItem(Player, Dagger);
  }
  if(Mask & (1UL << 9))
  {
    meleeweapon* Spear = meleeweapon::Spawn(SPEAR, NO_MATERIALS);
    Spear->InitMaterials(MAKE_MATERIAL(BALSA_WOOD),
                         MAKE_MATERIAL(BALSA_WOOD), true);
    AddItem(Player, Spear);
  }
  if(Mask & (1UL << 10))
  {
    shield* StarterShield = shield::Spawn(0, NO_MATERIALS);
    StarterShield->InitMaterials(MAKE_MATERIAL(BALSA_WOOD));
    AddItem(Player, StarterShield);
  }
}

festring CustomItemsSummary(unsigned long Mask)
{
  festring Result;
  for(int Index = 0; Index < CustomStarterOptionCount(); ++Index)
    if(Mask & (1UL << Index))
    {
      if(!Result.IsEmpty())
        Result << ", ";
      Result << CustomStarterOptions[Index].Name;
    }
  if(Result.IsEmpty())
    Result = "Quest scroll only";
  return Result;
}

festring AttributeSummary(const int* Attributes)
{
  festring Result;
  for(int c = 0; c < ATTRIBUTES; ++c)
  {
    if(c)
      Result << (c == 5 ? "\n" : "  ");
    Result << AttributeNames[c] << ' ' << Attributes[c];
  }
  return Result;
}

festring OriginDetail(const StartDefinition& Definition)
{
  const OriginStatistics Statistics =
    startmode::GetOriginStatistics(festring(Definition.Id));
  festring Result;
  Result << "OVERVIEW :: " << Definition.Description
         << "\n\nATTRIBUTES :: ";
  for(int c = 0; c < ATTRIBUTES; ++c)
  {
    if(c)
      Result << '\n';
    Result << AttributeNames[c] << ' ' << Definition.Attributes[c];
  }
  Result << "\n\nSTARTING KIT :: Quest item: encrypted scroll";
  if(Definition.KitIndex >= 0)
  {
    const StarterKit& Kit = Kits[Definition.KitIndex];
    Result << "\nEquipment: " << Kit.Equipment
           << "\nMoney: " << Kit.Money << " gold"
           << "\nTraining: " << Kit.Training
           << "\nPet: " << (Kit.GuaranteesDog
             ? "Guaranteed colony dog"
             : "Respects the pet setting");
  }
  else if(Definition.KitIndex == -2)
    Result << "\nEquipment: Pickaxe and four bananas"
           << "\nMoney: None\nTraining: None\nPet: None";
  else
    Result << "\nEquipment: No ordinary equipment"
           << "\nMoney: None\nTraining: None\nPet: None";
  Result << "\n\nRECORD :: Runs: " << Statistics.Runs
         << "\nWins: " << Statistics.Wins;
  return Result;
}

void ConfigureStartList(felist& List, int PageLength = 15)
{
  List.SetAdaptivePresentationKind(adaptiveui::MENU_DETAIL);
  List.SetPageLength(PageLength);
}

truth ConfirmSelection(const StartSelection& Selection, cfestring& Title)
{
  felist Summary(Title);
  Summary.SetAdaptivePresentationKind(adaptiveui::MENU_ITEM_GRID);
  Summary.SetPageLength(16);
  const StartDefinition* Definition = Selection.OriginIndex >= 0
    ? &Definitions[Selection.OriginIndex] : 0;
  Summary.AddDescription(Definition ? Definition->Description
                                    : "A custom banana-colony worker.");
  festring FullSummary = Definition ? Definition->Description
                                    : "A custom banana-colony worker.";
  FullSummary << "\n\n" << AttributeSummary(Selection.Attributes);
  FullSummary << "\n\nQuest item: encrypted scroll\n";
  if(Selection.Mode == START_CUSTOM)
  {
    const festring Items = CustomItemsSummary(Selection.CustomItemMask);
    Summary.AddEntry(festring("Supplies: ") << Items,
                     LIGHT_GRAY, 0, NO_IMAGE, false);
    FullSummary << "Selected supplies: " << Items
                << "\nSupply points used: "
                << CustomItemPointsUsed(Selection.CustomItemMask)
                << '/' << CUSTOM_ITEM_POINTS
                << "\nTraining: None\nPet: Respects the pet setting";
  }
  else if(Selection.KitIndex >= 0)
  {
    const StarterKit& Kit = Kits[Selection.KitIndex];
    Summary.AddEntry(festring("Kit: ") << Kit.Name,
                     LIGHT_GRAY, 0, NO_IMAGE, false);
    Summary.SetLastEntryHelp(Kit.Summary);
    FullSummary << "Kit: " << Kit.Name
                << "\nEquipment: " << Kit.Equipment
                << "\nMoney: " << Kit.Money << " gold"
                << "\nTraining: " << Kit.Training
                << "\nPet: " << (Kit.GuaranteesDog
                  ? "Guaranteed colony dog"
                  : "Respects the pet setting");
  }
  else if(Selection.KitIndex == -2)
  {
    Summary.AddEntry(CONST_S("Kit: pickaxe and four bananas"), LIGHT_GRAY,
                     0, NO_IMAGE, false);
    FullSummary << "Equipment: Pickaxe and four bananas"
                << "\nMoney: None\nTraining: None\nPet: None";
  }
  else
  {
    Summary.AddEntry(CONST_S("Kit: quest scroll only"), LIGHT_GRAY,
                     0, NO_IMAGE, false);
    FullSummary << "Equipment: No ordinary equipment"
                << "\nMoney: None\nTraining: None\nPet: None";
  }
  Summary.AddEntry(CONST_S("Begin journey"), GREEN);
  Summary.SetLastEntryHelp(FullSummary);
  const uint Answer = Summary.Draw();
  return Answer == 0;
}

truth ChooseCustomItems(StartSelection& Selection)
{
  int SelectedSupply = 0;
  for(;;)
  {
    const int Used = CustomItemPointsUsed(Selection.CustomItemMask);
    festring Title;
    Title << "Choose starting supplies - " << CUSTOM_ITEM_POINTS - Used
          << " points remaining";
    felist List(Title);
    List.SetAdaptivePresentationKind(adaptiveui::MENU_CHARACTER_SHEET);
    List.SetPageLength(16);
    List.AddDescription(CONST_S("Build your own starting loadout. Selected supplies are marked; you may continue with unused points."));
    for(int c = 0; c < CustomStarterOptionCount(); ++c)
    {
      const bool Selected = Selection.CustomItemMask & (1UL << c);
      const bool Available = Selected
        || Used + CustomStarterOptions[c].Cost <= CUSTOM_ITEM_POINTS;
      festring Row;
      Row << (Selected ? "[x] " : "[ ] ")
          << CustomStarterOptions[c].Name << "  ("
          << CustomStarterOptions[c].Cost << " point"
          << (CustomStarterOptions[c].Cost == 1 ? ")" : "s)");
      List.AddEntry(Row, Available ? (Selected ? GREEN : LIGHT_GRAY)
                                   : DARK_GRAY,
                    0, NO_IMAGE, true);
      List.SetLastEntryAdaptiveAvailable(Available);
      List.SetLastEntryHelp(CustomStarterOptions[c].Description);
    }
    List.AddEntry(CONST_S("Continue"), GREEN);
    List.SetSelected(SelectedSupply);
    const uint Pick = List.Draw();
    if(Pick == ESCAPED)
      return false;
    if(Pick < uint(CustomStarterOptionCount()))
    {
      SelectedSupply = int(Pick);
      const unsigned long Bit = 1UL << Pick;
      if(Selection.CustomItemMask & Bit)
        Selection.CustomItemMask &= ~Bit;
      else if(Used + CustomStarterOptions[Pick].Cost <= CUSTOM_ITEM_POINTS)
        Selection.CustomItemMask |= Bit;
    }
    else if(Pick == uint(CustomStarterOptionCount()))
      return true;
  }
}

truth ChooseCustomAttributes(StartSelection& Selection)
{
  int SelectedAttribute = 0;
  for(;;)
  {
    const int Remaining = START_ATTRIBUTE_TOTAL
                        - startmode::GetAttributeTotal(Selection.Attributes);
    festring Title;
    Title << "Custom Colonist - " << Remaining << " points remaining";
    felist List(Title);
    List.SetAdaptivePresentationKind(adaptiveui::MENU_ATTRIBUTE_ALLOCATOR);
    List.SetPageLength(16);
    List.AddDescription(CONST_S("Adjust every attribute directly with its minus and plus buttons. Values are limited to 7-13 and must total 110."));
    for(int c = 0; c < ATTRIBUTES; ++c)
    {
      festring Row;
      Row << AttributeNames[c] << '|' << Selection.Attributes[c];
      List.AddEntry(Row, LIGHT_GRAY);
    }
    List.AddEntry(CONST_S("Reset to 10"), LIGHT_GRAY);
    List.AddEntry(CONST_S("Randomize"), LIGHT_GRAY);
    const truth Valid = startmode::ValidateAttributes(Selection.Attributes);
    List.AddEntry(CONST_S("Continue"), Valid ? GREEN : DARK_GRAY,
                  0, NO_IMAGE, true);
    List.SetLastEntryAdaptiveAvailable(Valid);
    List.SetSelected(SelectedAttribute);
    const uint Pick = List.Draw();
    if(Pick == ESCAPED)
      return false;
    if(Pick >= KEY_MENU_ADJUST_DECREASE_BASE
       && Pick <= KEY_MENU_ADJUST_DECREASE_MAX)
    {
      SelectedAttribute = int(Pick - KEY_MENU_ADJUST_DECREASE_BASE);
      if(SelectedAttribute < ATTRIBUTES
         && Selection.Attributes[SelectedAttribute] > START_ATTRIBUTE_MIN)
        --Selection.Attributes[SelectedAttribute];
    }
    else if(Pick >= KEY_MENU_ADJUST_INCREASE_BASE
            && Pick <= KEY_MENU_ADJUST_INCREASE_MAX)
    {
      SelectedAttribute = int(Pick - KEY_MENU_ADJUST_INCREASE_BASE);
      if(SelectedAttribute < ATTRIBUTES
         && Selection.Attributes[SelectedAttribute] < START_ATTRIBUTE_MAX
         && Remaining > 0)
        ++Selection.Attributes[SelectedAttribute];
    }
    else if(Pick < ATTRIBUTES)
      SelectedAttribute = int(Pick);
    else if(Pick == ATTRIBUTES)
      startmode::ResetAttributes(Selection.Attributes);
    else if(Pick == ATTRIBUTES + 1)
      startmode::RandomizeAttributes(Selection.Attributes);
    else if(Pick == ATTRIBUTES + 2 && Valid)
      return true;
  }
}
}

StartSelection::StartSelection()
: Mode(START_CLASSIC), OriginIndex(-1), KitIndex(-1), CustomItemMask(0)
{
  startmode::ResetAttributes(Attributes);
}

int startmode::GetDefinitionCount()
{ return int(sizeof(Definitions) / sizeof(Definitions[0])); }

int startmode::GetStandardDefinitionCount() { return 6; }

const StartDefinition& startmode::GetDefinition(int Index)
{ return Definitions[Index]; }

int startmode::GetKitCount()
{ return int(sizeof(Kits) / sizeof(Kits[0])); }

const StarterKit& startmode::GetKit(int Index) { return Kits[Index]; }

int startmode::GetAttributeTotal(const int* Attributes)
{
  int Total = 0;
  for(int c = 0; c < ATTRIBUTES; ++c)
    Total += Attributes[c];
  return Total;
}

truth startmode::ValidateAttributes(const int* Attributes)
{
  if(GetAttributeTotal(Attributes) != START_ATTRIBUTE_TOTAL)
    return false;
  for(int c = 0; c < ATTRIBUTES; ++c)
    if(Attributes[c] < START_ATTRIBUTE_MIN || Attributes[c] > START_ATTRIBUTE_MAX)
      return false;
  return true;
}

void startmode::ResetAttributes(int* Attributes)
{
  for(int c = 0; c < ATTRIBUTES; ++c)
    Attributes[c] = 10;
}

void startmode::RandomizeAttributes(int* Attributes)
{
  for(int c = 0; c < ATTRIBUTES; ++c)
    Attributes[c] = START_ATTRIBUTE_MIN;
  int Points = START_ATTRIBUTE_TOTAL - START_ATTRIBUTE_MIN * ATTRIBUTES;
  while(Points)
  {
    const int Attribute = RAND() % ATTRIBUTES;
    if(Attributes[Attribute] < START_ATTRIBUTE_MAX)
    {
      ++Attributes[Attribute];
      --Points;
    }
  }
}

truth startmode::Choose(StartSelection& Selection)
{
  Selection = StartSelection();
  for(;;)
  {
    felist Modes(CONST_S("Choose your character mode"));
    ConfigureStartList(Modes);
    Modes.AddDescription(CONST_S("Classic preserves IVAN's original randomized start. Origins and Custom use exact displayed attributes."));
    Modes.AddEntry(CONST_S("Classic"), GREEN);
    Modes.SetLastEntryHelp(CONST_S("The original IVAN start: small random attribute changes, random starting money, and the configured pet behavior."));
    Modes.AddEntry(CONST_S("Banana Origins"), LIGHT_GRAY);
    Modes.SetLastEntryHelp(CONST_S("Choose a banana-colony background with fixed attributes and a themed starter kit."));
    Modes.AddEntry(CONST_S("Custom Colonist"), LIGHT_GRAY);
    Modes.SetLastEntryHelp(CONST_S("Allocate 110 attribute points on one sheet, then build a starter loadout from a supply-point pool."));
    Modes.SetSelected(Selection.Mode == START_CUSTOM ? 2
                      : Selection.Mode == START_PRESET
                        || Selection.Mode == START_CHALLENGE ? 1 : 0);
    const uint Mode = Modes.Draw();
    if(Mode == ESCAPED)
      return false;
    if(Mode == 0)
    {
      Selection = StartSelection();
      return true;
    }
    if(Mode == 1)
    {
      for(;;)
      {
        felist Sections(CONST_S("Choose a Banana Origin"));
        ConfigureStartList(Sections);
        Sections.AddDescription(CONST_S("Balanced origins total 110 attribute points. Challenge origins are deliberately weaker but remain scoreable."));
        Sections.AddEntry(CONST_S("Balanced Origins"), LIGHT_GRAY);
        Sections.SetLastEntryHelp(CONST_S("Six themed colony backgrounds with balanced attributes and starter kits."));
        Sections.AddEntry(CONST_S("Challenge Origins"), RED);
        Sections.SetLastEntryHelp(CONST_S("Deliberately disadvantaged starts for players seeking a harder journey."));
        Sections.SetSelected(Selection.Mode == START_CHALLENGE ? 1 : 0);
        const uint Section = Sections.Draw();
        if(Section == ESCAPED)
          break;
        if(Section > 1)
          continue;

        const int First = Section == 0 ? 0 : GetStandardDefinitionCount();
        const int Last = Section == 0 ? GetStandardDefinitionCount()
                                      : GetDefinitionCount();
        for(;;)
        {
          felist Origins(Section == 0 ? CONST_S("Balanced Origins")
                                      : CONST_S("Challenge Origins"));
          ConfigureStartList(Origins);
          Origins.AddDescription(Section == 0
            ? CONST_S("Choose a balanced banana-colony background.")
            : CONST_S("Challenge starts are intentionally weaker and use the Challenge score filter."));
          for(int c = First; c < Last; ++c)
          {
            Origins.AddEntry(Definitions[c].Name,
                             Section == 0 ? LIGHT_GRAY : RED);
            Origins.SetLastEntryHelp(OriginDetail(Definitions[c]));
          }
          if(Selection.OriginIndex >= First && Selection.OriginIndex < Last)
            Origins.SetSelected(Selection.OriginIndex - First);
          const uint Origin = Origins.Draw();
          if(Origin == ESCAPED)
            break;
          const int DefinitionIndex = First + int(Origin);
          if(DefinitionIndex < First || DefinitionIndex >= Last)
            continue;
          Selection.Mode = Definitions[DefinitionIndex].Difficulty == START_DIFFICULT
                         ? START_CHALLENGE : START_PRESET;
          Selection.OriginIndex = DefinitionIndex;
          Selection.KitIndex = Definitions[DefinitionIndex].KitIndex;
          Selection.CustomItemMask = 0;
          std::copy(Definitions[DefinitionIndex].Attributes,
                    Definitions[DefinitionIndex].Attributes + ATTRIBUTES,
                    Selection.Attributes);
          return true;
        }
      }
      continue;
    }
    if(Mode == 2)
    {
      const truth ResumeCustom = Selection.Mode == START_CUSTOM;
      Selection.Mode = START_CUSTOM;
      Selection.OriginIndex = -1;
      if(!ResumeCustom)
      {
        ResetAttributes(Selection.Attributes);
        Selection.KitIndex = -1;
        Selection.CustomItemMask = 0;
      }
      for(;;)
      {
        if(!ChooseCustomAttributes(Selection))
          break;
        for(;;)
        {
          if(!ChooseCustomItems(Selection))
            break;
          if(ConfirmSelection(Selection, CONST_S("Custom Colonist")))
            return true;
          /* Summary Back returns to supplies, preserving the full build. */
        }
      }
    }
  }
}

void startmode::Apply(character* Player, const StartSelection& Selection)
{
  if(Selection.Mode == START_CLASSIC)
    return;

  for(int c = 0; c < ATTRIBUTES; ++c)
    Player->EditAttribute(c, Selection.Attributes[c] - Player->GetAttribute(c, false));

  if(Selection.Mode == START_CUSTOM)
  {
    Player->SetMoney((Selection.CustomItemMask & (1UL << 1)) ? 30 : 0);
    AddCustomStarterItems(Player, Selection.CustomItemMask);
    return;
  }

  long Money = 0;
  if(Selection.KitIndex >= 0)
    Money = Kits[Selection.KitIndex].Money;
  Player->SetMoney(Money);
  AddKitItems(Player, Selection.KitIndex);

  if(Selection.KitIndex >= 0)
  {
    const StarterKit& Kit = Kits[Selection.KitIndex];
    if(Kit.TrainingCategory1 >= 0)
      Player->GetCWeaponSkill(Kit.TrainingCategory1)->AddHit(Kit.TrainingHits1);
    if(Kit.TrainingCategory2 >= 0)
      Player->GetCWeaponSkill(Kit.TrainingCategory2)->AddHit(Kit.TrainingHits2);
  }
}

void startmode::CreatePet(const StartSelection& Selection)
{
  truth CreateDog = !ivanconfig::GetNoPet();
  if(Selection.Mode != START_CLASSIC)
  {
    if(Selection.OriginIndex >= 0 && Definitions[Selection.OriginIndex].ForbidPet)
      CreateDog = false;
    if(Selection.KitIndex >= 0 && Kits[Selection.KitIndex].GuaranteesDog)
      CreateDog = true;
  }
  if(CreateDog)
  {
    character* Doggie = dog::Spawn();
    Doggie->SetTeam(game::GetTeam(0));
    game::GetWorldMap()->GetPlayerGroup().push_back(Doggie);
    Doggie->SetAssignedName(ivanconfig::GetDefaultPetName());
  }
}

void startmode::SetCurrent(const StartSelection& Selection)
{
  CurrentSelection = Selection;
  CurrentVictoryRecorded = false;
  if(Selection.Mode == START_CLASSIC)
  {
    CurrentOriginName = "Classic";
    CurrentModeName = "Classic";
  }
  else if(Selection.Mode == START_CUSTOM)
  {
    CurrentOriginName = "Custom Colonist";
    CurrentModeName = "Custom";
  }
  else
  {
    CurrentOriginName = Definitions[Selection.OriginIndex].Name;
    CurrentModeName = Selection.Mode == START_CHALLENGE ? "Challenge" : "Preset";
  }
}

void startmode::ResetCurrent() { SetCurrent(StartSelection()); }
const StartSelection& startmode::GetCurrent() { return CurrentSelection; }
cfestring& startmode::GetCurrentOriginName() { return CurrentOriginName; }
cfestring& startmode::GetCurrentModeName() { return CurrentModeName; }

festring startmode::GetStoryFlavor()
{
  if(CurrentSelection.Mode == START_CLASSIC)
    return festring();
  if(CurrentSelection.Mode == START_CUSTOM)
    return festring("You had performed many jobs around the banana colony, learning to rely on the particular talents and tools you chose for yourself.");
  return festring(Definitions[CurrentSelection.OriginIndex].Flavor);
}

festring startmode::GetScoreMarker()
{
  festring Marker;
  Marker << "{IVANSTART:" << CurrentModeName << '|' << CurrentOriginName << '}';
  return Marker;
}

OriginStatistics startmode::GetOriginStatistics(cfestring& Id)
{
  std::vector<festring> Ids;
  std::vector<long> Runs;
  std::vector<long> Wins;
  LoadOriginStatistics(Ids, Runs, Wins);
  for(size_t Index = 0; Index < Ids.size(); ++Index)
    if(Ids[Index] == Id)
    {
      OriginStatistics Result;
      Result.Runs = Runs[Index];
      Result.Wins = Wins[Index];
      return Result;
    }
  return OriginStatistics();
}

void startmode::RecordCurrentRun()
{
  EditOriginStatistics(CurrentStatisticsId(), 1, 0);
}

void startmode::RecordVictory()
{
  if(CurrentVictoryRecorded)
    return;
  EditOriginStatistics(CurrentStatisticsId(), 0, 1);
  CurrentVictoryRecorded = true;
}

void startmode::Save(outputfile& SaveFile)
{
  festring OriginId;
  festring KitId;
  if(CurrentSelection.OriginIndex >= 0)
    OriginId = Definitions[CurrentSelection.OriginIndex].Id;
  if(CurrentSelection.KitIndex >= 0)
    KitId = Kits[CurrentSelection.KitIndex].Id;
  else if(CurrentSelection.Mode == START_CHALLENGE)
    KitId = ChallengeKitId(CurrentSelection.OriginIndex);
  SaveFile << STARTMODE_SAVE_MAGIC << int(CurrentSelection.Mode)
           << OriginId << KitId;
  for(int c = 0; c < ATTRIBUTES; ++c)
    SaveFile << CurrentSelection.Attributes[c];
  SaveFile << CurrentSelection.CustomItemMask;
}

void startmode::Load(inputfile& SaveFile)
{
  ulong Magic = 0;
  SaveFile >> Magic;
  if(Magic != STARTMODE_SAVE_MAGIC && Magic != STARTMODE_SAVE_MAGIC_V2
     && Magic != STARTMODE_SAVE_MAGIC_V1)
  {
    ResetCurrent();
    return;
  }
  int Mode = 0;
  StartSelection Selection;
  festring OriginId;
  festring KitId;
  if(Magic == STARTMODE_SAVE_MAGIC_V1)
    SaveFile >> Mode >> Selection.OriginIndex >> Selection.KitIndex;
  else
  {
    SaveFile >> Mode >> OriginId >> KitId;
    Selection.OriginIndex = FindDefinition(OriginId);
    Selection.KitIndex = FindKit(KitId);
  }
  Selection.Mode = StartMode(Mode);
  if((Magic == STARTMODE_SAVE_MAGIC || Magic == STARTMODE_SAVE_MAGIC_V2)
     && Selection.Mode == START_CHALLENGE
     && Selection.OriginIndex >= 0
     && KitId == ChallengeKitId(Selection.OriginIndex))
    Selection.KitIndex = Definitions[Selection.OriginIndex].KitIndex;
  for(int c = 0; c < ATTRIBUTES; ++c)
    SaveFile >> Selection.Attributes[c];
  if(Magic == STARTMODE_SAVE_MAGIC)
    SaveFile >> Selection.CustomItemMask;
  else if(Selection.Mode == START_CUSTOM)
    Selection.CustomItemMask = CustomMaskForLegacyKit(Selection.KitIndex);

  truth Valid = Selection.Mode >= START_CLASSIC
             && Selection.Mode <= START_CHALLENGE;
  if(Valid && Selection.Mode == START_CLASSIC)
    Valid = Selection.OriginIndex == -1 && Selection.KitIndex == -1
         && (Magic == STARTMODE_SAVE_MAGIC_V1
             || (OriginId.IsEmpty() && KitId.IsEmpty()));
  else if(Valid && Selection.Mode == START_CUSTOM)
    Valid = Selection.OriginIndex == -1
         && ((Magic == STARTMODE_SAVE_MAGIC && Selection.KitIndex == -1
              && ValidCustomItemMask(Selection.CustomItemMask))
             || (Magic != STARTMODE_SAVE_MAGIC && Selection.KitIndex >= 0
                 && Selection.KitIndex < GetKitCount()))
         && (Magic == STARTMODE_SAVE_MAGIC_V1 || OriginId.IsEmpty())
         && ValidateAttributes(Selection.Attributes);
  else if(Valid)
    Valid = Selection.OriginIndex >= 0
         && Selection.OriginIndex < GetDefinitionCount()
         && Selection.KitIndex == Definitions[Selection.OriginIndex].KitIndex
         && ((Selection.Mode == START_PRESET
              && Definitions[Selection.OriginIndex].Difficulty == START_STANDARD)
             || (Selection.Mode == START_CHALLENGE
                 && Definitions[Selection.OriginIndex].Difficulty == START_DIFFICULT));
  if(Valid && (Magic == STARTMODE_SAVE_MAGIC
               || Magic == STARTMODE_SAVE_MAGIC_V2)
     && Selection.Mode == START_CHALLENGE)
    Valid = KitId == ChallengeKitId(Selection.OriginIndex);
  if(!Valid)
    ResetCurrent();
  else
    SetCurrent(Selection);
}

truth startmode::LoadOptional(inputfile& SaveFile)
{
  const long Position = SaveFile.TellPos();
  SaveFile.SeekPosEnd(0);
  const long End = SaveFile.TellPos();
  SaveFile.SeekPosBegin(Position);
  if(Position < 0 || End <= Position)
  {
    ResetCurrent();
    return false;
  }
  Load(SaveFile);
  return true;
}

truth startmode::RunSelfTests(festring& Failure)
{
  if(CustomStarterOptionCount() <= 0
     || CustomStarterOptionCount() >= int(sizeof(unsigned long) * 8))
  {
    Failure = "The custom starter-item pool cannot be represented safely.";
    return false;
  }
  for(int Index = 0; Index < CustomStarterOptionCount(); ++Index)
    if(!CustomStarterOptions[Index].Name
       || !*CustomStarterOptions[Index].Name
       || CustomStarterOptions[Index].Cost < 1
       || CustomStarterOptions[Index].Cost > CUSTOM_ITEM_POINTS)
    {
      Failure = festring("Invalid custom starter option at index ")
              << Index << '.';
      return false;
    }
  for(int Kit = 0; Kit < GetKitCount(); ++Kit)
    if(!ValidCustomItemMask(CustomMaskForLegacyKit(Kit)))
    {
      Failure = festring("Legacy custom kit migration exceeds the supply pool at index ")
              << Kit << '.';
      return false;
    }

  if(GetStandardDefinitionCount() != GetKitCount())
  {
    Failure = "Balanced origin and starter kit counts differ.";
    return false;
  }

  const int ExpectedPrimaryTraining[] =
    { AXES, SMALL_SWORDS, CRAFTING, POLE_ARMS, BLUNT_WEAPONS, -1 };
  const int ExpectedSecondaryTraining[] =
    { -1, -1, -1, SHIELDS, CRAFTING, -1 };
  for(int c = 0; c < GetKitCount(); ++c)
  {
    if(!Kits[c].Id || !*Kits[c].Id || !Kits[c].Name || !*Kits[c].Name
       || FindKit(festring(Kits[c].Id)) != c
       || Kits[c].TrainingCategory1 != ExpectedPrimaryTraining[c]
       || Kits[c].TrainingCategory2 != ExpectedSecondaryTraining[c])
    {
      Failure = festring("Starter kit definition is invalid at index ") << c << '.';
      return false;
    }
  }
  if(!Kits[5].GuaranteesDog)
  {
    Failure = "The Kennel Keeper kit must guarantee a dog.";
    return false;
  }

  for(int c = 0; c < GetDefinitionCount(); ++c)
  {
    const StartDefinition& Definition = GetDefinition(c);
    if(!Definition.Id || !*Definition.Id
       || FindDefinition(festring(Definition.Id)) != c)
    {
      Failure = festring("Origin definition has an invalid stable ID at index ")
              << c << '.';
      return false;
    }
    for(int Attribute = 0; Attribute < ATTRIBUTES; ++Attribute)
      if(Definition.Attributes[Attribute] < START_ATTRIBUTE_MIN
         || Definition.Attributes[Attribute] > START_ATTRIBUTE_MAX)
      {
        Failure = festring(Definition.Name) << " has an out-of-range attribute.";
        return false;
      }
    if(Definition.Difficulty == START_STANDARD
       && !ValidateAttributes(Definition.Attributes))
    {
      Failure = festring(Definition.Name) << " does not total 110 attributes.";
      return false;
    }
    if(Definition.Difficulty == START_STANDARD
       && (Definition.KitIndex < 0 || Definition.KitIndex >= GetKitCount()))
    {
      Failure = festring(Definition.Name) << " has an invalid starter kit.";
      return false;
    }
  }

  int Attributes[ATTRIBUTES];
  ResetAttributes(Attributes);
  if(!ValidateAttributes(Attributes))
  {
    Failure = "The reset custom attribute pool is invalid.";
    return false;
  }
  for(int Attempt = 0; Attempt < 1000; ++Attempt)
  {
    RandomizeAttributes(Attributes);
    if(!ValidateAttributes(Attributes))
    {
      Failure = "Custom attribute randomization produced an invalid pool.";
      return false;
    }
  }
  return true;
}
