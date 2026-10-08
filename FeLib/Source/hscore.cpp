/*
 *
 *  Iter Vehemens ad Necem (IVAN)
 *  Copyright (C) Timo Kiviluoto
 *  Released under the GNU General
 *  Public License
 *
 *  See LICENSING which should be included
 *  along with this file for more details
 *
 */

#include "hscore.h"
#include "save.h"
#include "felist.h"
#include "feio.h"
#include "femath.h"

#include <string>

/* Increment this if changes make highscores incompatible */
#define HIGH_SCORE_VERSION 129

namespace
{
enum scorefilter
{
  SCORE_ALL = 0,
  SCORE_CLASSIC,
  SCORE_PRESET,
  SCORE_CUSTOM,
  SCORE_CHALLENGE
};

int CurrentScoreFilter = SCORE_ALL;

int ScoreMode(cfestring& Entry, festring& Display)
{
  const std::string Raw = Entry.CStr();
  const std::string Prefix = "{IVANSTART:";
  if(Raw.find(Prefix) != 0)
  {
    Display = Entry;
    Display << " [Classic]";
    return SCORE_CLASSIC;
  }

  const std::string::size_type End = Raw.find('}');
  const std::string::size_type Split = Raw.find('|', Prefix.size());
  if(End == std::string::npos || Split == std::string::npos || Split > End)
  {
    Display = Entry;
    return SCORE_CLASSIC;
  }

  const std::string Mode = Raw.substr(Prefix.size(), Split - Prefix.size());
  const std::string Origin = Raw.substr(Split + 1, End - Split - 1);
  Display = Raw.substr(End + 1).c_str();
  Display << " [" << Mode.c_str();
  if(Origin != Mode && Origin != "Classic")
    Display << ": " << Origin.c_str();
  Display << ']';

  if(Mode == "Preset") return SCORE_PRESET;
  if(Mode == "Custom") return SCORE_CUSTOM;
  if(Mode == "Challenge") return SCORE_CHALLENGE;
  return SCORE_CLASSIC;
}
}

cfestring& highscore::GetEntry(int I) const { return Entry[I]; }
long highscore::GetScore(int I) const { return Score[I]; }
long highscore::GetSize() const { return Entry.size(); }

highscore::highscore(cfestring& File) : LastAdd(0xFF), Version(HIGH_SCORE_VERSION), DefaultFile(File) { Load(File); }

truth highscore::Add(long NewScore, cfestring& NewEntry,
                     time_t NewTime, long NewRandomID)
{
  for(uint c = 0; c < Score.size(); ++c)
    if(Score[c] < NewScore)
    {
      Entry.insert(Entry.begin() + c, NewEntry);
      Score.insert(Score.begin() + c, NewScore);
      Time.insert(Time.begin() + c, NewTime);
      RandomID.insert(RandomID.begin() + c, NewRandomID);

      if(Score.size() > MAX_HIGHSCORES)
      {
        Entry.resize(MAX_HIGHSCORES, festring());
        Score.resize(MAX_HIGHSCORES);
        Time.resize(MAX_HIGHSCORES);
        RandomID.resize(MAX_HIGHSCORES);
      }

      LastAdd = c;
      return true;
    }

  if(Score.size() < MAX_HIGHSCORES)
  {
    LastAdd = Score.size();
    Entry.push_back(NewEntry);
    Score.push_back(NewScore);
    Time.push_back(NewTime);
    RandomID.push_back(NewRandomID);
    return true;
  }
  else
  {
    LastAdd = MAX_HIGHSCORES;
    return false;
  }
}

void highscore::Draw() const
{
  if(Score.empty())
  {
    iosystem::TextScreen(CONST_S("There are no entries yet. "
                                 "Play a game to correct this."));
    return;
  }

  if(GetVersion() != HIGH_SCORE_VERSION)
  {
    iosystem::TextScreen(CONST_S("The highscore file is for another version of IVAN."));
    return;
  }

  felist Filters(CONST_S("Adventurers' Hall of Fame - Filter"));
  Filters.AddDescription(CONST_S("Scores are calculated and ordered normally. Character mode only labels and filters the entries."));
  Filters.AddEntry(CONST_S("All"), LIGHT_GRAY);
  Filters.AddEntry(CONST_S("Classic"), LIGHT_GRAY);
  Filters.AddEntry(CONST_S("Preset"), LIGHT_GRAY);
  Filters.AddEntry(CONST_S("Custom"), LIGHT_GRAY);
  Filters.AddEntry(CONST_S("Challenge"), LIGHT_GRAY);
  Filters.SetSelected(CurrentScoreFilter);
  const uint Filter = Filters.Draw();
  if(Filter == ESCAPED || Filter > SCORE_CHALLENGE)
    return;
  CurrentScoreFilter = int(Filter);

  felist List(CONST_S("Adventurers' Hall of Fame"));
  festring Desc;
  int Visible = 0;

  for(uint c = 0; c < Score.size(); ++c)
  {
    festring DisplayEntry;
    const int Mode = ScoreMode(Entry[c], DisplayEntry);
    if(CurrentScoreFilter != SCORE_ALL && CurrentScoreFilter != Mode)
      continue;
    Desc.Empty();
    Desc << c + 1;
    Desc.Resize(5, ' ');
    Desc << Score[c];
    Desc.Resize(13, ' ');
    Desc << DisplayEntry;
    List.AddEntry(Desc, c == uint(LastAdd) ? WHITE : LIGHT_GRAY, 13);
    List.SetLastEntryHelp(festring() << "The brave, foolish souls who ventured into the world of IVAN.");
    ++Visible;
  }

  if(!Visible)
  {
    iosystem::TextScreen(CONST_S("There are no scores for this character mode yet."));
    return;
  }

  List.SetFlags(FADE);
  List.SetPageLength(40);
  List.Draw();
}

void highscore::Save(cfestring& File) const
{
  outputfile HighScore(File.IsEmpty() ? DefaultFile : File);
  long CheckSum = HIGH_SCORE_VERSION + LastAdd;
  for(size_t c = 0; c < Score.size(); ++c)
  {
    CheckSum += Score[c] + Entry[c].GetCheckSum() + RandomID[c];
  }

  HighScore << ushort(HIGH_SCORE_VERSION) << Score
            << Entry << Time << RandomID << LastAdd << CheckSum;
}

/* This function needs much more error handling */
void highscore::Load(cfestring& File)
{
  cfestring Path = File.IsEmpty() ? DefaultFile : File;

  {
    inputfile HighScore(Path, 0, false);

    if(!HighScore.IsOpen())
      return;

    HighScore.Get();

    if(HighScore.Eof())
      return;
  }

  inputfile HighScore(Path, 0, false);
  HighScore >> Version;
  HighScore >> Score >> Entry >> Time >> RandomID >> LastAdd;
}

truth highscore::MergeToFile(highscore* To) const
{
  truth MergedSomething = false;

  for(uint c = 0; c < Score.size(); ++c)
    if(!To->Find(Score[c], Entry[c], Time[c], RandomID[c]))
    {
      To->Add(Score[c], Entry[c], Time[c], RandomID[c]);
      MergedSomething = true;
    }

  return MergedSomething;
}

truth highscore::Add(long NewScore, cfestring& NewEntry)
{
  return Add(NewScore, NewEntry, time(0), RAND());
}

/* Because of major stupidity this return the number of NEXT
   from the right entry, 0 = not found */

int highscore::Find(long AScore, cfestring& AEntry,
                    time_t ATime, long ARandomID)
{
  for(uint c = 0; c < Score.size(); ++c)
  {
    if(AScore == Score[c] && Entry[c] == AEntry
       && ATime == Time[c] && ARandomID == RandomID[c])
      return c + 1;
  }

  return 0;
}

truth highscore::LastAddFailed() const
{ return LastAdd == MAX_HIGHSCORES; }

void highscore::Clear()
{
  Entry.clear();
  Score.clear();
  Time.clear();
  RandomID.clear();
  Version = HIGH_SCORE_VERSION;
  LastAdd = 0xFF;
}

truth highscore::CheckVersion() const
{
  return Version == HIGH_SCORE_VERSION;
}
