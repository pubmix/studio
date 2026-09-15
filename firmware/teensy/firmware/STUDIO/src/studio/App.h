#pragma once
#include "AudioEngine.h"
namespace studio {
struct FileContext {enum class Action:uint8_t{Browse,LoadTrack,AssignSlot,Preview};Action action=Action::Browse;uint8_t target=0;Mode returnMode=Mode::Home;};
// Snapshot history is bounded, foreground-only; test default depth 8.
class History {std::array<Project,8> undo_{},redo_{};unsigned u_=0,r_=0;static void push(std::array<Project,8>&,unsigned&,const Project&);public:void checkpoint(const Project&p){push(undo_,u_,p);r_=0;}bool undo(Project&);bool redo(Project&);};
class App {
 AudioEngine&audio_;
public:
 Project project{};Session session{};Settings settings{};Status status{};History history;FileContext files{};
 explicit App(AudioEngine&a):audio_(a){}
 void mode(Mode);void control(const ControlEvent&);void edit(){history.checkpoint(project);session.dirty=true;}
 bool undo(){bool ok=history.undo(project);session.dirty|=ok;return ok;}bool redo(){bool ok=history.redo(project);session.dirty|=ok;return ok;}
 bool openDub(IStemEngine&);void openFiles(FileContext::Action,uint8_t);bool chooseAsset(uint32_t,SoundKind);
 void midi(uint8_t,uint8_t,uint8_t);void note(uint8_t,float,bool);
 void selectEffect(Effect);void designFx(const Fx&);void designSound(const Sound&);void setStep(unsigned,const Step&);
 bool save(IStorage&,const char*);bool load(IStorage&,const char*);bool autosave(IStorage&,uint64_t ms);bool recover(IStorage&);
private:uint64_t lastAutosave_=0;
};
}
