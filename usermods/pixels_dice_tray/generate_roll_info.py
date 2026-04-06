'''
File for generating roll labels and info text for the InfoMenu.

Uses a very limited markdown language for styling text.
'''
import math
from pathlib import Path
import re
from textwrap import indent

# Variables for calculating values in info text
CASTER_LEVEL = 9
SPELL_ABILITY_MOD = 6
BASE_ATK_BONUS = 6
SIZE_BONUS = 1
STR_BONUS = 2
DEX_BONUS = -1

# TFT library color values
TFT_BLACK       =0x0000
TFT_NAVY        =0x000F
TFT_DARKGREEN   =0x03E0
TFT_DARKCYAN    =0x03EF
TFT_MAROON      =0x7800
TFT_PURPLE      =0x780F
TFT_OLIVE       =0x7BE0
TFT_LIGHTGREY   =0xD69A
TFT_DARKGREY    =0x7BEF
TFT_BLUE        =0x001F
TFT_GREEN       =0x07E0
TFT_CYAN        =0x07FF
TFT_RED         =0xF800
TFT_MAGENTA     =0xF81F
TFT_YELLOW      =0xFFE0
TFT_WHITE       =0xFFFF
TFT_ORANGE      =0xFDA0
TFT_GREENYELLOW =0xB7E0
TFT_PINK        =0xFE19
TFT_BROWN       =0x9A60
TFT_GOLD        =0xFEA0
TFT_SILVER      =0xC618
TFT_SKYBLUE     =0x867D
TFT_VIOLET      =0x915C


class Size:
    def __init__(self, w, h):
        self.w = w
        self.h = h


# Font 1 6x8
# Font 2 12x16
CHAR_SIZE = {
    1: Size(6, 8),
    2: Size(12, 16),
}

SCREEN_SIZE = Size(128, 128)

# Calculates distance for short range spell.
def short_range() -> int:
    return 25 + 5 * CASTER_LEVEL

# Entries in markdown language.
# Parameter 0 of the tuple is the roll name
# Parameter 1 of the tuple is the roll info.
# The text will be shown when the roll type is selected. An error will be raised
# if the text would unexpectedly goes past the end of the screen. There are a
# few styling parameters that need to be on their own lines:
# $COLOR - The color for the text
# $SIZE - Sets the text size (see CHAR_SIZE)
# $WRAP - By default text won't wrap and generate an error. This enables text wrapping. Lines will wrap mid-word.
ENTRIES = [
    tuple(["Barb Chain", f'''\
$COLOR({TFT_RED})
Barb Chain
$COLOR({TFT_WHITE})
Atk/CMD {BASE_ATK_BONUS + SPELL_ABILITY_MOD}
Range: {short_range()}
$WRAP(1)
$SIZE(1)
Summon {1 + math.floor((CASTER_LEVEL-1)/3)} chains. Make a melee atk 1d6 or a trip CMD=AT. On a hit make Will save or shaken 1d4 rnds.
''']),
    tuple(["Saves", f'''\
$COLOR({TFT_GREEN})
Saves
$COLOR({TFT_WHITE})
FORT 8
REFLEX 8
WILL 9
''']),
    tuple(["Skill", f'''\
Skill
''']),
    tuple(["Attack", f'''\
Attack
Melee +{BASE_ATK_BONUS + SIZE_BONUS + STR_BONUS}
Range +{BASE_ATK_BONUS + SIZE_BONUS + DEX_BONUS}
''']),
    tuple(["Cure", f'''\
Cure
Lit 1d8+{min(5, CASTER_LEVEL)}
Mod 2d8+{min(10, CASTER_LEVEL)}
Ser 3d8+{min(15, CASTER_LEVEL)}
''']),
    tuple(["Concentrate", f'''\
Concentrat
+{CASTER_LEVEL + SPELL_ABILITY_MOD}
$SIZE(1)
Defensive 15+2*SP_LV
Dmg 10+DMG+SP_LV
Grapple 10+CMB+SP_LV
''']),
]

RE_SIZE = re.compile(r'\$SIZE\(([0-9])\)')
RE_COLOR = re.compile(r'\$COLOR\(([0-9]+)\)')
RE_WRAP = re.compile(r'\$WRAP\(([0-9])\)')

END_HEADER_TXT = '// GENERATED\n'

def main():
    roll_info_file = Path(__file__).parent / 'roll_info.h'
    old_contents = open(roll_info_file, 'r').read()

    end_header = old_contents.index(END_HEADER_TXT)

    with open(roll_info_file, 'w') as fd:
        fd.write(old_contents[:end_header+len(END_HEADER_TXT)])

        for key, entry in enumerate(ENTRIES):
            size = 2
            wrap = False
            y_loc = 0
            results = []
            for line in entry[1].splitlines():
                if line.startswith('$'):
                    m_size = RE_SIZE.match(line)
                    m_color = RE_COLOR.match(line)
                    m_wrap = RE_WRAP.match(line)
                    if m_size:
                        size = int(m_size.group(1))
                        results.append(f'tft.setTextSize({size});')
                    elif m_color:
                        results.append(
                            f'tft.setTextColor({int(m_color.group(1))});')
                    elif m_wrap:
                        wrap = bool(int(m_wrap.group(1)))
                    else:
                        print(f'Entry {key} unknown modifier "{line}".')
                        exit(1)
                else:
                    max_chars_per_line = math.floor(
                        SCREEN_SIZE.w / CHAR_SIZE[size].w)
                    if len(line) > max_chars_per_line:
                        if wrap:
                            while len(line) > max_chars_per_line:
                                results.append(
                                    f'tft.println("{line[:max_chars_per_line]}");')
                                line = line[max_chars_per_line:].lstrip()
                                y_loc += CHAR_SIZE[size].h
                        else:
                            print(f'Entry {key} line "{line}" too long.')
                            exit(1)

                    if len(line) > 0:
                        y_loc += CHAR_SIZE[size].h
                        results.append(f'tft.println("{line}");')

                    if y_loc > SCREEN_SIZE.h:
                        print(
                            f'Entry {key} line "{line}" went past bottom of screen.')
                        exit(1)

            result = indent('\n'.join(results), '  ')

            fd.write(f'''\
static void PrintRoll{key}() {{
{result}
}}

''')

        results = []
        for key, entry in enumerate(ENTRIES):
            results.append(f'''\
case {key}:
  return "{entry[0]}";''')

        cases = indent('\n'.join(results), '    ')

        fd.write(f'''\
static const char* GetRollName(uint8_t key) {{
  switch (key) {{
{cases}
  }}
  return "";
}}

''')

        results = []
        for key, entry in enumerate(ENTRIES):
            results.append(f'''\
case {key}:
  PrintRoll{key}();
  return;''')

        cases = indent('\n'.join(results), '    ')

        fd.write(f'''\
static void PrintRollInfo(uint8_t key) {{
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  switch (key) {{
{cases}
  }}
  tft.setTextColor(TFT_RED);
  tft.setCursor(0, 60);
  tft.println("Unknown");
}}

''')

        fd.write(f'static constexpr size_t NUM_ROLL_INFOS = {len(ENTRIES)};\n')


main()
