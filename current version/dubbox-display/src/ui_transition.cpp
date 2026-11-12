#include "ui_transition.h"
#include <Arduino.h>
#include "LCD.h"

namespace uitransition {
namespace {
// Official LT768x v4.2 datasheet, pp.64/173 and178:
// https://www.levetop.cn/uploadfiles/2023/05/LT768x_DS_V42_ENG.pdf
// REG91[3:0]=0xA: memory copy with opacity. REGB5=0..32:
// D=S0*(1-alpha/32)+S1*(alpha/32). All surfaces here are RGB565.
constexpr uint32_t kDurationMs=120, kFrameIntervalMs=20, kBusyTimeoutMs=200;
constexpr unsigned long kFrameBytes=720UL*1280UL*2UL;
static_assert(kOldFrame+kFrameBytes<=16UL*1024UL*1024UL,"snapshot exceeds display RAM");
State current=State::Idle;
unsigned long incoming=0, output=0;
uint32_t started=0, launchedAt=0;
uint8_t lastAlpha=0;
bool inFlight=false;
BlendObserver notifyBlend=nullptr;
bool busy(){return (ER5517.LCD_StatusRead()&0x08)!=0;}
bool fullFrame(unsigned long address){return address%kFrameBytes==0 && address<=16UL*1024UL*1024UL-kFrameBytes;}
void launch(uint8_t alpha){
  ER5517.BTE_S0_Color_16bpp();
  ER5517.BTE_S0_Memory_Start_Address(kOldFrame);
  ER5517.BTE_S0_Image_Width(720);
  ER5517.BTE_S0_Window_Start_XY(0,0);
  ER5517.BTE_S1_Color_16bpp();
  ER5517.BTE_S1_Memory_Start_Address(incoming);
  ER5517.BTE_S1_Image_Width(720);
  ER5517.BTE_S1_Window_Start_XY(0,0);
  ER5517.BTE_Destination_Color_16bpp();
  ER5517.BTE_Destination_Memory_Start_Address(output);
  ER5517.BTE_Destination_Image_Width(720);
  ER5517.BTE_Destination_Window_Start_XY(0,0);
  ER5517.BTE_Window_Size(720,1280);
  ER5517.LCD_CmdWrite(0xb5);
  ER5517.LCD_DataWrite(alpha);
  ER5517.BTE_Operation_Code(0x0a);
  ER5517.BTE_Enable();
  lastAlpha=alpha; launchedAt=millis(); inFlight=true;
  notifyBlend(kOldFrame,incoming,output,alpha);
}
}
bool begin(unsigned long newFrame,unsigned long destination,BlendObserver observer){
  if(current==State::Running || !observer || !fullFrame(newFrame) || !fullFrame(destination) ||
     newFrame==destination || newFrame==kOldFrame || destination==kOldFrame || busy())return false;
  incoming=newFrame;output=destination;notifyBlend=observer;
  started=millis();launchedAt=started;lastAlpha=0;inFlight=false;
  current=State::Running;
  return true;
}
State update(){
  if(current!=State::Running)return current;
  const uint32_t now=millis();
  if(inFlight){
    if(busy()){
      if(now-launchedAt>kBusyTimeoutMs)current=State::Failed;
      return current;
    }
    inFlight=false;
    if(lastAlpha==32){current=State::Complete;return current;}
  }
  if(now-launchedAt<kFrameIntervalMs)return current;
  const uint32_t elapsed=now-started;
  const uint8_t alpha=elapsed>=kDurationMs?32:static_cast<uint8_t>(elapsed*32/kDurationMs);
  if(alpha>lastAlpha)launch(alpha);
  return current;
}
State state(){return current;}
bool hardwareBusy(){return busy();}
}
