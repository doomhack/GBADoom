if not exist ..\wads mkdir ..\wads
gbawadutil.exe -in doom1.wad -out ..\wads\doom1.wad -gus data\dgguspat\timidity.cfg -oversample 4
gbawadutil.exe -in doomu.wad -out ..\wads\doomu.wad -gus data\dgguspat\timidity.cfg -oversample 4
gbawadutil.exe -in doom2.wad -out ..\wads\doom2.wad -gus data\dgguspat\timidity.cfg -oversample 4
gbawadutil.exe -in plutonia.wad -out ..\wads\plutonia.wad -gus data\dgguspat\timidity.cfg -oversample 4
gbawadutil.exe -in tnt.wad -out ..\wads\tnt.wad -gus data\dgguspat\timidity.cfg -oversample 4

gbawadutil.exe -in doomu.wad -out ..\wads\sigil.wad -pwad sigil.wad -gus data\dgguspat\timidity.cfg -oversample 4
