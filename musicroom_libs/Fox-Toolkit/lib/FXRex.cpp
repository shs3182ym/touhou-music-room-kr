/********************************************************************************
*                                                                               *
*                 R e g u l a r   E x p r e s s i o n   C l a s s               *
*                                                                               *
*********************************************************************************
* Copyright (C) 1999,2010 by Jeroen van der Zijp.   All Rights Reserved.        *
*********************************************************************************
* This library is free software; you can redistribute it and/or modify          *
* it under the terms of the GNU Lesser General Public License as published by   *
* the Free Software Foundation; either version 3 of the License, or             *
* (at your option) any later version.                                           *
*                                                                               *
* This library is distributed in the hope that it will be useful,               *
* but WITHOUT ANY WARRANTY; without even the implied warranty of                *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 *
* GNU Lesser General Public License for more details.                           *
*                                                                               *
* You should have received a copy of the GNU Lesser General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>          *
********************************************************************************/
#include "xincs.h"
#include "fxver.h"
#include "fxdefs.h"
#include "fxascii.h"
#include "fxunicode.h"
#include "FXHash.h"
#include "FXStream.h"
#include "FXString.h"
#include "FXElement.h"
#include "FXRex.h"


/*
  The Story:
  ==========

  As with most regex implementations, this one is inspired by Henry Spencer's
  original implementation.

  This is however an ab-initio implementation, with the following goals:

        o Full C++ implementation, no simple C++ wrapper.
        o Trade speed and memory in favor of speed, but keep it compact where
          possible.
        o Thread-safe:
            - No global variables used during parsing or execution.
            - Multiple threads could use same single FXRex at the same time.
        o Perl-like syntax for character classes.
        o Additional features such as lazy/greedy/possessive closures, counting
          repeats, back references, trailing context.
        o Forward and backward subject string scanning mode.
        o Single line/multi line matching modes.
        o 8-bit safe (you can use it to grep binary data!).
        o When parsing fails, or when created with default constructor, FXRex
          is initialized to a "fallback" program; its thus safe to invoke match
          at any time.
        o The default fallback program will fail to match anything.
        o Convenient feature: disallow empty string matches; this is nice as
          it prevents a common problem, for example searching for "a*" in "bbba";
          without the NotEmpty option, this matches "" and not the "a".
        o Another convenient feature is the ability to compile verbatim strings.
          This is practical as it allows FXRex to be populated with a simple
          string with no interpretation of special characters ^*?+{}()\$.[].

  Because this is completely new implementation of regular expressions, and
  not merely an extension of a previous implementation, some people may want to
  adapt it for use outside of FOX.  This is perfectly OK with me.

  However:

        o The Author is not responsible for the consequences of using this
          software.

        o Recipient should be clearly informed of the origin of the software;
          and if alterations have been made, this must be stated clearly.

        o Software will continue to fall under Lesser GPL, unless specific
          permission from the Author has been obtained.


  Implementation notes:
  =====================

  This implementation does not use "nodes" with "next" pointers; instead, the "next"
  opcode is located implicitly by simple sequentiality.  This has beneficial effect
  on speed, as one can simply add to the program counter instead of performing a
  memory reference.

  Sometimes one needs to jump out of sequence; this is implemented by an
  explicit jump instruction.  Because it works with relative offsets, there
  is no need to distinguish between forward and backward jumps.

  Henry Spencer implemented a trick to "prefix" simple single-character matching
  opcodes with a closure operator, by shifting down already generated code and
  inserting the closure operator in front of it.

  FXRex uses the same trick of shifting down code; but this same trick is also
  useful for branches!

  FXRex only generates a branch node when an alternative has in fact been seen;
  if no alternative is there, we've saved ourselves both a branch node and a
  jump node!

  This has interesting side-effects:

        o The common case of 1 single branch now no longer needs a branch opcode
          and corresponding jump opcode at all!

        o When an alternative is found, we insert a branch node in front and a jump
          node behind the already generated code.  This can be done easily as
          branches and jumps within the shifted block of code are relative, and have
          already been resolved!

        o The matching algorithm for a branch opcode simplifies as well:- either
          it matches the first branch, or it continues after the jump.

        o Its easier to dig out some info from the program, and in fact so easy
          that this digging can be moved to the execute phase.

  When a repeat is prefixed in front of a simple single-character match, counted
  repeats are simplified: {1}, {1,1} is eliminated, {}, {0,} becomes *, {1,} becomes
  +, and {0,1} becomes ?.

  Because single-character repeats are prefixed with a repeat instruction, there is
  no recursion needed; single character repeats are therefore very fast.

  Complex repeats are implemented using branch loop constructs and may involve
  recursions (in fact only the fixed repeat is non-recursive!).  Hence complex repeats
  should be avoided when single-character repeats will suffice.

  OP_BRANCH and OP_BRANCHREV implement alternatives. For OP_BRANCH, first the inline
  code immediately following the offset is executed; if the inline code fails, OP_BRANCH
  takes a jump to the new location and tries the alternative.

  For OP_BRANCHREV, it works the opposite way: OP_BRANCHREV takes the alternative
  first, before trying the inline code.

  Having both OP_BRANCH and OP_BRANCHREV substantially simplifies the design of
  complex, greedy or lazy matches:- the greedy and lazy match turn out to have the
  same code structure, except we're using OP_BRANCHREV for the lazy matches and
  OP_BRANCH for the greedy ones.

  OP_JUMP is an unconditional jump to a new location. OP_JUMPLT and OP_JUMPGT jump
  when the loop counter is less than or greater than the given value, respectively.


  Atomic Matching Groups
  ======================

  For example, trying to match pattern "\d+foo" against subject string "123456bar",
  the matcher will eat all digits up till "6", and then backtrack by trying digits
  up till 5, and so on.  A atomic subgroup match will simply fail if the sub-pattern
  fails to match at the end.  This can be written as: "(?>\d+)foo".  Atomic groups
  are thus more efficient since no repeated tries are being made.


  Greedy, Lazy, and Possessive Matches
  ====================================

  Greedy: the "a*" in pattern "a*ardvark" matching subject string "aardvark" will match
  "aa", then backtrack and match "a", after which the match succeeds.

  Lazy: the "a*?" in pattern "a*?ardvark" will first match "", then try match "a" after
  which the match succeeds.

  Possessive: the "a*+" in pattern "a*+ardvark" will match "aa", then fail without
  backing off.

  Possessive matches and Atomic matching groups are closely related in terms of
  controlling the recursion depth of the matcher.


  Syntax:
  =======

      Special Constructs

      (?i X )   Match sub pattern case insensitive
      (?I X )   Match sub pattern case sensitive
      (?n X )   Match sub pattern with newlines
      (?N X )   Match sub pattern with no newlines
      (?: X )   Non-capturing parentheses
      (?= X )   Zero width positive lookahead
      (?! X )   Zero width negative lookahead
      (?<= X )  Zero width positive lookbehind
      (?<! X )  Zero width negative lookbehind
      (?> X )   Atomic grouping (possessive match)

      Logical Operators

      X Y       X followed by Y
      X | Y     Either X or Y
      ( X )     Sub pattern (capturing if FXRex::Capture)

      Greedy Quantifiers

      X *       Match 0 or more
      X +       Match 1 or more
      X ?       Match 0 or 1
      X {}      Match 0 or more
      X {n}     Match n times
      X {,m}    Match no more than m times
      X {n,}    Match n or more
      X {n,m}   Match at least n but no more than m times

      Lazy Quantifiers

      X *?      Match 0 or more
      X +?      Match 1 or more
      X ??      Match 0 or 1
      X {}?     Match 0 or more times
      X {n}?    Match n times
      X {,m}?   Match no more than m times
      X {n,}?   Match n or more
      X {n,m}?  Match at least n but no more than m times

      Possessive (non-backtracking) Quantifiers

      X *+      Match 0 or more
      X ++      Match 1 or more
      X ?+      Match 0 or 1
      X {}+     Match 0 or more times
      X {n}+    Match n times
      X {,m}+   Match no more than m times
      X {n,}+   Match n or more
      X {n,m}+  Match at least n but no more than m times

      Boundary Matching
      ^         Match begin of line [if at begin of pattern]
      $         Match end of line [if at end of pattern]
      \<        Begin of word
      \>        End of word
      \b        Word boundary
      \B        Word interior
      \A        Match only beginning of string
      \Z        Match only and end of string

      Character Classes

      [abc]     Match a, b, or c
      [^abc]    Match any but a, b, or c
      [a-zA-Z]  Match upper- or lower-case a through z
      []]       Matches ]
      [-]       Matches -

      Predefined character classes

      .         Match any character
      \d        Digit [0-9]
      \D        Non-digit [^0-9]
      \s        Space [ \t\n\r\f\v]
      \S        Non-space [^ \t\n\r\f\v]
      \w        Word character [a-zA-Z_0-9]
      \W        Non-word character [^a-zA-Z0-9_]
      \l        Letter [a-zA-Z]
      \L        Non-letter [^a-zA-Z]
      \h        Hex digit [0-9a-fA-F]
      \H        Non-hex digit [^0-9a-fA-F]
      \u        Single uppercase character
      \U        Single lowercase character
      \p        Punctuation (not including '_')
      \P        Non punctuation

      Characters

      x           Any character
      \\          Back slash character
      \033        Octal
      \x1b        Hex
      \u1FD1      Unicode U+1FD1 (GREEK SMALL LETTER IOTA WITH MACRON))
      \U00010450  Wide unicode U+10450 (SHAVIAN LETTER PEEP)
      \a          Alarm, bell
      \e          Escape character
      \t          Tab
      \f          Form feed
      \n          Newline
      \r          Return
      \v          Vertical tab
      \cx         Control character

      Back References

      \1        Reference to 1st capturing group
      \2        Reference to 2nd capturing group
      ...
      \9        Reference to 9th capturing group

      POSIX character classes (US-ASCII only)

      \P{name}    Matches anything BUT what \p{name} matches

      \p{Lower}   A lower-case alphabetic character: [a-z]
      \p{Upper}   An upper-case alphabetic character: [A-Z]
      \p{ASCII}   All ASCII: [\x00-\x7F]
      \p{Alpha}   An alphabetic character:[\p{Lower}\p{Upper}]
      \p{Digit}   A decimal digit: [0-9]
      \p{Alnum}   An alphanumeric character: [\p{Alpha}\p{Digit}]
      \p{Punct}   Punctuation: One of !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~
      \p{Graph}   A visible character: [\p{Alnum}\p{Punct}]
      \p{Print}   A printable character: [\p{Graph}]
      \p{Blank}   A space or a tab: [ \t]
      \p{Cntrl}   A control character: [\x00-\x1F\x7F]
      \p{XDigit}  A hexadecimal digit: [0-9a-fA-F]
      \p{Space}   A whitespace character: [ \t\n\x0B\f\r]


      Unicode general character categories

      \p{C}     Other (Cc | Cf | Cn | Co | Cs)
      \p{Cc}    Control
      \p{Cf}    Format
      \p{Cn}    Unassigned
      \p{Co}    Private use
      \p{Cs}    Surrogate

      \p{L}     Letter (Ll | Lm | Lo | Lt | Lu)
      \p{Ll}    Lower case letter
      \p{Lm}    Modifier letter
      \p{Lo}    Other letter
      \p{Lt}    Title case letter
      \p{Lu}    Upper case letter

      \p{M}     Mark (Mc | Me | Mn)
      \p{Mc}    Spacing mark
      \p{Me }   Enclosing mark
      \p{Mn}    Non-spacing mark

      \p{N}     Number (Nd | Nl | No)
      \p{Nd}    Decimal number
      \p{Nl}    Letter number
      \p{No}    Other number

      \p{P}     Punctuation (Pc | Pd | Pe | Pf | Pi | Po | Ps)
      \p{Pc}    Connector punctuation
      \p{Pd}    Dash punctuation
      \p{Pe}    Close punctuation
      \p{Pf}    Final punctuation
      \p{Pi}    Initial punctuation
      \p{Po}    Other punctuation
      \p{Ps}    Open punctuation

      \p{S}     Symbol (Sc | Sk | Sm | So)
      \p{Sc}    Currency symbol
      \p{Sk}    Modifier symbol
      \p{Sm}    Mathematical symbol
      \p{So}    Other symbol

      \p{Z}     Separator (Zl | Zp | Zs)
      \p{Zl}    Line separator
      \p{Zp}    Paragraph separator
      \p{Zs}    Space separator


      Unicode script categories

      \p{Arab}  Arabic
      \p{Armn}  Armenian
      \p{Beng}  Bengali
      \p{Bopo}  Bopomofo
      \p{Brai}  Braille
      \p{Bugi}  Buginese
      \p{Buhd}  Buhid
      \p{Cans}  Canadian_Aboriginal
      \p{Cher}  Cherokee
      \p{Copt}  Coptic (Qaac)
      \p{Cprt}  Cypriot
      \p{Cyrl}  Cyrillic
      \p{Deva}  Devanagari
      \p{Dsrt}  Deseret
      \p{Ethi}  Ethiopic
      \p{Geor}  Georgian
      \p{Glag}  Glagolitic
      \p{Goth}  Gothic
      \p{Grek}  Greek
      \p{Gujr}  Gujarati
      \p{Guru}  Gurmukhi
      \p{Hang}  Hangul
      \p{Hani}  Han
      \p{Hano}  Hanunoo
      \p{Hebr}  Hebrew
      \p{Hira}  Hiragana
      \p{Hrkt}  Katakana_Or_Hiragana
      \p{Ital}  Old_Italic
      \p{Kana}  Katakana
      \p{Khar}  Kharoshthi
      \p{Khmr}  Khmer
      \p{Knda}  Kannada
      \p{Laoo}  Lao
      \p{Latn}  Latin
      \p{Limb}  Limbu
      \p{Linb}  Linear_B
      \p{Mlym}  Malayalam
      \p{Mong}  Mongolian
      \p{Mymr}  Myanmar
      \p{Ogam}  Ogham
      \p{Orya}  Oriya
      \p{Osma}  Osmanya
      \p{Qaai}  Inherited
      \p{Runr}  Runic
      \p{Shaw}  Shavian
      \p{Sinh}  Sinhala
      \p{Sylo}  Syloti_Nagri
      \p{Syrc}  Syriac
      \p{Tagb}  Tagbanwa
      \p{Tale}  Tai_Le
      \p{Talu}  New_Tai_Lue
      \p{Taml}  Tamil
      \p{Telu}  Telugu
      \p{Tfng}  Tifinagh
      \p{Tglg}  Tagalog
      \p{Thaa}  Thaana
      \p{Thai}  Thai
      \p{Tibt}  Tibetan
      \p{Ugar}  Ugaritic
      \p{Xpeo}  Old_Persian
      \p{Yiii}  Yi
      \p{Zyyy}  Common


  Grammar:
  ========

      exp        ::= branch { "|" branch }*
      branch     ::= { piece }*
      piece      ::= atom [ rep ]
      rep        ::= ( "*" | "+" | "?" | counts ) [ "?" ]
      counts     ::= "{" digits ["," [ digits ] ] "}"
      atom       ::= "(" exp ")" | "[" [^] range "]" | characters
      range      ::= { character | character "-" character } +
      characters ::= { character }*
      digits     ::= { digit }*

  To do:
  ======

  - Look into optimizing character class when possible (e.g.
    collapse [0-9] to OP_DIGIT and [^A] into OP_NOT_CHAR).
  - Should \D, \W, \L match newline?
  - Repeating back references, only possible if capturing parentheses
    are known NOT to match "".
  - Note the \uXXXX is going to be used for UNICODE perhaps:
    See: http://www.unicode.org/unicode/reports/tr18.
  - Implement possessive and atomic groups
  - We need reverse matching capability (Sexegers!) so we can scan backwards
    through the text.  This also helps for fully general LookBehind implementation.
  - Look behind would be nice...
*/


// As close to infinity as we're going to get; this seems big
// enough.  We can not make it too large as this may wrap around when
// added to something else!
#define ONEINDIG 16384

// Number of capturing sub-expressions allowed
#define NSUBEXP  10

// Size of string buffer
#define MAXCHARS 512

// Set operations
#define EXCL(set,ch) (set[((FXuchar)(ch))>>4]&=~(1<<(((FXuchar)(ch))&15)))
#define INCL(set,ch) (set[((FXuchar)(ch))>>4]|=(1<<(((FXuchar)(ch))&15)))
#define ISIN(set,ch) (set[((FXuchar)(ch))>>4]&(1<<(((FXuchar)(ch))&15)))
#define CLEAR(set)   (set[0]=set[1]=set[2]=set[3]=set[4]=set[5]=set[6]=set[7]=set[8]=set[9]=set[10]=set[11]=set[12]=set[13]=set[14]=set[15]=0)

//#define REXDEBUG 1

using namespace FX;

/*******************************************************************************/

namespace {

// Opcodes of the engine
enum {
  OP_END,               // End of pattern reached
  OP_FAIL,              // Always fail
  OP_SUCCEED,           // Always succeed
  OP_LINE_BEG,          // Beginning of line
  OP_LINE_END,          // End of line
  OP_WORD_BEG,          // Beginning of word
  OP_WORD_END,          // End of word
  OP_WORD_BND,          // Word boundary
  OP_WORD_INT,          // Word interior
  OP_STR_BEG,           // Beginning of string
  OP_STR_END,           // End of string
  OP_ANY_OF,            // Any character in set
  OP_ANY_BUT,           // Any character not in set
  OP_ANY,               // Any character but no newline
  OP_ANY_NL,            // Any character including newline
  OP_SPACE,             // White space
  OP_SPACE_NL,          // White space including newline
  OP_NOT_SPACE,         // Non-white space
  OP_DIGIT,             // Digit
  OP_NOT_DIGIT,         // Non-digit
  OP_NOT_DIGIT_NL,      // Non-digit including newline
  OP_LETTER,            // Letter
  OP_NOT_LETTER,        // Non-letter
  OP_NOT_LETTER_NL,     // Non-letter including newline
  OP_WORD,              // Word character
  OP_NOT_WORD,          // Non-word character
  OP_NOT_WORD_NL,       // Non-word character including newline
  OP_HEX,               // Hex digit
  OP_NOT_HEX,           // Non hex digit
  OP_NOT_HEX_NL,        // Non hex digit including newline
  OP_PUNCT,             // Punctuation
  OP_NOT_PUNCT,         // Non punctuation
  OP_NOT_PUNCT_NL,      // Non punctuation including newline
//  OP_CHARS,             // Match literal string
//  OP_CHARS_CI,          // Match literal string, case insensitive
  OP_CHARS_NEW,         // Match literal string
  OP_CHARS_CI_NEW,      // Match literal string, case insensitive
  OP_CHAR,              // Single character
  OP_CHAR_CI,           // Single character, case insensitive
  OP_JUMP,              // Jump to another location
  OP_BRANCH,            // Branch: jump after trying following code
  OP_BRANCHREV,         // Branch: jump before trying following code
  OP_STAR,              // Greedy * (simple)
  OP_MIN_STAR,          // Lazy * (simple)
  OP_POS_STAR,          // Possessive * (simple)
  OP_PLUS,              // Greedy + (simple)
  OP_MIN_PLUS,          // Lazy + (simple)
  OP_POS_PLUS,          // Possessive + (simple)
  OP_QUEST,             // Greedy ? (simple)
  OP_MIN_QUEST,         // Lazy ? (simple)
  OP_POS_QUEST,         // Possessive ? (simple)
  OP_REP,               // Greedy counted repeat (simple)
  OP_MIN_REP,           // Lazy counted repeat (simple)
  OP_POS_REP,           // Possessive counted repeat (simple)
  OP_AHEAD_NEG,         // Negative look-ahead
  OP_AHEAD_POS,         // Positive look-ahead
  OP_BEHIND_NEG,        // Negative look-behind
  OP_BEHIND_POS,        // Positive look-behind
  OP_ATOMIC,            // Atomic subgroup
  OP_UPPER,             // Match upper case
  OP_LOWER,             // Match lower case
  OP_SUB_BEG_0,         // Start of substring 0, 1, ...
  OP_SUB_BEG_1,
  OP_SUB_BEG_2,
  OP_SUB_BEG_3,
  OP_SUB_BEG_4,
  OP_SUB_BEG_5,
  OP_SUB_BEG_6,
  OP_SUB_BEG_7,
  OP_SUB_BEG_8,
  OP_SUB_BEG_9,
  OP_SUB_END_0,         // End of substring 0, 1, ...
  OP_SUB_END_1,
  OP_SUB_END_2,
  OP_SUB_END_3,
  OP_SUB_END_4,
  OP_SUB_END_5,
  OP_SUB_END_6,
  OP_SUB_END_7,
  OP_SUB_END_8,
  OP_SUB_END_9,
  OP_REF_0,             // Back reference to substring 0, 1, ...
  OP_REF_1,
  OP_REF_2,
  OP_REF_3,
  OP_REF_4,
  OP_REF_5,
  OP_REF_6,
  OP_REF_7,
  OP_REF_8,
  OP_REF_9,
  OP_REF_CI_0,          // Case insensitive back reference to substring 0, 1, ...
  OP_REF_CI_1,
  OP_REF_CI_2,
  OP_REF_CI_3,
  OP_REF_CI_4,
  OP_REF_CI_5,
  OP_REF_CI_6,
  OP_REF_CI_7,
  OP_REF_CI_8,
  OP_REF_CI_9,
  OP_ZERO_0,            // Zero counter 0, 1, ...
  OP_ZERO_1,
  OP_ZERO_2,
  OP_ZERO_3,
  OP_ZERO_4,
  OP_ZERO_5,
  OP_ZERO_6,
  OP_ZERO_7,
  OP_ZERO_8,
  OP_ZERO_9,
  OP_INCR_0,            // Increment counter 0, 1, ...
  OP_INCR_1,
  OP_INCR_2,
  OP_INCR_3,
  OP_INCR_4,
  OP_INCR_5,
  OP_INCR_6,
  OP_INCR_7,
  OP_INCR_8,
  OP_INCR_9,
  OP_JUMPLT_0,          // Jump if counter 0, 1, ... less than value
  OP_JUMPLT_1,
  OP_JUMPLT_2,
  OP_JUMPLT_3,
  OP_JUMPLT_4,
  OP_JUMPLT_5,
  OP_JUMPLT_6,
  OP_JUMPLT_7,
  OP_JUMPLT_8,
  OP_JUMPLT_9,
  OP_JUMPGT_0,          // JUmp if counter 0, 1, ... greater than value
  OP_JUMPGT_1,
  OP_JUMPGT_2,
  OP_JUMPGT_3,
  OP_JUMPGT_4,
  OP_JUMPGT_5,
  OP_JUMPGT_6,
  OP_JUMPGT_7,
  OP_JUMPGT_8,
  OP_JUMPGT_9
  };


// Flags
enum {
  FLG_WORST  = 0,           // Worst case
  FLG_WIDTH  = 1,           // Matches >=1 character
  FLG_SIMPLE = 2            // Simple
  };


// Structure used during matching
struct FXExecute {
  const FXchar  *str;               // String
  const FXchar  *str_beg;           // Begin of string
  const FXchar  *str_end;           // End of string
  const FXchar  *bak_beg[NSUBEXP];  // Back reference start
  const FXchar  *bak_end[NSUBEXP];  // Back reference end
  FXint         *sub_beg;           // Begin of substring i
  FXint         *sub_end;           // End of substring i
  const FXshort *code;              // Program code
  FXint          npar;              // Number of capturing parentheses
  FXint          count[10];         // Counters for counted repeats
  FXint          mode;              // Match mode

  // Attempt to match
  FXbool attempt(const FXchar* string);

  // Match at current string position
  FXbool match(const FXshort* prog);

  // Execute
  FXbool execute(const FXchar* fm,const FXchar* to);
  };


// Structure used during compiling
struct FXCompile {
  const FXchar  *pat;               // Pattern string pointer
  FXshort       *code;              // Program code
  FXshort       *pc;                // Program counter
  FXint          mode;              // Compile mode
  FXint          nbra;              // Number of counting braces
  FXint          npar;              // Number of capturing parentheses
  FXint          tok;               // Current token while parsing

  // Code generation
  FXshort* append(FXshort op);
  FXshort* append(FXshort op,FXshort arg);
  FXshort* append(FXshort op,FXshort arg1,FXshort arg2);
  FXshort* append(FXshort op,const FXshort set[]);

  FXshort* append(FXshort op,FXshort len,const FXchar *data);

  FXshort* insert(FXshort *ptr,FXshort op);
  FXshort* insert(FXshort *ptr,FXshort op,FXshort arg);
  FXshort* insert(FXshort *ptr,FXshort op,FXshort arg1,FXshort arg2);

  // Patch branches
  void patch(FXshort *fm,FXshort *to);

  // Fix value
  void fix(FXshort *ptr,FXshort val);

  // Lexing
  FXshort token();
  FXshort gethex();
  FXshort getoct();
  FXshort getctl();

  // Parsing
  FXRex::Error compile(FXint& flags);
  FXRex::Error verbatim(FXint& flags);
  FXRex::Error expression(FXint& flags,FXshort& smin,FXshort& smax);
  FXRex::Error alternative(FXint& flags,FXshort& smin,FXshort& smax);
  FXRex::Error piece(FXint& flags,FXshort& smin,FXshort& smax);
  FXRex::Error atom(FXint& flags,FXshort& smin,FXshort& smax);
  FXRex::Error charset();
  };


/*******************************************************************************/

// FXCompile members

enum {
  TK_END=256,           // End of string

  TK_BAD,               // Bad token

  TK_LPAREN,            // Parentheses
  TK_RPAREN,

  TK_ALTER,             // Alternatives
  TK_DASH,              // Dash

  TK_STAR,              // Zero or more repeat
  TK_PLUS,              // One or more repeat
  TK_QUEST,             // Zero or one repeat

  TK_LBRACE,            // Counted repeat
  TK_RBRACE,
  TK_COMMA,

  TK_CARET,             // Zero-width Assertions
  TK_DOLLAR,
  TK_BEGWORD,
  TK_ENDWORD,
  TK_BEGTEXT,
  TK_ENDTEXT,
  TK_WORDBOUND,
  TK_INTRAWORD,

  TK_LBRACK,            // General character classes
  TK_RBRACK,

  TK_WORD,              // Special character classes
  TK_NONWORD,
  TK_SPACE,
  TK_NONSPACE,
  TK_DIGIT,
  TK_NONDIGIT,
  TK_HEXDIGIT,
  TK_NONHEXDIGIT,
  TK_PUNCT,
  TK_NONPUNCT,
  TK_LETTER,
  TK_NONLETTER,
  TK_UPPER,
  TK_LOWER,
  TK_ANY,

  TK_BACKREF1,          // Back references
  TK_BACKREF2,
  TK_BACKREF3,
  TK_BACKREF4,
  TK_BACKREF5,
  TK_BACKREF6,
  TK_BACKREF7,
  TK_BACKREF8,
  TK_BACKREF9
  };


// Parse hex escape code
FXint hex(const FXchar*& pat){
  register FXint ch,n;
  for(ch=0,n=2; Ascii::isHexDigit(*pat) && n; n--){
    ch=(ch<<4)+Ascii::digitValue(*pat++);
    }
  return ch;
  }


// Parse octal escape code
FXint oct(const FXchar*& pat){
  register FXint ch,n;
  for(ch=0,n=3; '0'<=*pat && *pat<='7' && n; n--){
    ch=(ch<<3)+(*pat++-'0');
    }
  return ch;
  }


// Parse control code
FXint ctl(const FXchar*& pat){
  register FXint ch=Ascii::toUpper(*pat);
  if('@'<=ch && ch<='_'){
    pat++;
    return ch-'@';
    }
  return TK_BAD;
  }


// Read control code
FXshort FXCompile::getctl(){
  register FXshort ch=TK_BAD;
  if('@'<=*pat && *pat<='_'){
    ch=*pat++ - '@';
    }
  return TK_BAD;
  }


// Read octal digits
FXshort FXCompile::getoct(){
  register FXshort ch=TK_BAD;
  if('0'<=*pat && *pat<'7'){
    ch=*pat++ - '0';
    if('0'<=*pat && *pat<'7'){
      ch=(ch<<3) + *pat++ - '0';
      if('0'<=*pat && *pat<'7'){
        ch=(ch<<3) + *pat++ - '0';
        }
      }
    }
  return ch;
  }


// Read hex digits
FXshort FXCompile::gethex(){
  register FXshort ch=TK_BAD;
  if(Ascii::isHexDigit(*pat)){
    ch=Ascii::digitValue(*pat++);
    if(Ascii::isHexDigit(*pat)){
      ch=(ch<<4)+Ascii::digitValue(*pat++);
      }
    }
  return ch;
  }


// Lexing
FXshort FXCompile::token(){
  register FXshort ch;
  switch(ch=(FXuchar)*pat++){
    case   0: return TK_END;                       // End of pattern
    case '(': return TK_LPAREN;
    case ')': return TK_RPAREN;
    case '[': return TK_LBRACK;
    case ']': return TK_RBRACK;
    case '{': return TK_LBRACE;
    case '}': return TK_RBRACE;
    case '|': return TK_ALTER;
    case '-': return TK_DASH;
    case '*': return TK_STAR;
    case '+': return TK_PLUS;
    case '?': return TK_QUEST;
    case ',': return TK_COMMA;
    case '^': return TK_CARET;
    case '$': return TK_DOLLAR;
    case '.': return TK_ANY;
    case '\\':
      switch(ch=(FXuchar)*pat++){
        case  0 : return TK_BAD;                        // End of pattern
        case 'a': return '\a';                          // Bell
        case 'e': return '\033';                        // Escape
        case 'f': return '\f';                          // Form feed
        case 'n': return '\n';                          // Newline
        case 'r': return '\r';                          // Return
        case 't': return '\t';                          // Tab
        case 'v': return '\v';                          // Vertical tab
        case 'w': return TK_WORD;
        case 'W': return TK_NONWORD;
        case 's': return TK_SPACE;
        case 'S': return TK_NONSPACE;
        case 'd': return TK_DIGIT;
        case 'D': return TK_NONDIGIT;
        case 'h': return TK_HEXDIGIT;
        case 'H': return TK_NONHEXDIGIT;
        case 'p': return TK_PUNCT;
        case 'P': return TK_NONPUNCT;
        case 'l': return TK_LETTER;
        case 'L': return TK_NONLETTER;
        case 'u': return TK_UPPER;
        case 'U': return TK_LOWER;
        case 'b': return TK_WORDBOUND;
        case 'B': return TK_INTRAWORD;
        case 'A': return TK_BEGTEXT;
        case 'Z': return TK_ENDTEXT;
        case '<': return TK_BEGWORD;
        case '>': return TK_ENDWORD;
        case '1': return TK_BACKREF1;
        case '2': return TK_BACKREF2;
        case '3': return TK_BACKREF3;
        case '4': return TK_BACKREF4;
        case '5': return TK_BACKREF5;
        case '6': return TK_BACKREF6;
        case '7': return TK_BACKREF7;
        case '8': return TK_BACKREF8;
        case '9': return TK_BACKREF9;
        case 'c': return getctl();                      // Control character
        case '0': return getoct();                      // Octal digit
        case 'x': return gethex();                      // Hex digit
        }
    }
  return ch;
  }

#if 0

// FXuchar* pat

FXwchar FXCompile::wc(){
  register FXwchar w=*pat++;
  if(__unlikely(0xC0<=w)){ w=(w<<6)^0x3080^*pat++;
  if(__unlikely(0x800<=w)){ w=(w<<6)^0x20080^*pat++;
  if(__unlikely(0x10000<=w)){ w=(w<<6)^0x400080^*pat++;
  if(__unlikely(0x200000<=w)){ w=(w<<6)^0x8000080^*pat++;
  if(__unlikely(0x4000000<=w)){ w=(w<<6)^0x80^*pat++; }}}}}
  return w;
  }

FXint utf2wc(FXwchar& w,const FXchar* src){
  register FXint n=0;
  w=(FXuchar)src[n++];
  if(__unlikely(0xC0<=w)){ w=(w<<6)^(FXuchar)src[n++]^0x3080;
  if(__unlikely(0x800<=w)){ w=(w<<6)^(FXuchar)src[n++]^0x20080;
  if(__unlikely(0x10000<=w)){ w=(w<<6)^(FXuchar)src[n++]^0x400080;
  if(__unlikely(0x200000<=w)){ w=(w<<6)^(FXuchar)src[n++]^0x8000080;
  if(__unlikely(0x4000000<=w)){ w=(w<<6)^(FXuchar)src[n++]^0x80; }}}}}
  return n;
  }

const FXwchar SURROGATE_OFFSET=0x10000-(0xD800<<10)-0xDC00;
const FXwchar LEAD_OFFSET=0xD800-(0x10000>>10);
const FXwchar TAIL_OFFSET=0xDC00;


// Read wc from program
FXwchar FXExecute::wc(const FXnchar* prog){
  register FXwchar w=*prog++;
  if(__unlikely((w&0xDC00)==0xD800)){
    FXASSERT(0xDC00<=*prog && *prog<0xE000);
    w=(w<<10) + *prog++ + SURROGATE_OFFSET;
    }
  return w;
  }


// Convert to utf16be
FXint FXCompile::appendWc(FXwchar w){
  register FXnchar *val=pc;
  if(code){ pc[0]=w; }
  if(__unlikely(0xFFFF<w)){
    if(code){ pc[0]=(wc>>10)+LEAD_OFFSET; pc[1]=(wc&0x3FF)+TAIL_OFFSET; }
    pc++;
    }
  pc++;
  return val;
  }

#endif


// Compiler main
FXRex::Error FXCompile::compile(FXint& flags){
  FXRex::Error err; FXshort smin,smax;
  if(*pat=='\0') return FXRex::ErrEmpty;
  if(mode&FXRex::Verbatim)
    err=verbatim(flags);
  else
    err=expression(flags,smin,smax);
  if(err) return err;
  if(*pat!='\0') return FXRex::ErrParent;
  append(OP_END);
  return FXRex::ErrOK;
  }


// Parse without interpretation of magic characters
FXRex::Error FXCompile::verbatim(FXint& flags){
  FXchar buf[MAXCHARS],ch;
  FXshort len;
  flags=FLG_WIDTH;
  while(*pat!='\0'){
    len=0;
    do{
      ch=*pat++;
      if(mode&FXRex::IgnoreCase) ch=Ascii::toLower(ch);                 // FIXME copy straight into regex code
      buf[len++]=(FXuchar)ch;
      }
    while(*pat!='\0' && len<MAXCHARS);
    if(len==1){
      flags|=FLG_SIMPLE;
      append((mode&FXRex::IgnoreCase)?OP_CHAR_CI:OP_CHAR,buf[0]);
      }
    else{
      append((mode&FXRex::IgnoreCase)?OP_CHARS_CI_NEW:OP_CHARS_NEW,len,buf);
      }
    }
  return FXRex::ErrOK;
  }


// Parse expression
FXRex::Error FXCompile::expression(FXint& flags,FXshort& smin,FXshort& smax){
  FXRex::Error err; FXint flg; FXshort *at,*jp,smn,smx;
  at=pc;
  jp=NULL;
  err=alternative(flags,smin,smax);
  if(err) return err;
  while(*pat=='|'){
    pat++;
    insert(at,OP_BRANCH,pc-at+3);
    append(OP_JUMP,jp?jp-pc-1:0);
    jp=pc-1;
    at=pc;
    err=alternative(flg,smn,smx);
    if(err) return err;
    if(!(flg&FLG_WIDTH)) flags&=~FLG_WIDTH;
    if(smn<smin) smin=smn;
    if(smx>smax) smax=smx;
    }
  patch(jp,pc);
  return FXRex::ErrOK;
  }


// Parse branch
FXRex::Error FXCompile::alternative(FXint& flags,FXshort& smin,FXshort& smax){
  FXRex::Error err; FXint flg; FXshort smn,smx;
  flags=FLG_WORST;
  smin=0;
  smax=0;
  while(*pat!='\0' && *pat!='|' && *pat!=')'){
    err=piece(flg,smn,smx);
    if(err) return err;
    if(flg&FLG_WIDTH) flags|=FLG_WIDTH;
    smin=smin+smn;
    smax=FXMIN(smax+smx,ONEINDIG);
    }
  return FXRex::ErrOK;
  }


// Parse piece
FXRex::Error FXCompile::piece(FXint& flags,FXshort& smin,FXshort& smax){
  FXRex::Error err; FXshort *ptr,ch,rep_min,rep_max,lazy;

  // Remember point before atom
  ptr=pc;

  // Process atom
  err=atom(flags,smin,smax);

  // Error in atom
  if(err) return err;

  // Followed by repetition
  if((ch=*pat)=='*' || ch=='+' || ch=='?' || ch=='{'){
    pat++;

    // Repeats may not match empty
    if(!(flags&FLG_WIDTH)) return FXRex::ErrNoAtom;

    // Handle repetition type
    if(ch=='*'){                        // Repeat E [0..INF>
      rep_min=0;
      rep_max=ONEINDIG;
      flags&=~FLG_WIDTH;                // No width!
      smin=0;
      smax=ONEINDIG;
      }
    else if(ch=='+'){                   // Repeat E [1..INF>
      rep_min=1;
      rep_max=ONEINDIG;
      smax=ONEINDIG;
      }
    else if(ch=='?'){                   // Repeat E [0..1]
      rep_min=0;
      rep_max=1;
      flags&=~FLG_WIDTH;                // No width!
      smin=0;
      }
    else{                               // Repeat E [N..M]
      rep_min=0;
      rep_max=ONEINDIG;
      if(*pat!='}'){
        while(Ascii::isDigit(*pat)){
          rep_min=10*rep_min+(*pat-'0');
          pat++;
          }
        rep_max=rep_min;
        if(*pat==','){
          pat++;
          rep_max=ONEINDIG;
          if(*pat!='}'){
            rep_max=0;
            while(Ascii::isDigit(*pat)){
              rep_max=10*rep_max+(*pat-'0');
              pat++;
              }
            }
          }
        if(rep_min>rep_max) return FXRex::ErrRange;     // Illegal range
        if(rep_max>ONEINDIG) return FXRex::ErrCount;    // Bad count
        if(rep_max<=0) return FXRex::ErrCount;          // Bad count
        }
      if(rep_min==0){                   // No width!
        flags&=~FLG_WIDTH;
        }
      smin=rep_min*smin;
      smax=FXMIN(rep_max*smax,ONEINDIG);
      if(*pat!='}') return FXRex::ErrBrace;             // Unmatched brace
      pat++;
      }

    // Handle greedy, lazy, or possessive forms
    if(*pat=='?'){      // Lazy
      lazy=1; pat++;
      }
    else if(*pat=='+'){ // Possessive
      lazy=2; pat++;
      }
    else{               // Greedy
      lazy=0;
      }

    // Handle only non-trivial cases
    if(rep_min!=1 || rep_max!=1){

      // For simple repeats we prefix the last operation
      if(flags&FLG_SIMPLE){
        if(rep_min==0 && rep_max==ONEINDIG){
          insert(ptr,OP_STAR+lazy);
          }
        else if(rep_min==1 && rep_max==ONEINDIG){
          insert(ptr,OP_PLUS+lazy);
          }
        else if(rep_min==0 && rep_max==1){
          insert(ptr,OP_QUEST+lazy);
          }
        else{
          insert(ptr,OP_REP+lazy,rep_min,rep_max);
          }
        }

      // For complex repeats we build loop constructs
      else{
        FXASSERT(lazy!=2);                              // FIXME not yet implemented
        if(rep_min==0 && rep_max==ONEINDIG){
          /*    ________
          **   |        \
          ** --B--(...)--J--+--                 (...){0,ONEINDIG}
          **    \___________|
          */
          insert(ptr,lazy?OP_BRANCHREV:OP_BRANCH,pc-ptr+3);
          append(OP_JUMP,ptr-pc-1);
          }
        else if(rep_min==1 && rep_max==ONEINDIG){
          /*    ________
          **   |        \
          ** --+--(...)--B--                    (...){1,ONEINDIG}
          **
          */
          append(lazy?OP_BRANCH:OP_BRANCHREV,ptr-pc-1);
          }
        else if(rep_min==0 && rep_max==1){
          /*
          **
          ** --B--(...)--+--                    (...){0,1}
          **    \________|
          */
          insert(ptr,lazy?OP_BRANCHREV:OP_BRANCH,pc-ptr+1);
          }
        else if(0<rep_min && rep_min==rep_max){
          /*       ___________
          **      |           \
          ** --Z--+--(...)--I--L--              (...){n,n}
          **
          */
          if(nbra>=NSUBEXP) return FXRex::ErrComplex;
          insert(ptr,OP_ZERO_0+nbra);
          append(OP_INCR_0+nbra);
          append(OP_JUMPLT_0+nbra,rep_min,ptr-pc-1);
          nbra++;
          }
        else if(rep_min==0 && rep_max<ONEINDIG){
          /*       ___________
          **      |           \
          ** --Z--B--(...)--I--L--+--           (...){0,n}
          **       \______________|
          */
          if(nbra>=NSUBEXP) return FXRex::ErrComplex;
          insert(ptr,OP_ZERO_0+nbra);
          insert(ptr+1,lazy?OP_BRANCHREV:OP_BRANCH,pc-ptr+4);
          append(OP_INCR_0+nbra);
          append(OP_JUMPLT_0+nbra,rep_max,ptr-pc-1);
          nbra++;
          }
        else if(0<rep_min && rep_max==ONEINDIG){
          /*       ________________
          **      |   ___________  \
          **      |  |           \  \
          ** --Z--+--+--(...)--I--L--B--        (...){n,ONEINDIG}
          */
          if(nbra>=NSUBEXP) return FXRex::ErrComplex;
          insert(ptr,OP_ZERO_0+nbra);
          append(OP_INCR_0+nbra);
          append(OP_JUMPLT_0+nbra,rep_min,ptr-pc-1);
          append(lazy?OP_BRANCH:OP_BRANCHREV,ptr-pc);
          nbra++;
          }
        else{
          /*       ___________________
          **      |   ___________     \
          **      |  |           \     \
          ** --Z--+--+--(...)--I--L--G--B--+--  (...){n,m}
          **                          \____|
          */
          if(nbra>=NSUBEXP) return FXRex::ErrComplex;
          insert(ptr,OP_ZERO_0+nbra);
          append(OP_INCR_0+nbra);
          append(OP_JUMPLT_0+nbra,rep_min,ptr-pc-1);
          append(OP_JUMPGT_0+nbra,rep_max,3);
          append(lazy?OP_BRANCH:OP_BRANCHREV,ptr-pc);
          nbra++;
          }
        }
      }
    }
  return FXRex::ErrOK;
  }


// Parse atom
FXRex::Error FXCompile::atom(FXint& flags,FXshort& smin,FXshort& smax){
  FXchar buf[MAXCHARS];
  FXshort *ptr,*pp,level,len,ch;
  FXint save;
  FXRex::Error err;
  const FXchar *p;
  flags=FLG_WORST;                                      // Assume the worst
  smin=smax=0;
  switch(*pat){
    case '(':                                           // Subexpression grouping
      ch=*++pat;
      if(ch=='?'){
        ch=*++pat;
        if(ch==':'){                                    // Non capturing parentheses
          pat++;
          err=expression(flags,smin,smax);
          if(err) return err;                           // Propagate error
          }
        else if(ch=='i' || ch=='I' || ch=='n' || ch=='N'){
          pat++;
          save=mode;                                    // Save flags
          if(ch=='i') mode|=FXRex::IgnoreCase;
          if(ch=='I') mode&=~FXRex::IgnoreCase;
          if(ch=='n') mode|=FXRex::Newline;
          if(ch=='N') mode&=~FXRex::Newline;
          err=expression(flags,smin,smax);
          mode=save;                                    // Restore flags
          if(err) return err;                           // Propagate error
          }
        else if(ch=='>'){                               // Atomic sub group (possessive match)
          pat++;
          append(OP_ATOMIC);
          ptr=append(0);
          err=expression(flags,smin,smax);
          if(err) return err;                           // Propagate error
          append(OP_SUCCEED);
          patch(ptr,pc);                                // If subgroup matches, go here!
          }
        else if(ch=='=' || ch=='!'){                    // Positive or negative look ahead
          pat++;
          append((ch=='=')?OP_AHEAD_POS:OP_AHEAD_NEG);
          ptr=append(0);
          err=expression(flags,smin,smax);
          if(err) return err;                           // Propagate error
          append(OP_SUCCEED);
          patch(ptr,pc);                                // If trailing context matches (fails), go here!
          flags=FLG_WORST;                              // Look ahead has no width!
          smin=smax=0;
          }
        else if(ch=='<'){                               // Positive or negative look-behind
          ch=*++pat;
          if(ch!='=' && ch!='!') return FXRex::ErrToken;
          pat++;
          append((ch=='=')?OP_BEHIND_POS:OP_BEHIND_NEG);
          pp=append(0);
          ptr=append(0);
          err=expression(flags,smin,smax);
          if(err) return err;                           // Propagate error
          if(smin!=smax || smax==ONEINDIG) return FXRex::ErrBehind;
          append(OP_SUCCEED);
          fix(pp,smax);                                 // Fix up lookbehind size
          patch(ptr,pc);                                // If trailing context matches (fails), go here!
          flags=FLG_WORST;                              // Look behind has no width!
          smin=smax=0;
          }
        else{
          return FXRex::ErrToken;
          }
        }
      else if(mode&FXRex::Capture){                     // Capturing
        level=++npar;
        if(level>=NSUBEXP) return FXRex::ErrComplex;    // Expression too complex
        append(OP_SUB_BEG_0+level);
        err=expression(flags,smin,smax);
        if(err) return err;                             // Propagate error
        append(OP_SUB_END_0+level);
        }
      else{                                             // Normal
        err=expression(flags,smin,smax);
        if(err) return err;                             // Propagate error
        }
      if(*pat!=')') return FXRex::ErrParent;            // Unmatched parenthesis
      pat++;
      flags&=~FLG_SIMPLE;
      break;
    case '.':                                           // Any character
      pat++;
      append((mode&FXRex::Newline)?OP_ANY_NL:OP_ANY);
      flags=FLG_WIDTH|FLG_SIMPLE;
      smin=smax=1;
      break;
    case '^':                                           // Begin of line
      pat++;
      append(OP_LINE_BEG);
      break;
    case '$':                                           // End of line
      pat++;
      append(OP_LINE_END);
      break;
    case '*':                                           // No preceding atom
    case '+':
    case '?':
    case '{':
      return FXRex::ErrNoAtom;
    case '\0':                                          // Technically, this can not happen!
    case '|':
    case ')':
      return FXRex::ErrNoAtom;
    case '}':                                           // Unmatched brace
      return FXRex::ErrBrace;
    case '[':
      pat++;
      err=charset();
      if(err) return err;                               // Bad character class
      if(*pat!=']') return FXRex::ErrBracket;           // Unmatched bracket
      pat++;
      flags=FLG_WIDTH|FLG_SIMPLE;
      smin=smax=1;
      break;
    case ']':                                           // Unmatched bracket
      return FXRex::ErrBracket;
    case '\\':                                          // Escape sequences which are NOT part of simple character-run
      ch=*(pat+1);
      switch(ch){
        case '\0':                                      // Unexpected pattern end
          return FXRex::ErrNoAtom;
        case 'w':                                       // Word character
          pat+=2;
          append(OP_WORD);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'W':                                       // Non-word character
          pat+=2;
          append((mode&FXRex::Newline)?OP_NOT_WORD_NL:OP_NOT_WORD);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 's':                                       // Space
          pat+=2;
          append((mode&FXRex::Newline)?OP_SPACE_NL:OP_SPACE);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'S':                                       // Non-space
          pat+=2;
          append(OP_NOT_SPACE);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'd':                                       // Digit
          pat+=2;
          append(OP_DIGIT);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'D':                                       // Non-digit
          pat+=2;
          append((mode&FXRex::Newline)?OP_NOT_DIGIT_NL:OP_NOT_DIGIT);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'h':                                       // Hex digit
          pat+=2;
          append(OP_HEX);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'H':                                       // Non-hex digit
          pat+=2;
          append((mode&FXRex::Newline)?OP_NOT_HEX_NL:OP_NOT_HEX);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'p':                                       // Punctuation
          pat+=2;
          append(OP_PUNCT);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'P':                                       // Non-punctuation
          pat+=2;
          append((mode&FXRex::Newline)?OP_NOT_PUNCT_NL:OP_NOT_PUNCT);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'l':                                       // Letter
          pat+=2;
          append(OP_LETTER);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'L':                                       // Non-letter
          pat+=2;
          append((mode&FXRex::Newline)?OP_NOT_LETTER_NL:OP_NOT_LETTER);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'u':                                       // Upper case
          pat+=2;
          append(OP_UPPER);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'U':                                       // Lower case
          pat+=2;
          append(OP_LOWER);
          flags=FLG_WIDTH|FLG_SIMPLE;
          smin=smax=1;
          return FXRex::ErrOK;
        case 'b':                                       // Word boundary
          pat+=2;
          append(OP_WORD_BND);
          return FXRex::ErrOK;
        case 'B':                                       // Word interior
          pat+=2;
          append(OP_WORD_INT);
          return FXRex::ErrOK;
        case 'A':                                       // Match only beginning of string
          pat+=2;
          append(OP_STR_BEG);
          return FXRex::ErrOK;
        case 'Z':                                       // Match only and end of string
          pat+=2;
          append(OP_STR_END);
          return FXRex::ErrOK;
        case '<':                                       // Begin of word
          pat+=2;
          append(OP_WORD_BEG);
          return FXRex::ErrOK;
        case '>':                                       // End of word
          pat+=2;
          append(OP_WORD_END);
          return FXRex::ErrOK;
        case '1':                                       // Back reference to previously matched subexpression
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if(!(mode&FXRex::Capture)) return FXRex::ErrBackRef;     // Can't do backreferences
          level=ch-'0';
          if(level>npar) return FXRex::ErrBackRef;                 // Back reference out of range
          append((mode&FXRex::IgnoreCase)?(OP_REF_CI_0+level):(OP_REF_0+level));
          pat+=2;
          smin=0;
          smax=ONEINDIG;
          return FXRex::ErrOK;
        }
      /*fall*/
    default:
      len=0;
      do{
        p=pat;                                          // In case we need to back up...
        ch=*pat;
        switch(ch){
          case '\\':
            ch=*(pat+1);
            switch(ch){
              case 'w':                                 // Bail out on special matching constructs
              case 'W':
              case 's':
              case 'S':
              case 'd':
              case 'D':
              case 'h':
              case 'H':
              case 'p':
              case 'P':
              case 'l':
              case 'L':
              case 'u':
              case 'U':
              case 'b':
              case 'B':
              case 'A':
              case 'Z':
              case '<':
              case '>':
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7':
              case '8':
              case '9':
                goto x;
              case 'a':                                 // Bell
                pat+=2;
                ch='\a';
                break;
              case 'e':                                 // Escape
                pat+=2;
                ch='\033';
                break;
              case 'f':                                 // Form feed
                pat+=2;
                ch='\f';
                break;
              case 'n':                                 // Newline
                pat+=2;
                ch='\n';
                break;
              case 'r':                                 // Return
                pat+=2;
                ch='\r';
                break;
              case 't':                                 // Tab
                pat+=2;
                ch='\t';
                break;
              case 'v':                                 // Vertical tab
                pat+=2;
                ch='\v';
                break;
              case 'c':                                 // Control character
                pat+=2;
                ch=*pat++;
                if(ch=='\0') return FXRex::ErrNoAtom;      // Unexpected pattern end
                ch=Ascii::toUpper(ch)-'@';
                break;
              case '0':                                 // Octal digit
                pat+=2;
                ch=oct(pat);
                if(ch>=256) return FXRex::ErrToken;        // Characters should be 0..255
                break;
              case 'x':                                 // Hex digit
                pat+=2;
                ch=hex(pat);
                if(ch>=256) return FXRex::ErrToken;        // Characters should be 0..255
                break;
              case '\0':                                // Unexpected pattern end
                return FXRex::ErrNoAtom;
              default:
                pat+=2;
                break;
              }
            break;
          case '^':                                     // Bail out on magic characters
          case '$':
          case '.':
          case '(':
          case ')':
          case '[':
          case ']':
          case '|':
            goto x;
          case '\0':                                    // Unexpected pattern end
            return FXRex::ErrNoAtom;
          default:
            pat++;
            break;
          }

        // Make lower case?
        if(mode&FXRex::IgnoreCase) ch=Ascii::toLower(ch);

        // Add to buffer
        buf[len++]=(FXuchar)ch;
        }
      while(*pat!='\0' && *pat!='*' && *pat!='+' && *pat!='?' && *pat!='{' && len<MAXCHARS);    // FIXME copy straight into regex code

      // Back up one character if followed by a repetition: aaa* is interpreted as (aa)a*
x:    if(1<len && (*pat=='*' || *pat=='+' || *pat=='?' || *pat=='{')){
        pat=p;
        len--;
        }

      FXASSERT(1<=len);

      // Had at least 1 character
      flags=FLG_WIDTH;
      smin=smax=len;

      // Simple only if length is 1
      if(len==1){
        flags|=FLG_SIMPLE;
        append((mode&FXRex::IgnoreCase)?OP_CHAR_CI:OP_CHAR,buf[0]);
        }

      // Array of characters
      else{
        append((mode&FXRex::IgnoreCase)?OP_CHARS_CI_NEW:OP_CHARS_NEW,len,buf);
        }
      break;
    }
  return FXRex::ErrOK;
  }


// True if character is a word character
inline FXbool isWord(FXchar ch){
  return Ascii::isAlphaNumeric(ch) || ch=='_';
  }


// True if character is punctuation (delimiter) character
inline FXbool isDelim(FXchar ch){
  return Ascii::isPunct(ch) && ch!='_';
  }


// The new character class structure:
//
//          <N>
//   ( <lo_1> <hi_1> )
//   ( <lo_2> <hi_2> )
//      ...    ...
//   ( <lo_N> <hi_N> )
//
// Parse character class
FXRex::Error FXCompile::charset(){
  register FXint first,last,op,i;
  FXshort set[16];
  CLEAR(set);
  first=-1;
  if(*pat=='^'){                                  // Negated character class
    op=OP_ANY_BUT;
    pat++;
    }
  else{
    op=OP_ANY_OF;
    }
  if(*pat=='-' || *pat==']') goto in;             // '-' and ']' are literal at begin
  while(*pat!='\0' && *pat!=']'){
in: last=(FXuchar)*pat++;
    if(last=='\\'){
      last=*pat++;
      switch(last){
        case 'w':
          for(i=0; i<256; i++) {if(isWord(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'W':
          for(i=0; i<256; i++){ if(!isWord(i)) INCL(set,i); }
          first=-1;
          continue;
        case 's':
          for(i=0; i<256; i++){ if(Ascii::isSpace(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'S':
          for(i=0; i<256; i++){ if(!Ascii::isSpace(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'd':
          for(i=0; i<256; i++){ if(Ascii::isDigit(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'D':
          for(i=0; i<256; i++){ if(!Ascii::isDigit(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'h':
          for(i=0; i<256; i++){ if(Ascii::isHexDigit(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'H':
          for(i=0; i<256; i++){ if(!Ascii::isHexDigit(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'p':
          for(i=0; i<256; i++){ if(isDelim(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'P':
          for(i=0; i<256; i++){ if(!isDelim(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'l':
          for(i=0; i<256; i++){ if(Ascii::isLetter(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'L':
          for(i=0; i<256; i++){ if(!Ascii::isLetter(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'u':
          for(i=0; i<256; i++){ if(Ascii::isUpper(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'U':
          for(i=0; i<256; i++){ if(Ascii::isLower(i)) INCL(set,i); }
          first=-1;
          continue;
        case 'a':                             // Bell
          last='\a';
          break;
        case 'e':                             // Escape
          last='\033';
          break;
        case 'b':                             // Backspace
          last='\b';
          break;
        case 'f':                             // Form feed
          last='\f';
          break;
        case 'n':                             // Newline
          last='\n';
          break;
        case 'r':                             // Return
          last='\r';
          break;
        case 't':                             // Tab
          last='\t';
          break;
        case 'v':                             // Vertical tab
          last='\v';
          break;
        case 'c':                             // Control character
          last=*pat++;
          if(last=='\0') return FXRex::ErrNoAtom;// Unexpected pattern end
          last=Ascii::toUpper(last)-'@';
          break;
        case '0':                             // Octal digit
          last=oct(pat);
          break;
        case 'x':                             // Hex digit
          last=hex(pat);
          break;
        case '\0':
          return FXRex::ErrNoAtom;               // Unexpected pattern end
        }
      }
    if(first==-1){
      if(mode&FXRex::IgnoreCase){
        INCL(set,Ascii::toLower(last));
        INCL(set,Ascii::toUpper(last));
        }
      else{
        INCL(set,last);
        }
      if(*pat=='-' && *(pat+1)!='\0' && *(pat+1)!=']'){
        first=last;
        pat++;
        }
      }
    else{
      if(first>=last) return FXRex::ErrRange;   // Bad range
      if(mode&FXRex::IgnoreCase){
        for(i=first; i<=last; i++){
          INCL(set,Ascii::toLower(i));
          INCL(set,Ascii::toUpper(i));
          }
        }
      else{
        for(i=first; i<=last; i++){
          INCL(set,i);
          }
        }
      first=-1;
      }
    }

  // Are we matching newlines
  if((op==OP_ANY_BUT) && !(mode&FXRex::Newline) && !ISIN(set,'\n')){
    INCL(set,'\n');
    }

  // Emit opcode
  append(op,set);
  return FXRex::ErrOK;
  }


// Append opcode
FXshort* FXCompile::append(FXshort op){
  register FXshort *val=pc;
  if(code){
    pc[0]=op;
    }
  pc++;
  return val;
  }


// Append one-argument opcode
FXshort* FXCompile::append(FXshort op,FXshort arg){
  register FXshort *val=pc;
  if(code){
    pc[0]=op;
    pc[1]=arg;
    }
  pc+=2;
  return val;
  }


// Append two-argument opcode
FXshort* FXCompile::append(FXshort op,FXshort arg1,FXshort arg2){
  register FXshort *val=pc;
  if(code){
    pc[0]=op;
    pc[1]=arg1;
    pc[2]=arg2;
    }
  pc+=3;
  return val;
  }


// Append character class opcode
FXshort* FXCompile::append(FXshort op,const FXshort set[]){
  register FXshort *val=pc;
  if(code){
    pc[0]=op;
    pc[1]=set[0];
    pc[2]=set[1];
    pc[3]=set[2];
    pc[4]=set[3];
    pc[5]=set[4];
    pc[6]=set[5];
    pc[7]=set[6];
    pc[8]=set[7];
    pc[9]=set[8];
    pc[10]=set[9];
    pc[11]=set[10];
    pc[12]=set[11];
    pc[13]=set[12];
    pc[14]=set[13];
    pc[15]=set[14];
    pc[16]=set[15];
    }
  pc+=17;
  return val;
  }


// Append character array
FXshort* FXCompile::append(FXshort op,FXshort len,const FXchar *data){
  register FXshort *val=pc;
  if(code){
    pc[0]=op;
    pc[1]=len;
    memcpy(pc+2,data,len);
    }
  pc+=(len+5)>>1;
  return val;
  }


// Insert opcode at ptr
FXshort* FXCompile::insert(FXshort *ptr,FXshort op){
  if(code){
    memmove(ptr+1,ptr,sizeof(FXshort)*(pc-ptr));
    ptr[0]=op;
    }
  pc+=1;
  return ptr;
  }


// Insert one-argument opcode at ptr
FXshort* FXCompile::insert(FXshort *ptr,FXshort op,FXshort arg){
  if(code){
    memmove(ptr+2,ptr,sizeof(FXshort)*(pc-ptr));
    ptr[0]=op;
    ptr[1]=arg;
    }
  pc+=2;
  return ptr;
  }


// Insert two-argument opcode at ptr
FXshort* FXCompile::insert(FXshort *ptr,FXshort op,FXshort arg1,FXshort arg2){
  if(code){
    memmove(ptr+3,ptr,sizeof(FXshort)*(pc-ptr));
    ptr[0]=op;
    ptr[1]=arg1;
    ptr[2]=arg2;
    }
  pc+=3;
  return ptr;
  }



// Patch linked set of branches or jumps
// Example:
//
//      Before:        After:
//      ==========================
//      0:  OP_JUMP    0:  OP_JUMP
//      1:  0          1:  9
//      2:  ....       2:  ....
//      3:  OP_JUMP    3:  OP_JUMP
//      4:  -3         4:  6
//      5:  ....       5:  ....
//      6:  ....       6:  ....
//      7:  OP_JUMP    7:  OP_JUMP
// fm-> 8:  -4         8:  2
//      9:  ....       9:  ....
// to->10:  ....      10:  ....
//
void FXCompile::patch(FXshort *fm,FXshort *to){
  register FXshort delta;
  if(code && fm){
    do{
      delta=*fm;
      *fm=to-fm;
      fm+=delta;
      }
    while(delta);
    }
  }


// Fix value
void FXCompile::fix(FXshort *ptr,FXshort val){
  if(code && ptr){
    ptr[0]=val;
    }
  }


/*******************************************************************************/

// FXExecute members

// The workhorse
FXbool FXExecute::match(const FXshort* prog){
  register FXint no,keep,rep_min,rep_max,greed,op;
  register const FXchar *save,*beg,*end;
  register FXchar ch;
  for(;;){
    op=*prog++;
    switch(op){
      case OP_END:
        return true;
      case OP_FAIL:           // Fail (sub) pattern
        return false;
      case OP_SUCCEED:        // Succeed (sub) pattern
        return true;
      case OP_JUMP:
        prog+=*prog;
        break;
      case OP_BRANCH:         // Jump after trying following code
        save=str;
        if(match(prog+1)) return true;
        str=save;
        prog+=*prog;
        break;
      case OP_BRANCHREV:      // Jump before trying following code
        save=str;
        if(match(prog+*prog)) return true;
        str=save;
        prog++;
        break;
      case OP_LINE_BEG:       // Must be at begin of line
        if(str_beg<str){
          if(*(str-1)!='\n') return false;
          }
        else{
          if(mode&FXRex::NotBol) return false;
          }
        break;
      case OP_LINE_END:       // Must be at end of line
        if(str<str_end){
          if(*str!='\n') return false;
          }
        else{
          if(mode&FXRex::NotEol) return false;
          }
        break;
      case OP_WORD_BEG:       // Must be at begin of word
        if(str_beg<str && isWord(*(str-1))) return false;               // FIXME before start of subject is non-word
        if(str_end<=str || !isWord(*str)) return false;
        break;
      case OP_WORD_END:       // Must be at end of word                 // FIXME after end of subject is non-word
        if(str<str_end && isWord(*str)) return false;
        if(str<=str_beg || !isWord(*(str-1))) return false;
        break;
      case OP_WORD_BND:       // Must be at word boundary               // FIXME see above
        if(!(((str==str_beg || !isWord(*(str-1))) && (str<str_end && isWord(*str))) ||
             ((str==str_end || !isWord(*str)) && (str_beg<str && isWord(*(str-1)))))) return false;
        break;
      case OP_WORD_INT:       // Must be inside a word                  // FIXME see above
        if(str==str_beg || !isWord(*(str-1))) return false;
        if(str==str_end || !isWord(*str)) return false;
        break;
      case OP_STR_BEG:        // Must be at begin of entire string
        if(str!=str_beg) return false;
        break;
      case OP_STR_END:        // Must be at end of entire string
        if(str!=str_end) return false;
        break;
      case OP_ANY_OF:         // Match a character in a set
        if(str==str_end || !ISIN(prog,*str)) return false;
        prog+=16;
        str++;
        break;
      case OP_ANY_BUT:        // Match a character NOT in a set
        if(str==str_end || ISIN(prog,*str)) return false;
        prog+=16;
        str++;
        break;
      case OP_CHAR:           // Match single character
        if(str==str_end || *prog != (FXuchar)*str) return false;
        prog++;
        str++;
        break;
      case OP_CHAR_CI:        // Match single character, disregard case
        if(str==str_end || *prog != Ascii::toLower(*str)) return false;
        prog++;
        str++;
        break;
//      case OP_R_CHAR:
//        if(str==str_beg || *prog != *(str-1)) return false;
//        prog++;
//        str--;
//        break;
      case OP_CHARS_NEW:      // Match a run of 1 or more characters
        no=*prog++;
        if(str+no>str_end) return false;
        save=(const FXchar*)prog;
        prog+=((no+1)>>1);
        do{
          if(*save != *str) return false;
          save++;
          str++;
          }
        while(--no);
        break;
      case OP_CHARS_CI_NEW:  // Match a run of 1 or more characters, disregard case
        no=*prog++;
        if(str+no>str_end) return false;
        save=(const FXchar*)prog;
        prog+=((no+1)>>1);
        do{
          if(*save != Ascii::toLower(*str)) return false;
          save++;
          str++;
          }
        while(--no);
        break;
      case OP_SPACE:          // Match space
        if(str==str_end || *str=='\n' || !Ascii::isSpace(*str)) return false;
        str++;
        break;
      case OP_SPACE_NL:       // Match space including newline
        if(str==str_end || !Ascii::isSpace(*str)) return false;
        str++;
        break;
      case OP_NOT_SPACE:      // Match non-space
        if(str==str_end || Ascii::isSpace(*str)) return false;
        str++;
        break;
      case OP_DIGIT:          // Match a digit 0..9
        if(str==str_end || !Ascii::isDigit(*str)) return false;
        str++;
        break;
      case OP_NOT_DIGIT:      // Match a non-digit
        if(str==str_end || *str=='\n' || Ascii::isDigit(*str)) return false;
        str++;
        break;
      case OP_NOT_DIGIT_NL:   // Match a non-digit including newline
        if(str==str_end || Ascii::isDigit(*str)) return false;
        str++;
        break;
      case OP_HEX:            // Match a hex digit 0..9A-Fa-f
        if(str==str_end || !Ascii::isHexDigit(*str)) return false;
        str++;
        break;
      case OP_NOT_HEX:        // Match a non-hex digit
        if(str==str_end || *str=='\n' || Ascii::isHexDigit(*str)) return false;
        str++;
        break;
      case OP_NOT_HEX_NL:     // Match a non-hex digit including newline
        if(str==str_end || Ascii::isHexDigit(*str)) return false;
        str++;
        break;
      case OP_PUNCT:          // Match a punctuation
        if(str==str_end || !isDelim(*str)) return false;
        str++;
        break;
      case OP_NOT_PUNCT:      // Match a non-punctuation
        if(str==str_end || *str=='\n' || isDelim((FXuchar) *str)) return false;
        str++;
        break;
      case OP_NOT_PUNCT_NL:   // Match a non-punctuation including newline
        if(str==str_end || isDelim(*str)) return false;
        str++;
        break;
      case OP_LETTER:         // Match a letter a..z, A..Z
        if(str==str_end || !Ascii::isLetter(*str)) return false;
        str++;
        break;
      case OP_NOT_LETTER:     // Match a non-letter
        if(str==str_end || *str=='\n' || Ascii::isLetter(*str)) return false;
        str++;
        break;
      case OP_NOT_LETTER_NL:  // Match a non-letter including newline
        if(str==str_end || Ascii::isLetter(*str)) return false;
        str++;
        break;
      case OP_WORD:           // Match a word character a..z,A..Z,0..9,_
        if(str==str_end || !isWord(*str)) return false;
        str++;
        break;
      case OP_NOT_WORD:       // Match a non-word character
        if(str==str_end || *str=='\n' || isWord(*str)) return false;
        str++;
        break;
      case OP_NOT_WORD_NL:    // Match a non-word character including newline
        if(str==str_end || isWord(*str)) return false;
        str++;
        break;
      case OP_UPPER:          // Match if uppercase
        if(str==str_end || !Ascii::isUpper(*str)) return false;
        str++;
        break;
      case OP_LOWER:          // Match if lowercase
        if(str==str_end || !Ascii::isLower(*str)) return false;
        str++;
        break;
      case OP_ANY:            // Match any character
        if(str==str_end || *str=='\n') return false;
        str++;
        break;
      case OP_ANY_NL:         // Matches any character including newline
        if(str==str_end) return false;
        str++;
        break;
      case OP_MIN_PLUS:       // Lazy one or more repetitions
        rep_min=1;
        rep_max=ONEINDIG;
        greed=0;
        goto rep;
      case OP_POS_PLUS:       // Possessive one or more repetitions
        rep_min=1;
        rep_max=ONEINDIG;
        greed=2;
        goto rep;
      case OP_PLUS:           // Greedy one or more repetitions
        rep_min=1;
        rep_max=ONEINDIG;
        greed=1;
        goto rep;
      case OP_MIN_QUEST:      // Lazy zero or one
        rep_min=0;
        rep_max=1;
        greed=0;
        goto rep;
      case OP_POS_QUEST:      // Possessive zero or one
        rep_min=0;
        rep_max=1;
        greed=2;
        goto rep;
      case OP_QUEST:          // Greedy zero or one
        rep_min=0;
        rep_max=1;
        greed=1;
        goto rep;
      case OP_MIN_REP:        // Lazy bounded repeat
        rep_min=*prog++;
        rep_max=*prog++;
        greed=0;
        goto rep;
      case OP_POS_REP:        // Possessive bounded repeat
        rep_min=*prog++;
        rep_max=*prog++;
        greed=2;
        goto rep;
      case OP_REP:            // Greedy bounded repeat
        rep_min=*prog++;
        rep_max=*prog++;
        greed=1;
        goto rep;
      case OP_MIN_STAR:       // Lazy zero or more repetitions
        rep_min=0;
        rep_max=ONEINDIG;
        greed=0;
        goto rep;
      case OP_POS_STAR:       // Possessive zero or more repetitions
        rep_min=0;
        rep_max=ONEINDIG;
        greed=2;
        goto rep;
      case OP_STAR:           // Greedy zero or more repetitions
        rep_min=0;
        rep_max=ONEINDIG;
        greed=1;

        // We need to match more characters than are available
rep:    if(str+rep_min>str_end) return false;
        beg=str;
        end=str+rep_max;
        if(end>str_end) end=str_end;
        save=beg;

        // Find out how much could be matched
        op=*prog++;
        switch(op){
          case OP_CHAR:
            ch=*prog++;
            while(save<end && (FXuchar)*save==ch) save++;
            break;
          case OP_CHAR_CI:
            ch=*prog++;
            while(save<end && Ascii::toLower(*save)==ch && *save!='\n') save++;
            break;
          case OP_CHARS_NEW:
            no=*prog++;
            prog+=((no+1)>>1);
            // FIXME //
            FXASSERT(0);
            return false;
            break;
          case OP_CHARS_CI_NEW:
            no=*prog++;
            prog+=((no+1)>>1);
            // FIXME //
            FXASSERT(0);
            return false;
            break;
          case OP_ANY_OF:
            while(save<end && ISIN(prog,*save)) save++;
            prog+=16;
            break;
          case OP_ANY_BUT:
            while(save<end && !ISIN(prog,*save)) save++;
            prog+=16;
            break;
          case OP_SPACE:
            while(save<end && *save!='\n' && Ascii::isSpace(*save)) save++;
            break;
          case OP_SPACE_NL:
            while(save<end && Ascii::isSpace(*save)) save++;
            break;
          case OP_NOT_SPACE:
            while(save<end && !Ascii::isSpace(*save)) save++;
            break;
          case OP_DIGIT:
            while(save<end && Ascii::isDigit(*save)) save++;
            break;
          case OP_NOT_DIGIT:
            while(save<end && *save!='\n' && !Ascii::isDigit(*save)) save++;
            break;
          case OP_NOT_DIGIT_NL:
            while(save<end && !Ascii::isDigit(*save)) save++;
            break;
          case OP_HEX:
            while(save<end && Ascii::isHexDigit(*save)) save++;
            break;
          case OP_NOT_HEX:
            while(save<end && *save!='\n' && !Ascii::isHexDigit(*save)) save++;
            break;
          case OP_NOT_HEX_NL:
            while(save<end && !Ascii::isHexDigit(*save)) save++;
            break;
          case OP_PUNCT:
            while(save<end && isDelim(*save)) save++;
            break;
          case OP_NOT_PUNCT:
            while(save<end && *save!='\n' && !isDelim(*save)) save++;
            break;
          case OP_NOT_PUNCT_NL:
            while(save<end && !isDelim(*save)) save++;
            break;
          case OP_LETTER:
            while(save<end && Ascii::isLetter(*save)) save++;
            break;
          case OP_NOT_LETTER:
            while(save<end && *save!='\n' && !Ascii::isLetter(*save)) save++;
            break;
          case OP_NOT_LETTER_NL:
            while(save<end && !Ascii::isLetter(*save)) save++;
            break;
          case OP_WORD:
            while(save<end && isWord(*save)) save++;
            break;
          case OP_NOT_WORD:
            while(save<end && *save!='\n' && !isWord(*save)) save++;
            break;
          case OP_NOT_WORD_NL:
            while(save<end && !isWord(*save)) save++;
            break;
          case OP_UPPER:
            while(save<end && Ascii::isUpper(*save)) save++;
            break;
          case OP_LOWER:
            while(save<end && Ascii::isLower(*save)) save++;
            break;
          case OP_ANY:
            while(save<end && *save!='\n') save++;
            break;
          case OP_ANY_NL:
            save=end;                   // Big byte
            break;
          default:
            fxerror("FXRex::match: bad opcode (%d) at: %p on line: %d\n",op,prog-1,__LINE__);
            break;
          }

        // Matched fewer than the minimum desired so bail out
        if(save<beg+rep_min) return false;

        // We must match between beg and end characters
        beg+=rep_min;
        end=save;

        switch(greed){
          case 0:                       // Lazily match the fewest characters
            while(beg<=end){
              str=beg;
              if(match(prog)) return true;
              beg++;
              }
            return false;
          case 1:                       // Greedily match the most characters
            while(beg<=end){
              str=end;
              if(match(prog)) return true;
              end--;
              }
            return false;
          case 2:                       // Possessive match
            return match(prog);
          }
        return false;
      case OP_SUB_BEG_0:                // Capturing open parentheses
      case OP_SUB_BEG_1:
      case OP_SUB_BEG_2:
      case OP_SUB_BEG_3:
      case OP_SUB_BEG_4:
      case OP_SUB_BEG_5:
      case OP_SUB_BEG_6:
      case OP_SUB_BEG_7:
      case OP_SUB_BEG_8:
      case OP_SUB_BEG_9:
        no=op-OP_SUB_BEG_0;
        bak_beg[no]=save=str;           // Back reference start set
        bak_end[no]=NULL;               // Back reference end
        if(match(prog)){                // Match the rest
          if(no<npar) sub_beg[no]=save-str_beg;
          return true;
          }
        return false;
      case OP_SUB_END_0:                // Capturing close parentheses
      case OP_SUB_END_1:
      case OP_SUB_END_2:
      case OP_SUB_END_3:
      case OP_SUB_END_4:
      case OP_SUB_END_5:
      case OP_SUB_END_6:
      case OP_SUB_END_7:
      case OP_SUB_END_8:
      case OP_SUB_END_9:
        no=op-OP_SUB_END_0;
        bak_end[no]=save=str;           // Back reference end
        if(match(prog)){                // Match the rest
          if(no<npar) sub_end[no]=save-str_beg;
          return true;
          }
        return false;
      case OP_REF_0:                    // Back reference to capturing parentheses
      case OP_REF_1:
      case OP_REF_2:
      case OP_REF_3:
      case OP_REF_4:
      case OP_REF_5:
      case OP_REF_6:
      case OP_REF_7:
      case OP_REF_8:
      case OP_REF_9:
        no=op-OP_REF_0;
        beg=bak_beg[no];                // Get back reference start
        end=bak_end[no];                // Get back reference end
        if(!beg) return false;
        if(!end) return false;
        if(beg<end){                                  // Empty capture matches!
          if(str+(end-beg)>str_end) return false;     // Not enough characters left
          do{
            if(*beg != *str) return false;            // No match
            beg++;
            str++;
            }
          while(beg<end);
          }
        break;
      case OP_REF_CI_0:               // Back reference to capturing parentheses
      case OP_REF_CI_1:
      case OP_REF_CI_2:
      case OP_REF_CI_3:
      case OP_REF_CI_4:
      case OP_REF_CI_5:
      case OP_REF_CI_6:
      case OP_REF_CI_7:
      case OP_REF_CI_8:
      case OP_REF_CI_9:
        no=op-OP_REF_CI_0;
        beg=bak_beg[no];                // Get back reference start
        end=bak_end[no];                // Get back reference end
        if(!beg) return false;
        if(!end) return false;
        if(beg<end){                                  // Empty capture matches!
          if(str+(end-beg)>str_end) return false;     // Not enough characters left
          do{
            if(Ascii::toLower(*beg) != Ascii::toLower(*str)) return false;      // No match
            beg++;
            str++;
            }
          while(beg<end);
          }
        break;
      case OP_AHEAD_NEG:                // Positive or negative look ahead
      case OP_AHEAD_POS:
        save=str;
        keep=match(prog+1);             // Match the assertion
        str=save;
        if((op-OP_AHEAD_NEG)!=keep) return false;       // Didn't get what we expected
        prog=prog+*prog;                // Jump to code after OP_SUCCEED
        break;
      case OP_BEHIND_NEG:               // Positive or negative look-behind
      case OP_BEHIND_POS:
        no=*prog++;                     // Backward skip amount
        keep=false;
        if(str_beg<=str-no){            // Can we go back far enough?
          save=str;
          str-=no;
          keep=match(prog+1);           // Match the assertion
          str=save;
          }
        if((op-OP_BEHIND_NEG)!=keep) return false;      // Didn't get what we expected
        prog=prog+*prog;                // Jump to code after OP_SUCCEED
        break;
      case OP_ATOMIC:
        if(!match(prog+1)) return false;
        prog=prog+*prog;                // Jump to code after OP_SUCCEED
        break;
      case OP_ZERO_0:                   // Initialize counter for counting repeat
      case OP_ZERO_1:
      case OP_ZERO_2:
      case OP_ZERO_3:
      case OP_ZERO_4:
      case OP_ZERO_5:
      case OP_ZERO_6:
      case OP_ZERO_7:
      case OP_ZERO_8:
      case OP_ZERO_9:
        count[op-OP_ZERO_0]=0;
        break;
      case OP_INCR_0:                   // Increment counter for counting repeat
      case OP_INCR_1:
      case OP_INCR_2:
      case OP_INCR_3:
      case OP_INCR_4:
      case OP_INCR_5:
      case OP_INCR_6:
      case OP_INCR_7:
      case OP_INCR_8:
      case OP_INCR_9:
        count[op-OP_INCR_0]++;
        break;
      case OP_JUMPLT_0:               // Jump if counter less than value
      case OP_JUMPLT_1:
      case OP_JUMPLT_2:
      case OP_JUMPLT_3:
      case OP_JUMPLT_4:
      case OP_JUMPLT_5:
      case OP_JUMPLT_6:
      case OP_JUMPLT_7:
      case OP_JUMPLT_8:
      case OP_JUMPLT_9:
        if(count[op-OP_JUMPLT_0] < *prog++)   // Compare with value
          prog+=*prog;
        else
          prog++;
        break;
      case OP_JUMPGT_0:               // Jump if counter greater than value
      case OP_JUMPGT_1:
      case OP_JUMPGT_2:
      case OP_JUMPGT_3:
      case OP_JUMPGT_4:
      case OP_JUMPGT_5:
      case OP_JUMPGT_6:
      case OP_JUMPGT_7:
      case OP_JUMPGT_8:
      case OP_JUMPGT_9:
        if(count[op-OP_JUMPGT_0] > *prog++)   // Compare with value
          prog+=*prog;
        else
          prog++;
        break;
      default:
        fxerror("FXRex::match: bad opcode (%d) at: %p on line: %d\n",op,prog-1,__LINE__);
        break;
      }
    }
  return false;
  }


// regtry - try match at specific point; 0 failure, 1 success
FXbool FXExecute::attempt(const FXchar* string){
  register FXint i=npar;
  str=string;
  do{--i;sub_beg[i]=sub_end[i]=-1;}while(i);            // Possibly move this to FXExecute::execute?
  if(match(code+1)){
    if(string!=str || !(mode&FXRex::NotEmpty)){         // Match if non-empty or empty is allowed!
      sub_beg[0]=string-str_beg;
      sub_end[0]=str-str_beg;
      return true;
      }
    }
  return false;
  }


// Match subject string, returning number of matches found
FXbool FXExecute::execute(const FXchar* fm,const FXchar* to){
  register FXint ch;

  // Simple case
  if(fm==to) return attempt(fm);

  // Match backwards
  if(mode&FXRex::Backward){
#if 0
    if(code[1]==OP_STR_BEG){                          // Anchored at string start
      return (fm==str_beg) && attempt(str_beg);
      }
    if(code[1]==OP_LINE_BEG){                         // Anchored at BOL
      while(fm<=to){
        if(((to==str_beg)||(*(to-1)=='\n')) && attempt(to)) return true;
        to--;
        }
      return false;
      }
    if(code[1]==OP_CHAR || code[1]==OP_CHARS){        // Known starting character
      ch=(code[1]==OP_CHAR)?code[2]:code[3];
      if(to==str_end) to--;
      while(fm<=to){
        if((FXuchar)*to==ch && attempt(to)) return true;
        to--;
        }
      return false;
      }
    if(code[1]==OP_CHAR_CI || code[1]==OP_CHARS_CI){  // Known starting character, ignoring case
      ch=(code[1]==OP_CHAR_CI)?code[2]:code[3];
      if(to==str_end) to--;
      while(fm<=to){
        if(Ascii::toLower((FXuchar)*to)==ch && attempt(to)) return true;
        to--;
        }
      return false;
      }
#endif
    while(fm<=to){                                    // General case
      if(attempt(to)) return true;
      to--;
      }
    }

  // Match forwards
  else{
#if 0
    if(code[1]==OP_STR_BEG){                          // Anchored at string start
      return (fm==str_beg) && attempt(str_beg);
      }
    if(code[1]==OP_LINE_BEG){                         // Anchored at BOL
      while(fm<=to){
        if(((fm==str_beg)||(*(fm-1)=='\n')) && attempt(fm)) return true;
        fm++;
        }
      return false;
      }
    if(code[1]==OP_CHAR || code[1]==OP_CHARS){        // Known starting character
      ch=(code[1]==OP_CHAR)?code[2]:code[3];
      if(to==str_end) to--;
      while(fm<=to){
        if((FXuchar)*fm==ch && attempt(fm)) return true;
        fm++;
        }
      return false;
      }
    if(code[1]==OP_CHAR_CI || code[1]==OP_CHARS_CI){  // Known starting character, ignoring case
      ch=(code[1]==OP_CHAR_CI)?code[2]:code[3];
      if(to==str_end) to--;
      while(fm<=to){
        if(Ascii::toLower((FXuchar)*fm)==ch && attempt(fm)) return true;
        fm++;
        }
      return false;
      }
#endif
    while(fm<=to){                                   // General case
      if(attempt(fm)) return true;
      fm++;
      }
    }
  return false;
  }

}

/*******************************************************************************/

namespace FX {

// Table of error messages
const FXchar *const FXRex::errors[]={
  "OK",
  "Empty pattern",
  "Unmatched parenthesis",
  "Unmatched bracket",
  "Unmatched brace",
  "Bad character range",
  "Bad escape sequence",
  "Bad counted repeat",
  "No atom preceding repetition",
  "Repeat following repeat",
  "Bad backward reference",
  "Bad character class",
  "Expression too complex",
  "Out of memory",
  "Illegal token",
  "Bad look-behind pattern"
  };


// Default program always fails
const FXshort FXRex::fallback[]={2,OP_FAIL};



// Construct empty regular expression object
FXRex::FXRex():code((FXshort*)(void*)fallback){
  }


// Copy regex object
FXRex::FXRex(const FXRex& orig):code((FXshort*)(void*)fallback){
  if(orig.code!=fallback){
    dupElms(code,orig.code,orig.code[0]);
    }
  }


// Compile expression from pattern; fail if error
FXRex::FXRex(const FXchar* pattern,FXint mode,FXRex::Error* error):code((FXshort*)(void*)fallback){
  FXRex::Error err=parse(pattern,mode);
  if(error){ *error=err; }
  }


// Compile expression from pattern; fail if error
FXRex::FXRex(const FXString& pattern,FXint mode,FXRex::Error* error):code((FXshort*)(void*)fallback){
  FXRex::Error err=parse(pattern.text(),mode);
  if(error){ *error=err; }
  }


// Assignment
FXRex& FXRex::operator=(const FXRex& orig){
  if(code!=orig.code){
    if(code!=fallback) freeElms(code);
    code=(FXshort*)(void*)fallback;
    if(orig.code!=fallback){
      dupElms(code,orig.code,orig.code[0]);
      }
    }
  return *this;
  }


/*******************************************************************************/

#ifdef REXDEBUG
#include "fxrexdbg.h"
#endif


// Parse pattern
FXRex::Error FXRex::parse(const FXchar* pattern,FXint mode){
  FXRex::Error err=FXRex::ErrEmpty;
  FXCompile cs;
  FXint flags,size;

  // Free old code, if any
  if(code!=fallback) freeElms(code);
  code=(FXshort*)(void*)fallback;

  // Check
  if(pattern){

    // Fill in compile data
    cs.code=NULL;
    cs.pc=NULL;
    cs.pat=pattern;
    cs.mode=mode;
    cs.nbra=0;
    cs.npar=0;

    // Unknown size
    cs.append(0);

    // Check syntax and amount of memory needed
    err=cs.compile(flags);
    if(!err){

      // Compile code unless only syntax checking
      if(!(mode&FXRex::Syntax)){

        // Allocate new code
        size=cs.pc-((FXshort*)NULL);
        if(!allocElms(code,size)){
          code=(FXshort*)(void*)fallback;
          return FXRex::ErrMemory;
          }

        // Fill in compile data
        cs.code=code;
        cs.pc=code;
        cs.pat=pattern;
        cs.mode=mode;
        cs.nbra=0;
        cs.npar=0;

        // Size of program
        cs.append(size);

        // Generate program
        err=cs.compile(flags);

        // Dump for debugging
#ifdef REXDEBUG
        if(fxTraceLevel>100) dump(code);
#endif
        }
      }
    }
  return err;
  }


// Parse pattern, return error code if syntax error is found
FXRex::Error FXRex::parse(const FXString& pattern,FXint mode){
  return parse(pattern.text(),mode);
  }


/*******************************************************************************/


// Match subject string, returning true if match found
FXbool FXRex::match(const FXchar* string,FXint len,FXint* beg,FXint* end,FXint mode,FXint npar,FXint fm,FXint to) const {
  if(!string || len<0 || npar<1 || NSUBEXP<npar){ fxerror("FXRex::match: bad argument.\n"); }
  if(fm<0) fm=0;
  if(to>len) to=len;
  if(__likely(fm<=to)){
    FXint abeg[NSUBEXP];
    FXint aend[NSUBEXP];
    FXExecute ms;
    if(!beg) beg=abeg;
    if(!end) end=aend;
    ms.str_beg=string;
    ms.str_end=string+len;
    ms.sub_beg=beg;
    ms.sub_end=end;
    ms.code=code;
    ms.npar=npar;
    ms.mode=mode;
    return ms.execute(string+fm,string+to);
    }
  return false;
  }


// Search for match in string
FXbool FXRex::match(const FXString& string,FXint* beg,FXint* end,FXint mode,FXint npar,FXint fm,FXint to) const {
  return match(string.text(),string.length(),beg,end,mode,npar,fm,to);
  }


// Return substitution string
FXString FXRex::substitute(const FXchar* string,FXint len,FXint* beg,FXint* end,const FXString& replace,FXint npar){
  register FXint ch,n,i=0;
  FXString result;
  if(!string || len<0 || !beg || !end || npar<1 || NSUBEXP<npar){ fxerror("FXRex::substitute: bad argument.\n"); }
  while((ch=replace[i++])!='\0'){
    if(ch=='&'){
      if(0<=beg[0] && end[0]<=len){result.append(&string[beg[0]],end[0]-beg[0]);}
      }
    else if(ch=='\\' && '0'<=replace[i] && replace[i]<='9'){
      n=replace[i++]-'0';
      if(n<npar && 0<=beg[n] && end[n]<=len){result.append(&string[beg[n]],end[n]-beg[n]);}
      }
    else{
      if(ch=='\\' && (replace[i]=='\\' || replace[i]=='&')){ch=replace[i++];}
      result.append(ch);
      }
    }
  return result;
  }


// Return substitution string
FXString FXRex::substitute(const FXString& string,FXint* beg,FXint* end,const FXString& replace,FXint npar){
  return substitute(string.text(),string.length(),beg,end,replace,npar);
  }


// Equality
FXbool FXRex::operator==(const FXRex& rex) const {
  return code==rex.code || (code[0]==rex.code[0] && memcmp(code,rex.code,sizeof(FXint)*code[0])==0);
  }


// Inequality
FXbool FXRex::operator!=(const FXRex& rex) const {
  return !operator==(rex);
  }


// Save
FXStream& operator<<(FXStream& store,const FXRex& s){
  FXshort size=s.code[0];
  store << size;
  store.save(s.code+1,size-1);
  return store;
  }


// Load
FXStream& operator>>(FXStream& store,FXRex& s){
  FXshort size;
  store >> size;
  allocElms(s.code,size);
  store.load(s.code+1,size-1);
  return store;
  }


// Clean up
FXRex::~FXRex(){
  if(code!=fallback) freeElms(code);
  }

}
