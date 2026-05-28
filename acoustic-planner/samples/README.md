# REW samples

Drop your REW exports here:

- **`*.txt`** — REW → *File ▸ Export ▸ Export measurement as text* (primary; the
  parser is tuned to this).
- **`*.wav`** — REW impulse-response export (optional; enables RT60-from-IR).

Once a real sample is here, we confirm the exact delimiter, column order, and
header lines against `Sources/Measurement/REWTextParser.swift`.

(These files are user data and are git-ignored by default — see `.gitignore`.)
