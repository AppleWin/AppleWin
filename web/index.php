<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html><head>
  <meta http-equiv="Content-Type" content="text/html; charset=windows-1252">
  <link rel="shortcut icon" href="applewin.ico">
  <title>Apple //e Emulator for Windows</title></head>
<body alink="gold" background="background.gif" bgcolor="mediumpurple" link="orange" text="powderblue" vlink="plum">
<p align="center"><font face="Tahoma"><u><font size="5">AppleWin 1.13.1</font></u></font></p>
<p align="center"><font face="Tahoma"><img src="applewin.gif" title="" alt="Apple //e Emulator Logo" height="384" width="560"></font></p>
<p align="center"><font face="Tahoma">Download <a href="http://download.berlios.de/applewin/AppleWin1.13.1.zip">Version 1.13.1</a><br>
<a href="#OldReleases">Download older releases</a><br>
</font></p>

<p align="center"><font face="Tahoma"><small><a href="mailto:tomch%20AT%20users.berlios.de">Tom Charlesworth</a></small></font></p>
<p><font face="Tahoma"></font><br></p>

<p><font face="Tahoma">
AppleWin is now hosted at <a href="http://developer.berlios.de/">BerliOS</a> and is distributed under the terms of the <a href="http://www.gnu.org/copyleft/gpl.html">GNU General Public License</a><br>
The SVN repository is located here: <a href="https://developer.berlios.de/projects/applewin/">AppleWin SVN</a>
</font></p>
<p><font face="Tahoma"></font><br></p>


<p><font face="Tahoma"><u>1.13.1 - 07 May 2006</u></font></p>
<font face="Tahoma">
<ul><li>Fix: [Bug #7375] &nbsp;Crashes on Win98/ME</li></ul></font>

<p><font face="Tahoma"><u>1.13.0 - 02 May 2006</u></font></p>
<p><font face="Tahoma">
<ul>
	<li>New: Uthernet card support<br></li>
	<ul>
		<li>Allows internet access when used with the Contiki OS<br></li>
		<li>See: <a href="Uthernet.txt">Uthernet.txt</a><br></li>
	</ul>
	<li>New: Floating bus support<br></li>
	<ul>
		<li>Fixes the hang at Drol's cut-scene<br></li>
		<li>Bob Bishop's Money Munchers is a little bit closer to working<br></li>
	</ul>
	<li>Change: Added support for SSC receive IRQ (eg. Z-Link)<br></li>
	<li>Fix: Checkerboard cursor is back for //e mode<br></li>
	<li>Fix: [Bug #6778] enable harddisk not working in 1.12.9.1<br></li>
	<li>Fix: [Bug #6790] Right click menu stops working on drives<br></li>
	<li>Fix: [Bug #7231] AppleWin installed in path with spaces<br></li>
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

<p><font face="Tahoma"><font face="Tahoma"><b>Restrictions:</b><br>

</font>
<font face="Tahoma">-&nbsp;NB. The following aren't saved out to the save-state file yet:<br>
&nbsp; . Phasor card&nbsp; (only the Mockingboards are)<br>
&nbsp; . RAMWorks card
</font></font></p>

<p><font face="Tahoma"><font face="Tahoma"><u>1.12.7.1 - 08 Jan 2006</u><br>
- Fixed cmd-line switches -d1/-d2 to work with filenames with spaces<br>
- Reset: Init Phasor back to Mockingboard mode<br>
- Benchmark button acts immediately<br>
- Fixes to speaker emulation introduced in 1.12.7.0<br>
- Adjusted speaker freq to work better with MJ Mahon's RT.SYNTH.dsk<br>
- Fixed Bxx; ABS,X; ABS,Y; (IND),Y opcodes: take variable cycles depending on branch taken &amp; page crossed<br>
<span style="text-decoration: underline;"></span></font></font></p>

<p><font face="Tahoma"><font face="Tahoma"><span style="text-decoration: underline;"></span><u>1.12.7.0 (30 Dec 2005)</u><br>
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
</font></font></p>

<p><font face="Tahoma"><font face="Tahoma"><u>1.12.6.1 (23 Apr 2005)</u><br>
- Added support for Votrax speech: emulated using SSI263 phonemes<br>
- Added joystick x/y trim control to Input property sheet<br>
- Added support for double-lores graphics<br>
- Updated Applewin.chm<br>
- Load state: F12 nows works like Ctrl-F12<br>
</font></font></p>

<p><font face="Tahoma"><font face="Tahoma">
<a href="History.txt">History</a>
<a href="Wishlist.txt">Wishlist</a>
</font></font></p>

<p align="left"><font face="Tahoma"><font face="Tahoma">
Tested with the following Mockingboard/Phasor titles:<br>
<blockquote>
  Adventure Construction Set<br>
  Berzap!<br>
  Broadsides (SSI) - Card must be in slot-4. Appears to be noise only<br>
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
</font></font></p>

<p align="left"><font face="Tahoma"><font face="Tahoma">There are docs on the web that
claimed these titles support Mockingboard. The titles that I managed to
find didn't appear to support it:<br>
<blockquote>
  One on One<br>
  Guitar Master (Can't find)<br>
  Lancaster<br>
  Music Star (Can't find)<br>
  Thunder Bombs<br>
  Under Fire<br>
</blockquote>
</font></font></p>

<p><font face="Tahoma"><font face="Tahoma"><u><a name="OldReleases"></a>Old releases:</u></font></font></p>
<p><font face="Tahoma"><font face="Tahoma">
  Download <a href="http://download.berlios.de/applewin/AppleWin1.13.0.zip">Version 1.13.0</a><br></font></font><font face="Tahoma"><font face="Tahoma">
  Download <a href="http://download.berlios.de/applewin/AppleWin1.12.9.1.zip">Version 1.12.9.1</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.12.6.0.zip">Version 1.12.6.0</a><br>
  Download <a href="http://download.berlios.de/applewin/AppleWin1.10.4.zip">Version 1.10.4</a> (Oliver Schmidt's last version)<br>
<br>
</font></font></p>

<hr>
<font face="Tahoma"><a href="http://developer.berlios.de">
<img src="http://developer.berlios.de/bslogo.php?group_id=6117&amp;type=1" alt="BerliOS Logo" border="0" height="32" width="124"></a>

<a href="http://www.nvu.com" hreflang="en">
<img src="http://www.nvu.com/made-with-Nvu-t.png" alt="Document made with Nvu" border="0"></a>
<br>



</font></body></html>
