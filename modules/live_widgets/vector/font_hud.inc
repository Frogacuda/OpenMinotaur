const char *hudGlyph(char c) {
    // A small independently drawn single-stroke alphabet on a 4x6 grid.
    // Each two-digit token is x,y; | lifts the pen. No external font data.
    switch (c) {
    case 'A':
        return "06 20 46|14 34";
    case 'B':
        return "00 06 36 45 44 33 03|33 42 41 30 00";
    case 'C':
        return "40 10 01 05 16 46";
    case 'D':
        return "00 06 26 45 41 20 00";
    case 'E':
        return "40 00 06 46|03 33";
    case 'F':
        return "40 00 06|03 33";
    case 'G':
        return "40 10 01 05 16 46 43 23";
    case 'H':
        return "00 06|40 46|03 43";
    case 'I':
        return "00 40|20 26|06 46";
    case 'J':
        return "00 40 45 36 16 05";
    case 'K':
        return "00 06|40 03 46";
    case 'L':
        return "00 06 46";
    case 'M':
        return "06 00 23 40 46";
    case 'N':
        return "06 00 46 40";
    case 'O':
        return "10 30 41 45 36 16 05 01 10";
    case 'P':
        return "06 00 30 41 42 33 03";
    case 'Q':
        return "10 30 41 45 36 16 05 01 10|24 46";
    case 'R':
        return "06 00 30 41 42 33 03|23 46";
    case 'S':
        return "40 10 01 02 34 45 36 06";
    case 'T':
        return "00 40|20 26";
    case 'U':
        return "00 05 16 36 45 40";
    case 'V':
        return "00 26 40";
    case 'W':
        return "00 16 23 36 40";
    case 'X':
        return "00 46|40 06";
    case 'Y':
        return "00 23 40|23 26";
    case 'Z':
        return "00 40 06 46";
    case '0':
        return "10 30 41 45 36 16 05 01 10|05 41";
    case '1':
        return "02 20 26|06 46";
    case '2':
        return "01 10 30 41 42 06 46";
    case '3':
        return "00 30 41 32 22 33 44 45 36 06";
    case '4':
        return "40 04 44|40 46";
    case '5':
        return "40 00 03 33 44 45 36 06";
    case '6':
        return "40 10 01 05 16 36 45 44 33 03";
    case '7':
        return "00 40 16";
    case '8':
        return "10 30 41 42 14 05 16 36 45 44 12 01 10";
    case '9':
        return "43 13 02 01 10 30 41 45 36 06";
    case '-':
        return "03 43";
    case '+':
        return "03 43|20 26";
    case ':':
        return "21 21|25 25";
    case '.':
        return "26 26";
    case '/':
        return "06 40";
    default:
        return "";
    }
}
