**GBADoom**

A port of prBoom to the GBA. The objective of the project is to get full-fat PC Doom running on the GBA.

This project is currently a WIP but we're getting to the point where there is a playable game in there.

What's working:

-Supports Doom Shareware, Retail, Utimate and Doom2 IWADS.
-Renderer is largely intact. Z-Depth lighting is gone and there is mip-mapping but it's otherwise complete.
-Monster behaviour etc is all intact. (I.e sound propergation etc)
-Framerate is pretty variable. Simple areas run at ~25fps. Complex areas (Eg: E4M2) chug aloang at about 5 FPS. It's running like SNES or 3DO Doom.
-Sound and music support.

Still to do:

-Still needs too much memory. GBA only has 256Kb of work ram. Some levels will crash out as they are larger than this.
-Status bar is partially rendered. It's actually quite tricky because we are page-flipping we need to draw the stbar twice when it updates else it will flicker.
-Save-load are stubbed out.
-Demo compatability is broken.
-Weapon-swith controls not implimented.
-General optimisation. We're never going to get a perfect 35FPS but I think there is still another 50-100% left without changing the visual quality/correctness/game behaviour. For reference, the first time I ran a build under the emulator it ran at about 3FPS.
-Probably a bunch of other stuff that borked too...
