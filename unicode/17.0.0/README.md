# Unicode 17.0.0 grapheme and White_Space data

Unmodified data downloaded from Unicode's public versioned distribution:

- https://www.unicode.org/Public/17.0.0/ucd/auxiliary/GraphemeBreakProperty.txt
- https://www.unicode.org/Public/17.0.0/ucd/auxiliary/GraphemeBreakTest.txt
- https://www.unicode.org/Public/17.0.0/ucd/emoji/emoji-data.txt
- https://www.unicode.org/Public/17.0.0/ucd/DerivedCoreProperties.txt
- https://www.unicode.org/Public/17.0.0/ucd/PropList.txt

The Unicode data license is included as `LICENSE.txt`. Copyright notices in the
data files are retained. `scripts/build-grapheme-data.py` reads these local
files to produce `grapheme_data.h`; ordinary builds do not require downloads.
`tests/grapheme_unit_test.py` checks all 766 official grapheme-break cases in
both compiler constant evaluation and generated runtime code.

`PropList.txt` supplies the 25 White_Space code points used by text.trim.
`tests/text_engine_test.py` also checks text matching against the original
grapheme boundaries from every official conformance case.
