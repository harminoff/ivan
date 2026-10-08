/*
 * Shared new-game character starts for IVAN.
 */

#ifndef __STARTMODE_H__
#define __STARTMODE_H__

#include "festring.h"
#include "ivandef.h"

class character;
class outputfile;
class inputfile;

enum StartMode
{
  START_CLASSIC = 0,
  START_PRESET = 1,
  START_CUSTOM = 2,
  START_CHALLENGE = 3
};

enum StartDifficulty
{
  START_STANDARD = 0,
  START_DIFFICULT = 1
};

struct StarterKit
{
  const char* Id;
  const char* Name;
  const char* Summary;
  const char* Equipment;
  const char* Training;
  long Money;
  int TrainingCategory1;
  int TrainingHits1;
  int TrainingCategory2;
  int TrainingHits2;
  truth GuaranteesDog;
};

struct StartDefinition
{
  const char* Id;
  const char* Name;
  const char* Description;
  const char* Flavor;
  StartDifficulty Difficulty;
  int Attributes[ATTRIBUTES];
  int KitIndex;
  truth ForbidPet;
};

struct StartSelection
{
  StartSelection();

  StartMode Mode;
  int OriginIndex;
  int KitIndex;
  unsigned long CustomItemMask;
  int Attributes[ATTRIBUTES];
};

struct OriginStatistics
{
  OriginStatistics() : Runs(0), Wins(0) { }
  long Runs;
  long Wins;
};

class startmode
{
 public:
  static truth Choose(StartSelection&);
  static void Apply(character*, const StartSelection&);
  static void CreatePet(const StartSelection&);
  static void SetCurrent(const StartSelection&);
  static void ResetCurrent();

  static const StartSelection& GetCurrent();
  static cfestring& GetCurrentOriginName();
  static cfestring& GetCurrentModeName();
  static festring GetStoryFlavor();
  static festring GetScoreMarker();
  static OriginStatistics GetOriginStatistics(cfestring&);
  static void RecordCurrentRun();
  static void RecordVictory();

  static int GetDefinitionCount();
  static int GetStandardDefinitionCount();
  static const StartDefinition& GetDefinition(int);
  static int GetKitCount();
  static const StarterKit& GetKit(int);

  static int GetAttributeTotal(const int*);
  static truth ValidateAttributes(const int*);
  static void ResetAttributes(int*);
  static void RandomizeAttributes(int*);

  static void Save(outputfile&);
  static void Load(inputfile&);
  static truth LoadOptional(inputfile&);
  static truth RunSelfTests(festring&);
};

#endif
