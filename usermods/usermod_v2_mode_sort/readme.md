# Mode Sort

v2 usermod that provides data about modes and
palettes to other usermods. Notably it provides:
* A direct method for a mode or palette name
* Ability to retrieve mode and palette names in 
  alphabetical order

```char **getModesQStrings()```

Provides a char* array (pointers) to the names of the
palettes contained in JSON_mode_names, in the same order as 
JSON_mode_names. These strings end in double quote (")
(or \0 if there is a problem).

```byte *getModesAlphaIndexes()```

A byte array designating the indexes of names of the
modes in alphabetical order. "Solid" will always remain 
at the top of the list.

```char **getPalettesQStrings()```

Provides a char* array (pointers) to the names of the
palettes contained in JSON_palette_names, in the same order as 
JSON_palette_names. These strings end in double quote (")
(or \0 if there is a problem).

```byte *getPalettesAlphaIndexes()```

A byte array designating the indexes of names of the
palettes in alphabetical order. "Default" and those
starting with "(" will always remain at the top of the list.
