#include <Preferences.h>
#include <stdexcept>
#include <cstdio>
#include "src/services/WifiNetworkStore.h"

void check(bool value,const char* reason) { if(!value)throw std::runtime_error(reason); }
int main() {
  try {
    using Store=WifiNetworkStore;
    Store store;
    check(store.begin()&&store.count()==0,"empty first boot");
    TestPrefs::entries["ssid"]={'H','o','m','e'};
    TestPrefs::entries["pass"]={'s','e','c','r','e','t'};
    check(store.begin()&&store.count()==1,"legacy migration");
    check(!TestPrefs::entries.count("ssid")&&!TestPrefs::entries.count("pass"),"legacy secrets retained");
    char ssid[33],pass[65];
    check(store.credentials(0,ssid,sizeof(ssid),pass,sizeof(pass))&&!strcmp(pass,"secret"),"migrated password lost");
    for(int i=0;i<4;++i) { char name[16];snprintf(name,sizeof(name),"Network-%d",i);check(store.save(name,"pw",{}),"fill records"); }
    check(store.count()==5,"capacity");
    check(!store.save("Overflow","pw",{}),"implicit eviction");
    check(store.save("Home","new",{})&&store.count()==5&&store.find("Home")==0,"same SSID must update and move first");
    check(store.credentials(0,ssid,sizeof(ssid),pass,sizeof(pass))&&!strcmp(pass,"new"),"updated password lost");
    TestPrefs::failWrite=true;
    check(!store.save("Replacement","pw",{},4)&&store.find("Replacement")==-1&&store.count()==5,"failed commit changed records");
    check(!store.forget(0)&&store.find("Home")==0,"failed forget changed records");
    TestPrefs::failWrite=false;
    check(store.save("Replacement","pw",{},4)&&store.count()==5&&store.find("Replacement")==0,"explicit replacement");
    Store reloaded;check(reloaded.begin()&&reloaded.count()==5&&reloaded.find("Replacement")==0,"reboot load");
    check(!reloaded.save("   ","pw",{})&&!reloaded.save("bad\nname","pw",{}),"invalid SSID accepted");
    while(reloaded.count())check(reloaded.forget(0),"forget failed");
    check(store.begin()&&store.count()==0,"empty history resurrected legacy credentials");
    TestPrefs::entries["networks"][0]^=0x80;
    check(!store.begin()&&store.count()==0,"corrupt history accepted");
    puts("PASS: legacy migration, MRU/dedup, capacity/replacement, commit failure, reboot, forget, corrupt data");
    return 0;
  } catch(const std::exception& e) {fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
