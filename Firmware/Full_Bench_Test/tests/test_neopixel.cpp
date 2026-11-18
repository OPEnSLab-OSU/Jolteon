// Exercise the actual Loom driver; only electrical/USB output is replaced.
#include <cassert>
#include <Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.cpp>
FakeSerial Serial;
FakeWatchdog Watchdog;
std::vector<PixelFrame> frames;
std::vector<int> begun,modes;
void pinMode(int pin,int mode) { assert(mode==OUTPUT); modes.push_back(pin); }
void Manager::registerModule(Module *) {}
int main() {
  Loom_Neopixel pixel(false,false,true,NEO_GRB);
  assert(!pixel.configure_port(3,16));
  assert(!pixel.configure_port(2,-1));
  assert(!pixel.configure_port(2,16,0));
  assert(!pixel.configure_port(2,16,257));
  assert(pixel.configure_port(2,13,2,64));
  assert(frames.empty() && begun.empty() && modes.empty());
  pixel.initialize();
  assert(begun==std::vector<int>{13} && modes==std::vector<int>{13});
  assert(frames.size()==1 && frames.back().pin==13 && frames.back().brightness==64);
  pixel.set_color(2,0,255,0,0,false);
  pixel.set_color(2,1,0,255,0,false);
  assert(frames.size()==1);
  assert(pixel.can_show(2) && !pixel.can_show(0) && !pixel.can_show(3));
  pixel.show(2);
  assert(frames.size()==2 && frames.back().colors[0]==0xff0000 && frames.back().colors[1]==0x00ff00);
  pixel.set_color(2,1,0,0,255); // Existing callers still transmit immediately.
  assert(frames.size()==3 && frames.back().colors[1]==0xff);
  pixel.set_color(2,2,1,1,1); pixel.set_color(3,0,1,1,1);
  pixel.set_color(0,0,1,1,1); pixel.show(0); pixel.show(3);
  assert(frames.size()==3);
  pixel.enable_pin(2,false); pixel.show(2); assert(!pixel.can_show(2));
  assert(frames.size()==3);
  pixel.enable_pin(2,true);
  assert(frames.back().pin==13 && modes==std::vector<int>({13,13}));
  // Defaults still initialize only A2, without touching A0/A1.
  begun.clear(); modes.clear(); frames.clear();
  Loom_Neopixel original; original.initialize();
  assert(begun==std::vector<int>{16} && modes==std::vector<int>{16});
  puts("PASS: Loom NeoPixel pin routing, staged frames, legacy immediate show and disabled analog ports");
}
