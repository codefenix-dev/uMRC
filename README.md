<img width="304" height="240" alt="file_id" src="https://github.com/user-attachments/assets/d7ec4955-7692-45d5-a526-f0a4c531be62" />


## 💬 What is uMRC?

uMRC is a full-featured, cross-platform [Multi-Relay Chat](https://status-na-multi.relaychat.net) client for BBSes. It runs as a native door on your system, letting you access Multi-Relay Chat without having to install and maintain a Mystic BBS instance or Python. In other words, as long as your BBS is capable of running DOOR32.SYS doors, then you and your users can participate in MRC.


<img width="638" height="474" alt="sschat" src="https://github.com/user-attachments/assets/723dc2fe-b379-4ea4-a846-8b5769c938b1" />

It should be compatible with any DOOR32.SYS capable BBS such as EleBBS, WWIV, Synchronet, Mystic, and others. It runs on Windows 7 and later, with a special Windows XP build also available.

Pre-compiled Linux binaries are also available, or may be compiled from source using the Install Instructions below.

If you use NetFoss to start up your DOS-based BBS, then the NFU utility bundled with NetFoss should run the uMRC Client. As of this writing, Renegade and Oblivion/2 have been confirmed.


## 📋 Features:

- Secure SSL MRC host connections
- Automatic host reconnection & rejoin
- TAB username auto-completion
- Password masking (e.g.: `/identify ********`)
- Mention counter
- Built-in chat and mention history scrollback
- [Pipe code color](https://wiki.mysticbbs.com/doku.php?id=displaycodes#color_codes_pipe_colors) support
- [CTCP command](https://en.wikipedia.org/wiki/Client-to-client_protocol) support
- Sysop-editable ANSI status bar themes
- Twit filter
- Support for termsizes beyond 80x25 (dropfile dependent)


## 📄 Common Files Included:

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


## 📌 Install Instructions:

This is a **condensed** set of basic install steps. For more detailed info,
check the **readme.txt** file included in the ZIP archive for your
platform.

If you wish to build your own binaries, refer to the build.txt file for
basic instructions.

> ⚠️ If you have any existing MRC process currently running, stop it at
> this time. The MRC host generally does not play well with concurrent
> connections from the same BBS.

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

   The remaining prompts are about your BBS. [Pipe color codes](https://wiki.mysticbbs.com/doku.php?id=displaycodes#color_codes_pipe_colors) 01 - 23
   are allowed in the fields as indicated by setup.

   Your settings will be saved to a file called mrc.cfg.


3. Run **umrc-bridge**. This program is responsible for maintaining a
   connection to the MRC host and passing chat traffic back and forth
   between the host and your BBS. It must run continuously in order for
   umrc-client to work, so it's recommended to have this program
   launch on system startup. It will make automatic reconnection attempts
   as needed.

   Check the **readme.txt** file for a complete description of
   available umrc-bridge options.


4. Set up a menu item to launch a native door on your BBS.

   DOOR32.SYS is the standard drop file to use in most cases, but other drop file
   formats like DOOR.SYS or CHAIN.TXT may be used and may actually be preferable 
   reasons explained further below.

   The basic DOOR32.SYS command line syntax is:

     Windows: `umrc-client -D c:\path\to\DOOR32.SYS`

	 Linux & macOS: `./umrc-client -D /path/to/DOOR32.SYS`

> ❗**NOTE to Linux Users:**
>
> It has been observed that using the DOOR32.SYS drop file type with this door can
> lead to user-input issues on Mystic under Linux. **If DOOR32.SYS does not work
> properly with your BBS type, then use DOOR.SYS instead.**
>
> Also refer to your BBS software documentation on whether the `./` prefix
> needs to be included.

   If you get an error saying, "Invalid config. Run setup", it means you either
   did not run Setup, or you're not launching umrc-client from its own directory.
   On certain BBSes (like Mystic) it is necessary to use a batch file or bash
   script to launch the door.

   On Windows, include the `-SILENT` option to prevent the local Windows GUI
   from popping up while the door is running. This is highly recommended
   since umrc-client takes a noticeable performance hit when outputting
   to both the BBS and its own local Window, especially while paging through
   the chat scrollback.

   ```cmd
   umrc-client -D c:\path\to\DOOR32.SYS -SILENT
   ```

   To run the client locally from the command line, use the -L option:

   ```cmd
   umrc-client -L
   ```

   NOTE: It will only run as user number 1 (Sysop) in local mode.

   umrc-client honors the termsize reported by the drop file. Some drop files,
   like CHAIN.TXT report both the terminal's available columns and rows to the 
   door, allowing umrc-client to run at any termsize, such as 132x37.

   ```cmd
   umrc-client -D c:\path\to\CHAIN.TXT
   ```
   
   The standard termsize of 80x24 gets used by default if no row and/or column 
   size information is given in the drop file, as well as in local mode.


<img width="637" height="473" alt="ssmenu" src="https://github.com/user-attachments/assets/1833241d-4c05-4e73-928b-973131495f3e" />


## 📖 uMRC Client Usage:

When a user first launches the uMRC client door, they'll be given the option
to confirm the user name that was detected, or enter a custom one (in case the
BBS passed the user's real name to the door). After the user confirms their
name, they'll be given a short list of options to configure, after which point
they'll be taken to the main menu. This is explained further in **readme.txt**.

<img width="637" height="472" alt="sssettings" src="https://github.com/user-attachments/assets/95105cf0-f9d9-406f-8e12-241fda6eef50" />

User settings are saved the userdata subdirectory. One file per user,
named using their alias or username on the BBS. If a user's settings
ever need to be reset, simply delete (or rename) the file associated
with them.



## 👥 Meetups!

Now that your BBS offers MRC, here's where the most action is!

### 🚂 thE grAvY trAIn

- Saturdays at 21h00 UK / 16h00 Eastern / 13h00 Pacific
- Host: MeaTLoTioN
- More info at: https://erb.pw

Type `/meetups` in chat for a current list of meetups.



## 🚩 Known Issues & Limitations:

- The font size of the local window in Windows cannot be adjusted. This
  is an internal limitation of the OpenDoors kit.

- At the time of this writing, when using the `!ddial` command, the MRC
  host returns extraneous packets missing the BODY field. uMRC treats
  these as invalid, since they contain fewer than 6 tildes (~). The
  Syncrhonet mrc-connector service logs similar warnings for `!ddial`.



## 💻 Technical Notes:

uMRC is written in C, and was developed and compiled on Windows using
Microsoft Visual Studio Community 2022. The Windows XP build was compiled using
Visual Studio 2017 and the v141_xp platform toolkit in order to specifically 
target Windows XP. The Linux binaries were compiled on Ubuntu 22.04 using gcc.

uMRC makes extensive use of threading, both in umrc-bridge and umrc-client.
Separate threads are used for establishing connections to the MRC host,
listening for and handling client connections from local clients, and
handling user input while displaying incoming messages to the output window.

Secure SSL sockets are implemented using [LibreSSL](https://www.libressl.org), a variant of OpenSSL. SSL
is used only from the umrc-bridge to the MRC host, while local umrc-client
connections to the umrc-bridge are made using standard TCP/IP sockets, using
the same port number as selected in the Setup program. It's worth noting that
a much older version of LibreSSL (2.5.5 from 2017) was used for the Windows XP
build.

You should NOT have ports 5000/5001 open on your firewall/router, since
umrc-client makes OUTBOUND requests to the MRC host on ports 5000/5001.
If you leave these ports open on your end, then you may occasionally see
umrc-bridge log odd messages, caused by outside HTTP requests to your system.



## 🙌 Thanks, Acknowledgements, and Links:

Thanks to **StackFault** at **The Bottomless Abyss BBS**, MRC HQ!

- https://bbs.bottomlessabyss.net
- telnet://bbs.bottomlessabyss.net:2023
- ssh://bbs.bottomlessabyss.net:2222

### 🌐 uMRC Test Sites:

**StingRay** of A-**Net Online**

- https://a-net-online.lol/bbs/
- telnet://a-net-online.lol
- ssh://a-net-online.lol


**X-Bit** of **x-bit.org**

- https://x-bit.org
- telnet://x-bit.org
- ssh://x-bit.org:22222


**Exodus** of **The Titantic BBS**

- https://www.rgbbs.info
- telnet://ttb.rgbbs.info


**Shurato** of **Shurato's Heavenly Sphere**

- https://shsbbs.net
- telnet://shsbbs.net



## ⌨️ Other MRC Clients ##

If you haven't yet decided on which MRC client to set up on your BBS, have a
look at this alternative:

- **ANETMRC**: https://github.com/anetonline/anetmrc

Also check out the **MRC Clients Directory** for a current listing of actively
developed clients: https://status-na-multi.relaychat.net/clients

2026 truly became the year of the custom MRC client, with more options now
available than ever before!


## ✌ Enjoy!

See you in chat!

