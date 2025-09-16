import colorsys
import json
import openpyxl

named_colors = {'AliceBlue': '0xF0F8FF', 'AntiqueWhite': '0xFAEBD7', 'Aqua': '0x00FFFF',
                'Aquamarine': '0x7FFFD4', 'Azure': '0xF0FFFF', 'Beige': '0xF5F5DC', 'Bisque': '0xFFE4C4',
                'Black': '0x000000', 'BlanchedAlmond': '0xFFEBCD', 'Blue': '0x0000FF',
                'BlueViolet': '0x8A2BE2', 'Brown': '0xA52A2A', 'BurlyWood': '0xDEB887',
                'CadetBlue': '0x5F9EA0', 'Chartreuse': '0x7FFF00', 'Chocolate': '0xD2691E',
                'Coral': '0xFF7F50', 'CornflowerBlue': '0x6495ED', 'Cornsilk': '0xFFF8DC',
                'Crimson': '0xDC143C', 'Cyan': '0x00FFFF', 'DarkBlue': '0x00008B', 'DarkCyan': '0x008B8B',
                'DarkGoldenRod': '0xB8860B', 'DarkGray': '0xA9A9A9', 'DarkGrey': '0xA9A9A9',
                'DarkGreen': '0x006400', 'DarkKhaki': '0xBDB76B', 'DarkMagenta': '0x8B008B',
                'DarkOliveGreen': '0x556B2F', 'DarkOrange': '0xFF8C00', 'DarkOrchid': '0x9932CC',
                'DarkRed': '0x8B0000', 'DarkSalmon': '0xE9967A', 'DarkSeaGreen': '0x8FBC8F',
                'DarkSlateBlue': '0x483D8B', 'DarkSlateGray': '0x2F4F4F', 'DarkSlateGrey': '0x2F4F4F',
                'DarkTurquoise': '0x00CED1', 'DarkViolet': '0x9400D3', 'DeepPink': '0xFF1493',
                'DeepSkyBlue': '0x00BFFF', 'DimGray': '0x696969', 'DimGrey': '0x696969',
                'DodgerBlue': '0x1E90FF', 'FireBrick': '0xB22222', 'FloralWhite': '0xFFFAF0',
                'ForestGreen': '0x228B22', 'Fuchsia': '0xFF00FF', 'Gainsboro': '0xDCDCDC',
                'GhostWhite': '0xF8F8FF', 'Gold': '0xFFD700', 'GoldenRod': '0xDAA520', 'Gray': '0x808080',
                'Grey': '0x808080', 'Green': '0x008000', 'GreenYellow': '0xADFF2F', 'HoneyDew': '0xF0FFF0',
                'HotPink': '0xFF69B4', 'IndianRed': '0xCD5C5C', 'Indigo': '0x4B0082', 'Ivory': '0xFFFFF0',
                'Khaki': '0xF0E68C', 'Lavender': '0xE6E6FA', 'LavenderBlush': '0xFFF0F5',
                'LawnGreen': '0x7CFC00', 'LemonChiffon': '0xFFFACD', 'LightBlue': '0xADD8E6',
                'LightCoral': '0xF08080', 'LightCyan': '0xE0FFFF', 'LightGoldenRodYellow': '0xFAFAD2',
                'LightGray': '0xD3D3D3', 'LightGrey': '0xD3D3D3', 'LightGreen': '0x90EE90',
                'LightPink': '0xFFB6C1', 'LightSalmon': '0xFFA07A', 'LightSeaGreen': '0x20B2AA',
                'LightSkyBlue': '0x87CEFA', 'LightSlateGray': '0x778899', 'LightSlateGrey': '0x778899',
                'LightSteelBlue': '0xB0C4DE', 'LightYellow': '0xFFFFE0', 'Lime': '0x00FF00',
                'LimeGreen': '0x32CD32', 'Linen': '0xFAF0E6', 'Magenta': '0xFF00FF', 'Maroon': '0x800000',
                'MediumAquaMarine': '0x66CDAA', 'MediumBlue': '0x0000CD', 'MediumOrchid': '0xBA55D3',
                'MediumPurple': '0x9370DB', 'MediumSeaGreen': '0x3CB371', 'MediumSlateBlue': '0x7B68EE',
                'MediumSpringGreen': '0x00FA9A', 'MediumTurquoise': '0x48D1CC', 'MediumVioletRed': '0xC71585',
                'MidnightBlue': '0x191970', 'MintCream': '0xF5FFFA', 'MistyRose': '0xFFE4E1',
                'Moccasin': '0xFFE4B5', 'NavajoWhite': '0xFFDEAD', 'Navy': '0x000080', 'OldLace': '0xFDF5E6',
                'Olive': '0x808000', 'OliveDrab': '0x6B8E23', 'Orange': '0xFFA500', 'OrangeRed': '0xFF4500',
                'Orchid': '0xDA70D6', 'PaleGoldenRod': '0xEEE8AA', 'PaleGreen': '0x98FB98',
                'PaleTurquoise': '0xAFEEEE', 'PaleVioletRed': '0xDB7093', 'PapayaWhip': '0xFFEFD5',
                'PeachPuff': '0xFFDAB9', 'Peru': '0xCD853F', 'Pink': '0xFFC0CB', 'Plum': '0xDDA0DD',
                'PowderBlue': '0xB0E0E6', 'Purple': '0x800080', 'RebeccaPurple': '0x663399', 'Red': '0xFF0000',
                'RosyBrown': '0xBC8F8F', 'RoyalBlue': '0x4169E1', 'SaddleBrown': '0x8B4513', 'Salmon': '0xFA8072',
                'SandyBrown': '0xF4A460', 'SeaGreen': '0x2E8B57', 'SeaShell': '0xFFF5EE', 'Sienna': '0xA0522D',
                'Silver': '0xC0C0C0', 'SkyBlue': '0x87CEEB', 'SlateBlue': '0x6A5ACD', 'SlateGray': '0x708090',
                'SlateGrey': '0x708090', 'Snow': '0xFFFAFA', 'SpringGreen': '0x00FF7F', 'SteelBlue': '0x4682B4',
                'Tan': '0xD2B48C', 'Teal': '0x008080', 'Thistle': '0xD8BFD8', 'Tomato': '0xFF6347',
                'Turquoise': '0x40E0D0', 'Violet': '0xEE82EE', 'Wheat': '0xF5DEB3', 'White': '0xFFFFFF',
                'WhiteSmoke': '0xF5F5F5', 'Yellow': '0xFFFF00', 'YellowGreen': '0x9ACD32'}

def shift_color(col, shift=30, sat=1.0, val=1.0):
    r = (col & (255 << 16)) >> 16
    g = (col & (255 << 8)) >> 8
    b = col & 255
    hsv = colorsys.rgb_to_hsv(r, g, b)
    h = (((hsv[0] * 360) + shift) % 360) / 360
    rgb = colorsys.hsv_to_rgb(h, hsv[1] * sat, hsv[2] * val)
    return (int(rgb[0]) << 16) + (int(rgb[1]) << 8) + int(rgb[2])

def parse_sheet(ws):
    print(f'Parsing worksheet {ws.title}')
    ir = {"desc": ws.title}
    rows = ws.rows
    keys = [col.value.lower() for col in next(rows)]
    for row in rows:
        rec = dict(zip(keys, [col.value for col in row]))
        if rec.get('code') is None:
            continue
        cd = {"label": rec.get('label')}
        if rec.get('row'):
            cd['pos'] = f'{rec["row"]}x{rec["col"]}'
        if rec.get('comment'):
            cd['cmnt'] = rec.get('comment')
        if rec.get('rpt'):
            cd['rpt'] = bool(rec['rpt'])
                       
        if rec.get('cmd'):
            cd['cmd'] = rec['cmd']
        elif all((rec.get('primary'), rec.get('secondary'), rec.get('tertiary'))):
            c1 = int(rec.get('primary'), 16)
            c2 = int(rec.get('secondary'), 16)
            c3 = int(rec.get('tertiary'), 16)
            cd['cmd'] = f'FP=5&CL=h{c1:X}&C2=h{c2:X}&C3=h{c3:X}'
        elif all((rec.get('primary'), rec.get('secondary'))):
            c1 = int(rec.get('primary'), 16)
            c2 = int(rec.get('secondary'), 16)
            c3 = shift_color(c1, -1, sat=0.66, val=0.66)
            cd['cmd'] = f'FP=5&CL=h{c1:X}&C2=h{c2:X}&C3=h{c3:X}'
        elif rec.get('primary'):
            c1 = int(rec.get('primary'), 16)
            c2 = shift_color(c1, 30)
            c3 = shift_color(c1, -10, sat=0.66, val=0.66)
            cd['cmd'] = f'FP=5&CL=h{c1:X}&C2=h{c2:X}&C3=h{c3:X}'
        elif rec.get('label') in named_colors:
            c1 = int(named_colors[rec.get('label')], 16)
            c2 = shift_color(c1, 30)
            c3 = shift_color(c1, -10, sat=0.66, val=0.66)
            cd['cmd'] = f'FP=5&CL=h{c1:X}&C2=h{c2:X}&C3=h{c3:X}'
        else:
            print(f'Did not find a command or color for {rec["label"]}. Hint use named CSS colors as labels')
        ir[rec['code']] = cd
            
    with open(f'{ws.title}_ir.json', 'w') as fp:
        json.dump(ir, fp, indent=2)        

if __name__ == '__main__':
    wb = openpyxl.load_workbook('IR_Remote_Codes.xlsx')
    for ws in wb.worksheets:
        parse_sheet(ws)
