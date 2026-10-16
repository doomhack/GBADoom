#ifndef __DEH_TEXT__
#define __DEH_TEXT__

/*
 * The strings DeHackEd Text blocks can replace. GbaWadUtil writes all of
 * them, replaced or not, to the GBATEXT lump; the engine reads them with
 * D_TEXT(id) (dstrings.h). Shared with GbaWadUtil (C++).
 *
 * DEH_TEXTS(X) is X(id, GBADoom's text). Keep each group in order:
 * hu_stuff.c counts on from HUSTR_E1M1, HUSTR_1, PHUSTR_1 and THUSTR_1, and
 * GbaWadUtil rewraps E1TEXT to T6TEXT to fit the screen.
 *
 * DEH_VANILLA_TEXTS(X) is X(id, vanilla text) for the strings GBADoom
 * reworded to fit the screen. A patch finds the string it replaces by
 * its vanilla text.
 */

#define DEH_TEXTS(X) \
  X(GOTARMOR, "Picked up the armor.") \
  X(GOTMEGA, "Picked up the MegaArmor!") \
  X(GOTHTHBONUS, "Picked up a health bonus.") \
  X(GOTARMBONUS, "Picked up an armor bonus.") \
  X(GOTSTIM, "Picked up a stimpack.") \
  X(GOTMEDIKIT, "Picked up a medikit.") \
  X(GOTMEDINEED, "Got a medikit you REALLY need!") \
  X(GOTSUPER, "Supercharge!") \
  X(GOTBLUECARD, "Picked up a blue keycard.") \
  X(GOTYELWCARD, "Picked up a yellow keycard.") \
  X(GOTREDCARD, "Picked up a red keycard.") \
  X(GOTBLUESKUL, "Picked up a blue skull key.") \
  X(GOTYELWSKUL, "Picked up a yellow skull key.") \
  X(GOTREDSKULL, "Picked up a red skull key.") \
  X(GOTINVUL, "Invulnerability!") \
  X(GOTBERSERK, "Berserk!") \
  X(GOTINVIS, "Partial Invisibility") \
  X(GOTSUIT, "Radiation Shielding Suit") \
  X(GOTMAP, "Computer Area Map") \
  X(GOTVISOR, "Light Amplification Visor") \
  X(GOTMSPHERE, "MegaSphere!") \
  X(GOTCLIP, "Picked up a clip.") \
  X(GOTCLIPBOX, "Picked up a box of bullets.") \
  X(GOTROCKET, "Picked up a rocket.") \
  X(GOTROCKBOX, "Picked up a box of rockets.") \
  X(GOTCELL, "Picked up an energy cell.") \
  X(GOTCELLBOX, "Picked up an energy cell pack.") \
  X(GOTSHELLS, "Picked up 4 shotgun shells.") \
  X(GOTSHELLBOX, "Picked up a box of shells.") \
  X(GOTBACKPACK, "Picked up a backpack of ammo!") \
  X(GOTBFG9000, "You got the BFG9000!  Oh, yes.") \
  X(GOTCHAINGUN, "You got the chaingun!") \
  X(GOTCHAINSAW, "A chainsaw!  Find some meat!") \
  X(GOTLAUNCHER, "You got the rocket launcher!") \
  X(GOTPLASMA, "You got the plasma gun!") \
  X(GOTSHOTGUN, "You got the shotgun!") \
  X(GOTSHOTGUN2, "You got the super shotgun!") \
  X(PD_BLUEO, "This object needs a blue key") \
  X(PD_REDO, "This object needs a red key") \
  X(PD_YELLOWO, "This object needs a yellow key") \
  X(PD_BLUEK, "This door needs a blue key") \
  X(PD_REDK, "This door needs a red key") \
  X(PD_YELLOWK, "This door needs a yellow key") \
  X(HUSTR_E1M1, "E1M1: Hangar") \
  X(HUSTR_E1M2, "E1M2: Nuclear Plant") \
  X(HUSTR_E1M3, "E1M3: Toxin Refinery") \
  X(HUSTR_E1M4, "E1M4: Command Control") \
  X(HUSTR_E1M5, "E1M5: Phobos Lab") \
  X(HUSTR_E1M6, "E1M6: Central Processing") \
  X(HUSTR_E1M7, "E1M7: Computer Station") \
  X(HUSTR_E1M8, "E1M8: Phobos Anomaly") \
  X(HUSTR_E1M9, "E1M9: Military Base") \
  X(HUSTR_E2M1, "E2M1: Deimos Anomaly") \
  X(HUSTR_E2M2, "E2M2: Containment Area") \
  X(HUSTR_E2M3, "E2M3: Refinery") \
  X(HUSTR_E2M4, "E2M4: Deimos Lab") \
  X(HUSTR_E2M5, "E2M5: Command Center") \
  X(HUSTR_E2M6, "E2M6: Halls of the Damned") \
  X(HUSTR_E2M7, "E2M7: Spawning Vats") \
  X(HUSTR_E2M8, "E2M8: Tower of Babel") \
  X(HUSTR_E2M9, "E2M9: Fortress of Mystery") \
  X(HUSTR_E3M1, "E3M1: Hell Keep") \
  X(HUSTR_E3M2, "E3M2: Slough of Despair") \
  X(HUSTR_E3M3, "E3M3: Pandemonium") \
  X(HUSTR_E3M4, "E3M4: House of Pain") \
  X(HUSTR_E3M5, "E3M5: Unholy Cathedral") \
  X(HUSTR_E3M6, "E3M6: Mt. Erebus") \
  X(HUSTR_E3M7, "E3M7: Limbo") \
  X(HUSTR_E3M8, "E3M8: Dis") \
  X(HUSTR_E3M9, "E3M9: Warrens") \
  X(HUSTR_E4M1, "E4M1: Hell Beneath") \
  X(HUSTR_E4M2, "E4M2: Perfect Hatred") \
  X(HUSTR_E4M3, "E4M3: Sever The Wicked") \
  X(HUSTR_E4M4, "E4M4: Unruly Evil") \
  X(HUSTR_E4M5, "E4M5: They Will Repent") \
  X(HUSTR_E4M6, "E4M6: Against Thee Wickedly") \
  X(HUSTR_E4M7, "E4M7: And Hell Followed") \
  X(HUSTR_E4M8, "E4M8: Unto The Cruel") \
  X(HUSTR_E4M9, "E4M9: Fear") \
  X(HUSTR_1, "level 1: entryway") \
  X(HUSTR_2, "level 2: underhalls") \
  X(HUSTR_3, "level 3: the gantlet") \
  X(HUSTR_4, "level 4: the focus") \
  X(HUSTR_5, "level 5: the waste tunnels") \
  X(HUSTR_6, "level 6: the crusher") \
  X(HUSTR_7, "level 7: dead simple") \
  X(HUSTR_8, "level 8: tricks and traps") \
  X(HUSTR_9, "level 9: the pit") \
  X(HUSTR_10, "level 10: refueling base") \
  X(HUSTR_11, "level 11: 'o' of destruction!") \
  X(HUSTR_12, "level 12: the factory") \
  X(HUSTR_13, "level 13: downtown") \
  X(HUSTR_14, "level 14: the inmost dens") \
  X(HUSTR_15, "level 15: industrial zone") \
  X(HUSTR_16, "level 16: suburbs") \
  X(HUSTR_17, "level 17: tenements") \
  X(HUSTR_18, "level 18: the courtyard") \
  X(HUSTR_19, "level 19: the citadel") \
  X(HUSTR_20, "level 20: gotcha!") \
  X(HUSTR_21, "level 21: nirvana") \
  X(HUSTR_22, "level 22: the catacombs") \
  X(HUSTR_23, "level 23: barrels o' fun") \
  X(HUSTR_24, "level 24: the chasm") \
  X(HUSTR_25, "level 25: bloodfalls") \
  X(HUSTR_26, "level 26: the abandoned mines") \
  X(HUSTR_27, "level 27: monster condo") \
  X(HUSTR_28, "level 28: the spirit world") \
  X(HUSTR_29, "level 29: the living end") \
  X(HUSTR_30, "level 30: icon of sin") \
  X(HUSTR_31, "level 31: wolfenstein") \
  X(HUSTR_32, "level 32: grosse") \
  X(PHUSTR_1, "level 1: congo") \
  X(PHUSTR_2, "level 2: well of souls") \
  X(PHUSTR_3, "level 3: aztec") \
  X(PHUSTR_4, "level 4: caged") \
  X(PHUSTR_5, "level 5: ghost town") \
  X(PHUSTR_6, "level 6: baron's lair") \
  X(PHUSTR_7, "level 7: caughtyard") \
  X(PHUSTR_8, "level 8: realm") \
  X(PHUSTR_9, "level 9: abattoire") \
  X(PHUSTR_10, "level 10: onslaught") \
  X(PHUSTR_11, "level 11: hunted") \
  X(PHUSTR_12, "level 12: speed") \
  X(PHUSTR_13, "level 13: the crypt") \
  X(PHUSTR_14, "level 14: genesis") \
  X(PHUSTR_15, "level 15: the twilight") \
  X(PHUSTR_16, "level 16: the omen") \
  X(PHUSTR_17, "level 17: compound") \
  X(PHUSTR_18, "level 18: neurosphere") \
  X(PHUSTR_19, "level 19: nme") \
  X(PHUSTR_20, "level 20: the death domain") \
  X(PHUSTR_21, "level 21: slayer") \
  X(PHUSTR_22, "level 22: impossible mission") \
  X(PHUSTR_23, "level 23: tombstone") \
  X(PHUSTR_24, "level 24: the final frontier") \
  X(PHUSTR_25, "level 25: temple of darkness") \
  X(PHUSTR_26, "level 26: bunker") \
  X(PHUSTR_27, "level 27: anti-christ") \
  X(PHUSTR_28, "level 28: the sewers") \
  X(PHUSTR_29, "level 29: odyssey of noises") \
  X(PHUSTR_30, "level 30: the gateway of hell") \
  X(PHUSTR_31, "level 31: cyberden") \
  X(PHUSTR_32, "level 32: go 2 it") \
  X(THUSTR_1, "level 1: system control") \
  X(THUSTR_2, "level 2: human bbq") \
  X(THUSTR_3, "level 3: power control") \
  X(THUSTR_4, "level 4: wormhole") \
  X(THUSTR_5, "level 5: hanger") \
  X(THUSTR_6, "level 6: open season") \
  X(THUSTR_7, "level 7: prison") \
  X(THUSTR_8, "level 8: metal") \
  X(THUSTR_9, "level 9: stronghold") \
  X(THUSTR_10, "level 10: redemption") \
  X(THUSTR_11, "level 11: storage facility") \
  X(THUSTR_12, "level 12: crater") \
  X(THUSTR_13, "level 13: nukage processing") \
  X(THUSTR_14, "level 14: steel works") \
  X(THUSTR_15, "level 15: dead zone") \
  X(THUSTR_16, "level 16: deepest reaches") \
  X(THUSTR_17, "level 17: processing area") \
  X(THUSTR_18, "level 18: mill") \
  X(THUSTR_19, "level 19: shipping/respawning") \
  X(THUSTR_20, "level 20: central processing") \
  X(THUSTR_21, "level 21: administration center") \
  X(THUSTR_22, "level 22: habitat") \
  X(THUSTR_23, "level 23: lunar mining project") \
  X(THUSTR_24, "level 24: quarry") \
  X(THUSTR_25, "level 25: baron's den") \
  X(THUSTR_26, "level 26: ballistyx") \
  X(THUSTR_27, "level 27: mount pain") \
  X(THUSTR_28, "level 28: heck") \
  X(THUSTR_29, "level 29: river styx") \
  X(THUSTR_30, "level 30: last call") \
  X(THUSTR_31, "level 31: pharaoh") \
  X(THUSTR_32, "level 32: caribbean") \
  X(E1TEXT, \
    "Once you beat the big badasses\n" \
    "and clean out the moon base\n" \
    "you're supposed to win?\n" \
    "Where's your ticket home?\n" \
    "What the hell is this? It's not\n" \
    "supposed to end this way!\n" \
    "\n" \
    "It stinks like rotten meat, but\n" \
    "looks like the lost Deimos base.\n" \
    "You're stuck on The Shores of\n" \
    "Hell.\n" \
    "The only way out is through.") \
  X(E2TEXT, \
    "You've done it! The hideous\n" \
    "cyber-demon that ruled the\n" \
    "lost Deimos moon base has been\n" \
    "slain and you are triumphant!\n" \
    "But...where are you?\n" \
    "You clamber to the edge of the\n" \
    "moon and see the awful truth.\n" \
    "\n" \
    "Deimos floats above Hell itself!\n" \
    "You've never heard of anyone\n" \
    "escaping from Hell, but you'll\n" \
    "make the bastards sorry they\n" \
    "ever heard of you!") \
  X(E3TEXT, \
    "The loathsome spiderdemon that\n" \
    "masterminded the invasion of the\n" \
    "moon bases and caused so much\n" \
    "death has had its ass kicked.\n" \
    "\n" \
    "A hidden doorway opens.\n" \
    "You've proven too tough for\n" \
    "Hell to contain, and now Hell at\n" \
    "last plays fair -- you emerge\n" \
    "from the door to see the green\n" \
    "fields of Earth! Home at last.\n") \
  X(E4TEXT, \
    "the spider mastermind must have\n" \
    "sent forth its legions of\n" \
    "hellspawn before your final\n" \
    "confrontation with that\n" \
    "terrible beast from hell.\n" \
    "but you stepped forward and\n" \
    "brought eternal damnation\n" \
    "and suffering upon the horde as\n" \
    "a true hero would in the face\n" \
    "of evil.\n" \
    "besides, someone is gonna pay for\n" \
    "what happened to daisy.\n" \
    "next stop, hell on earth!") \
  X(C1TEXT, \
    "YOU HAVE ENTERED DEEPLY INTO\n" \
    "THE INFESTED STARPORT. SOMETHING\n" \
    "IS WRONG. THE MONSTERS HAVE\n" \
    "BROUGHT THEIR OWN REALITY WITH\n" \
    "THEM, AND THE STARPORT'S TECH\n" \
    "IS BEING SUBVERTED BY THEM.\n" \
    "AHEAD, YOU SEE AN OUTPOST OF\n" \
    "HELL, A FORTIFIED ZONE. IF YOU\n" \
    "CAN GET PAST YOU CAN PENETRATE\n" \
    "INTO THE HAUNTED HEART OF THE\n" \
    "STARBASE AND FIND THE SWITCH\n" \
    "WHICH HOLDS EARTH'S POPULATION\n" \
    "HOSTAGE.") \
  X(C2TEXT, \
    "YOU HAVE WON! YOUR VICTORY HAS\n" \
    "ENABLED HUMANKIND TO EVACUATE\n" \
    "EARTH AND ESCAPE THE NIGHTMARE.\n" \
    "NOW YOU ARE THE ONLY HUMAN LEFT.\n" \
    "CANNIBALS, ALIENS AND EVIL\n" \
    "SPIRITS ARE YOUR NEIGHBORS.\n" \
    "BUT THEN, EARTH CONTROL BEAMS\n" \
    "DOWN A MESSAGE: SENSORS HAVE\n" \
    "LOCATED THE SOURCE OF THE ALIEN\n" \
    "INVASION. THE ALIEN BASE IS IN\n" \
    "THE HEART OF YOUR OWN HOME CITY.\n" \
    "SLOWLY AND PAINFULLY YOU GET UP\n" \
    "AND RETURN TO THE FRAY.") \
  X(C3TEXT, \
    "YOU ARE AT THE CORRUPT HEART OF\n" \
    "THE CITY, SURROUNDED BY THE\n" \
    "CORPSES OF YOUR ENEMIES. YOU\n" \
    "SEE NO WAY TO DESTROY THE\n" \
    "CREATURES' ENTRYWAY ON THIS SIDE,\n" \
    "SO YOU CLENCH YOUR TEETH AND\n" \
    "PLUNGE THROUGH IT.\n" \
    "\n" \
    "THERE MUST BE A WAY TO CLOSE\n" \
    "IT ON THE OTHER SIDE. WHAT DO\n" \
    "YOU CARE IF YOU'VE GOT TO GO\n" \
    "THROUGH HELL TO GET TO IT?" \
    ) \
  X(C4TEXT, \
    "THE HORRENDOUS VISAGE OF THE\n" \
    "BIGGEST DEMON YOU'VE EVER SEEN\n" \
    "CRUMBLES BEFORE YOU, AFTER YOU\n" \
    "PUMP ROCKETS INTO HIS EXPOSED\n" \
    "BRAIN. THE MONSTERS THRASHING\n" \
    "LIMBS DEVASTATING UNTOLD MILES\n" \
    "OF HELL'S SURFACE.\n" \
    "YOU'VE DONE IT. THE INVASION IS\n" \
    "OVER. HELL IS A WRECK.\n" \
    "YOU BEGIN THE LONG TREK BACK\n" \
    "HOME. REBUILDING EARTH WILL\n" \
    "BE A LOT LESS FUN THAN\n" \
    "RUINING IT WAS.") \
  X(C5TEXT, \
    "CONGRATULATIONS, YOU'VE FOUND\n" \
    "THE SECRET LEVEL! LOOKS LIKE\n" \
    "IT'S BEEN BUILT BY HUMANS,\n" \
    "RATHER THAN DEMONS. YOU WONDER\n" \
    "WHO THE INMATES OF THIS CORNER\n" \
    "OF HELL WILL BE.") \
  X(C6TEXT, \
    "CONGRATULATIONS, YOU'VE FOUND\n" \
    "THE SUPER SECRET LEVEL!\n" \
    "YOU'D BETTER BLAZE THROUGH\n" \
    "THIS ONE!") \
  X(P1TEXT, \
    "You gloat over the Guardian's\n" \
    "steaming carcass. With its death,\n" \
    "you've wrested the Accelerator\n" \
    "from the stinking claws of Hell.\n" \
    "Damn! There was supposed to be a\n" \
    "working prototype, but the\n" \
    "demons must have taken it.\n" \
    "\n" \
    "You must find the prototype, or\n" \
    "all your struggles will be\n" \
    "wasted. Keep moving, keep\n" \
    "fighting, keep killing. Oh yes,\n" \
    "keep living, too.") \
  X(P2TEXT, \
    "Even the deadly Arch-Vile\n" \
    "labyrinth could not stop you,\n" \
    "and you've gotten to the\n" \
    "prototype Accelerator which is\n" \
    "soon efficiently and\n" \
    "permanently deactivated.\n" \
    "\n" \
    "You're good at that\n" \
    "kind of thing.") \
  X(P3TEXT, \
    "You've bashed and battered your\n" \
    "way into the heart of the\n" \
    "devil-hive. Time for a\n" \
    "Search-and-Destroy mission,\n" \
    "aimed at the Gatekeeper, whose\n" \
    "foul offspring is cascading to\n" \
    "Earth. Yeah, he's bad. But\n" \
    "you know who's worse!\n" \
    "\n" \
    "Grinning evilly, you check your\n" \
    "gear, and get ready to give the\n" \
    "bastard a little Hell of your\n" \
    "own making!") \
  X(P4TEXT, \
    "The Gatekeeper's evil face is\n" \
    "splattered all over the place.\n" \
    "As its corpse collapses, an\n" \
    "inverted Gate sucks down the\n" \
    "last prototype Accelerator and\n" \
    "the remaining demons. You're\n" \
    "done. Hell is back to pounding\n" \
    "bad dead folks instead of good\n" \
    "live ones. Tell your grandkids to\n" \
    "put a rocket launcher in your\n" \
    "coffin. If you go to Hell when\n" \
    "you die, you'll need it for some\n" \
    "final cleaning-up ...") \
  X(P5TEXT, \
    "You've found the second-hardest\n" \
    "level we got. Hope you have a\n" \
    "saved game a level or two\n" \
    "previous. If not, be prepared\n" \
    "to die aplenty.\n" \
    "For master marines only.") \
  X(P6TEXT, \
    "Betcha wondered just what WAS\n" \
    "the hardest level we had ready\n" \
    "for ya? Now you know.\n" \
    "No one gets out alive.") \
  X(T1TEXT, \
    "You've fought your way out of\n" \
    "the infested experimental labs.\n" \
    "It seems that UAC has once again\n" \
    "gulped it down. With their high\n" \
    "turnover, it must be hard for\n" \
    "poor old UAC to buy health\n" \
    "insurance nowadays..\n" \
    "\n" \
    "Ahead lies the military complex,\n" \
    "now swarming with diseased\n" \
    "horrors hot to get their teeth\n" \
    "into you. With luck, it still has\n" \
    "some ordnance laying around.") \
  X(T2TEXT, \
    "You hear the grinding of heavy\n" \
    "machinery ahead. You sure hope\n" \
    "they're not stamping out new\n" \
    "hellspawn, but you're ready to\n" \
    "ream out a whole herd if you\n" \
    "have to. They might be planning a\n" \
    "blood feast, but you feel about\n" \
    "as mean as two thousand maniacs\n" \
    "packed into one mad killer.\n" \
    "\n" \
    "You don't plan to go down easy.") \
  X(T3TEXT, \
    "The vista opening ahead looks\n" \
    "real damn familiar. Smells\n" \
    "familiar, too -- like fried\n" \
    "excrement. You didn't like this\n" \
    "place before, and you sure as\n" \
    "hell ain't planning to like it\n" \
    "now. The more you brood on it,\n" \
    "the madder you get.\n" \
    "Hefting your gun, an evil grin\n" \
    "trickles onto your face.\n" \
    "Time to take some names.") \
  X(T4TEXT, \
    "Suddenly, all is silent from one\n" \
    "horizon to the other. The echo\n" \
    "of Hell fades away, the\n" \
    "nightmare sky turns blue, the\n" \
    "heaps of monster corpses\n" \
    "evaporate along with the evil\n" \
    "stench. Jeeze, maybe you've done\n" \
    "it. Have you really won?\n" \
    "\n" \
    "Something rumbles in the\n" \
    "distance. A blue light begins to\n" \
    "glow inside the ruined skull of\n" \
    "the demon-spitter.") \
  X(T5TEXT, \
    "What now? Looks totally\n" \
    "different. Kind of like King Tut's\n" \
    "condo. Well, whatever's here\n" \
    "can't be any worse than usual.\n" \
    "Can it? Or maybe it's best to let\n" \
    "sleeping gods lie..") \
  X(T6TEXT, \
    "Time for a vacation. You've burst\n" \
    "the bowels of hell and by golly\n" \
    "you're ready for a break. You\n" \
    "mutter to yourself, Maybe\n" \
    "someone else can kick Hell's ass\n" \
    "next time around. Ahead lies a\n" \
    "quiet town, with peaceful\n" \
    "flowing water, quaint buildings,\n" \
    "and presumably no Hellspawn.\n" \
    "\n" \
    "As you step off the transport,\n" \
    "you hear the stomp of a\n" \
    "cyberdemon's iron shoe.") \
  X(CC_ZOMBIE, "ZOMBIEMAN") \
  X(CC_SHOTGUN, "SHOTGUN GUY") \
  X(CC_HEAVY, "HEAVY WEAPON DUDE") \
  X(CC_IMP, "IMP") \
  X(CC_DEMON, "DEMON") \
  X(CC_LOST, "LOST SOUL") \
  X(CC_CACO, "CACODEMON") \
  X(CC_HELL, "HELL KNIGHT") \
  X(CC_BARON, "BARON OF HELL") \
  X(CC_ARACH, "ARACHNOTRON") \
  X(CC_PAIN, "PAIN ELEMENTAL") \
  X(CC_REVEN, "REVENANT") \
  X(CC_MANCU, "MANCUBUS") \
  X(CC_ARCH, "ARCH-VILE") \
  X(CC_SPIDER, "THE SPIDER MASTERMIND") \
  X(CC_CYBER, "THE CYBERDEMON") \
  X(CC_HERO, "OUR HERO") \
  /* m_cheat.c */ \
  X(STSTR_DQDON, "Degreelessness Mode On") \
  X(STSTR_DQDOFF, "Degreelessness Mode Off") \
  X(STSTR_KFAADDED, "Very Happy Ammo Added") \
  X(STSTR_FAADDED, "Ammo (no keys) Added") \
  X(STSTR_NCON, "No Clipping Mode ON") \
  X(STSTR_NCOFF, "No Clipping Mode OFF") \
  X(STSTR_CHOPPERS, "... doesn't suck - GM") \
  /* Lump names, last: f_finale.c's backgrounds and g_game.c's skies. */ \
  X(FLOOR4_8, "FLOOR4_8") \
  X(SFLR6_1, "SFLR6_1") \
  X(MFLR8_4, "MFLR8_4") \
  X(MFLR8_3, "MFLR8_3") \
  X(SLIME16, "SLIME16") \
  X(RROCK14, "RROCK14") \
  X(RROCK07, "RROCK07") \
  X(RROCK17, "RROCK17") \
  X(RROCK13, "RROCK13") \
  X(RROCK19, "RROCK19") \
  X(SKY1, "SKY1") \
  X(SKY2, "SKY2") \
  X(SKY3, "SKY3") \
  X(SKY4, "SKY4")

typedef enum
{
#define DEH_TEXT_ENUM(id, text) TXT_##id,
  DEH_TEXTS(DEH_TEXT_ENUM)
#undef DEH_TEXT_ENUM
  NUMTEXTS
} textnum_t;

#define DEH_VANILLA_TEXTS(X) \
  X(GOTMEDINEED, "Picked up a medikit that you REALLY need!") \
  X(GOTSHELLBOX, "Picked up a box of shotgun shells.") \
  X(GOTBACKPACK, "Picked up a backpack full of ammo!") \
  X(PD_BLUEO, "You need a blue key to activate this object") \
  X(PD_REDO, "You need a red key to activate this object") \
  X(PD_YELLOWO, "You need a yellow key to activate this object") \
  X(PD_BLUEK, "You need a blue key to open this door") \
  X(PD_REDK, "You need a red key to open this door") \
  X(PD_YELLOWK, "You need a yellow key to open this door") \
  X(PHUSTR_25, "level 25: the temple of darkness") \
  X(E1TEXT, \
    "Once you beat the big badasses and\n" \
    "clean out the moon base you're supposed\n" \
    "to win, aren't you? Aren't you? Where's\n" \
    "your fat reward and ticket home? What\n" \
    "the hell is this? It's not supposed to\n" \
    "end this way!\n" \
    "\n" \
    "It stinks like rotten meat, but looks\n" \
    "like the lost Deimos base.  Looks like\n" \
    "you're stuck on The Shores of Hell.\n" \
    "The only way out is through.\n" \
    "\n" \
    "To continue the DOOM experience, play\n" \
    "The Shores of Hell and its amazing\n" \
    "sequel, Inferno!\n") \
  X(E2TEXT, \
    "You've done it! The hideous cyber-\n" \
    "demon lord that ruled the lost Deimos\n" \
    "moon base has been slain and you\n" \
    "are triumphant! But ... where are\n" \
    "you? You clamber to the edge of the\n" \
    "moon and look down to see the awful\n" \
    "truth.\n" \
    "\n" \
    "Deimos floats above Hell itself!\n" \
    "You've never heard of anyone escaping\n" \
    "from Hell, but you'll make the bastards\n" \
    "sorry they ever heard of you! Quickly,\n" \
    "you rappel down to  the surface of\n" \
    "Hell.\n" \
    "\n" \
    "Now, it's on to the final chapter of\n" \
    "DOOM! -- Inferno.") \
  X(E3TEXT, \
    "The loathsome spiderdemon that\n" \
    "masterminded the invasion of the moon\n" \
    "bases and caused so much death has had\n" \
    "its ass kicked for all time.\n" \
    "\n" \
    "A hidden doorway opens and you enter.\n" \
    "You've proven too tough for Hell to\n" \
    "contain, and now Hell at last plays\n" \
    "fair -- for you emerge from the door\n" \
    "to see the green fields of Earth!\n" \
    "Home at last.\n" \
    "\n" \
    "You wonder what's been happening on\n" \
    "Earth while you were battling evil\n" \
    "unleashed. It's good that no Hell-\n" \
    "spawn could have come through that\n" \
    "door with you ...") \
  X(E4TEXT, \
    "the spider mastermind must have sent forth\n" \
    "its legions of hellspawn before your\n" \
    "final confrontation with that terrible\n" \
    "beast from hell.  but you stepped forward\n" \
    "and brought forth eternal damnation and\n" \
    "suffering upon the horde as a true hero\n" \
    "would in the face of something so evil.\n" \
    "\n" \
    "besides, someone was gonna pay for what\n" \
    "happened to daisy, your pet rabbit.\n" \
    "\n" \
    "but now, you see spread before you more\n" \
    "potential pain and gibbitude as a nation\n" \
    "of demons run amok among our cities.\n" \
    "\n" \
    "next stop, hell on earth!") \
  X(C1TEXT, \
    "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\n" \
    "STARPORT. BUT SOMETHING IS WRONG. THE\n" \
    "MONSTERS HAVE BROUGHT THEIR OWN REALITY\n" \
    "WITH THEM, AND THE STARPORT'S TECHNOLOGY\n" \
    "IS BEING SUBVERTED BY THEIR PRESENCE.\n" \
    "\n" \
    "AHEAD, YOU SEE AN OUTPOST OF HELL, A\n" \
    "FORTIFIED ZONE. IF YOU CAN GET PAST IT,\n" \
    "YOU CAN PENETRATE INTO THE HAUNTED HEART\n" \
    "OF THE STARBASE AND FIND THE CONTROLLING\n" \
    "SWITCH WHICH HOLDS EARTH'S POPULATION\n" \
    "HOSTAGE.") \
  X(C2TEXT, \
    "YOU HAVE WON! YOUR VICTORY HAS ENABLED\n" \
    "HUMANKIND TO EVACUATE EARTH AND ESCAPE\n" \
    "THE NIGHTMARE.  NOW YOU ARE THE ONLY\n" \
    "HUMAN LEFT ON THE FACE OF THE PLANET.\n" \
    "CANNIBAL MUTATIONS, CARNIVOROUS ALIENS,\n" \
    "AND EVIL SPIRITS ARE YOUR ONLY NEIGHBORS.\n" \
    "YOU SIT BACK AND WAIT FOR DEATH, CONTENT\n" \
    "THAT YOU HAVE SAVED YOUR SPECIES.\n" \
    "\n" \
    "BUT THEN, EARTH CONTROL BEAMS DOWN A\n" \
    "MESSAGE FROM SPACE: \"SENSORS HAVE LOCATED\n" \
    "THE SOURCE OF THE ALIEN INVASION. IF YOU\n" \
    "GO THERE, YOU MAY BE ABLE TO BLOCK THEIR\n" \
    "ENTRY.  THE ALIEN BASE IS IN THE HEART OF\n" \
    "YOUR OWN HOME CITY, NOT FAR FROM THE\n" \
    "STARPORT.\" SLOWLY AND PAINFULLY YOU GET\n" \
    "UP AND RETURN TO THE FRAY.") \
  X(C3TEXT, \
    "YOU ARE AT THE CORRUPT HEART OF THE CITY,\n" \
    "SURROUNDED BY THE CORPSES OF YOUR ENEMIES.\n" \
    "YOU SEE NO WAY TO DESTROY THE CREATURES'\n" \
    "ENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\n" \
    "TEETH AND PLUNGE THROUGH IT.\n" \
    "\n" \
    "THERE MUST BE A WAY TO CLOSE IT ON THE\n" \
    "OTHER SIDE. WHAT DO YOU CARE IF YOU'VE\n" \
    "GOT TO GO THROUGH HELL TO GET TO IT?") \
  X(C4TEXT, \
    "THE HORRENDOUS VISAGE OF THE BIGGEST\n" \
    "DEMON YOU'VE EVER SEEN CRUMBLES BEFORE\n" \
    "YOU, AFTER YOU PUMP YOUR ROCKETS INTO\n" \
    "HIS EXPOSED BRAIN. THE MONSTER SHRIVELS\n" \
    "UP AND DIES, ITS THRASHING LIMBS\n" \
    "DEVASTATING UNTOLD MILES OF HELL'S\n" \
    "SURFACE.\n" \
    "\n" \
    "YOU'VE DONE IT. THE INVASION IS OVER.\n" \
    "EARTH IS SAVED. HELL IS A WRECK. YOU\n" \
    "WONDER WHERE BAD FOLKS WILL GO WHEN THEY\n" \
    "DIE, NOW. WIPING THE SWEAT FROM YOUR\n" \
    "FOREHEAD YOU BEGIN THE LONG TREK BACK\n" \
    "HOME. REBUILDING EARTH OUGHT TO BE A\n" \
    "LOT MORE FUN THAN RUINING IT WAS.\n") \
  X(C5TEXT, \
    "CONGRATULATIONS, YOU'VE FOUND THE SECRET\n" \
    "LEVEL! LOOKS LIKE IT'S BEEN BUILT BY\n" \
    "HUMANS, RATHER THAN DEMONS. YOU WONDER\n" \
    "WHO THE INMATES OF THIS CORNER OF HELL\n" \
    "WILL BE.") \
  X(C6TEXT, \
    "CONGRATULATIONS, YOU'VE FOUND THE\n" \
    "SUPER SECRET LEVEL!  YOU'D BETTER\n" \
    "BLAZE THROUGH THIS ONE!\n") \
  X(P1TEXT, \
    "You gloat over the steaming carcass of the\n" \
    "Guardian.  With its death, you've wrested\n" \
    "the Accelerator from the stinking claws\n" \
    "of Hell.  You relax and glance around the\n" \
    "room.  Damn!  There was supposed to be at\n" \
    "least one working prototype, but you can't\n" \
    "see it. The demons must have taken it.\n" \
    "\n" \
    "You must find the prototype, or all your\n" \
    "struggles will have been wasted. Keep\n" \
    "moving, keep fighting, keep killing.\n" \
    "Oh yes, keep living, too.") \
  X(P2TEXT, \
    "Even the deadly Arch-Vile labyrinth could\n" \
    "not stop you, and you've gotten to the\n" \
    "prototype Accelerator which is soon\n" \
    "efficiently and permanently deactivated.\n" \
    "\n" \
    "You're good at that kind of thing.") \
  X(P3TEXT, \
    "You've bashed and battered your way into\n" \
    "the heart of the devil-hive.  Time for a\n" \
    "Search-and-Destroy mission, aimed at the\n" \
    "Gatekeeper, whose foul offspring is\n" \
    "cascading to Earth.  Yeah, he's bad. But\n" \
    "you know who's worse!\n" \
    "\n" \
    "Grinning evilly, you check your gear, and\n" \
    "get ready to give the bastard a little Hell\n" \
    "of your own making!") \
  X(P4TEXT, \
    "The Gatekeeper's evil face is splattered\n" \
    "all over the place.  As its tattered corpse\n" \
    "collapses, an inverted Gate forms and\n" \
    "sucks down the shards of the last\n" \
    "prototype Accelerator, not to mention the\n" \
    "few remaining demons.  You're done. Hell\n" \
    "has gone back to pounding bad dead folks \n" \
    "instead of good live ones.  Remember to\n" \
    "tell your grandkids to put a rocket\n" \
    "launcher in your coffin. If you go to Hell\n" \
    "when you die, you'll need it for some\n" \
    "final cleaning-up ...") \
  X(P5TEXT, \
    "You've found the second-hardest level we\n" \
    "got. Hope you have a saved game a level or\n" \
    "two previous.  If not, be prepared to die\n" \
    "aplenty. For master marines only.") \
  X(P6TEXT, \
    "Betcha wondered just what WAS the hardest\n" \
    "level we had ready for ya?  Now you know.\n" \
    "No one gets out alive.") \
  X(T1TEXT, \
    "You've fought your way out of the infested\n" \
    "experimental labs.   It seems that UAC has\n" \
    "once again gulped it down.  With their\n" \
    "high turnover, it must be hard for poor\n" \
    "old UAC to buy corporate health insurance\n" \
    "nowadays..\n" \
    "\n" \
    "Ahead lies the military complex, now\n" \
    "swarming with diseased horrors hot to get\n" \
    "their teeth into you. With luck, the\n" \
    "complex still has some warlike ordnance\n" \
    "laying around.") \
  X(T2TEXT, \
    "You hear the grinding of heavy machinery\n" \
    "ahead.  You sure hope they're not stamping\n" \
    "out new hellspawn, but you're ready to\n" \
    "ream out a whole herd if you have to.\n" \
    "They might be planning a blood feast, but\n" \
    "you feel about as mean as two thousand\n" \
    "maniacs packed into one mad killer.\n" \
    "\n" \
    "You don't plan to go down easy.") \
  X(T3TEXT, \
    "The vista opening ahead looks real damn\n" \
    "familiar. Smells familiar, too -- like\n" \
    "fried excrement. You didn't like this\n" \
    "place before, and you sure as hell ain't\n" \
    "planning to like it now. The more you\n" \
    "brood on it, the madder you get.\n" \
    "Hefting your gun, an evil grin trickles\n" \
    "onto your face. Time to take some names.") \
  X(T4TEXT, \
    "Suddenly, all is silent, from one horizon\n" \
    "to the other. The agonizing echo of Hell\n" \
    "fades away, the nightmare sky turns to\n" \
    "blue, the heaps of monster corpses start \n" \
    "to evaporate along with the evil stench \n" \
    "that filled the air. Jeeze, maybe you've\n" \
    "done it. Have you really won?\n" \
    "\n" \
    "Something rumbles in the distance.\n" \
    "A blue light begins to glow inside the\n" \
    "ruined skull of the demon-spitter.") \
  X(T5TEXT, \
    "What now? Looks totally different. Kind\n" \
    "of like King Tut's condo. Well,\n" \
    "whatever's here can't be any worse\n" \
    "than usual. Can it?  Or maybe it's best\n" \
    "to let sleeping gods lie..") \
  X(T6TEXT, \
    "Time for a vacation. You've burst the\n" \
    "bowels of hell and by golly you're ready\n" \
    "for a break. You mutter to yourself,\n" \
    "Maybe someone else can kick Hell's ass\n" \
    "next time around. Ahead lies a quiet town,\n" \
    "with peaceful flowing water, quaint\n" \
    "buildings, and presumably no Hellspawn.\n" \
    "\n" \
    "As you step off the transport, you hear\n" \
    "the stomp of a cyberdemon's iron shoe.")

#endif
