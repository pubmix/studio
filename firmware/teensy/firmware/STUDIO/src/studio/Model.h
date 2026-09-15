#pragma once
#include <array>
#include <cstdint>
#include <cmath>
namespace studio {
constexpr unsigned Lanes=4, Steps=16, SampleRate=44100, BlockSize=128;
constexpr const char* StemNames[]={"VOCALS","MELODY","BASS","RHYTHM"};
constexpr const char* StemFiles[]={"vocals.wav","melody.wav","bass.wav","rhythm.wav"};
inline float unit(float v){return std::isfinite(v)?(v<0?0:v>1?1:v):0;}
enum class Mode:uint8_t {Home,Dub,Jam,Daw,Files,Settings};
enum class Page:uint8_t {Performance,Browser,Design,Sequencer,Keyboard,Pads,Master,Recovery};
enum class Theme:uint8_t {Dark,Bright,Retro,Win,Aero,Analog,Terminal,Pixel};
enum class Setting:uint8_t {Audio,Midi,Storage,Display,Controls,System,About};
enum class Effect:uint8_t {DubEcho,DigitalDelay,RoomReverb,LowPass,HighPass,Drive,Count};
enum class SoundKind:uint8_t {Sine,Saw,Drum,Sample};
enum class Scale:uint8_t {Major,Minor,Pentatonic,Blues,Dorian,Mixolydian,Chromatic};
enum class Curve:uint8_t {Linear,Square,Root,Smooth};
struct Mapping {float low=0,high=1; Curve curve=Curve::Linear; float at(float x)const;};
struct Fx {Effect effect=Effect::DubEcho; bool enabled=false; float intensity=.5f; float timeSeconds=.18f; Mapping wet{0,.72f,Curve::Square},feedback{.08f,.91f,Curve::Smooth},drive{0,.25f,Curve::Square},tone{.9f,.3f,Curve::Linear},width{.2f,.85f,Curve::Linear};};
struct Step {bool on=false; uint8_t note=60; float velocity=.8f,gate=.75f;};
struct Pattern {std::array<Step,Steps> steps{};};
struct Sound {uint32_t asset=0; SoundKind kind=SoundKind::Sine; float macro=.5f,attack=.005f,release=.12f,cutoff=.8f,drive=0;};
struct LoopEvent {uint32_t tick=0; uint8_t note=60; float velocity=0; bool on=false;};
struct LiveLoop {std::array<LoopEvent,64> events{}; uint8_t count=0; uint32_t lengthTicks=384; bool recording=false,playing=false;};
struct Track {Sound sound{}; Pattern pattern{}; LiveLoop loop{}; float volume=.7f; bool armed=false;};
struct DubLane {Fx fx{}; float volume=.7f;};
struct Project {uint32_t version=1,id=1,stemSet=0; char name[48]="Untitled"; float bpm=92,master=.7f; uint8_t key=0,octave=4,countInBars=0; Scale scale=Scale::Minor; bool scaleLock=false,metronome=false; Theme theme=Theme::Dark; std::array<DubLane,4> dub{}; std::array<Track,4> tracks{}; std::array<uint32_t,4> slots{{1,2,3,4}};};
struct Session {Mode mode=Mode::Home; Mode musicMode=Mode::Dub; Page page=Page::Performance; uint8_t lane=0,browserIndex=0; bool dirty=false;};
struct Settings {uint32_t sampleRate=SampleRate,autosaveMs=30000; uint16_t bufferFrames=BlockSize; uint8_t midiChannel=1; bool usbMidi=true,externalClock=false,previewToMain=false,jamPhysicalMapping=false;};
struct Status {bool storageReady=false,audioReady=false,midiConnected=false,updateAvailable=false; uint64_t freeBytes=0; uint32_t underruns=0; const char* firmware="0.1.0-prototype";};
struct DawRegion {uint32_t asset=0; uint64_t startTick=0,lengthTicks=0; uint8_t track=0;};
}
