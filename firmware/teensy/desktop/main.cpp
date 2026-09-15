#include "Host.h"
#include "studio/App.h"
#include "studio/Ui.h"
#include <iostream>
#include <sstream>
using namespace studio;
int main(int argc,char**argv){HostAssets assets;for(unsigned i=1;i<=7;++i){std::vector<Stereo>s(SampleRate/4);for(unsigned n=0;n<s.size();++n){float x=.3f*std::sin(float(n)*(i==7?.04f:.02f*i))*std::exp(-float(n)/2000);s[n]={x,x};}assets.addMemory(i,std::move(s));}AudioEngine engine(assets);App app(engine);HostStorage storage("STUDIO-data");TextDisplay display;
 if(argc>1){StemSet set;std::string error;if(!assets.preparedFolder(argv[1],set,error)){std::cerr<<error<<"\n";return 1;}PreparedStemEngine prepared;prepared.submit(set,assets);app.openDub(prepared);}
 std::cout<<"STUDIO desktop simulator. Type help. Audio advances with render; no audio-device driver.\n";
 std::string line;while(std::cout<<"> "&&std::getline(std::cin,line)){std::istringstream in(line);std::string cmd;in>>cmd;if(cmd=="quit")break;if(cmd=="help")std::cout<<"home dub jam daw files settings | lane 1..4 | top | turn N | click | fader 0..1 | shot | play stop record | note N | off N | render SECONDS | save NAME | load NAME | undo redo | theme 0..7 | jam-controls | list | quit\n";
 if(cmd=="home")app.mode(Mode::Home);if(cmd=="dub")app.mode(Mode::Dub);if(cmd=="jam")app.mode(Mode::Jam);if(cmd=="daw")app.mode(Mode::Daw);if(cmd=="files")app.openFiles(FileContext::Action::Browse,0);if(cmd=="settings")app.mode(Mode::Settings);
 int n=0;float v=0;if(cmd=="lane"){in>>n;if(n>=1&&n<=4)app.session.lane=n-1;}
 for(auto pair:{std::pair<const char*,Control>{"top",Control::Top},{"turn",Control::EncoderTurn},{"click",Control::EncoderClick},{"fader",Control::Fader},{"shot",Control::Bottom},{"play",Control::Play},{"stop",Control::Stop},{"record",Control::Record}})if(cmd==pair.first){in>>v;app.control({pair.second,app.session.lane,v,0});}
 if(cmd=="note"||cmd=="off"){in>>n;if(n>=0&&n<=127)app.note(n,.8f,cmd=="note");}
 if(cmd=="render"){double seconds=0;in>>seconds;if(seconds>=0&&seconds<=60){double energy=0;for(uint64_t i=0;i<uint64_t(seconds*SampleRate);++i){auto f=engine.process(app.project,app.session.musicMode,app.settings);energy+=f.main.l*f.main.l+f.main.r*f.main.r;}std::cout<<"frames="<<engine.transport.frames<<" ticks="<<engine.transport.ticks()<<" energy="<<energy<<" peak="<<engine.master.meters.left<<" clip="<<engine.master.meters.clipping<<"\n";}}
 if(cmd=="save"||cmd=="load"){std::string name;in>>name;name="Projects/"+name+".studio";std::cout<<(cmd=="save"?app.save(storage,name.c_str()):app.load(storage,name.c_str()))<<"\n";}
 if(cmd=="undo")app.undo();if(cmd=="redo")app.redo();if(cmd=="theme"){in>>n;if(n>=0&&n<8)app.project.theme=Theme(n);}if(cmd=="jam-controls")app.settings.jamPhysicalMapping=true;if(cmd=="list")for(auto&f:storage.browse(""))std::cout<<f.path<<"\n";render(app,display);
 }return 0;}
