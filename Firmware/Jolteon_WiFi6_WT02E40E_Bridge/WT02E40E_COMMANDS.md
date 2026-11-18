# WT02E40E command reference

The native command grammar is available over BLE command write, UART/RTT shell, and Wi-Fi UDP command packets. Use the `wt` prefix in UART/RTT shell. The BLE and UDP command parsers accept commands with or without the `wt` prefix. The UART also supports the optional AT mode below.

BLE command responses and BLE TX notifications use FIFO flush queues. Short messages are sent as one queued notification. Longer messages are framed into `~S/~C/~E` chunks and each frame is queued in order. The web console also queues outgoing BLE commands, Wi-Fi command UDP packets, and raw UDP sends so scan item pulls and bridge tests do not overlap.

## Build target

```txt
nrf7002dk/nrf5340/cpuapp
```

Build with sysbuild:

```powershell
west build -b nrf7002dk/nrf5340/cpuapp . --sysbuild --pristine
west flash -d build
```

## Response and bridge streams

Command responses and bridge payloads are kept under the 255-byte app budget for normal replies. When a response or payload does not fit in one BLE notification payload, it uses framed stream messages. BLE command responses stream on the command response characteristic, BLE TX/bridge payloads stream on the TX notify characteristic, and Wi-Fi UDP streams use the same frame strings as separate UDP packets:

```txt
~S001
~C001000<payload>
~C001001<payload>
~E001002
```

The web console reassembles command response, BLE TX, and UDP streams before logging or parsing them. Request IDs are preserved inside reassembled command response payloads.

## UART AT compatibility

The same firmware supports ATWINC1500 UART, ESP-AT, and the native `wt` shell at 115200 baud, 8N1, with no hardware flow control. After entering AT mode, the firmware recognizes distinct commands from either AT dialect. The startup method selects the reply to the shared bare `AT` command. Send `ATO` to return to the native UART shell. RTT, BLE, and Wi-Fi UDP continue using the native command grammar.

### ATWINC1500 UART

Send these 13 **binary bytes** after the board starts. The packet needs no line ending or preceding text command.

```text
A5 01 F2 00 00 0C 00 0C 00 61 2F 52 EF
```

Wait for the line beginning with `+OK+AT=AT:` before sending a command. The startup response contains WT02E40E identification. Subsequent bare `AT` replies begin with `+OK+AT=Ver:`. WINC command names are case-insensitive. Send text commands with CR/LF, using comma-delimited fields without ESP-style quotes.

| Commands | Supported operation |
| --- | --- |
| `AT`, `AT+VER`, `AT+LIST`, `AT+ECHO=0`, `AT+ECHO=1` | Check the interface, show supported syntax, or set line echo. |
| `AT+SCAN`, `AT+SCAN=6` | Read `+EVT+SCAN` records followed by `+OK+SCAN`. A supplied channel selects a 2.4 GHz channel. |
| `AT+DHCP=1`, `AT+CONN=MyNetwork,2,MyPassword`, `AT+CONN=OpenNetwork,1` | Join using DHCP. Wait for `+OK+CONN` and `+EVT+DHCP`. Allow up to 35 seconds. |
| `AT+MODE`, `AT+DISMODE`, `AT+GETIP`, `AT+GETMAC`, `AT+RSSI`, `AT+RESDNS=example.com` | Read station state, disconnect, or query network information. |
| `AT+TCPCLI=192.168.1.10,5000`, `AT+UDP=5000`, `AT+UDPCFG`, `AT+UDPCB`, `AT+CLOSE`, `AT+CLOSEALL` | Open and manage IPv4 TCP clients and UDP sockets. |

Use the hexadecimal socket handle returned when the socket opens. The following example sends five bytes on socket 0. Here, `01` values denote literal bytes and the characters between them are ASCII.

```text
01 S00005HELLO 01 E
```

Omit the separating spaces and line ending on the wire. Send frames carry up to 1,400 payload bytes, including binary bytes. A `U` send frame also takes a one-time UDP destination as `<SOH>U<socket><length>,<ip>,<port>,<payload><SOH>E`. Automatic receive emits `+EVT+RECV` with binary data ending in `0x17`. Select manual receive with `AT+TCPCLI=<ip>,<port>,0,0` or `AT+UDP=<local port>,,,0`, then request a packet with `<SOH>R<socket><SOH>E`. Wait for command results before sending the next request.

The implemented subset covers UART station bring-up and plain TCP/UDP. SPI/WiFi101, TLS, TCP listeners, HTTP commands, provisioning, and WINC-specific firmware functions are outside this subset. Host libraries that require an Atmel chip ID or firmware version need adjustment. [Atmel ATWINC1500 AT Command Reference](https://forum.microchip.com/sfc/servlet.shepherd/document/download/0693l00000VtuVtAAJ)

### ESP-AT

Send `AT` followed by CR/LF at the normal 115200-baud UART shell to enter AT mode. Wait for the `OK` line before sending the next command. The shell can echo that first `AT`. After entry, responses use CR/LF and end in `OK` or `ERROR`, with command echo off by default. Send `ATO` and wait for `OK` to return to the `wt` shell. A reboot also returns to the shell. Commands are uppercase and should be sent one line at a time.

| Command | Supported operation |
| --- | --- |
| `AT`, `ATE0`, `ATE1`, `AT+GMR`, `AT+RST` | Check the interface, control echo, report WT firmware version, or reboot. |
| `AT+CWMODE?`, `AT+CWMODE=0`, `AT+CWMODE=1` | Query, turn off, or enable Wi-Fi station mode. |
| `AT+CWLAP` | List scan results as `+CWLAP:(security,"ssid",rssi,"bssid",channel)`, up to the bridge's scan limit. |
| `AT+CWJAP="ssid","password"` | Join an open network (empty password) or WPA2-Personal network once, without saving credentials. Success requires an IPv4 address. Allow up to 35 seconds for setup and connection. |
| `AT+CWJAP?`, `AT+CWSTATE?`, `AT+CWQAP` | Read the current AP or station state, or disconnect while leaving station mode enabled. |
| `AT+CIFSR`, `AT+CIPSTAMAC?` | Read station IPv4 and MAC, or MAC alone. |
| `AT+BLEINIT?`, `AT+BLEINIT=2`, `AT+BLEINIT=0` | Read, enable, or stop the bridge's BLE peripheral service. |
| `AT+BLENAME?`, `AT+BLENAME="name"` | Read or change the BLE name. The legacy scan response fits up to 29 bytes of name. |
| `AT+BLEADVSTART`, `AT+BLEADVSTOP`, `AT+BLEADDR?` | Control connectable advertising or read the public BLE address, when available. Stopping advertising leaves an existing connection active. |

For quoted values, escape a literal quote, backslash or comma as `\"`, `\\` or `\,`. Echo is optional. Keep it off when entering Wi-Fi credentials. The AT interface shares the bridge's live radio state. RTT, BLE, and Wi-Fi UDP still use the native grammar. Existing bridge traffic can also generate unsolicited UART lines, so wait for the final result line.

This is a subset of [ESP-AT Wi-Fi](https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/Wi-Fi_AT_Commands.html) and [ESP-AT BLE](https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/BLE_AT_Commands.html). Unsupported commands and forms return `ERROR`. Wi-Fi SoftAP, socket and transparent-data modes, BLE central and generic GATT commands, and BLE 5 extended advertising are outside the subset. BLE clients still use the WT service and characteristics.

## Commands

```txt
wt status
wt status json
wt id
wt version
wt config
wt config json
wt config save
wt config reset
wt boot status
wt boot mode idle|ble|wifi|both
wt name get
wt name set <ble-name>
wt mode idle [delay]
wt mode ble [delay]
wt mode wifi [delay]
wt mode both [delay]
wt ble on
wt ble off [delay]
wt ble status [json]
wt ble name
wt ble name get
wt ble name set <ble-name>
wt wifi on
wt wifi off [delay]
wt wifi status [json]
wt wifi reconnect
wt wifi scan
wt wifi scan json
wt wifi scan full json
wt wifi scan last [json]
wt wifi scan item <index> [json]
wt wifi scan connect <index> <password> [wpa2|auto|wpa3]
wt wifi scan open <index>
wt wifi scan clear
wt wifi cred list
wt wifi cred set <ssid> <password> [wpa2|auto|wpa3]
wt wifi cred add <ssid> <password> [wpa2|auto|wpa3]
wt wifi cred open <ssid>
wt wifi cred forget <ssid>
wt wifi cred clear
wt wifi cmd status
wt wifi cmd on
wt wifi cmd off
wt wifi cmd port [port]
wt discovery status
wt discovery on
wt discovery off
wt bridge status
wt bridge target <ipv4> <port>
wt bridge ble on|off
wt bridge uart on|off
wt bridge wifi on|off
wt bridge all on|off
wt bridge send <message>
wt led status
wt led test [all|ble|wifi|activity|alert]
wt led pulse [all|ble|wifi|activity|alert]
wt ping
wt ping ble
wt ping wifi <ipv4> <port>
wt tx ble <message>
wt tx uart <message>
wt tx wifi <ipv4> <port> <message>
wt tx both <ipv4> <port> <message>
wt fw status
wt fw reboot [delay]
wt fw reboot bootloader [delay]
```

## `tx both`: forward to the other transports

`both` (alias `all`) excludes the transport carrying the command:

| Command arrives over | Payload destinations |
|---|---|
| UART (`wt tx both ...`) | BLE TX notification and Wi-Fi UDP |
| BLE (`tx both ...`) | UART and Wi-Fi UDP |
| Wi-Fi (`tx both ...`) | UART and BLE TX notification |

Each destination is attempted independently. BLE requires a connected client
subscribed to TX notifications. Wi-Fi requires an IPv4 address and a destination
IP/port. Use `0.0.0.0 0` to leave the Wi-Fi destination unset. The response reports
`source`, `unavailable`, `notify_off`, `no_target`, `queued`, or `failed` per
destination, plus `accepted`. `ok` means at least one destination accepted the
payload locally; it does not prove delivery to a remote client. Command responses
still return to their caller.

The response also includes `tag`, a bounded first word (letters, digits, `#`,
`_`, `-`, `.`) of the payload, or `-` when empty. Senders can use a unique leading
token to correlate retries; the payload is still transmitted unchanged.

```txt
wt tx both 0.0.0.0 0 "BLE-only fallback from the SAMD21"
wt tx both 192.168.1.50 5000 "BLE and Wi-Fi from the SAMD21"
```

From BLE, `tx uart "j18 all"` sends a command to the SAMD21 verification sketch.
`tx both 192.168.1.50 5000 "j18 summary"` also forwards that text to Wi-Fi.
The target IP/port are ignored for Wi-Fi-origin `tx both`, since Wi-Fi is the source.

## Quoted arguments

Use quotes for SSIDs, passwords, names, and payloads with spaces.

```txt
wt wifi cred set "My Home WiFi" "correct horse battery staple" wpa2
wt wifi cred add "Lab WiFi" "password with spaces" auto
wt wifi cred open "Coffee Shop WiFi"
wt wifi cred forget "My Home WiFi"
wt tx ble "hello with spaces"
wt tx uart "quote test: \"hello\""
wt tx wifi 192.168.1.50 5000 "payload with spaces"
```

Backslash escapes are supported inside quoted BLE and UDP command arguments for quotes, backslashes, tabs, carriage returns, and newlines.


## Wi-Fi power model

```txt
wt wifi on         # prepare/enable Wi-Fi interface, no automatic association
wt wifi reconnect  # connect/reconnect using stored credentials
wt wifi off        # turn Wi-Fi request off
```

`wt wifi scan json` can run from BLE-only boot. It temporarily powers Wi-Fi if needed, scans, then turns Wi-Fi back off unless Wi-Fi was already explicitly on.

## Wi-Fi scan

BLE-safe scan. The scan path waits for Wi-Fi radio readiness and retries briefly if the radio is busy. When Wi-Fi is off, scan uses temporary radio power and turns it back off after the scan. Use `wt wifi reconnect` to associate with stored credentials.

```txt
wt wifi scan json
wt wifi scan item 1 json
wt wifi scan item 2 json
```

Full streamed scan:

```txt
wt wifi scan full json
```

Connect from a scanned AP:

```txt
wt wifi scan connect 1 "wifi password" wpa2
wt wifi status
```

Open AP by scan index:

```txt
wt wifi scan open 1
wt wifi status
```

## LED states

```txt
LED0 solid         BLE client connected
LED0 short off dip  BLE command response or TX notify queued to host
LED0 medium blink  BLE advertising
LED0 heartbeat     BLE ready, idle, not connected
LED0 slow blink    BLE requested before ready
LED1 solid         Wi-Fi associated and IPv4 bound
LED1 fast blink    Wi-Fi scan running
LED1 double pulse  Wi-Fi associated, waiting for IPv4
LED1 slow blink    Wi-Fi requested or associating
LED1 off           Wi-Fi not requested
LED2 pulse         Bridge, UDP TX, discovery, or command activity when led2 exists
LED3 pulse         Alert/error activity when led3 exists
```

Indicator commands:

```txt
wt led status
wt led test all
wt led pulse ble
wt led pulse wifi
wt led pulse activity
wt led pulse alert
```


## Demo flows

BLE sanity:

```txt
wt status
wt ble status
wt config
```

UART to BLE TX:

```txt
wt tx ble "hello from uart"
```

BLE to UART:

```txt
tx uart "hello from ble"
```

BLE-safe scan and item pull:

```txt
wifi scan json
wifi scan last json
wifi scan item 1 json
wifi scan item 2 json
wifi scan connect 1 "wifi password" wpa2
wifi status
```

Full streamed scan:

```txt
wifi scan full json
```

Wi-Fi command listener:

```txt
mode both
wifi reconnect
wifi cmd on
wifi status
```

Wi-Fi UDP command to BLE TX:

```txt
tx ble "hello from wifi command path"
```

Bridge all outputs:

```txt
bridge target 192.168.1.50 5000
bridge all on
bridge send "hello across enabled bridge outputs"
```

Safe delayed cutoff:

```txt
ble off 5s
wifi off 5s
mode ble 5s
```

## BLE characteristics

```txt
Service UUID:             7f1c0001-2b5a-4f2d-9a31-d6a5f4e040e1
TX notify UUID:           7f1c0002-2b5a-4f2d-9a31-d6a5f4e040e1
Status read/notify UUID:  7f1c0003-2b5a-4f2d-9a31-d6a5f4e040e1
Command write UUID:       7f1c0004-2b5a-4f2d-9a31-d6a5f4e040e1
Command response UUID:    7f1c0005-2b5a-4f2d-9a31-d6a5f4e040e1
```

## Wi-Fi UDP command path

```txt
Default board command port: 5001
Node/webapp receive port:  5000
```

Enable and check the UDP command listener:

```txt
wt wifi cmd on
wt wifi cmd status
```

Change the board command port:

```txt
wt wifi cmd port 5002
```
