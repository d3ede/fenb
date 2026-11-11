FENB-UI.ttf is a static, subsetted build of Inter (weight 450, optical size 14).
Source: https://github.com/google/fonts/tree/main/ofl/inter
License: SIL Open Font License 1.1; see OFL.txt.
The embedded subset includes Latin, Cyrillic, punctuation and UI symbols.
The unmodified source is retained as Inter.ttf for reproducibility.

Build-time reduction with fontTools (not required at runtime):
python -m fontTools.varLib.instancer Inter.ttf wght=450 opsz=14 --output=FENB-UI-full.ttf
python -m fontTools.subset FENB-UI-full.ttf --unicodes=U+0020-007E,U+00A0-00FF,U+0400-052F,U+2000-206F,U+20BD,U+2190-2199,U+2212 --layout-features=* --no-hinting --output-file=FENB-UI.ttf
node tools/embed-font.mjs embeds the subset in the DLL without a duplicate heap copy.
