```
       ┌──── ▄█▀▀▀▀▀█▄ ───┬───┬───┬─┐
     ┌──── ▄██ ▄███ ▀▀█▄ ·────────┐ ┤
   ┌──── ▄█▀▀ ▐█▀ ▀ █ ██  ▄ ▄ ▄ · │ │
 ┌──── ▄█▀ ▄██▄▀█▄▄██ ██  █████ · │ ┤
┌─── ▄█▀ ▐██▀▀█▌ ▀▀▀ ▄█▀  ▐▄█▄▌   │ │
┌─ ▄█▀ ▄▄ ██▄████  ▄█▀  · ▐█╫█▌ cf│ ┤
 ▄▀▀ ▄█▀██▄▀██▌  ▄█▀  · · · · · · ┘ │
 █ ▐█▀██▄▀██▄▀ ▄█▀    Universal     ┘
▄▀  ██▄▀██ ▀ ▄█▀   Multi-Relay Chat  
▀ ▐█▄▀██ ▀ ▄█▀       Client Door     
▐█▄▀██▄▀ ▄█▀ .│┌────────────────────┐
 ▀███▀ ▄█▀ .││|│ ■ Win32 & Linux    │
█▄ ▀ ▄█▀ .││|│││ ■ DOOR32.SYS       │
 ▀█▄█▀ .││|│││││ ■ SSL capable      │
 └ ▀ ──┴─┴─┴─┴─┴──────────────────── 
```



## What is uMRC?

uMRC lets you run Multi-Relay Chat on your BBS without having to install and
maintain a Mystic BBS instance and Python. As long as your BBS is capable of
running 32-bit doors, then you and your users can participate in MRC.

<img width="637" height="472" alt="image" src="https://github.com/user-attachments/assets/4cd1acf1-4efc-48b7-87b0-f34185f9a240" />


It should be compatible with any DOOR32.SYS capable BBS such as EleBBS, WWIV, 
Synchronet, Mystic, and possibly others. It has been tested on Windows 7 and 
later, but Windows versions earlier than 7 have not been confirmed. The Linux
build has been tested on Ubuntu 22.04 and should run on similar.

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



## Common Files Included:

- **setup**:            Setup utility
- **umrc-bridge**:      MRC host connection program (multiplexer)
- **umrc-client**:      MRC Client door
- **\[screens\]**:      Subdirectory containing ANSI & text files
  + **intro.ans**:      Intro/main menu & status screen
  + **help.txt**:       Help file showing basic chat commands
  + **helpctcp.txt**:   Help file on CTCP command usage
  + **helptwit.txt**:   Help file on twit filter management
- **\[themes\]**:       Subdirectory containing ANSI files
  + **\*.ans**:         ANSI theme files

Refer to the readme.txt file inside the archive for files specific to 
your platform.


## Install Instructions:

> If you have any existing MRC process currently running, stop it at 
> this time. The MRC host generally does not play well with concurrent 
> connections from the same BBS.

If you're on Windows, skip this paragraph and proceed to step #1.
On Debian-based systems like Ubuntu, run the build.sh script to compile from source.
First run `sudo apt install gcc` if you don't already have gcc installed,
then run `sudo apt install libssl-dev` since the bridge makes use
of LibreSSL, then `./build.sh` should complete successfully. It should 
create a subdirectory named "bin" and place the compiled binaries into it
along with all other necessary asset subdirectories and files.

1. Place all files to their own directory, keeping the structure 
   the same as shown above in the list of files included.      

2. Run **setup**, and press `1` to begin.

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
   
   
3. Run **umrc-bridge**. This program is responsible for maintaining a 
   connection to the MRC host and passing chat traffic back and forth 
   between the host and your BBS. It must run continuously in order for 
   umrc-client to work, so it's recommended to have this program 
   launch on system startup.
   
   When umrc-bridge starts up, it should say "Ready for clients" if
   successful.
   
   umrc-bridge takes an optional `-V` command line switch, which enables
   verbose logging. This can be used for troubleshooting purposes.

   umrc-bridge makes automatic reconnect attempts in the event the
   connection to the MRC host is lost, up to 10 times by default. This
   can be changed by starting umrc-bridge with the `-Rx` option, where
   `x` is the number of retries (e.g.: `-R30` for 30 retries). To make
   infinite reconnection attempts, specify `-R0`.   
   
   
4. Set up a menu item to launch a 32-bit door on your BBS.

   The command line syntax is:

     Windows: `umrc-client -D c:\path\to\DOOR32.SYS`
	 
	 Linux: `./umrc-client -D /path/to/DOOR32.SYS`

> **NOTE to Linux Users:**
>
> It has been observed that using the DOOR32.SYS drop file type  with this door can
> lead to user-input issues on Mystic under Linux. **If DOOR32.SYS does not work 
> properly with your BBS type, then use DOOR.SYS instead.**
>	 
> Also refer to your BBS software documentation on whether the `./` prefix
> needs to be included.

   If you get an error saying, "Invalid config. Run setup", it means you either
   did not run Setup, or you're not launching umrc-client from the directory
   it's located in. On some BBSes (like Mystic) it will be necessary to use
   a batch file or bash script to launch the door. Something like the below ought
   to do the trick:

   ```
   :: call this file "launch.bat" and pass the node number (%N) to it
   C:
   cd \path_to\umrc
   umrc-client -D c:\path_to\node%1\door32.sys
   ```

   Optionally (on Windows only) include the `-SILENT` option to prevent the 
   local Windows GUI from popping up while the door is running. This is highly
   recommended, since umrc-client takes a noticeable performance hit when outputting
   to both the BBS and the local Window, especially while paging through the chat
   scrollback.

   ```
   umrc-client -D c:\path\to\DOOR32.SYS -SILENT
   ```

   You can test it locally from the command line as well:

   ```
   umrc-client -L
   ```

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

umrc-bridge makes requests for server stats every 60 seconds and stores 
them in the mrcstats.dat file for display elsewhere on the BBS.

The stats are separated by spaces and follow this order:
   1 - BBSES
   2 - Rooms
   3 - Users
   4 - Activity (0=NONE, 1=LOW, 2=MED, 3, HI)
   5 - Latency (as calculated by the client)
    
    
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

- Masked input becomes unmasked when changing text colors with left and 
  right arrows. 

- Height and width are limited to 80 columns by 24 rows. OpenDoors has
  a default maximum row limit of 23, but umrc-client is hard-coded with
  this setting adjusted to 24. As far as I know, OpenDoors cannot 
  automatically detect the terminal's height and width.

- The font size of the local window in Windows cannot be adjusted. This 
  seems to be an internal limitation of the OpenDoors kit.

- umrc-bridge occasionally reports "partial packets" in verbose mode.
  Most partial packets are either ignored or handled gracefully by the
  bridge when they occur.

- At the time of this writing, when using the `!ddial` command, the MRC
  host returns extraneous packets missing the BODY field. uMRC treats
  these as invalid, since they contain fewer than 6 tildes (~). 
  Syncrhonet mrc-connector service logs similar warnings for `!ddial`.



## Future Plans:

As of this writing, I don't plan on making many changes or adding a lot of
extra features. Bugs will be fixed, and changes will be made to support 
future changes to the MRC protocol, but that's about it.



## Technical Notes:

uMRC is written in C, and was developed and compiled on Windows using 
Microsoft Visual Studio Community 2022. The Linux build was compiled on 
Ubuntu 22.04 using gcc.

uMRC makes extensive use of threading, both in umrc-bridge and umrc-client.
Separate threads are used for establishing connections to the MRC host,
listening for and handling client connections from local clients, and
handling user input while displaying incoming messages to the output window.

Secure SSL sockets are implemented using LibreSSL, a variant of OpenSSL. SSL
is used only from the umrc-bridge to the MRC host, while local umrc-client 
connections to the umrc-bridge are made using standard TCP/IP sockets, using 
the same port number as selected in the Setup program.

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
