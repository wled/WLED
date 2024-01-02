#pragma once

#include "wled.h"

//
// v2 usermod that provides data about modes and
// palettes to other usermods. Notably it provides:
// * A direct method for a mode or palette name
// * Ability to retrieve mode and palette names in 
//   alphabetical order
// 
// char **getModesQStrings()
// Provides an array of char* (pointers) to the names of the
// palettes within JSON_mode_names, in the same order as 
// JSON_mode_names. These strings end in double quote (")
// (or \0 if there is a problem).
//
// byte *getModesAlphaIndexes()
// An array of byte designating the indexes of names of the
// modes in alphabetical order. "Solid" will always remain 
// at the front of the list.
//
// char **getPalettesQStrings()
// Provides an array of char* (pointers) to the names of the
// palettes within JSON_palette_names, in the same order as 
// JSON_palette_names. These strings end in double quote (")
// (or \0 if there is a problem).
//
// byte *getPalettesAlphaIndexes()
// An array of byte designating the indexes of names of the
// palettes in alphabetical order. "Default" and those
// starting with "(" will always remain at the front of the list.
//

// Number of modes at the start of the list to not sort
#define MODE_SORT_SKIP_COUNT 1

// Which list is being sorted
char **listBeingSorted = nullptr;

/**
 * Modes and palettes are stored as strings that
 * end in a quote character. Compare two of them.
 * We are comparing directly within either
 * JSON_mode_names or JSON_palette_names.
 */
int re_qstringCmp(const void *ap, const void *bp) {
    char *a = listBeingSorted[*((byte *)ap)];
    char *b = listBeingSorted[*((byte *)bp)];
    int i = 0;
    do {
        char aVal = pgm_read_byte_near(a + i);
        if (aVal >= 97 && aVal <= 122) {
            // Lowercase
            aVal -= 32;
        }
        char bVal = pgm_read_byte_near(b + i);
        if (bVal >= 97 && bVal <= 122) {
            // Lowercase
            bVal -= 32;
        }
        // Really we shouldn't ever get to '\0'
        if (aVal == '"' || bVal == '"' || aVal == '\0' || bVal == '\0') {
            // We're done. one is a substring of the other
            // or something happenend and the quote didn't stop us.
            if (aVal == bVal) {
                // Same value, probably shouldn't happen
                // with this dataset
                return 0;
            }
            else if (aVal == '"' || aVal == '\0') {
                return -1;
            }
            else {
                return 1;
            }
        }
        if (aVal == bVal) {
            // Same characters. Move to the next.
            i++;
            continue;
        }
        // We're done
        if (aVal < bVal) {
            return -1;
        }
        else {
            return 1;
        }
    } while (true);
    // We shouldn't get here.
    return 0;
}

class ModeSortUsermod : public Usermod {
private:

    // Pointers the start of the mode names within JSON_mode_names
    char **modes_qstrings = nullptr;

    // Array of mode indexes in alphabetical order.
    byte *modes_alpha_indexes = nullptr;

    // Pointers the start of the palette names within JSON_palette_names
    char **palettes_qstrings = nullptr;

    // Array of palette indexes in alphabetical order.
    byte *palettes_alpha_indexes = nullptr;

public:
    /**
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() {
        // Sort the modes and palettes on startup
        // as they are guarantted to change.
        sortModesAndPalettes();
    }

    char **getModesQStrings() {
        return modes_qstrings;
    }

    byte *getModesAlphaIndexes() {
        return modes_alpha_indexes;
    }

    char **getPalettesQStrings() {
        return palettes_qstrings;
    }

    byte *getPalettesAlphaIndexes() {
        return palettes_alpha_indexes;
    }

    /**
     * This Usermod doesn't have anything for loop.
     */
    void loop() {}

    /**
     * Sort the modes and palettes to the index arrays
     * modes_alpha_indexes and palettes_alpha_indexes.
     */
    void sortModesAndPalettes() {
        modes_qstrings = re_findModeStrings(JSON_mode_names, strip.getModeCount());
        modes_alpha_indexes = re_initIndexArray(strip.getModeCount());
        re_sortModes(modes_qstrings, modes_alpha_indexes, strip.getModeCount(), MODE_SORT_SKIP_COUNT);

        palettes_qstrings = re_findModeStrings(JSON_palette_names, strip.getPaletteCount());
        palettes_alpha_indexes = re_initIndexArray(strip.getPaletteCount());

        int skipPaletteCount = 1;
        while (true) {
            // How many palette names start with '*' and should not be sorted?
            // (Also skipping the first one, 'Default').
            if (pgm_read_byte_near(palettes_qstrings[skipPaletteCount]) == '*') {
                skipPaletteCount++;
            }
            else {
                break;
            }
        }
        re_sortModes(palettes_qstrings, palettes_alpha_indexes, strip.getPaletteCount(), skipPaletteCount);
    }

    byte *re_initIndexArray(int numModes) {
        byte *indexes = (byte *)malloc(sizeof(byte) * numModes);
        for (byte i = 0; i < numModes; i++) {
            indexes[i] = i;
        }
        return indexes;
    }

    /**
     * Return an array of mode or palette names from the JSON string.
     * They don't end in '\0', they end in '"'. 
     */
    char **re_findModeStrings(const char json[], int numModes) {
        char **modeStrings = (char **)malloc(sizeof(char *) * numModes);
        uint8_t modeIndex = 0;
        bool insideQuotes = false;
        // advance past the mark for markLineNum that may exist.
        char singleJsonSymbol;

        // Find the mode name in JSON
        bool complete = false;
        for (size_t i = 0; i < strlen_P(json); i++) {
            singleJsonSymbol = pgm_read_byte_near(json + i);
            if (singleJsonSymbol == '\0') break;
            switch (singleJsonSymbol) {
            case '"':
                insideQuotes = !insideQuotes;
                if (insideQuotes) {
                    // We have a new mode or palette
                    modeStrings[modeIndex] = (char *)(json + i + 1);
                }
                break;
            case '[':
                break;
            case ']':
                if (!insideQuotes) complete = true;
                break;
            case ',':
                if (!insideQuotes) modeIndex++;
            default:
                if (!insideQuotes) break;
            }
            if (complete) break;
        }
        return modeStrings;
    }

    /**
   * Sort either the modes or the palettes using quicksort.
   */
    void re_sortModes(char **modeNames, byte *indexes, int count, int numSkip) {
        listBeingSorted = modeNames;
        qsort(indexes + numSkip, count - numSkip, sizeof(byte), re_qstringCmp);
        listBeingSorted = nullptr;
    }

    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject &root) {}

    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject &root) {}

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
        return USERMOD_ID_MODE_SORT;
    }
};
