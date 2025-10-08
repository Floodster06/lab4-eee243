/*
 * Lookup table for Code39 character codes. The first element of each row is
 * the character encoded (e.g., the first row encodes the character '0'). This
 * is followed by a representation of the encoding in terms of a sequence of
 * narrow ('N') and wide ('W') bars.
 * ----
 * Tableau de consultation des codes de caractères du Code39. Le premier élément
 * de chaque ligne est le caractère codé (par exemple, la première ligne code 
 * le caractère « 0 »). Il est suivi d'une représentation du codage sous la 
 * forme d'une séquence de barres étroites (« N ») et larges (« W »).
 */
const char code39[44][10]= {
      {'0','N','N','N','W','W','N','W','N','N'},
      {'1','W','N','N','W','N','N','N','N','W'},
      {'2','N','N','W','W','N','N','N','N','W'},
      {'3','W','N','W','W','N','N','N','N','N'},
      {'4','N','N','N','W','W','N','N','N','W'},
      {'5','W','N','N','W','W','N','N','N','N'},
      {'6','N','N','W','W','W','N','N','N','N'},
      {'7','N','N','N','W','N','N','W','N','W'},
      {'8','W','N','N','W','N','N','W','N','N'},
      {'9','N','N','W','W','N','N','W','N','N'},
      {'A','W','N','N','N','N','W','N','N','W'},
      {'B','N','N','W','N','N','W','N','N','W'},
      {'C','W','N','W','N','N','W','N','N','N'},
      {'D','N','N','N','N','W','W','N','N','W'},
      {'E','W','N','N','N','W','W','N','N','N'},
      {'F','N','N','W','N','W','W','N','N','N'},
      {'G','N','N','N','N','N','W','W','N','W'},
      {'H','W','N','N','N','N','W','W','N','N'},
      {'I','N','N','W','N','N','W','W','N','N'},
      {'J','N','N','N','N','W','W','W','N','N'},
      {'K','W','N','N','N','N','N','N','W','W'},
      {'L','N','N','W','N','N','N','N','W','W'},
      {'M','W','N','W','N','N','N','N','W','N'},
      {'N','N','N','N','N','W','N','N','W','W'},
      {'O','W','N','N','N','W','N','N','W','N'},
      {'P','N','N','W','N','W','N','N','W','N'},
      {'Q','N','N','N','N','N','N','W','W','W'},
      {'R','W','N','N','N','N','N','W','W','N'},
      {'S','N','N','W','N','N','N','W','W','N'},
      {'T','N','N','N','N','W','N','W','W','N'},
      {'U','W','W','N','N','N','N','N','N','W'},
      {'V','N','W','W','N','N','N','N','N','W'},
      {'W','W','W','W','N','N','N','N','N','N'},
      {'X','N','W','N','N','W','N','N','N','W'},
      {'Y','W','W','N','N','W','N','N','N','N'},
      {'Z','N','W','W','N','W','N','N','N','N'},
      {'-','N','W','N','N','N','N','W','N','W'},
      {'.','W','W','N','N','N','N','W','N','N'},
      {',','N','W','W','N','N','N','W','N','N'},
      {'$','N','W','N','W','N','W','N','N','N'},
      {'/','N','W','N','W','N','N','N','W','N'},
      {'+','N','W','N','N','N','W','N','W','N'},
      {'%','N','N','N','W','N','W','N','W','N'},
      {'*','N','W','N','N','W','N','W','N','N'}
};
