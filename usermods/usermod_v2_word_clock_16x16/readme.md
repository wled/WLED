# Wordclock MK2 | WLED 16x16 w/ ESP32

## Resources
- NAS-00:/hardware/X-Carve/Projects/2017/Word Clock
- [Wordclock MK1 - w/ Text Shift / Rotation (Adobe Illistrator)](https://docs.google.com/spreadsheets/d/1PluM_poY26YVuXqRocmyo1mvG5tT44V26rKZcX5UzbI/edit?gid=0#gid=0)
- [Wordclock MK1 (Arduino w/ Firmata /w NeoPixel & Raspberry Pi Zero w/ Node-RED) - Build Sheet](https://docs.google.com/spreadsheets/d/1UgpLxv2-_UMIiSN81n5ciU93GWFkNPKmxRbwsBQ3MRw/edit?gid=35318254#gid=35318254)

## Layout
A 16×16 RGBW LED matrix occupies the center of the display for the word clock functionality. Outside this matrix, each corner contains a dedicated push button and a corresponding addressable RGBW LED on a seperate strip from the 16x16 matrix. (4x discrete RGBW LEDs and 4x push buttons).

### Layout (Words)

|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
| - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| I | T | K | I | S | S | T | W | E | N | T | Y | T | O | N | E |
| T | W | O | W | T | E | N | J | T | H | I | R | T | E | E | N |
| F | I | V | E | M | E | L | E | V | E | N | B | F | O | U | R |
| T | H | R | E | E | P | N | I | N | E | T | E | E | N | S | U |
| F | O | U | R | T | E | E | N | M | I | D | N | I | G | H | T |
| S | I | X | T | E | E | N | A | E | I | G | H | T | E | E | N |
| S | E | V | E | N | T | E | E | N | Y | T | W | E | L | V | E |
| M | I | N | U | T | E | S | D | Q | U | A | R | T | E | R | B |
| H | A | L | F | J | P | A | S | T | Q | U | N | T | I | L | L |
| S | E | V | E | N | T | H | R | E | E | E | L | E | V | E | N |
| E | I | G | H | T | E | N | I | N | E | T | W | E | L | V | E |
| S | I | X | F | I | V | E | F | O | U | R | T | W | O | N | E |
| Z | O | C | L | O | C | K | J | A | T | I | N | X | T | H | E |
| A | F | T | E | R | N | O | O | N | M | O | R | N | I | N | G |
| A | T | K | N | I | G | H | T | Z | E | V | E | N | I | N | G |
| & | W | A | R | M | C | O | O | L | H | O | T | C | O | L | D |

### Layout (LED IDs)
> NOTE: 16x16, serpentine.

|     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0   | 1   | 2   | 3   | 4   | 5   | 6   | 7   | 8   | 9   | 10  | 11  | 12  | 13  | 14  | 15  |
| 16  | 17  | 18  | 19  | 20  | 21  | 22  | 23  | 24  | 25  | 26  | 27  | 28  | 29  | 30  | 31  |
| 32  | 33  | 34  | 35  | 36  | 37  | 38  | 39  | 40  | 41  | 42  | 43  | 44  | 45  | 46  | 47  |
| 48  | 49  | 50  | 51  | 52  | 53  | 54  | 55  | 56  | 57  | 58  | 59  | 60  | 61  | 62  | 63  |
| 64  | 65  | 66  | 67  | 68  | 69  | 70  | 71  | 72  | 73  | 74  | 75  | 76  | 77  | 78  | 79  |
| 80  | 81  | 82  | 83  | 84  | 85  | 86  | 87  | 88  | 89  | 90  | 91  | 92  | 93  | 94  | 95  |
| 96  | 97  | 98  | 99  | 100 | 101 | 102 | 103 | 104 | 105 | 106 | 107 | 108 | 109 | 110 | 111 |
| 112 | 113 | 114 | 115 | 116 | 117 | 118 | 119 | 120 | 121 | 122 | 123 | 124 | 125 | 126 | 127 |
| 128 | 129 | 130 | 131 | 132 | 133 | 134 | 135 | 136 | 137 | 138 | 139 | 140 | 141 | 142 | 143 |
| 144 | 145 | 146 | 147 | 148 | 149 | 150 | 151 | 152 | 153 | 154 | 155 | 156 | 157 | 158 | 159 |
| 160 | 161 | 162 | 163 | 164 | 165 | 166 | 167 | 168 | 169 | 170 | 171 | 172 | 173 | 174 | 175 |
| 176 | 177 | 178 | 179 | 180 | 181 | 182 | 183 | 184 | 185 | 186 | 187 | 188 | 189 | 190 | 191 |
| 192 | 193 | 194 | 195 | 196 | 197 | 198 | 199 | 200 | 201 | 202 | 203 | 204 | 205 | 206 | 207 |
| 208 | 209 | 210 | 211 | 212 | 213 | 214 | 215 | 216 | 217 | 218 | 219 | 220 | 221 | 222 | 223 |
| 224 | 225 | 226 | 227 | 228 | 229 | 230 | 231 | 232 | 233 | 234 | 235 | 236 | 237 | 238 | 239 |
| 240 | 241 | 242 | 243 | 244 | 245 | 246 | 247 | 248 | 249 | 250 | 251 | 252 | 253 | 254 | 255 |
