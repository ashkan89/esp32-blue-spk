#include "physical_controls.h"
#include "pin_check.h"
#include "management.h"
#include "product.h"
#include "player_state.h"
#include "ui.h"
#include "alarm_clock.h"
#include <atomic>

namespace {
constexpr int controls[] = {PIN_ENCODER_A, PIN_ENCODER_B, PIN_PLAY_BUTTON};
constexpr int occupied[] = {PIN_MAP_I2S_BCLK,PIN_MAP_I2S_LRCK,PIN_MAP_I2S_DOUT,
  PIN_STATUS_LED,PIN_UI_BUTTON,PIN_OLED_SDA,PIN_OLED_SCL,PIN_LEDS,
  PIN_DF_TX,PIN_DF_RX,PIN_DF_BUSY,PIN_DF_IO1,PIN_DF_IO2,PIN_DF_ADKEY1,
  PIN_DF_ADKEY2,PIN_DF_LED,PIN_DF_USB_DETECT,PIN_BATTERY_SENSE,
  PIN_BATTERY_CHARGING,PIN_BATTERY_FULL};
constexpr bool pinsValid() {
  if ((PIN_ENCODER_A < 0) != (PIN_ENCODER_B < 0)) return false;
  for (unsigned i=0;i<3;++i) {
    const int p=controls[i]; if(p<0)continue;
    if(!PIN_OK_ANY(p)||PIN_IS_PSRAM(p)||p==0||p==2||p==5||p==12||p==15||p==1||p==3)return false;
    for(int used:occupied)if(p==used)return false;
    for(unsigned j=0;j<i;++j)if(p==controls[j])return false;
  }
  return true;
}
static_assert(pinsValid(), "Physical controls need distinct free non-strapping GPIOs; disable conflicting DFPlayer pins");
std::atomic<int> detents{0};
std::atomic<uint8_t> gesture{0};
void scan(void*) {
  static constexpr int8_t transition[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  uint8_t previous=0;int quarter=0;bool raw=false,stable=false;uint32_t changed=0,down=0;bool longSent=false;
  for(;;){
    const uint32_t now=millis();
    if(PIN_ENCODER_A>=0){
      const uint8_t current=(digitalRead(PIN_ENCODER_A)<<1)|digitalRead(PIN_ENCODER_B);
      if(current!=previous){quarter+=transition[(previous<<2)|current];previous=current;
        if(quarter>=4){detents.fetch_add(1);quarter=0;}else if(quarter<=-4){detents.fetch_sub(1);quarter=0;}}
    }
    if(PIN_PLAY_BUTTON>=0){
      const bool pressed=digitalRead(PIN_PLAY_BUTTON)==LOW;
      if(pressed!=raw){raw=pressed;changed=now;}
      if(now-changed>=25&&stable!=raw){stable=raw;
        if(stable){down=now;longSent=false;}else if(!longSent)gesture.store(1);
      }
      if(stable&&!longSent&&now-down>=1500){longSent=true;gesture.store(2);}
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
}
void physical_controls_begin(){
  if(PIN_ENCODER_A<0&&PIN_PLAY_BUTTON<0)return;
  for(int p:controls)if(p>=0)pinMode(p,PIN_IS_INPUT_ONLY(p)?INPUT:INPUT_PULLUP);
  if(xTaskCreate(scan,"controls",2048,nullptr,1,nullptr)!=pdPASS)product_event("controls-start-failed");
}
void physical_controls_loop(){
  const int delta=constrain(detents.exchange(0),-20,20);
  if(delta){PlayerInfo p;ps_snapshot(&p);management_media_action("volume",constrain((int)p.volume+delta*3,0,127));ui_wake();}
  const uint8_t action=gesture.exchange(0);
  if(action==1){AlarmStatus a;alarm_status(&a);if(a.state==ALARM_RINGING)alarm_snooze();else management_media_action("toggle",0);ui_wake();}
  if(action==2){product_pairing_window();ui_show_system_status(UI_STATUS_SUCCESS,"Guest pairing","Open for two minutes",-1,2000);}
}
