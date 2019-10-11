**GBADoom**

~*Well actually, it doesn't even run on a GBA yet.*~ It just about runs in an emulator.


After ~two~ three weeks of hacking, chopping, slashing and removing code it (surprisingly) still seems to work. 
More importantly, the memory usage is down from about 3-4Mb when I started to...~284kb~ 200kb in-game.
It is starting to look like it might be possible to get it within our 256kb memory budget.


Most of the options are gone, it will draw at 120x160px. (There's just no way I can get it to draw at full resolution.) The renderer is otherwise intact. ~Z-Depth lighting~, colormap effects etc are all still present and correct.


The multi-patch textures are ~going to be~ a major headache to get working fast.


Sound won't be too hard to get up and running (but i'm making the assumption that 11250hz samples can be used without re-sampling) but not sure how to do music. I could try the libtimitity route but I think it may be too slow. Maybe converting the music to MOD as part of the build process might work better?



If anyone has any ideas about the music, please drop me a line. Doom music is in a format that can relatively easily be converted to MIDI.






Q) Wheres the ROM?

A) ~yeh. Uh...Working on it. Come back later...~ There's a rom. it's unplayable, but there's a rom now.

Q) Doesn't GBA already have DOOM?

A) Yep...it's got a port of Jag Doom which has so much content cut and Doom2 running on a different engine. I want to get the cannonical PC Doom running on the GBA.

Q) Hm..k...so why is this Qt Windows project?

A) Because it's the easiest way to develop, test and debug while I delete all the things in the hope it'll fit in 256kb of ram.

Q) Delete what?

A) Everything that causes the internal data structs to be mutable. Like fast monsters. And settings.

Q) Will it have XYZ?

A) No. XYZ got deleted because it uses memory and CPU.

Q) Whats the plan?

1) Delete all the stuff that isn't needed.
2) Rewrite the IWAD handling to use const memory pointers so it can run from memory-mapped rom.
3) Remove as many non-const global vars as possible.
4) Move the remaining globals to a global struct which can be allocd in EWRAM to free up the 32kb of fast IWRAM.
5) Port the remaing mess to the GBA.
6) Move the hot-path code and data to IWRAM.
7) Play DOOM at about ~7fps~ 3fps. Yay!
