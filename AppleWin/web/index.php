<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html><head>
  <meta http-equiv="Content-Type" content="text/html; charset=windows-1252">
  <link rel="shortcut icon" href="applewin.ico">
  <title>Apple //e Emulator for Windows</title></head>
<body alink="gold" background="background.gif" bgcolor="mediumpurple" link="orange" text="powderblue" vlink="plum">
<p align="center"><font face="Tahoma"><u><font size="5">AppleWin 1.24.0</font></u></font></p>
<p align="center"><font face="Tahoma"><img src="applewin.gif" title="" alt="Apple //e Emulator Logo" height="384" width="560"></font></p>
<p align="center"><font face="Tahoma">Download <a href="http://download.berlios.de/applewin/AppleWin1.24.0.0.zip">Version 1.24.0</a><br>
<a href="#OldReleases">Download older releases</a><br>
</font></p>

<p align="center"><font face="Tahoma"><small><a href="mailto:tomch%20AT%20users.berlios.de">Tom Charlesworth</a></small></font></p>
<p><font face="Tahoma"></font><br></p>

<p><font face="Tahoma">
AppleWin is now hosted at <a href="http://developer.berlios.de/">BerliOS</a> and is distributed under the terms of the <a href="http://www.gnu.org/copyleft/gpl.html">GNU General Public License</a><br></font><font face="Tahoma">
The SVN repository is located here: <a href="https://developer.berlios.de/projects/applewin/">AppleWin SVN</a>
</font>
</p><p><font face="Tahoma">
Latest AppleWin compiled html help: <a href="AppleWin.chm">AppleWin.chm</a>
<br>
<font size="2">
<strong>NB. If you have trouble reading the CHM:</strong><br>
- On Windows XP, Windows 7, etc you can simply right-click on the CHM file, select "Properties", and click on the "Unblock" button.<br>
- Click "Apply" and the content should be visible.
</font></font></p>


<p><font face="Tahoma"><br></p><p><u>1.24.0 - 1 Jan 2014</u></p>
<li>Changes:</li>
<ul>
	<li>Support cursor keys (in addition to numpad) when using keyboard for joystick emulation.</li>
	<li>Support auto-fire for all 3 joystick buttons (via Config->Input).</li>
	<li>[Feature #5668] Added confirmation message box for reboot (F2).</li>
	<li>[Feature #5715] Added -no-printscreen-dlg to suppress the warning if AppleWin fails to capture the PrintScreen key.</li>
	<li>Changed save-state file persisted to Registry from filename to pathame.</li>
	<li>[Feature #5105] Added About dialog showing GPL (at startup, but only when AppleWin version changes).</li>
</ul>
<li>Fixes:</li>
<ul>
	<li>[Bug #19154] ProDOS Order 2IMG crashing.</li>
	<li>[Support #103098] Sometimes swapping disk could cause INIT to fail with ERROR #8.</li>
	<li>Fixed save-state bug for when 4K BANK1 is dirty (previously it would save the stale data instead).</li>
	<li>[Bug #18723,#19070] Mouse movement for CopyII+9.1 and ProTERM3.1.</li>
</ul>
<li>Debugger:</li>
<ul>
	<li>Added "disk info" command.</li>
	<li>[Bug #18940] Extend BSAVE and BLOAD Command To Memory Banks 0 and 1.</li>
</ul>
</font>


<p><font face="Tahoma"><br></p><p><u>1.23.0 - 26 Apr 2013</u></p>
<li><strong>Significant changes since 1.20.0:</strong></li>
<ul>
	<li>[Feature #003272 and #005335] Support 2x windowed mode:</li>
	<ul>
		<li>Toggle between 1x and 2x by using Resize button (or F6).</li>
		<li>Full screen now enabled by CTRL+Resize button (or CTRL+F6).</li>
	</ul>
    <li>[Feature #4399] Allow Z80 SoftCard to be inserted into slot 4 or 5. (Allows CP/M v3 to work)</li>
    <li>Reworked Configuration (property sheets) to allow multiple hardware changes.</li>
	<li>Added Troubleshooting section to help file.</li>
</ul>
<li>Fixes:</li>
<ul>
    <li>HDD firmware: Added support for SmartPort entrypoint.</li>
	<ul>
		<li>"Prince of Persia (Original 3.5 floppy for IIc+).2mg" now boots</li>
	</ul>
    <li>Fixed HDD firmware to allow epyx_californiagames_iicplus.2mg to boot.</li>
</ul>
<li>Debugger:</li>
<ul>
    <li>[Bug #018455] Improved rendering speed of debugger view.</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.20.0 - 23 Feb 2011</u></font></p>
<font face="Tahoma">
<li>Changes:</li>
<ul>
    <li>Standard, Text Optimized and Monochrome video modes now support half-pixel rendering accuracy!</li>
    <li>Colors tweaked to better match the real hardware.</li>
</ul>
<li>Fixes:</li>
<ul>
    <li>Fixed Mockingboard detection for Ultima III (ProDOS-8/harddisk version).</li>
</ul>
<li>Debugger:</li>
<ul>
    <li>See the Debugger_Changelog.txt for the various fixes and changes.</li>
</ul>
<li>Misc:</li>
<ul>
    <li>Updated acknowledgements. Cheers to Mike Harvey! (Nibble Magazine)</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.19.3 - 20 Dec 2010</u></font></p>
<font face="Tahoma">
<li><strong>Significant changes since 1.17.2:</strong></li>
<ul>
	<li>Added No-Slot-Clock (located in $C300 ROM space).</li>
	<li>Added disk/harddisk image r/w support for .gz/.zip (and .2mg) files.</li>
	<ul>
		<li>Multi-zip archives are read-only.</li>
	</ul>
</ul>
<li>Other changes since 1.17.2:</li>
<ul>
	<li>Set 50% scan lines as the default.</li>
	<li>Added HD activity LED.</li>
	<li>Caps LED now takes up less space in full-screen mode.</li>
	<li>Debugger 2.7.0.0 (see Debugger_Changelog.txt)</li>
	<li>Rebuilt for Win98 with: VS2005 Express, Windows SDK 6.0, SAPI 5.1</li>
	<li>Added "-speech" cmd-line switch</li>
	<ul>
		<li>Captures text from COUT and outputs to Speech API</li>
		<li>Ctrl+RESET and FullSpeed mode (ie. disk access) will purge the speech buffers</li>
	</ul>
	<li>Added disk/harddisk eject sub-menu to Disk Config tab.</li>
	<li>Removed .bin registration.</li>
</ul>
<li>Fixes since 1.17.2:</li>
<ul>
	<li>Fix for Win98 support (broken at 1.18.0.0)</li>
	<ul>
		<li>Fix OpenFileName dialogs for Win98 (use older WinVer4 struct).</li>
		<li>Fix for "APPLEWIN executed an invalid instruction" on Pentium II/266 CPU.</li>
	</ul>
	<li>Edits to the 'Printer dump filename' edit control are now recognised.</li>
	<li>[Bug #017055] DDial timer running very fast.</li>
	<ul>
		<li>TAPEIN.bit7 was being read from floating-bus. Now fixed high.</li>
	</ul>
	<li>[Bug# 007237] VBl IO reg not updated in 'Stepping' mode</li>
	<li>Loading save-state file (.aws) wasn't Win32-closing .dsk images in drives.</li>
	<li>[Bug #16699] Debugger: G xxxx not clearing BP</li>
	<li>[Bug #16688] Debugger RUN <script-file> still not 100%</li>
	<ul>
		<li>Honour absolute path (ie. don't prefix with CWD)</li>
	</ul>
	<li>[Bug #16632] Fix cmd-line -d1/d2 switches with relative path</li>
	<li>Fix speaker volume when booting with -d1 switch</li>
	<li>Fix debugger bugs:</li>
	<ul>
		<li>Crash when doing: help *</li>
		<li>[Bug #16651] Crash when doing: run <script-file></li>
	</ul>
	<li>[Bug #16652] Fix "Harddisk images aren't persisted when in different folders"</li>
	<li>[Bug #12724,14747] Fix "Registry persisted Window x,y position sometimes off screen"</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.17.2 - 13 Dec 2009</u></font></p>
<font face="Tahoma">
<li>Changes:</li>
<ul>
    <li>Enable XP visual themes &amp; corrected tab order in the Configuration dialog - thanks to Joshua Bell</li>
    <li>Updated Help chm's Debugger breakpoint commands (BC,BD,BE,BL now replaced by BPC,BPD,BPE.BPL)</li>
</ul>
<li>Fixes:</li>
<ul>
    <li>Fix: Crash when COM or TCP port opened before Configuration dialog is opened for the first time</li>
	<li>Fix: Reading SSC DIP SW1 for "external" CLK (115.2K mode)</li>
	<li>Fix: Reinstated debugger's GO command:</li>
    <ul>
       <li>G        : Go (Stepping Mode)</li>
       <li>G &lt;addr&gt; : Go (Stepping Mode) until PC=addr</li>
    </ul>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.17.1 - 27 Nov 2009</u></font></p>
<font face="Tahoma">
<li>Changes:</li>
<ul>
    <li>SSC: Support 112.5K Baud</li>
    <li>Dynamically generated 'Serial Port' drop-down menu</li>
    <ul>
       <li>Save "Serial Port Name" to Registry (instead of drop-down menu index as "Serial Port")</li>
    </ul>
    <li>Updated Help chm's "Transferring Disk Images" - thanks to David Schmidt</li>
    <li>New switches: -log, -no-mb, -spkr-max, -spkr-inc</li>
    <li>Debugger (v2.6.2.0):</li>
    <ul>
       <li>Added Applesoft BASIC symbols - thanks to Bob Sander-Cederlof</li>
       <li>Return on blank line to toggle full screen console</li>
       <li>Page Up/Down of console history while editing</li>
    </ul>
</ul>
<li>Fixes:</li>
<ul>
    <li>Fix for Speaker underflow problem (-spkr-max=200, -spkr-inc=20)</li>
    <li>Fix for SSC (big transfers): use queue instead of single byte buffer</li>
    <li>Don't hog CPU in PAUSED mode</li>
    <li>Implemented the shift key mod for II/II+. This fixes a problem with Homeword</li>
    <li>[Bug #14879] Double-Lo-Res Graphics colors not correct</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.16.1 - 21 Jun 2009</u></font></p>
<font face="Tahoma">
<li>Changes:</li>
<ul>
    <li>Ctrl-F2 now functions as CONTROL-RESET (same as Ctrl-Break)</li>
    <li>Video Mode now shown in Window Title</li>
    <li>50% Scan Lines (can use Shift+Ctrl+F9 to toggle)</li>
    <ul>
        <li>Added: Checkbox for "50% Scan lines" in the configuration tab, next to video mode</li>
        <li>Supported by PrintScreen and Shift-PrintScreen</li>
    </ul>
    <li>Added command line "-noreg" to not register file extensions</li>
    <li>Added support for up to 40 track (160KB) disk images</li>
    <li>Debugger:</li>
    <ul>
        <li>Symbols Length raised from 13 to 31</li>
        <li>Pressing the Reboot button (F2) with breakpoints active, keeps the debugger running</li>
        <li>symsrc is now relocatable, i.e. symsrc load "filename" [,offset]
        <ul>
            <li>Changes the address where debugger symbols are bound to by the offset (if specified)</li>
        </ul>
        </li>
        <li>Pressing Shift, Ctrl, or Alt, when viewing the current Apple output no longer kicks you back into the debugger.<br>
        (Allows for Ctrl-Shift-F9, and Shift-F9 previewing.)</li>
    </ul>
</ul>
<li>Fixes:</li>
<ul>
    <li>Fixed: Full screen drive LED status not showing up when floppy disks being accessed were set to read-only mode</li>
    <li>Fixed FLASHing 'S' in AppleII+ mode!</li>
    <li>Fixed flash rate for NTSC</li>
    <li>Fixed maximum volume bug when doubling-clicking a .dsk image to execute with AppleWin</li>
    <li>[Bug #14557] Loading serial port# from Registry (caused AppleWin to crash when booting Apple Pascal &amp; other weird crashes)</li>
    <li>[Bug #15394] Audio under-run (set process priority to Above Normal when in non-Full Speed mode)</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.16.0 (beta) - 1 Feb 2009</u></font></p>
<font face="Tahoma">
<li>Changes:</li>
<ul>
    <li>Support for Apple // Game Server via TCP (port 1977) : beta</li>
    <li>For GPL reasons, switched the following modules:</li>
    <ul>
        <li>AY8910 (from MAME  to FUSE) : beta Mockingboard/Phasor</li>
        <li>MC6821 (from MAME  to VICE) : beta Mouse card</li>
        <li>Z80    (from Z80Em to VICE) : beta CP/M Softcard</li>
    </ul>
    <li>Printer support:</li>
    <ul>
        <li>Printer dump filename</li>
        <li>Filter unprintable characters</li>
        <li>Append to print-file</li>
        <li>Terminate printing after n seconds of idle</li>
        <li>Encoding conversion for Pravets</li>
        <li>Dump to printer (CAUTION! Enabled via command line switch: -use-real-printer)</li>
    </ul>
    <li>Added Pravets 8M</li>
</ul>
<li>Fixes:</li>
<ul>
    <li>BugID-014557: Fix for loading serial port from Registry (caused AppleWin to crash when booting Apple Pascal)</li>
    <li>Some floating bus bugs (fixes the Bulgarian game: "Walking in the town" & Annunciator read)</li>
    <li>FLASH rate (now 3Hz, was 6Hz)</li>
    <li>Fix for Willy Byte & MB support (strange 6522 behaviour!)</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.15.0 (beta) - 24 Aug 2008</u></font></p>
<font face="Tahoma">
<li>Changes:</li>
<ul>
    <li>CP/M support (Microsoft CP/M SoftCard in slot-5)</li>
    <ul>
        <li>No save-state support</li>
        <li>No Z80 debugging support</li>
    </ul>
    <li>PrintScrn key now saves screen shots</li>
    <li>Added new video mode: "Monochrome - Authentic"</li>
    <li>Debugger 2.6.0.6</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.14.2 - 23 Jun 2008</u></font></p>
<font face="Tahoma">
<li>Changes:</li>
<ul>
    <li>Support for Bulgarian clones: Pravets 82 &amp; 8A</li>
    <li>Mouse can be configured to show/hide crosshairs; and can be restricted (or not) to AppleWin's window</li>
    <li>Added 'Send to CiderPress' function via the context menu of the drive buttons</li>
    <li>Added support for "The Freeze's" F8 ROM (Apple][ & Apple][+ only)</li>
    <li>Added -f8rom &lt;rom-file&gt; cmd line switch to allow loading a custom 2K Rom at $F800</li>
    <li>Support Shift-F9 to cycle backwards through video modes</li>
    <li>Support Ctrl-F9 to cycle through the character sets</li>
</ul>
<li>Fixes:</li>
<ul>
    <li>Mouse support for Contiki v1.3, Blazing Paddles & GEOS</li>
    <li>Mouse support now integrates much better with Windows (when in unrestricted mode)</li>
    <li>Extended HDD image file filter to include *.po</li>
    <li>[Bug #13425] Full Screen mode: drawing/erasing of the buttons on the RHS of the screen</li>
    <li>[Bug #12723] DOSMaster .hdv/.po images work</li>
    <li>[Bug #11592] Infiltrator now boots</li>
</ul>
</font>


<p><font face="Tahoma"></font><br></p><p><font face="Tahoma"><u>1.14.0 - 8 Aug 2007 (beta)</u></font></p>
<p><font face="Tahoma">Fixes:<u></u></font></p>
<font face="Tahoma">
<ul>
    <li>Super Serial Card: PR#2 &amp; IN#2 now working</li>
    <li>Full support for Peripheral Expansion ROM (at $C800) &amp; $CFFF access</li>
    <li>F2 (Power-cycle) when ROM is switched *out* caused Apple to freeze</li>
</ul>
</font><p><font face="Tahoma">Changes:</font></p>
<font face="Tahoma">
<ul>
    <li>Attempt to use drive1's image name as the name for the .aws file</li>
    <li>Added Apple//e (original 6502 version with "Venetian Blinds" self-test)</li>
    <li>Turbo mode via Scroll Lock (temporary or toggle mode) - selectable via UI</li>
</ul></font><p><font face="Tahoma">Beta:</font></p>
<font face="Tahoma">
<ul>
    <li>Mouse Interface card support in slot 4 (selectable via UI)</li>
    <ul>
        <li>Full 6821 emulation &amp; 2K ROM. Based on code by Kyle Kim (Apple in PC)</li>
        <li>Tested with: Dazzle Draw, Blazing Paddles, Archon II: Adept, Orge[Fix], Dragon Wars</li>
    </ul>
</ul>
</font>


<p><font face="Tahoma"><u></u></font></p><p><font face="Tahoma"><u>1.13.2 -&nbsp;7 April 2007</u></font></p>
<font face="Tahoma">
<ul>
    <li>Added: Apple ][ (non-autostart monitor)</li>
    <li>Added: 6502 NMOS illegal opcode support (for ][ &amp; ][+)</li>
    <li>Added: 65C02 CMOS undefined opcode support (for //e)</li>
    <li>Added: Simple parallel printer support in slot-1<br>
        &nbsp; . Creates (or overwrites) a file called "Printer.txt" in AppleWin.exe's folder<br>
        &nbsp; . Eg. PR#1, then LIST, then PR#0<br>
        &nbsp; . The file will auto-close 10 seconds after the last printed output</li>
    <li>Fix: [Bug #7238] FLASH support in 80-column mode</li>
    <li>Fix: [Bug #8300] 80-col text in Silvern Castle got corrupted</li>
    <li>Fix: Speech with MB/Phasor for short phonemes - bug in DirectSound in WinXP (see KB327698)</li>
    <li>Fix: Disk ][ track stepping (Mabel's Mansion now works)</li>
    <li>Docs updated (although debugger docs still partially out of date)</li>
    <li>Debugger:<br>&nbsp; + Now uses Apple font<br>
        &nbsp; + Can configure entry to debugger via specific opcode or illegal(6502)/undefined(65C02) opcode - use BRKOP cmd<br>
        &nbsp;&nbsp;&nbsp; - So you can run Apple at full-speed until it hits your breakpoint (eg. BRK)<br>
        &nbsp; + BRKOP, BRK # to enter debugger<br>
        &nbsp; + Mouse support: button &amp; wheel<br>
        &nbsp; + BLOAD/BSAVE<br>
        &nbsp; + Search command: S/SH<br>
        &nbsp; + New DISASM command<br>
        &nbsp; + bookmarks, via bm, bma, bmc, bml, ctrl-#, alt-#<br>
        &nbsp; + HELP RANGE<br>&nbsp; + HELP OPERATORS<br>
        &nbsp; + PRINT, PRINTF<br>&nbsp; + ctrl-v (paste) support<br>
        &nbsp; ... &amp; lots more (doc's to be updated soon)</li>
</ul>
</font>


<p><font face="Tahoma"><u>1.13.1 -&nbsp;7 May 2006</u></font></p>
<font face="Tahoma">
<ul><li>Fix: [Bug #7375] &nbsp;Crashes on Win98/ME</li></ul></font>

<p><font face="Tahoma"><u>1.13.0 -&nbsp;2 May 2006</u></font></p>
<p><font face="Tahoma">
<ul>
	<li>New: Uthernet card support</li>
	<ul>
		<li>Allows internet access when used with the Contiki OS</li>
		<li>See: <a href="Uthernet.txt">Uthernet.txt</a></li>
	</ul>
	<li>New: Floating bus support</li>
	<ul>
		<li>Fixes the hang at Drol's cut-scene</li>
		<li>Bob Bishop's Money Munchers is a little bit closer to working</li>
	</ul>
	<li>Change: Added support for SSC receive IRQ (eg. Z-Link)</li>
	<li>Fix: Checkerboard cursor is back for //e mode</li>
	<li>Fix: [Bug #6778] enable harddisk not working in 1.12.9.1</li>
	<li>Fix: [Bug #6790] Right click menu stops working on drives</li>
	<li>Fix: [Bug #7231] AppleWin installed in path with spaces</li>
</ul>
</font></p>

<p><font face="Tahoma"><u>1.12.9.1 - 10 Mar 2006</u><br>
</font></p><ul>
<li><font face="Tahoma">Right-Click on drive icon for disk popup menu. Options are: <br>
	</font><ul>
<font face="Tahoma"> 	<li>Eject disk<br></li>
	<li>Read only (write protection on.)</li>
	<li>Read / Write (write protection off.)<br>
		<b>Note</b>: <i>If a file is read-only, the Read only option will be checked.</i></li>
	</font></ul>
</li><li><font face="Tahoma">Fixed invalid F7 opcode addressing mode, so Lock N' Chase is now playable.</font></li>
<li><font face="Tahoma">Video Blanking Timing now has preliminary support.<br>
i.e. Drol now longer stalls at the cutscene.<br>
	<b>Note</b>: <i>The VBL is not exact timing (yet), so some games like Karateka might exhibit a little choppiness.</i></font></li>
<li><font face="Tahoma">Debugger 2.5.0.16<br>
	</font><ul>
<font face="Tahoma">	<li>New Command: #G, same as Apple "Monitor" go command, where # is an hex address.<br>
		&nbsp; i.e. C600G</li>
	<li>New Command: #L, same as Apple "Monitor" list command, where # is an hex address.<br>
		&nbsp; i.e. 300L</li>
	<li>New Command: //<br>
		Starts a line comment anywhere in the line.</li>
	<li>New Command: RUN "filename", to run a debugger script</li>
	<li>New Command: ECHO ...<br>
		Text may be quoted: ECHO "...text..."<br>
		Echo the current line, since scripts don't echo their commands.</li>
	<li>New Command: SH address ## [? ?? ##]<br>
		&nbsp;   You can now search memory!<br>
		&nbsp;   i.e.<br>
		&nbsp;   SH 800,8000 AD ? C0 // search for one byte gap, AD xx C0<br>
		&nbsp;   SH 800,8000 C030 // search for two bytes: 30 C0<br></li>
	<li>Mini-Assembler preview:<br>
		<b>Note</b>: <i>None of the indexed/indirect modes are working (yet), expressions are not evaluated.</i><br>
		&nbsp;   usage: A address<br>
		&nbsp;   usage: A<br>
		&nbsp;   The assembler prompt is the '!' -- for your mini-assembler fans ;-)<br>
		&nbsp;   The format is:   label <whitespace> mnemonic [<target>]<br>
		&nbsp;   To exit the assembler, press Enter without any input.<br>
&nbsp; The spacebar to execute the next instruction is disabled while
in assembler mode. You must press space, if you don't wish to define a
label.</target></whitespace></li>
	<li>Fixed Console sometimes not drawing.</li>
	<li>Fixed UI bug: Memory View text over-writing buttons.</li>
  	<li>Changed BPX to now defaults to setting breakpoint at cursor.</li>
  	<li>Changed BP to now default to setting breakpoint at Program Counter (PC)</li>
  	<li>Added new color scheme: BW.  (This used to be an alias for MONO before.)</li>
	</font></ul>
</li></ul>
<p></p>

<p><font face="Tahoma"><font face="Tahoma"><u>1.12.9.0 - 25 Feb 2006</u><br>
- Moved source to BerliOS &amp; released under GPL<br>
- </font><font face="Tahoma">Debugger v2.4.2.16<br>
&nbsp; + Breakpoint on memory address added: BPM&nbsp;address[,length]<br>
</font></font></p>


<p><font face="Tahoma"><font face="Tahoma"><u>1.12.8.0 - 22 Feb 2006<br>

</u>
- <span style="font-weight: bold;">*** Major re-write of debugger by Michael Pohoreski ***</span><br>
&nbsp; . Debugger v2.4.2.15: Improvements &amp; new features abound!<br>
&nbsp;&nbsp;&nbsp; + Syntax coloring, navigation, execution (eg. step-out), view memory as varying Ascii types<br>
&nbsp;&nbsp;&nbsp; + Symbol maintenance (main, user, source), source-level debugging, mini-calculator<br>
&nbsp;&nbsp;&nbsp; + Breakpoints: conditional on register, profiling + much more<br>
&nbsp; . See: <a href="Intro_To_New_Debugger.htm">Introduction to New Debugger</a><br>
- Fixed speaker volume not being set correctly at start-up<br>
- Fixed emulation speed control (was only running at 1MHz)<br>
- Fixed internal ADC: was flagged as write to memory<br>
- Fixed internal BRK: only pushed PC+1, not PC+2 to stack<br>
- Fixed CPU not getting properly reset (eg. SP) on Ctrl+Reset<br>
- Changed attenuation on AY8910 outputs before mixing: MB: none (was 2/3), Phasor: still 2/3<br>
</font></font></p>

<p><font face="Tahoma"><font face="Tahoma"><u>1.12.7.2 - 25 Jan 2006<br>
</u>- Fixed crash-bug in C++ 65C02 emu code for opcode $7C : JMP (ABS,X)<br>
- Updated help file (by Brian Broker)<br>
- Added ability to use Shift+Insert to paste from clipboard during emulation<br>
- Added buttons to Config-&gt;Sound tab to select Mockingboard (s4 &amp; s5), Phasor (s4) or none<br>
- Removed keyboard buffer from Config-&gt;Input (this was redundant from 1.12.7.0)<br>
- Fixed speaker click (eg. when selecting disk image)<br>
- Added check to prevent loading old &amp; incompatible 6502 DLLs (caused random 6502 crashes to monitor)<br>
</font><font face="Tahoma">- Added support for AE's RAMWorks III, which adds up to 8MB (cmd-line switch only):<br>
&nbsp; . -r &lt;#pages&gt;&nbsp; : where #pages = [1..127], each page is 64KB.<br>
</font></font></p>

<p><font face="Tahoma"><b>Restrictions:</b><br></font>
<font face="Tahoma">-&nbsp;NB. The following aren't saved out to the save-state file yet:<br>
&nbsp; . Phasor card&nbsp; (only the Mockingboards are)<br>
&nbsp; . RAMWorks card
</font></p>

<p><font face="Tahoma"><u>1.12.7.1 -&nbsp;8 Jan 2006</u><br>
- Fixed cmd-line switches -d1/-d2 to work with filenames with spaces<br>
- Reset: Init Phasor back to Mockingboard mode<br>
- Benchmark button acts immediately<br>
- Fixes to speaker emulation introduced in 1.12.7.0<br>
- Adjusted speaker freq to work better with MJ Mahon's RT.SYNTH.dsk<br>
- Fixed Bxx; ABS,X; ABS,Y; (IND),Y opcodes: take variable cycles depending on branch taken &amp; page crossed<br>
<span style="text-decoration: underline;"></span></font></p>

<p><font face="Tahoma"><span style="text-decoration: underline;"></span><u>1.12.7.0 (30 Dec 2005)</u><br>
- Public release<br>
- Fixed Apple][+ ROM (IRQ vector was vectoring to $FF59/OLDRST)<br>
- Added cmd-line switches (-f, -d1, -d2)<br>
&nbsp;&nbsp;&nbsp; .
-f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
: full-screen<br>
&nbsp;&nbsp;&nbsp; . -dn &lt;image&gt;&nbsp; : Load a disk image into a drive. A disk in drive-1 will force a reboot.<br>
- Extended 6502 debugger (M1, M2, P0,...P4)<br>
&nbsp;&nbsp;&nbsp; . Mn &lt;addr&gt;&nbsp;&nbsp;&nbsp;&nbsp; : Memory window 1/2<br>
&nbsp;&nbsp;&nbsp; . Pn &lt;zp-addr&gt; : Display a zero-page pointer<br>
&nbsp;&nbsp;&nbsp; . Extended memory-dump to output SY6522/AY8910 regs with SYn/AYn, eg: M1 AY0<br>
&nbsp;&nbsp;&nbsp; . Added support for ACME symbol files<br>
- Phasor support (defaulting to Mockingboard mode, available in slots 4 &amp; 5)<br>
- Display updated at ~60Hz (instead of 20Hz)<br>
- Emulation broken into 1ms units (instead of video-frame units)<br>
- Uses internal CPU emulator instead of DLLs (DLLs to be phased out)<br>
</font></p>

<p><font face="Tahoma"><u>1.12.6.1 (23 Apr 2005)</u><br>
- Added support for Votrax speech: emulated using SSI263 phonemes<br>
- Added joystick x/y trim control to Input property sheet<br>
- Added support for double-lores graphics<br>
- Updated Applewin.chm<br>
- Load state: F12 nows works like Ctrl-F12<br>
</font></p>

<p><font face="Tahoma">
<a href="History.txt">History</a>
<a href="Wishlist.txt">Wishlist</a>
</font></p>

<p align="left"><font face="Tahoma">
Tested with the following Mockingboard/Phasor titles:<br>
<blockquote>
  Adventure Construction Set<br>
  Berzap!<br>
  Broadsides (SSI) - Card must be in slot-4. Appears to be noise-channel only<br>
  Crimewave (Votrax speech only)<br>
  Crypt of Medea (Votrax speech only)<br>
  <a href="http://www.tomcharlesworth.pwp.blueyonder.co.uk/Cybernoid.dsk">Cybernoid Music Disk</a><br>
  Lady Tut (Mockingboard version)<br>
  Mockingboard software (Sweet Micro Systems)<br>
  Music Construction Set<br>
  Night Flight<br>
  Phasor software (Applied Engineering)<br>
  Popeye<br>
  Rescue Raiders v1.3 (SSI263 speech only)<br>
  Silent Service (Microprose)<br>
  Skyfox<br>
  Spy Strikes Back<br>
  Ultima III (Mockingboard version)<br>
  Ultima IV<br>
  Ultima V<br>
  Willy Byte<br>
  Zaxxon (Mockingboard version)<br>
</blockquote>
</font></p>

<p align="left"><font face="Tahoma">There are docs on the web that
claimed these titles support Mockingboard. The titles that I managed to
find didn't appear to support it:<br>
<blockquote>
  One on One<br>
  Guitar Master (Can't find)<br>
  Lancaster<br>
  Music Star (Can't find)<br>
  Thunder Bombs<br>
</blockquote>
</font></p>

<p><font face="Tahoma"><u><a name="OldReleases"></a>Old releases:</u></font></p>
<p><font face="Tahoma">
  Download <a href="http://download.berlios.de/applewin/AppleWin1.23.0.0.zip">Version 1.23.0</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.20.0.0.zip">Version 1.20.0</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.19.3.0.zip">Version 1.19.3</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.17.2.0.zip">Version 1.17.2</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.17.1.0.zip">Version 1.17.1</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.16.1.0.zip">Version 1.16.1</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.16.0.0-beta.zip">Version 1.16.0 (beta)</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.15.0-beta.zip">Version 1.15.0 (beta)</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.14.2.zip">Version 1.14.2</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.14.0-beta.zip">Version 1.14.0 (beta)</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.13.2.zip">Version 1.13.2</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.13.1.zip">Version 1.13.1</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.13.0.zip">Version 1.13.0</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.12.9.1.zip">Version 1.12.9.1</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.12.6.0.zip">Version 1.12.6.0</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.10.4.zip">Version 1.10.4</a> (Oliver Schmidt's last version)<br>
<br>
</font></p>

<hr>
<font face="Tahoma"><a href="http://developer.berlios.de">
<img src="http://developer.berlios.de/bslogo.php?group_id=6117&amp;type=1" alt="BerliOS Logo" border="0" height="32" width="124"></a>

</font></body></html>
