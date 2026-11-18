"""Rebuild the schematic-derived vector wiring diagram and PNG preview."""
from pathlib import Path
import hashlib
import json
from xml.sax.saxutils import escape

OUT = Path(__file__).resolve().parent
colors = {'power':'#13704e','analog':'#087f8c','control':'#a45510','spi':'#6c40a2','i2c':'#2564ac','uart':'#b3383c','unused':'#657386'}
# Header pad numbers are the symbol pin numbers, NOT Arduino GPIO numbers.
left = [
 ('RESET','RESET','1','GND pushbutton','Momentary reset; do not use as a GPIO','control'),
 ('3V','power','2','No connection drawn','Carrier +3V3 comes from its own U3 regulator','unused'),
 ('AREF','PA03','3','AREF net only','No external reference source shown','unused'),
 ('GND','power','4','Common GND / JP4','All test instruments and supplies share ground','power'),
 ('A0 / D14','PA02','5','A0 net','Analog input; no sensor endpoint shown','analog'),
 ('A1 / D15','PB08','6','A1 net','Analog input; no sensor endpoint shown','analog'),
 ('A2 / D16','PB09','7','A2 net','Analog input; no sensor endpoint shown','analog'),
 ('A3 / D17','PA04','8','12V_EN -> U5 EN','HIGH enables sensor 12V boost; LOW disables','control'),
 ('A4 / D18','PA05','9','LTE_RST -> Q2 gate','HIGH pulls modem RESET_N LOW; idle LOW','control'),
 ('A5 / D19','PB02','10','4GONOFF -> Q1 gate','HIGH pulls modem PWR_ON LOW; idle LOW','control'),
 ('SCK / D24','PB11','11','CLK -> SD CLK / SPI1 pin 3','Feather SERCOM4 hardware clock','spi'),
 ('MOSI / D23','PB10','12','DI -> SD CMD / SPI1 pin 5','Host output, peripheral input','spi'),
 ('MISO / D22','PA12','13','DO -> SD DAT0 / SPI1 pin 6','Host input, peripheral output','spi'),
 ('RX / D0','PA11','14','0RX <- U4 B2/A2 <- SARA RXD','SARA pin 13 is OUTPUT (DCE naming)','uart'),
 ('TX / D1','PA10','15','1TX -> U4 B1/A1 -> SARA TXD','SARA pin 12 is INPUT (DCE naming)','uart'),
 ('NC','none','16','DFU net on NC symbol pad','Standard Feather has no GPIO on this pad','unused')]
right = [
 ('BAT','power','28','VBAT_FEATHER','From BQ_OUT through D2; also feeds U3','power'),
 ('EN','power','27','EN3V -> TP18','Feather regulator shutdown input; never GPIO','unused'),
 ('USB','power','26','5VUSB -> D3 -> BQ_IN','USB supply node; separate from boosted 5VOUT','power'),
 ('D13','PA17','25','Net "13" only','No carrier load shown; no automatic heartbeat','unused'),
 ('D12','PA19','24','RTC_INT -> DS3231 INT/SQW','Active LOW; firmware enables input pull-up','control'),
 ('D11','PA16','23','CS_SD -> microSD pin 2','LOW selects; R17 pull-up to switched 3VRAIL','spi'),
 ('D10','PA18','22','SDI_EN -> U7 DIR','HIGH: A-to-B TX; LOW: B-to-A RX','control'),
 ('D9','PA07','21','LOW_SDI12 -> U7 A','SDI-12 data; not available for analog sampling','control'),
 ('D6','PA20','20','Q4 + Q5 -> rail switches','HIGH enables BOTH 5VRAIL and HIGHVRAIL','power'),
 ('D5','PA15','19','PMOS2 gate -> 3VRAIL','LOW enables 3VRAIL; HIGH disables','power'),
 ('SCL / D21','PA23','18','SCL -> RTC + TCA9548A','R10 pull-up to switched 3VRAIL','i2c'),
 ('SDA / D20','PA22','17','SDA -> RTC + TCA9548A','R9 pull-up to switched 3VRAIL','i2c')]
parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1510" viewBox="0 0 1600 1510">',
 '<rect width="1600" height="1510" fill="#f4f7fb"/>',
 '<style>text{font-family:Arial,sans-serif} .title{font-size:32px;font-weight:bold;fill:#172b46} .small{font-size:15px;fill:#526176} .label{font-size:17px;font-weight:bold} .detail{font-size:13px;fill:#3e4f65} .pin{font-size:19px;font-weight:bold;fill:#172b46}</style>']
def text(x,y,s,cls='small',color=None):
    parts.append(f'<text x="{x}" y="{y}" class="{cls}"'+(f' fill="{color}"' if color else '')+'>'+escape(s)+'</text>')
text(45,58,'Jolteon Rewire | Feather M0 wiring','title')
text(45,91,'Schematic export: October 30, 2026, 09:32 | Top view, USB at top | Arduino labels and SAMD21G18 ports')
text(45,121,'Header-pad numbers below match the PDF symbol. They are NOT Arduino GPIO numbers.')
for i,(name,color) in enumerate(colors.items()):
    x=45+i*208; parts.append(f'<rect x="{x}" y="150" width="14" height="14" rx="3" fill="{color}"/>'); text(x+23,163,name.upper())
parts.append('<rect x="555" y="220" width="490" height="950" rx="28" fill="#fff" stroke="#c5d1df" stroke-width="3"/>')
parts.append('<rect x="745" y="220" width="110" height="60" rx="10" fill="#dce5ee" stroke="#8798ae"/>')
text(769,256,'USB','pin'); text(671,310,'FEATHER M0','title')
for side,rows in [('left',left),('right',right)]:
    for i,(pin,port,pad,label,detail,group) in enumerate(rows):
        isleft=side=='left'; y=350+(i if isleft else i+4)*50; color=colors[group]
        bx=45 if isleft else 1095; px=575 if isleft else 1019
        parts.append(f'<rect x="{bx}" y="{y-23}" width="460" height="46" rx="7" fill="#fff" stroke="{color}" stroke-opacity="0.3"/>')
        parts.append(f'<path d="M {505 if isleft else 1045} {y} H {555 if isleft else 1095}" stroke="{color}" stroke-width="3"/>')
        parts.append(f'<circle cx="{px}" cy="{y}" r="7" fill="{color}"/>')
        text(bx+12,y-4,label,'label',color); text(bx+12,y+14,detail,'detail')
        tx=595 if isleft else 829
        text(tx,y-2,pin,'pin'); text(tx,y+17,f'{port} | pad {pad}','detail')
text(590,1187,'UART labels follow u-blox DCE input/output definitions.','detail')
parts.append('<rect x="45" y="1220" width="1510" height="240" rx="14" fill="#fff" stroke="#c5d1df"/>')
text(66,1252,'POWER RAIL REFERENCE','label',colors['power'])
text(66,1281,'+3V3, 5VOUT and modem VCC_3.8V stay powered when D5/D6 turn the switched rails off.')
text(66,1308,'Optional probe points: +3V3, 5VOUT, 3VRAIL, 5VRAIL, TP20 modem supply and J_SDI12 pin 1 (12V).')
text(66,1335,'3V|5V probing header: pin 1 = 3VRAIL, pin 2 = 5VRAIL. J_SDI12: 1 = 12V, 2 = DATA, 3 = GND.')
text(66,1362,'UART function: Feather TX -> translator -> SARA TXD (input); SARA RXD (output) -> translator -> Feather RX.')
text(66,1389,'Mux: A0 tied HIGH, A1/A2 pulled LOW => 0x71; channels 0-3 gas, 4 pressure, 5-7 terminal I2C.')
text(66,1416,'SPI1 connector: 1 = 5VOUT, 2 = +3V3, 3 = CLK, 4 = GND, 5 = DI/MOSI, 6 = DO/MISO.')
text(66,1443,'A0-A2 accept only 0..3.3V. D9 is SDI data. Do not connect 5V/12V or tie the independent 3V supplies together.')
text(45,1490,'Source: docs/Jolteon_Rewire.pdf + installed Loomified Feather M0 variant. Schematic-derived; physical continuity untested.')
parts.append('</svg>')
(OUT/'Feather_M0_Wiring.svg').write_text('\n'.join(parts)+'\n',encoding='utf-8',newline='\n')
manifest={'source':'docs/Jolteon_Rewire.pdf','source_sha256':hashlib.sha256((OUT/'Jolteon_Rewire.pdf').read_bytes()).hexdigest(),
          'hardware_tested':False,'view':'top, USB at top','left':left,'right':right}
(OUT/'WIRING_SOURCE.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
print(OUT/'Feather_M0_Wiring.svg')
