# Spanish Braille Reference (Grade 1, no contractions)

Reference for mapping Spanish text to Braille dot patterns. For use by agents/LLMs. Grade 1 only; contractions are not covered.

---

## Braille cell (6 dots)

Positions in the cell:

- Top row: position 1 (left), position 4 (right)
- Middle row: position 2 (left), position 5 (right)
- Bottom row: position 3 (left), position 6 (right)

Each character is described as "dots" followed by position numbers (e.g. `1`, `1-2-4`).

---

## Basic letters (a–z)

| Char | Dots   | Char | Dots     | Char | Dots       |
|------|--------|------|----------|------|------------|
| a    | 1      | j    | 2-4-5    | s    | 2-3-4      |
| b    | 1-2    | k    | 1-3      | t    | 2-3-4-5    |
| c    | 1-4    | l    | 1-2-3    | u    | 1-3-6      |
| d    | 1-4-5  | m    | 1-3-4    | v    | 1-2-3-6    |
| e    | 1-5    | n    | 1-3-4-5  | w    | 2-4-5-6    |
| f    | 1-2-4  | o    | 1-3-5    | x    | 1-3-4-6    |
| g    | 1-2-4-5| p    | 1-2-3-4  | y    | 1-3-4-5-6  |
| h    | 1-2-5  | q    | 1-2-3-4-5| z    | 1-3-5-6    |
| i    | 2-4    | r    | 1-2-3-5  |      |            |

---

## Spanish ñ and accented letters

| Char | Dots       |
|------|------------|
| ñ    | 1-2-4-5-6  |
| á    | 1-6        |
| é    | 2-3-4-6    |
| í    | 3-4        |
| ó    | 3-4-6      |
| ú    | 2-3-5-6    |
| ü    | 1-2-5-6    |

Note: Spanish uses opening ¿ and ¡. In Braille they use the same dot pattern as ? and ! respectively (one sign for opening, one for closing).

---

## Numbers (0–9)

- **Number sign:** dots 3-4-5-6. Place it immediately before the digit sequence.
- Digits use the same patterns as letters a–j: 1→a, 2→b, 3→c, 4→d, 5→e, 6→f, 7→g, 8→h, 9→i, 0→j.
- A space (or non-digit) ends the number mode; repeat the number sign before the next number if needed.

| Digit | Dots   |
|-------|--------|
| 1     | 1      |
| 2     | 1-2    |
| 3     | 1-4    |
| 4     | 1-4-5  |
| 5     | 1-5    |
| 6     | 1-2-4  |
| 7     | 1-2-4-5|
| 8     | 1-2-5  |
| 9     | 2-4    |
| 0     | 2-4-5  |

---

## Punctuation

| Character        | Dots   |
|------------------|--------|
| Period (.)       | 2-5-6  |
| Comma (,)        | 2      |
| Question (?, ¿)  | 2-6    |
| Exclamation (!, ¡)| 2-3-5 |
| Semicolon (;)    | 2-3    |
| Colon (:)        | 2-5    |
| Hyphen (-)       | 3-6    |
| Apostrophe (')   | 3      |
| Opening quote (")| 2-3-6  |
| Closing quote (")| 3-5-6  |

---

## Capital letter

- **Capital sign:** dots 6. Place it immediately before the letter to capitalize.
- Spanish Braille has no "double capital" sign; use one capital sign per capitalized letter when needed.

---

## Exclusions

Contractions (Grade 2) are not covered; this reference is Grade 1 only.

---

## Example

"hola" → h: 1-2-5, o: 1-3-5, l: 1-2-3, a: 1 → dots sequences: `1-2-5`, `1-3-5`, `1-2-3`, `1`.
