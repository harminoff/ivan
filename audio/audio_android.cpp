/* Android music backend for IVAN. The desktop build continues to use audio.cpp. */

#include "audio.h"

#include "SDL_mixer.h"

#include <algorithm>
#include <cstdlib>

namespace
{
Mix_Music* CurrentMusicHandle = 0;

festring ToOggPath(cfestring& Path)
{
  festring Result = Path;
  const festring Suffix = ".mid";
  if(Result.GetSize() >= Suffix.GetSize())
    Result.Resize(Result.GetSize() - Suffix.GetSize());
  Result << ".ogg";
  return Result;
}

void FreeCurrentMusic()
{
  if(CurrentMusicHandle)
  {
    Mix_HaltMusic();
    Mix_FreeMusic(CurrentMusicHandle);
    CurrentMusicHandle = 0;
  }
}
}

musicfile::musicfile(cfestring& Filename, int LowThreshold, int HighThreshold)
: Filename(Filename), LowThreshold(LowThreshold), HighThreshold(HighThreshold), isPlaying(false)
{
}

int audio::MasterVolume = MAX_MASTER_VOLUME;
int audio::TargetIntensity = 0;
int audio::CurrentIntensity = 0;
bool audio::isInit = false;
int audio::PlaybackState = STOPPED;
volatile bool audio::isTrackPlaying = false;
bool audio::volumeChangeRequest = false;
int audio::CurrentPosition = 0;
int audio::CurrentMIDIOutPort = 1;
std::vector<musicfile> audio::Tracks;
RtMidiOut* audio::midiout = 0;
festring audio::CurrentTrack;
festring audio::MusDir;
int audio::DeltaVolumePerIntensity[MAX_MIDI_CHANNELS] = {0};
int audio::IntensityVolume[MAX_MIDI_CHANNELS] = {0};
int audio::InitialIntensityVolume[MAX_MIDI_CHANNELS] = {0};

void audio::error(RtMidiError::Type, const std::string&, void*)
{
}

void audio::Init(cfestring& MusicDirectory)
{
  MusDir = MusicDirectory;
  PlaybackState = STOPPED;
  CurrentTrack.Empty();
  Tracks.clear();
  if(Mix_QuerySpec(0, 0, 0) == 0)
    isInit = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0;
  else
    isInit = true;
  SetVolumeLevel(MasterVolume);
  atexit(audio::DeInit);
}

void audio::DeInit()
{
  FreeCurrentMusic();
  Tracks.clear();
  isInit = false;
}

int audio::Loop(void*)
{
  return 0;
}

int audio::PlayMIDIFile(cfestring& Filename, int32_t Loops)
{
  if(!isInit || CurrentMIDIOutPort == 0)
    return -1;

  FreeCurrentMusic();
  festring OggPath = ToOggPath(Filename);
  CurrentMusicHandle = Mix_LoadMUS(OggPath.CStr());
  if(!CurrentMusicHandle)
    return -1;

  Mix_VolumeMusic((MasterVolume * MIX_MAX_VOLUME) / MAX_MASTER_VOLUME);
  isTrackPlaying = Mix_PlayMusic(CurrentMusicHandle, Loops) == 0;
  return isTrackPlaying ? 0 : -1;
}

void audio::SendMIDIEvent(std::vector<unsigned char>*)
{
}

int audio::GetMIDIOutputDevices(std::vector<std::string>& DeviceNames)
{
  DeviceNames.push_back("Android audio");
  return 1;
}

int audio::ChangeMIDIOutputDevice(int NewPort)
{
  CurrentMIDIOutPort = NewPort;
  if(NewPort == 0)
    FreeCurrentMusic();
  return 0;
}

cfestring& audio::GetCurrentlyPlayedFile()
{
  return CurrentTrack;
}

void audio::SetVolumeLevel(int Volume)
{
  MasterVolume = std::max(0, std::min(Volume, int(MAX_MASTER_VOLUME)));
  Mix_VolumeMusic((MasterVolume * MIX_MAX_VOLUME) / MAX_MASTER_VOLUME);
}

int audio::GetVolumeLevel()
{
  return MasterVolume;
}

void audio::SendVolumeMessage(int TargetVolume)
{
  SetVolumeLevel(TargetVolume);
}

void audio::IntensityLevel(int Intensity)
{
  CurrentIntensity = Intensity;
}

void audio::RemoveMIDIFile(cfestring& Filename)
{
  for(std::vector<musicfile>::iterator i = Tracks.begin(); i != Tracks.end(); ++i)
    if(i->GetFilename() == Filename)
    {
      Tracks.erase(i);
      break;
    }
}

void audio::LoadMIDIFile(cfestring& Filename, int LowThreshold, int HighThreshold)
{
  Tracks.push_back(musicfile(Filename, LowThreshold, HighThreshold));
}

void audio::ClearMIDIPlaylist(cfestring& ExceptFilename)
{
  FreeCurrentMusic();
  CurrentTrack.Empty();
  if(ExceptFilename.IsEmpty())
  {
    Tracks.clear();
    return;
  }

  for(std::vector<musicfile>::iterator i = Tracks.begin(); i != Tracks.end();)
    if(i->GetFilename() == ExceptFilename)
      ++i;
    else
      i = Tracks.erase(i);
}

int audio::IsPlaybackStopped()
{
  return PlaybackState == STOPPED;
}

void audio::SetPlaybackStatus(uint8_t NewState)
{
  PlaybackState = NewState;
  if(!(NewState & PLAYING))
  {
    FreeCurrentMusic();
    return;
  }

  if(Tracks.empty() || CurrentMIDIOutPort == 0)
    return;

  const int Index = rand() % Tracks.size();
  CurrentTrack = Tracks[Index].GetFilename();
  festring FullPath = MusDir + CurrentTrack;
  PlayMIDIFile(FullPath, -1);
}

void audio::CalculateChannelVolumes(int, int*)
{
}
