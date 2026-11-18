import os
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import jolteon_menu as menu
os.environ['J18_ARDUINO_CLI']=str(Path(os.environ['LOCALAPPDATA'])/'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe')
os.environ['J18_OPENOCD']=r'C:\Program Files (x86)\OpenOCD\bin\openocd.exe'
sketch=ROOT/'.build/unix-menu-compile-fixture/Unix_Menu_Blinky'
engine=menu.Engine('linux')
for board in ('zero','jolteon','jolteon'):
    engine.action(sketch,board,'app','current',None,True,False,'stlink',False)
engine.probe(sketch,True,False,'stlink',False)
mac=menu.Engine('mac')
mac.probe(sketch,True,True,'stlink',False)
print('PASS native menu engine automatic profile changes/reuse and Linux/macOS probe configs; no hardware access.',flush=True)
