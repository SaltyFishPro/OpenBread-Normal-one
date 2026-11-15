#include "../../src/services/MusicLyrics.h"
#include <FS.h>
#include <esp_heap_caps.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
int main() {
  TestFs::root="tmp/lrc-test";
  MusicLyrics lyrics;
  lyrics.begin("/missing.mp3"); assert(lyrics.state()==MusicLyrics::State::Empty && !TestFs::handles);
  lyrics.begin("/song.mp3");
  assert(lyrics.state()==MusicLyrics::State::Loading);
  while(lyrics.state()==MusicLyrics::State::Loading) lyrics.tick();
  assert(lyrics.state()==MusicLyrics::State::Ready && lyrics.count()==4 && !TestFs::handles);
  assert(lyrics.current(499)==-1 && lyrics.current(500)==0);
  assert(std::string(lyrics.line(0))=="开始" && std::string(lyrics.line(1))=="第二行");
  assert(lyrics.current(1000)==1 && lyrics.current(4000)==3);
  assert(std::string(lyrics.line(2))=="重复" && std::string(lyrics.line(3))=="重复");
  lyrics.begin("/other.mp3"); assert(lyrics.state()==MusicLyrics::State::Loading);
  lyrics.end(); assert(!TestFs::handles && !TestPsram::used);
  TestPsram::failAllocation=true; lyrics.begin("/song.mp3");
  assert(lyrics.state()==MusicLyrics::State::Error && !TestFs::handles && !TestPsram::used);
  std::puts("PASS: LRC incremental load, sorted timestamps, multiple tags, missing and cancel/PSRAM cleanup");
}
