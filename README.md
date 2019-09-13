GBADoom

Well actually, it doesn't even run on a GBA yet.

Q) Wheres the ROM?
A) yeh. Uh...Working on it. Come back later...

Q) Doesn't GBA already have DOOM?
A) Yep...it's got a port of Jag Doom which has so much content cut and Doom2 running on a different engine. I want to get the cannonical PC Doom running on the GBA.

Q) Hm..k...so why is this Qt Windows project?
A) Because it's the easiest way to develop, test and debug while I delete all the things in the hope it'll fit in 256kb of ram.

Q) Delete what?
A) Everything that causes the internal data structs to be mutable. Like fast monsters. And settings.

Q) Will it have XYZ?
A) No. XYZ got deleted because it uses memory and CPU.

Q) Whats the plan?
A)  1) Delete all the stuff that isn't needed.
    2) Rewrite the IWAD handling to use const memory pointers so it can run from memory-mapped rom.
    3) Remove as many non-const global vars as possible.
    4) Move the remaining globals to a global struct which can be allocd in EWRAM to free up the 32kb of fast IWRAM.
    5) Port the remaing mess to the GBA.
    6) Move the hot-path code and data to IWRAM.
    7) Play DOOM at about 7fps. Yay!
