#include "theme.h"
#include <Preferences.h>
#include <cassert>
#include <cstdio>
int main() {
 using namespace theme;
 Preferences::stored=254; begin(); assert(current()==Id::Chalk);
 for (Id id : {Id::Petal, Id::Graphite, Id::Chalk}) {
   assert(select(id)); assert(current()==id); begin(); assert(current()==id);
   auto n=Preferences::writes; assert(select(id)); assert(Preferences::writes==n);
 }
 Preferences::writeOK=false; assert(!select(Id::Graphite)); assert(current()==Id::Chalk);
 Preferences::writeOK=true; Preferences::available=false;
 assert(!select(Id::Petal)); assert(current()==Id::Chalk);
 begin(); assert(current()==Id::Chalk);
 assert(!select(static_cast<Id>(10))); assert(current()==Id::Chalk);
 puts("PASS: three themes, persisted reload, invalid IDs, no-op writes, failed open/save rollback");
}
