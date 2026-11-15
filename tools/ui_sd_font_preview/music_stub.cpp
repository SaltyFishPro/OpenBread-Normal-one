#include "../../src/services/MusicService.h"
#include <cstring>
#include <cstdio>
namespace TestMusic {
MusicService::Page page;
MusicService::ListState list=MusicService::ListState::Ready;
MusicService::PlaybackState playback=MusicService::PlaybackState::Stopped;
MusicService::TrackInfo track;
uint32_t current=0, count=5;
bool favorite=false,shuffle=false;
unsigned volume=12;
}
bool MusicService::startListScan(SdCardService&) { return true; }
void MusicService::exitMusic(SdCardService&) {}
bool MusicService::loadPage(uint16_t p) {
  TestMusic::page=Page{};TestMusic::page.pageIndex=p;
  const char* names[]={"夜空中最亮的星.mp3","晴天.mp3","稻香.mp3","平凡之路.mp3","海阔天空.mp3"};
  for(unsigned i=0;i<kRowsPerPage && p*kRowsPerPage+i<TestMusic::count;++i) {
    std::snprintf(TestMusic::page.tracks[i].name,sizeof(TestMusic::page.tracks[i].name),"%s",names[p*kRowsPerPage+i]);
    std::snprintf(TestMusic::page.tracks[i].path,sizeof(TestMusic::page.tracks[i].path),"/music/%u.mp3",p*kRowsPerPage+i);
    ++TestMusic::page.count;
  } return TestMusic::page.count>0;
}
const MusicService::Page& MusicService::page() const{return TestMusic::page;}
uint32_t MusicService::trackCount() const{return TestMusic::count;}
MusicService::ListState MusicService::listState() const{return TestMusic::list;}
MusicService::PlaybackState MusicService::playbackState() const{return TestMusic::playback;}
const MusicService::TrackInfo& MusicService::currentTrack() const{return TestMusic::track;}
uint32_t MusicService::currentTrackIndex() const{return TestMusic::current;}
uint32_t MusicService::currentSeconds() const{return 14;}
uint32_t MusicService::durationSeconds() const{return 240;}
bool MusicService::shuffleEnabled() const{return TestMusic::shuffle;}
uint8_t MusicService::volume() const{return TestMusic::volume;}
const char* MusicService::errorText() const{return "";}
const char* MusicService::audioDebugText() const{return "";}
bool MusicService::isFavorite(const TrackInfo&) const{return TestMusic::favorite;}
bool MusicService::toggleFavorite(const TrackInfo&){TestMusic::favorite=!TestMusic::favorite;return true;}
bool MusicService::adjustVolume(int8_t n){TestMusic::volume+=n;return true;}
bool MusicService::playTrack(uint32_t index){
  TestMusic::current=index;
  std::snprintf(TestMusic::track.name,sizeof(TestMusic::track.name),"Track %u.mp3",index);
  std::snprintf(TestMusic::track.path,sizeof(TestMusic::track.path),"/music/%u.mp3",index);
  TestMusic::playback=PlaybackState::Playing;return true;
}
bool MusicService::togglePause(uint32_t){TestMusic::playback=TestMusic::playback==PlaybackState::Playing?PlaybackState::Paused:PlaybackState::Playing;return true;}
bool MusicService::previous(){return playTrack((TestMusic::current+TestMusic::count-1)%TestMusic::count);}
bool MusicService::next(){return playTrack((TestMusic::current+1)%TestMusic::count);}
void MusicService::setShuffleEnabled(bool b){TestMusic::shuffle=b;}
bool MusicService::headphonesInserted() const{return false;}
uint16_t MusicService::vuLevel() const{return 0;}
const uint8_t* MusicService::spectrumBands() const{static uint8_t data[16]={};return data;}
