```
 uMRC  ┌──── ▄█▀▀▀▀▀█▄ ───┬───┬───┬─┐
 v099┌──── ▄██ ▄███ ▀▀█▄ ·────────┐ ┤
   ┌──── ▄█▀▀ ▐█▀ ▀ █ ██  ▄ ▄ ▄ · │ │
 ┌──── ▄█▀ ▄██▄▀█▄▄██ ██  █████ · │ ┤
┌─── ▄█▀ ▐██▀▀█▌ ▀▀▀ ▄█▀  ▐▄█▄▌   │ │
┌─ ▄█▀ ▄▄ ██▄████  ▄█▀  · ▐█╫█▌ cf│ ┤
 ▄▀▀ ▄█▀██▄▀██▌  ▄█▀  · · · · · · ┘ │
 █ ▐█▀██▄▀██▄▀ ▄█▀    Universal     ┘
▄▀  ██▄▀██ ▀ ▄█▀   Multi-Relay Chat  
▀ ▐█▄▀██ ▀ ▄█▀       Client Door     
▐█▄▀██▄▀ ▄█▀ .│┌────────────────────┐
 ▀███▀ ▄█▀ .││|│ ■ Win32            │
█▄ ▀ ▄█▀ .││|│││ ■ DOOR32.SYS       │
 ▀█▄█▀ .││|│││││ ■ SSL capable      │
 └ ▀ ──┴─┴─┴─┴─┴──────────────────── 
```

# uMRC

by Craig Hendricks  
codefenix@conchaos.synchro.net  
 telnet://conchaos.synchro.net  
  https://conchaos.synchro.net  



## What is uMRC?

uMRC lets you run Multi-Relay Chat on your BBS without having to install Mystic
BBS and Python. As long as your BBS is capable of running 32-bit Windows doors, 
then you and your users can participate in MRC.

<img width="637" height="472" alt="image" src="https://github.com/user-attachments/assets/4cd1acf1-4efc-48b7-87b0-f34185f9a240" />


It should be compatible with any DOOR32.SYS capable BBS such as EleBBS, WWIV, 
Synchronet, Mystic, and possibly others. It has been tested on Windows 7 and 
later, but Windows versions earlier than 7 have not been confirmed. 

If you use NetFoss to start up your DOS-based BBS, then the NFU utility 
bundled with NetFoss should run the uMRC Client. As of this writing, Renegade 
has been confirmed to run it.



## Features:

- Secure SSL MRC host connections 
- Automatic host reconnection & rejoin
- TAB username auto-completion
- Password masking (e.g.: `/identify ********`)
- Mention counter
- Built-in chat and mention history scrollback    
- Pipe code color support
- CTCP command support
- Sysop-editable 2-line ANSI status bar themes



## Files Included:

- **setup.exe**:        Setup utility
- **umrc-bridge.exe**:  MRC host connection program (multiplexer)
- **umrc-client.exe**:  MRC Client door
- **ODoors62.dll**:     OpenDoors door kit library    
- **libssl-43.dll**:    LibreSSL (OpenSSL) SSL library 
- **libcrypto-41.dll**: LibreSSL (OpenSSL) Cryptographic library 
- **\[screens\]**:      Subdirectory containing ANSI & text files
  + **intro.ans**:      Intro/main menu & status screen
  + **help.txt**:       Help file showing basic chat commands
  + **helpctcp.txt**:   Help file on CTCP command usage
- **\[themes\]**:       Subdirectory containing ANSI files
  + **\*.ans**:         ANSI theme files



## Install Instructions:

> If you have any existing MRC process currently running, stop it at 
> this time. The MRC host generally does not play well with concurrent 
> connections from the same BBS.

1. Extract all files to their own directory, keeping the structure 
   the same as shown above in the list of files included.
   
   Example:  c:\doors\umrc   
   

2. Run **setup.exe**, and press `1` to begin.

   For the first 3 prompts, you can simply press enter to accept the 
   default values:

    a. The **MRC host** address defaults to **mrc.bottomlessabyss.net**. If in 
       the future the MRC host address changes, it can be updated here.
   
    b. **SSL** is recommended for secure connections to the MRC host and 
       should be left enabled, but can optionally be disabled if needed.
      
    c. The **MRC host port number** defaults to 5001 if using SSL, 
       or 5000 if using a standard socket.


   The remaining prompts are about your BBS:
   
    d. BBS name \*
   
    e. BBS software (pick from a list or enter an option not listed)
   
    f. BBS website \* (include either http:// or https://)
   
    g. BBS telnet address & port \* (you don't need to include telnet://)
   
    h. BBS SSH address & port \* (you don't need to include ssh://)
   
    i. Sysop name \* (this is you!)
   
    j. Short description of your BBS \*
   
   
   Pipe color codes are allowed in options marked with an asterisk (\*)
   
   Supported pipe color codes:
   
   ```
    |01 ... dark blue
    |02 ... dark green
    |03 ... dark cyan
    |04 ... dark red
    |05 ... dark magenta
    |06 ... brown (dark yellow)
    |07 ... gray
    |08 ... dark gray (bright black)
    |09 ... bright blue
    |10 ... bright green
    |11 ... bright cyan
    |12 ... bright red
    |13 ... bright magenta
    |14 ... yellow
    |15 ... white
    |16 ... black background
    |17 ... blue background
    |18 ... green background
    |19 ... cyan background
    |20 ... red background
    |21 ... magenta background
    |22 ... brown background
    |23 ... gray background
   ```
   
   After you've entered your information, it will be displayed to you,
   with the option to re-enter everything, or quit. Your settings will 
   be saved to a file called mrc.cfg.
   
   
3. Run **umrc-bridge.exe**. This program is responsible for maintaining a 
   connection to the MRC host and passing chat traffic back and forth 
   between the host and your BBS. It must run continuously in order for 
   umrc-client.exe to work, so it's recommended to have this program 
   launch on system startup.
   
   When umrc-bridge starts up, it should say "Ready for clients" if
   successful.
   
   umrc-bridge takes an optional `-V` command line switch, which enables
   verbose logging. This can be used for troubleshooting purposes.
   
   
4. Set up a menu item to launch a 32-bit door on your BBS.

   The command line syntax is:

     `umrc-client -D c:\path\to\DOOR32.SYS`

   Optionally, include the `-SILENT` option to prevent the local Windows 
   GUI from popping up while the door is running.

     `umrc-client -D c:\path\to\DOOR32.SYS -SILENT`

   You can test it locally from the command line as well:

     `umrc-client -L`

   NOTE: It will only run as user number 1 (Sysop) in local mode.

At this point, you should be ready to launch the door and join chat.
    
<img width="632" height="469" alt="image" src="https://github.com/user-attachments/assets/91b3684a-028f-45a7-b817-4c30a7b86fe4" />
    

## uMRC Client Usage:   

When a user first launches the uMRC client door, they'll be given the option
to confirm the user name the door detected, or enter a custom one (in case the
BBS passed the user's real name to the door). After the user confirms their 
name, they'll be shown a brief list of their default settings with the option 
to change them.

1. **Display name**:

   The display name is the user's chatter name shown next to all their
   messages in chat.

   A default chatter name is formed with a randomized set of options: 
   chatter name color, prefix, and suffix.

2. **Default room**:

   This is the room the user will automatically join when they enter
   chat. If left blank, "lobby" will be the default room.
   
3. **Text color**:

   This is the default color used for all of the user's messages. The
   user may also use the left and right arrows to cycle through color
   options while in-chat.
   
4. **Chat sounds**:

   Console beeps can optionally be heard any time the user's name
   is mentioned in chat. These beeps are disabled by default. They 
   can be toggled on or off with the /sound command while in-chat,
   
5. **Join Message**:
   
   This is a short string that is sent to the chat room to announce
   that the user has entered the room. Pipe color codes are allowed.
   
6. **Exit Message**:
   
   This is a short string that is sent to the chat room to announce
   that the user has left the room. Pipe color codes are allowed.

7. **Theme**:

   This is a 2-line ANSI file in the "themes" subdirectory. The sysop
   can add/edit theme files as needed. Only the first 2 lines in the
   file are used. Anything after line #2 is ignored.
   
The user presses the `Q` key to save their options and quit to the 
main menu.


### Main Menu:

The main menu shows a short list of options and the current status 
of the MRC Host as reported by umrc-bridge.

Options:

`C` **Enter Chat**:       Places the user into chat, automatically joining 
                           their default room.       

`S` **Chatter Settings**: Lets the user modify their options.

`I` **Instructions**:     Shows a list of basic chat commands, as listed in 
                           the screens/help.ans file.
   
`Q` **Quit**:             Exits back to the BBS.
       
       
### MRC Stats, as reported by umrc-bridge:
    
- **State**:    ONLINE or OFFLINE. 60 seconds or less since last ping = ONLINE
- **Latency**:  Elapsed milliseconds between the last client message and the
                server's response.
- **BBSes**:    Number of BBSes currently connected to the MRC host.
- **Rooms**:    Current number of chat rooms.
- **Users**:    Total number of users in all chat room.
- **Activity**: `NUL`, `LOW`, `MED`, or `HI`, based on chatter activity.
    
    
### Basic Chat Usage:
    
Upon entering chat, the user will be greeted with the message of the day 
and other messages from the MRC server.

Anything the user types starting with a forward slash (/) will be treated
as a chat command. For example, `/help` for a list of commands.

Anything else the user types will be sent to the room as a chat message 
for others to read.

Basic room stats are shown near the bottom of the screen above the user's
text input, showing the current room, topic, user count, number of times
the user was mentioned, latency, and input buffer (max input: 140). These
stats update continuously throughout chat.

The door should let the user remain in chat up to the number of minutes
reported allowed by the BBS, and should not have an issue with any time 
spent idling.

The user types `/quit` or `/q` to exit chat and return to the main menu.



## Meetups!

Now that your BBS offers MRC, here's where the most action is!

### thE grAvY trAIn    

- Saturdays at 21h00 UK / 16h00 Eastern / 13h00 Pacific   
- Host: MeaTLoTioN   
- More info at: https://erb.pw
                                 
Type `/meetups` in chat for a current list of meetups.


    
## Known Issues & Limitations:

- Height and width are limited to 80 columns by 24 rows. OpenDoors has
  a default maximum row limit of 23, but umrc-client is hard-coded with
  this setting adjusted to 24. As far as I know, OpenDoors cannot 
  automatically detect the terminal's height and width.

- The font size of the local window cannot be adjusted. This seems to be
  an internal limitation of the OpenDoors kit.

- Masked input becomes unmasked when changing text colors with left and 
  right arrows. 

- umrc-bridge occasionally reports "partial packets" in verbose mode, 
  especially when using the `!ddial` command. Most partial packets are 
  either ignored or handled gracefully by the bridge when they occur (the 
  Syncrhonet mrc-connector service logs similar warnings for `!ddial`).



## Future Plans:

As of this writing, I don't plan on making many changes or adding a lot of
extra features. Bugs will be fixed, and changes will be made to support 
future changes to the MRC protocol, but that's about it.

Long-term, it may be worth re-writing the umrc-client door using another door
kit and/or other language which could be more easily ported to other platforms 
and used by other BBSes.



## Technical Notes:

uMRC is written in C, and was developed and compiled using Microsoft Visual 
Studio Community 2022.

uMRC makes extensive use of threading, both in umrc-bridge and umrc-client.
Separate threads are used for establishing connections to the MRC host,
listening for and handling client connections from local clients, and
handling user input while displaying incoming messages to the output window.

Secure SSL sockets are implemented using LibreSSL, a variant of OpenSSL. SSL
is used only from the umrc-bridge to the MRC host, while local umrc-client 
connections to the umrc-bridge are made using standard TCP/IP sockets, using 
the same port number as selected in the Setup program.

I chose OpenDoors because it's available, free, works well, and has many great 
functions built in for handling text movement on the screen, which uMRC uses 
extensively. Not everyone likes the local pop-up window that OpenDoors shows 
when running the door, but it can be hidden by using the `-SILENT` command line
switch.

You should NOT have ports 5000/5001 open on your firewall/router, since 
umrc-client makes OUTBOUND requests to the MRC host on ports 5000/5001. 
If you leave these ports open on your end, then you may occasionally see 
umrc-bridge log odd messages, caused by outside HTTP requests to your system.



## Thanks, Acknowledgements, and Links:

Thanks to **StackFault** at **The Bottomless Abyss BBS**, MRC HQ!

- https://bbs.bottomlessabyss.net
- telnet://bbs.bottomlessabyss.net:2023
- ssh://bbs.bottomlessabyss.net:2222

### uMRC Test Sites:

**StingRay** of A-**Net Online**

StingRay offers a wide array of BBSes and BBS-related services that all sysops
should avail themselves of. The main links here will get you on the path to 
everything he offers.

- https://a-net-online.lol/bbs/ 
- telnet://a-net-online.lol
- ssh://a-net-online.lol


**X-Bit** of **x-big.org**

xbit is a veteran sysop and league coordinator of the X-League (777) Inter-BBS 
network which re-launched in 2025. If you're interested in joining the network, 
check out these links below:

- https://x-bit.org 
- telnet://x-bit.org  
- ssh://x-bit.org:22222

xbit also contributed the "l33t" and "stars" themes!


**Exodus** of **The Titantic BBS**

Exodus is the network coordinator of MetroNet and maintains Renegade BBS. He was
instrumental in confirming uMRC could run on DOS-based systems like Renegade.

- https://www.rgbbs.info
- telnet://ttb.rgbbs.info


**Shurato** of **Shurato's Heavenly Sphere**

Shurato runs perhaps the most unique and customized instance of EleBBS anywhere,
and is always happy to try running new things on his board.
  
- https://shsbbs.net 
- telnet://shsbbs.net 
- for SSH, see: https://shsbbs.net/faq.html



## Enjoy!

See you in chat!


