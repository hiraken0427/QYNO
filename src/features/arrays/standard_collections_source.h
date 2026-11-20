/* Standard collection queries, specialized by the ordinary type checker. */
#ifndef QYNO_STANDARD_COLLECTIONS_SOURCE_H
#define QYNO_STANDARD_COLLECTIONS_SOURCE_H
#define QYNO_COLLECTIONS_SOURCE \
"filter(values, predicate);\n" \
"\tauto result=values\n" \
"\tresult.clear()\n" \
"\tfor(0 <= i < values.length, i++);\n" \
"\t\tif(predicate(values[i]));\n" \
"\t\t\tresult.append(values[i])\n" \
"\treturn result\n" \
"\n" \
"int_64 count(values, predicate);\n" \
"\tint_64 result=0\n" \
"\tfor(0 <= i < values.length, i++);\n" \
"\t\tif(predicate(values[i]));\n" \
"\t\t\tresult++\n" \
"\treturn result\n" \
"\n" \
"bool any(values, predicate);\n" \
"\tfor(0 <= i < values.length, i++);\n" \
"\t\tif(predicate(values[i]));\n" \
"\t\t\treturn true\n" \
"\treturn false\n" \
"\n" \
"bool all(values, predicate);\n" \
"\tfor(0 <= i < values.length, i++);\n" \
"\t\tif(not predicate(values[i]));\n" \
"\t\t\treturn false\n" \
"\treturn true\n" \
"\n"
#endif
