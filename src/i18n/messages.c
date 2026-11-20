#include "qyc.h"
#include <string.h>
typedef struct {char *text;const char *en,*ja;} Message;
char qyno_message_0000[11] = "\tInput | ";
char qyno_message_0001[111] = "(Default is input name.exe for Windows, no extension for Linux) \n";
char qyno_message_0002[96] = "--target <OS> The OS (windows or linux) to output to. Default is running OS\n";
char qyno_message_0003[55] = "--version Show version \n";
char qyno_message_0004[92] = "  -I <directory> Add an import search root (repeatable)\n";
char qyno_message_0005[102] = "-S Outputs only the assembly (.s) without creating an executable file \n";
char qyno_message_0006[65] = "-o <file> Specify output file name \n";
char qyno_message_0007[29] = "interface argument %.*s: %s\n";
char qyno_message_0008[54] = "%% can only be used between ints (%s and %s)";
char qyno_message_0009[50] = "%.*s is the same name as const on line %d";
char qyno_message_0010[84] = "%.*s is not a ref variable, so the location (addr) cannot be changed";
char qyno_message_0011[55] = "%.*s is already declared on line %d";
char qyno_message_0012[19] = "Argument %d";
char qyno_message_0013[115] = "Argument %d is a ref parameter. Use ref at the call site (for example, f(ref x))";
char qyno_message_0014[80] = "Argument %d is passed by value; remove ref";
char qyno_message_0015[79] = "Argument %d: A for-loop variable cannot be passed by ref";
char qyno_message_0016[118] = "Argument %d: ref requires %s (%s cannot be passed; ref does not convert types)";
char qyno_message_0017[102] = "Argument %d was inferred as %s. Check ref at the call site";
char qyno_message_0018[35] = "%s and %s cannot be compared";
char qyno_message_0019[35] = "%s and %s cannot be calculated";
char qyno_message_0020[28] = "Cannot read as %s";
char qyno_message_0021[46] = "%s does not have this method";
char qyno_message_0022[44] = "%s has no method %s";
char qyno_message_0023[89] = "%s does not take parentheses (since 0.0.21, replace %s() with %s)";
char qyno_message_0024[103] = "%s cannot be entered directly. Please specify a variable or terminal array element";
char qyno_message_0025[44] = "%s does not have field %s";
char qyno_message_0026[43] = "Unable to give execute permission to %s";
char qyno_message_0027[28] = "Unable to write to %s";
char qyno_message_0028[79] = "Cannot write to %s (please exit if it is running)";
char qyno_message_0029[93] = "%s accepts numeric values (int, float, double); received %s";
char qyno_message_0030[124] = "%s.init has required arguments. Initialize the entire element or field";
char qyno_message_0031[40] = "Unable to get size of %s";
char qyno_message_0032[41] = "Initialization of %s refused\n";
char qyno_message_0033[71] = "Variable locations in %s and %s cannot be compared";
char qyno_message_0034[29] = "%s has %d arguments";
char qyno_message_0035[84] = "%s has 1 or 2 arguments (value, number of digits) (%d is passed)";
char qyno_message_0036[58] = "%s has one argument (%d is passed)";
char qyno_message_0037[49] = "The argument of %s should be str / char";
char qyno_message_0038[40] = "Failed to write %s";
char qyno_message_0039[75] = "The number of digits for %s should be int (%s is passed)";
char qyno_message_0040[46] = "Search string for %s cannot be empty";
char qyno_message_0041[34] = "Out of range of %s";
char qyno_message_0042[75] = "%s can only be compared with == and != (%s cannot be used)";
char qyno_message_0043[96] = "%s must implement interface %s public method %.*s (contract: %s:%d)";
char qyno_message_0044[101] = "%s does not implement interface %s. Please pass the ref of the same contract";
char qyno_message_0045[129] = "%s does not refer to io.%s. Check using io, selective using, and the namespace";
char qyno_message_0046[60] = "%s can only be used in a loop (for/while)";
char qyno_message_0047[61] = "%s is obsolete. Please use %s";
char qyno_message_0048[64] = "%s is obsolete. Please use %s.%s";
char qyno_message_0049[130] = "%s is an existing constant/function/type/namespace. Cannot replace type name with omitted declaration";
char qyno_message_0050[28] = "Unable to load %s";
char qyno_message_0051[26] = "Unable to open %s: %s";
char qyno_message_0052[42] = "%s() accepts str or char";
char qyno_message_0053[129] = "The type, argument, array constraint, and readonly of %s.%.*s do not match interface %s (contract: %s:%d, implementation: %s:%d)";
char qyno_message_0054[100] = "%s.%.*s is private. Not accessible from this location (Declaration: %s:%d:%d)";
char qyno_message_0055[115] = "%s.%.*s cannot implement interface because it is private or init (contract: %s:%d, candidate: %s:%d)";
char qyno_message_0056[32] = "%s.%s has %d arguments";
char qyno_message_0057[40] = "%s.init has no argument %s";
char qyno_message_0058[48] = "%s.init is missing required argument %.*s";
char qyno_message_0059[18] = "Requires %s";
char qyno_message_0060[30] = "%s subscript is out of range";
char qyno_message_0061[69] = "%s should be bool (%s is specified)";
char qyno_message_0062[134] = "%s: ref %s requires a variable of type %s (a variable of type %s was supplied)";
char qyno_message_0063[46] = "%s: Value of %s cannot be converted to %s";
char qyno_message_0064[122] = "%s: double cannot be implicitly converted to float (literal-only expressions can be converted)";
char qyno_message_0065[112] = "%s: Please enter a location (x.addr or none) in the ref variable (%s is specified)";
char qyno_message_0066[108] = "%s: Variable %.*s of the inner block cannot be pointed to (because it disappears when the block is exited)";
char qyno_message_0067[75] = "&& is obsolete. Use and for conjunction";
char qyno_message_0068[21] = "'%s' required";
char qyno_message_0069[66] = "Please specify windows or linux for --target: %s";
char qyno_message_0070[43] = "-I requires an import search root";
char qyno_message_0071[52] = "Specify .addr immediately after the variable";
char qyno_message_0072[78] = ".value and .addr can only be written once immediately after the variable name";
char qyno_message_0073[53] = "Specify .value immediately after the variable";
char qyno_message_0074[51] = "Cannot compute a remainder with a zero divisor";
char qyno_message_0075[56] = "Cannot divide by zero or raise zero to a negative power";
char qyno_message_0076[30] = "Cannot divide by zero";
char qyno_message_0077[36] = "Cannot divide by complex zero";
char qyno_message_0078[37] = "Cannot divide by complex zero\n";
char qyno_message_0079[48] = "Negative powers of 0 are not defined";
char qyno_message_0080[47] = "Cannot compute a remainder with a zero divisor";
char qyno_message_0081[35] = "Cannot divide by zero";
char qyno_message_0082[53] = "=, +=, -=, ++, -- is required";
char qyno_message_0083[34] = "Unable to get CPU time";
char qyno_message_0084[39] = "Could not allocate DateTime constant";
char qyno_message_0085[48] = "Unable to get random number seed from OS";
char qyno_message_0086[36] = "QYNO runtime error: %s:%d:%d: ";
char qyno_message_0087[39] = "QYNO runtime error: %s:%d:%d: %s\n";
char qyno_message_0088[43] = "QYNO runtime error: %s:%d:%d: %s.%s: ";
char qyno_message_0089[87] = "QYNO runtime error: %s:%d:%d: Could not read input\n\t%d | %.*s\n";
char qyno_message_0090[35] = "Could not allocate SeekOrigin";
char qyno_message_0091[101] = "The base of ^ must be a number (int, float, double) (%s is specified)";
char qyno_message_0092[77] = "Please set the exponent of ^ to int (%s exponent is not supported)";
char qyno_message_0093[102] = "and / or / xor require two bool operands or two integer operands; the types cannot be mixed";
char qyno_message_0094[61] = "Please use append, remove_at, and clear for arrays.";
char qyno_message_0095[62] = "The as alias can only be specified for modules: %s";
char qyno_message_0096[34] = "An alias is required after as";
char qyno_message_0097[62] = "auto requires an initial value (e.g. auto value = 3)";
char qyno_message_0098[92] = "The initial value of auto requires a concrete value type (addr cannot be used)";
char qyno_message_0099[95] = "auto arrays infer their dimensions from the initializer (for example, auto values = [1, 2, 3])";
char qyno_message_0100[117] = "Please use auto to declare value variables and const (refs, function arguments, and return values are not supported)";
char qyno_message_0101[49] = "Base should be %s2 / 8 / 10 / 16";
char qyno_message_0102[35] = "base should be int";
char qyno_message_0103[51] = "Make offset from begin non-negative";
char qyno_message_0104[41] = "bnot can only be used with int";
char qyno_message_0105[33] = "case %s is duplicated";
char qyno_message_0106[46] = "case is not a valid value for enum";
char qyno_message_0107[51] = "Duplicate error code for case";
char qyno_message_0108[55] = "Please indent the body of the case one step.";
char qyno_message_0109[53] = "case should be a constant of io.ErrorCode";
char qyno_message_0110[52] = "case should be a constant of the same enum type";
char qyno_message_0111[47] = "case() should be the last case";
char qyno_message_0112[83] = "char requires a string containing exactly one grapheme cluster";
char qyno_message_0113[28] = "close failed";
char qyno_message_0114[73] = "Parsing close is too complicated. Please split the function";
char qyno_message_0115[76] = "Too many parsing paths for close. Please split the function";
char qyno_message_0116[46] = "Could not allocate analysis area for close";
char qyno_message_0117[55] = "real / imag of complex should be a real number";
char qyno_message_0118[52] = "The named argument of complex is real / imag";
char qyno_message_0119[42] = "Duplicate arguments to complex";
char qyno_message_0120[69] = "complex can have up to one value or two real / imag arguments";
char qyno_message_0121[45] = "The definition of const %.*s is circular";
char qyno_message_0122[101] = "Cannot be called from const/readonly %s. This method is not readonly";
char qyno_message_0123[60] = "const/readonly random number generator cannot be modified";
char qyno_message_0124[48] = "const/readonly values cannot be changed";
char qyno_message_0125[74] = "const requires an initial value (e.g. const float PI = 3.141592)";
char qyno_message_0126[53] = "Keep const dependencies below %d levels";
char qyno_message_0127[131] = "For the initial value of const, write an expression using only literals and other const (variables cannot be used)";
char qyno_message_0128[82] = "Methods and regular function calls cannot be used for const initial values.";
char qyno_message_0129[62] = "This expression cannot be used as the initial value of const.";
char qyno_message_0130[94] = "const constants, readonly values, arrays, strings and their elements cannot be changed";
char qyno_message_0131[48] = "const calculation exceeds int range";
char qyno_message_0132[63] = "Finding the remainder after dividing by 0 in const calculation";
char qyno_message_0133[43] = "Divide by 0 in const calculation";
char qyno_message_0134[76] = "Expansion of const model/array exceeds 1 million slots";
char qyno_message_0135[58] = "Expansion of const array exceeds 1 million elements";
char qyno_message_0136[43] = "There is no if corresponding to else";
char qyno_message_0137[55] = "Please indent the body of else one step";
char qyno_message_0138[64] = "Please indent the body of else one line with a tab.";
char qyno_message_0139[42] = "The definition of enum %s is circular";
char qyno_message_0140[66] = "Cannot specify named arguments for conversion from enum";
char qyno_message_0141[64] = "enum must have a member with value 0 to be the default value";
char qyno_message_0142[45] = "Numeric value not declared in enum";
char qyno_message_0143[52] = "Duplicate enum member name %s";
char qyno_message_0144[72] = "Enum member names must have non-reserved identifiers";
char qyno_message_0145[70] = "Please indent members one level in enums.";
char qyno_message_0146[42] = "Duplicate enum value %lld";
char qyno_message_0147[41] = "enum value is out of int_32 range";
char qyno_message_0148[30] = "enum value is undefined";
char qyno_message_0149[84] = "The value of enum must be an integer constant expression within the range of int_32";
char qyno_message_0150[36] = "Could not allocate enum value";
char qyno_message_0151[51] = "Each member of enum requires a newline";
char qyno_message_0152[48] = "enum name or number is undefined";
char qyno_message_0153[54] = "Unable to read enum name or number";
char qyno_message_0154[30] = "enum type name required";
char qyno_message_0155[41] = "Don't add ref to enum conversion";
char qyno_message_0156[61] = "Please specify only one value for enum conversion";
char qyno_message_0157[58] = "Please specify one argument for enum conversion";
char qyno_message_0158[80] = "Please use the same enum, integer, or string as the conversion source for enum.";
char qyno_message_0159[39] = "Could not allocate enum constant";
char qyno_message_0160[78] = "Forward or circular references to subsequent members of enum are not allowed";
char qyno_message_0161[16] = "Comparing enums";
char qyno_message_0162[61] = "The sequential number of enum is outside the range of int_32";
char qyno_message_0163[66] = "Compare enums of the same type using == / !=";
char qyno_message_0164[30] = "enum cannot be empty";
char qyno_message_0165[30] = "Unable to allocate enum";
char qyno_message_0166[55] = "Definition of enum member %s is circular";
char qyno_message_0167[42] = "Could not allocate enum member";
char qyno_message_0168[45] = "Could not allocate enum member name";
char qyno_message_0169[33] = "Could not allocate enum name";
char qyno_message_0170[48] = "enum type name %s conflicts with const";
char qyno_message_0171[52] = "enum type name %s conflicts with interface";
char qyno_message_0172[48] = "enum type name %s conflicts with model";
char qyno_message_0173[40] = "Duplicate enum type name %s";
char qyno_message_0174[49] = "enum type name %s conflicts with function";
char qyno_message_0175[39] = "enum definition requires newline";
char qyno_message_0176[39] = "End of enum definition required";
char qyno_message_0177[29] = "exists failed";
char qyno_message_0178[53] = "Unable to get file information for exists";
char qyno_message_0179[30] = "file is not open";
char qyno_message_0180[57] = "file cannot be used for output/string embedding";
char qyno_message_0181[33] = "file is closed";
char qyno_message_0182[52] = "Models containing file cannot be const";
char qyno_message_0183[76] = "The array that generates file is too large to prove close";
char qyno_message_0184[20] = "file is required";
char qyno_message_0185[60] = "You cannot return, break, or continue outside of finally.";
char qyno_message_0186[58] = "Please indent the body of finally by one level.";
char qyno_message_0187[49] = "%s of for should be a finite number";
char qyno_message_0188[96] = "Please set the for loop variable to int, float, or double (%s cannot be used)";
char qyno_message_0189[62] = "Make the for loop variable a finite number";
char qyno_message_0190[59] = "The first update result of for is not finite";
char qyno_message_0191[26] = "for increment is 0";
char qyno_message_0192[62] = "for increment is 0 (loop never ends)";
char qyno_message_0193[44] = "incrementing for does not advance the value";
char qyno_message_0194[92] = "increments in for do not advance the value (check the value after rounding)";
char qyno_message_0195[137] = "The increment amount of for should be constant (literal or const) (to check direction and 0 at compile time)";
char qyno_message_0196[107] = "Write the increment of for using either ++, --, += expression, or -= expression of loop variable %.*s";
char qyno_message_0197[53] = "Make the increment of for a finite number";
char qyno_message_0198[62] = "The value before updating for should be a finite number";
char qyno_message_0199[50] = "update result of for is not finite";
char qyno_message_0200[63] = "Indent the for body by one tab";
char qyno_message_0201[84] = "Please write the range of for in the form \"start value <= variable < end value\"";
char qyno_message_0202[79] = "Please write either <, <=, >, or >= at the end of for.";
char qyno_message_0203[59] = "Make the ending bound of for a finite number";
char qyno_message_0204[56] = "The starting value of for should be a finite number";
char qyno_message_0205[62] = "Please indent the body of if by one tab.";
char qyno_message_0206[13] = "if condition";
char qyno_message_0207[53] = "implements requires type name of interface";
char qyno_message_0208[50] = "Duplicate interface in implements";
char qyno_message_0209[56] = "The target of implements should be interface.";
char qyno_message_0210[36] = "Could not allocate implements";
char qyno_message_0211[58] = "Public specification cannot be attached to import / using";
char qyno_message_0212[35] = "import dependency is too deep";
char qyno_message_0213[41] = "Unable to allocate space for import";
char qyno_message_0214[16] = "import chain: ";
char qyno_message_0215[70] = "Cycle init and field default generation: %s";
char qyno_message_0216[144] = "Unable to trace file ownership responsibility of array elements by init. Please specify an initial value that indicates the number of elements.";
char qyno_message_0217[39] = "Duplicate init definition";
char qyno_message_0218[15] = "init arguments";
char qyno_message_0219[63] = "Types containing file cannot be specified as arguments to init";
char qyno_message_0220[48] = "Make init arguments value arguments";
char qyno_message_0221[87] = "Missing array cannot be generated by default due to required argument to init";
char qyno_message_0222[73] = "init returns only void/bool. cannot return array";
char qyno_message_0223[27] = "default arguments for init";
char qyno_message_0224[84] = "The default argument of init should be a generation expression with constant inputs";
char qyno_message_0225[45] = "default generation of init is circular";
char qyno_message_0226[57] = "The body of init should be in block format";
char qyno_message_0227[77] = "Please define init in a public void / bool block format.";
char qyno_message_0228[44] = "Please define init as public";
char qyno_message_0229[46] = "init is a name used only for model generation";
char qyno_message_0230[83] = "init is for generation only. Write {name=value} in a type context";
char qyno_message_0231[52] = "Models containing init cannot be const";
char qyno_message_0232[63] = "Values containing init cannot be generated at compile time";
char qyno_message_0233[51] = "cannot be assigned to the entire this in init";
char qyno_message_0234[50] = "cannot retrieve this.addr in init";
char qyno_message_0235[40] = "Duplicate init argument %s";
char qyno_message_0236[98] = "Inside {} of input, write the variable or array element you want to read.";
char qyno_message_0237[91] = "The input prompt should be int, float, double, bool, str, char.";
char qyno_message_0238[105] = "input has one argument (multiple values are written in the form input(\"display{a}, {b}\"))";
char qyno_message_0239[43] = "input can only be used inside functions";
char qyno_message_0240[56] = "int / str requires a value to convert first";
char qyno_message_0241[56] = "The named argument of int / str is base / prefix";
char qyno_message_0242[59] = "int / str has too many or duplicate arguments";
char qyno_message_0243[76] = "int() is being passed a value outside the range of int (or NaN)";
char qyno_message_0244[38] = "Duplicate interface %s";
char qyno_message_0245[59] = "interface %s has no contract method %.*s";
char qyno_message_0246[44] = "init cannot be declared on interface";
char qyno_message_0247[63] = "interface requires one or more method declarations";
char qyno_message_0248[50] = "Duplicate methods in interface";
char qyno_message_0249[58] = "Make interface methods public";
char qyno_message_0250[71] = "A newline is required at the end of a method declaration in interface";
char qyno_message_0251[74] = "Please specify types in interface contracts and init arguments";
char qyno_message_0252[68] = "Please specify the return type in the interface contract.";
char qyno_message_0253[50] = "No implementation method for interface";
char qyno_message_0254[65] = "interface specialization requires a concrete model type";
char qyno_message_0255[96] = "There are up to 4096 interface specializations in the entire program (Development source: %.*s)";
char qyno_message_0256[48] = "Could not allocate specialization for interface";
char qyno_message_0257[67] = "interface can only be used with ref / ref readonly arguments";
char qyno_message_0258[35] = "interface cannot be generated";
char qyno_message_0259[85] = "Please specify the type of the argument of the method that implements the interface.";
char qyno_message_0260[35] = "Unable to allocate interface";
char qyno_message_0261[53] = "No concrete type for interface method";
char qyno_message_0262[48] = "interface name duplicates const";
char qyno_message_0263[50] = "interface name duplicates model";
char qyno_message_0264[29] = "interface name required";
char qyno_message_0265[47] = "interface name duplicates function";
char qyno_message_0266[38] = "Could not allocate interface name";
char qyno_message_0267[44] = "Interface definition requires newline";
char qyno_message_0268[41] = "Close interface definition";
char qyno_message_0269[112] = "The interface argument can only be used for contract method calls and ref forwarding to the same contract";
char qyno_message_0270[52] = "cannot be assigned to the entire interface argument";
char qyno_message_0271[39] = "io.file.%s requires file";
char qyno_message_0272[54] = "Invalid number of arguments for io.file.%s (%d)";
char qyno_message_0273[35] = "Invalid argument for io.file.%s";
char qyno_message_0274[43] = "The path in io.path is invalid UTF-8";
char qyno_message_0275[44] = "The path in io.path is invalid UTF-8\n";
char qyno_message_0276[96] = "The io.path argument is invalid (please check NUL, extension, and path format)";
char qyno_message_0277[97] = "The io.path argument is invalid (please check NUL, extension, and path format) \n";
char qyno_message_0278[39] = "io.path results too large";
char qyno_message_0279[42] = "Unable to allocate space for io.path";
char qyno_message_0280[57] = "The argument of io.path.%s should be str/char";
char qyno_message_0281[35] = "length is read-only";
char qyno_message_0282[86] = "length is read-only. Please specify a subscript that can be substituted.";
char qyno_message_0283[56] = "Do not append a ref to calls to lib / io.path";
char qyno_message_0284[55] = "NaN or infinity cannot be specified for lib.length";
char qyno_message_0285[55] = "lib.length does not support length %s";
char qyno_message_0286[46] = "Unable to allocate code for lib.size";
char qyno_message_0287[60] = "lib.size result exceeds int_64 range";
char qyno_message_0288[61] = "lib.size result exceeds int_64 range\n";
char qyno_message_0289[62] = "lib.size does not support the size of the value %s";
char qyno_message_0290[30] = "main cannot be called";
char qyno_message_0291[63] = "Missing main function (requires int main();)";
char qyno_message_0292[154] = "Define the main function in the form int main();, int main(void);, int main(str args[]); (the argument is a variable-length one-dimensional array of str)";
char qyno_message_0293[42] = "main function cannot be private";
char qyno_message_0294[101] = "There is no case for %s in match. Please write all members or else";
char qyno_message_0295[34] = "match requires line break";
char qyno_message_0296[56] = "Please indent the case of match by one line.";
char qyno_message_0297[62] = "Please write else of match only once at the end.";
char qyno_message_0298[67] = "Please write case or else in match";
char qyno_message_0299[46] = "The target of match should be enum";
char qyno_message_0300[46] = "Could not allocate inspection area for match";
char qyno_message_0301[34] = "The end of match is required";
char qyno_message_0302[31] = "match cannot be empty";
char qyno_message_0303[31] = "Unable to expand match";
char qyno_message_0304[34] = "no_error cannot be caught";
char qyno_message_0305[14] = "target of not";
char qyno_message_0306[38] = "prefix should be bool";
char qyno_message_0307[84] = "Please write end=value in the second line of print (e.g. print(x, end=\"\"))";
char qyno_message_0308[76] = "Use print as a statement (no return value)";
char qyno_message_0309[95] = "The private definition %s cannot be referenced from outside the module in which it is declared";
char qyno_message_0310[97] = "If you want to mix quadruple and complex numbers, please specify the component type and convert.";
char qyno_message_0311[17] = "qyc: error:";
char qyno_message_0312[27] = "read failed";
char qyno_message_0313[21] = "count of read_bytes";
char qyno_message_0314[52] = "count of read_bytes should be non-negative";
char qyno_message_0315[63] = "count of read_bytes should be a non-negative int_64";
char qyno_message_0316[62] = "Please specify ref str as the output destination of read_line";
char qyno_message_0317[80] = "%s cannot be used for readonly file (the shared state will change)";
char qyno_message_0318[100] = "Cannot copy readonly file or value containing file as a modifiable value";
char qyno_message_0319[55] = "Cannot retrieve addr from readonly reference";
char qyno_message_0320[66] = "Specify readonly for methods and ref arguments";
char qyno_message_0321[38] = "Cannot seek on readonly file";
char qyno_message_0322[50] = "ref variable %.*s points to nothing";
char qyno_message_0323[107] = "The destination of the ref argument %.*s cannot be changed (it is fixed to the caller's variable)";
char qyno_message_0324[54] = "Unable to check close due to ref cycle";
char qyno_message_0325[73] = "ref can only be written as an argument of a function defined by yourself";
char qyno_message_0326[70] = "The upper bound of the ref array does not meet the declared condition";
char qyno_message_0327[66] = "Upper bound of ref array does not meet declared condition\n";
char qyno_message_0328[71] = "return requires a value (return type of function %.*s is %s)";
char qyno_message_0329[26] = "seek failed";
char qyno_message_0330[44] = "Set seek offset to int_64";
char qyno_message_0331[62] = "seek position is outside the range of start ~ EOF or int_64";
char qyno_message_0332[41] = "seek is only for binary mode";
char qyno_message_0333[113] = "The number of seconds for sleep should be greater than or equal to 0 and less than or equal to the int_64 limit.";
char qyno_message_0334[30] = "Could not allocate this";
char qyno_message_0335[81] = "this, addr, value, and length cannot be used as model field names";
char qyno_message_0336[41] = "Please enter a new line after throw;";
char qyno_message_0337[47] = "throw; can only be used in the body of a case";
char qyno_message_0338[51] = "internal functions of time cannot be used directly";
char qyno_message_0339[15] = "time argument";
char qyno_message_0340[72] = "The time argument must be a finite number or necessary integer.";
char qyno_message_0341[48] = "The time argument must be a finite value";
char qyno_message_0342[60] = "The string argument of time should be str/char";
char qyno_message_0343[45] = "Could not allocate default argument for time";
char qyno_message_0344[60] = "The date and time argument of time should be time.DateTime";
char qyno_message_0345[39] = "time.%s has %d~%d arguments";
char qyno_message_0346[68] = "The argument, date and time, format, and range of time are invalid.";
char qyno_message_0347[38] = "time string is too large";
char qyno_message_0348[41] = "Could not allocate time string";
char qyno_message_0349[50] = "try requires case or finally";
char qyno_message_0350[54] = "Please indent the body of try one level.";
char qyno_message_0351[58] = "Try_open error should be ref io.Error";
char qyno_message_0352[58] = "Please set the output destination of try_open to ref file";
char qyno_message_0353[70] = "try_open output destination must be unopened";
char qyno_message_0354[79] = "The arguments of try_open are optional: path, ref destination, ref error, mode";
char qyno_message_0355[62] = "Output destination of try_parse should be ref quadruple";
char qyno_message_0356[49] = "Make it an integer type after unsigned";
char qyno_message_0357[47] = "Please specify unsigned only once";
char qyno_message_0358[74] = "using %s introduces %s which conflicts with an existing name";
char qyno_message_0359[68] = "using %s with name %s collides with variable in %s:%d";
char qyno_message_0360[70] = "using %s name %s conflicts with using %s (%s:%d)";
char qyno_message_0361[37] = "using without parentheses";
char qyno_message_0362[86] = "Conflict using name %s. Please use qualified name";
char qyno_message_0363[44] = "Circular using reference: %s";
char qyno_message_0364[94] = "There is no target of using %s. Check the required imports and names";
char qyno_message_0365[40] = "Could not allocate space for using";
char qyno_message_0366[46] = "Return value of void should be ()";
char qyno_message_0367[65] = "Please indent the body of while one line with a tab.";
char qyno_message_0368[16] = "while condition";
char qyno_message_0369[25] = "write does not proceed";
char qyno_message_0370[70] = "Can be written to int, float, double, bool, str, char";
char qyno_message_0371[28] = "write failed";
char qyno_message_0372[88] = "Please use int, float, double, bool, str, char for write embedding.";
char qyno_message_0373[78] = "data of write_bytes should be a one-dimensional array of unsigned int_8";
char qyno_message_0374[81] = "There is no } corresponding to { ({ itself is written as \\{)";
char qyno_message_0375[79] = "A delimiter is required between {} and {} (e.g. {a}, {b})";
char qyno_message_0376[82] = "Please write the formula inside {} ({ itself is written as \\{)";
char qyno_message_0377[65] = "{field=value} requires model type context";
char qyno_message_0378[65] = "Embedding {expression} is currently only allowed in print";
char qyno_message_0379[74] = "|| has been deprecated. Please use or for logical sum";
char qyno_message_0380[61] = "The value entered in \"%s\" exceeds the range of %s";
char qyno_message_0381[42] = "There is no value to put in \"%s\"";
char qyno_message_0382[55] = "The value put in \"%s\" cannot be read as %s";
char qyno_message_0383[33] = "\"%s\" is required here";
char qyno_message_0384[121] = "Unable to ensure that this file is explicitly closed on all paths to normal termination (function %.*s)";
char qyno_message_0385[70] = "This file is not for writing in the specified mode";
char qyno_message_0386[70] = "This file is not for reading in specified mode";
char qyno_message_0387[93] = "There is no opening | corresponding to this | (absolute value is written as |x|)";
char qyno_message_0388[68] = "Please use this expression in assignments, prints, conditions, etc.";
char qyno_message_0389[44] = "This array operation has %d arguments";
char qyno_message_0390[43] = "Incorrect indentation";
char qyno_message_0391[71] = "Indentation is too deep. Please deepen one step at a time.";
char qyno_message_0392[82] = "Spaces are not allowed for indentation. please use tabs";
char qyno_message_0393[55] = "Could not allocate space for inline expansion";
char qyno_message_0394[10] = "error";
char qyno_message_0395[34] = "Error name required";
char qyno_message_0396[49] = "Reserved words cannot be used in error names";
char qyno_message_0397[51] = "Shift value and amount should be int";
char qyno_message_0398[52] = "The shift amount should be between 0 and %d.";
char qyno_message_0399[52] = "The shift amount should be between 0 and 63.";
char qyno_message_0400[67] = "Source file extension must be \".qy\": %s";
char qyno_message_0401[100] = "The directory path is invalid (empty/NUL/please check the path format)";
char qyno_message_0402[54] = "Do not append ref to directory path";
char qyno_message_0403[64] = "Please use str/char for the directory path.";
char qyno_message_0404[57] = "Do not attach ref to binary I/O calls";
char qyno_message_0405[25] = "path too long";
char qyno_message_0406[39] = "Path cannot be NUL";
char qyno_message_0407[46] = "Could not allocate path inspection area";
char qyno_message_0408[42] = "Unable to convert path to UTF-16";
char qyno_message_0409[16] = "Bit operations";
char qyno_message_0410[58] = "Please set the file path to str/char";
char qyno_message_0411[75] = "The file mode is r, w, a, rb, wb, or ab.";
char qyno_message_0412[61] = "Please set the file mode to str/char";
char qyno_message_0413[160] = "Only const constants and function definitions can be written at the beginning of the file (functions are written in the form return value type name(argument);)";
char qyno_message_0414[46] = "File string too large";
char qyno_message_0415[46] = "Unable to create diagnostic for file";
char qyno_message_0416[31] = "can't open file";
char qyno_message_0417[61] = "File size exceeds int_64 range";
char qyno_message_0418[43] = "Unable to get file information";
char qyno_message_0419[59] = "File operations cannot be used for const initial values";
char qyno_message_0420[44] = "Duplicate field %s";
char qyno_message_0421[71] = "Field %s requires default value or parent init";
char qyno_message_0422[56] = "Duplicate initialization of field %s";
char qyno_message_0423[56] = "Default value of field %s is circular";
char qyno_message_0424[68] = "Duplicate field and method name %s";
char qyno_message_0425[28] = "Initial value of field";
char qyno_message_0426[52] = "Could not allocate initial value for field";
char qyno_message_0427[28] = "Default value of field";
char qyno_message_0428[89] = "The default value of a field should be an expression that is determined at compile time.";
char qyno_message_0429[40] = "Could not allocate field";
char qyno_message_0430[34] = "field name required";
char qyno_message_0431[43] = "Could not allocate field name";
char qyno_message_0432[61] = "A newline is required at the end of a field declaration";
char qyno_message_0433[43] = "cannot hold field expression";
char qyno_message_0434[46] = "Could not allocate field number";
char qyno_message_0435[69] = "Please use a new line immediately after the ';' that starts a block.";
char qyno_message_0436[46] = "Could not allocate method target";
char qyno_message_0437[34] = "Could not allocate memory";
char qyno_message_0438[31] = "Member name required";
char qyno_message_0439[93] = "Module %s was not found (add an import search root with -I)";
char qyno_message_0440[87] = "Modules %s and %s refer to the same file with different names";
char qyno_message_0441[53] = "Unable to explore module %s: %s (%s)";
char qyno_message_0442[49] = "Could not allocate space for module";
char qyno_message_0443[52] = "Module name or identifier required";
char qyno_message_0444[38] = "Model %s is duplicated";
char qyno_message_0445[54] = "Model %s does not have field %s";
char qyno_message_0446[53] = "Model %s does not have method %.*s";
char qyno_message_0447[62] = "Field definition for model %s is circular";
char qyno_message_0448[79] = "Please define fields or methods in your model";
char qyno_message_0449[124] = "Use model auto for method return values. Please specify the field type";
char qyno_message_0450[46] = "Could not allocate copy of model";
char qyno_message_0451[52] = "Model field number is out of range";
char qyno_message_0452[65] = "Please indent one line in the model using tabs.";
char qyno_message_0453[40] = "Models are nested too deep";
char qyno_message_0454[46] = "Could not allocate initial values for model";
char qyno_message_0455[85] = "To initialize the model, write type variable = {field=value}";
char qyno_message_0456[58] = "Please initialize the model to the same type.";
char qyno_message_0457[78] = "Compare models of the same type using == or !=";
char qyno_message_0458[34] = "Could not allocate model";
char qyno_message_0459[47] = "Model name duplicates const";
char qyno_message_0460[28] = "model name required";
char qyno_message_0461[46] = "Model name duplicates function";
char qyno_message_0462[37] = "Could not allocate model name";
char qyno_message_0463[94] = "Model type %s is not defined. Check import and using";
char qyno_message_0464[58] = "Could not allocate field for model constant";
char qyno_message_0465[40] = "Could not allocate model constants";
char qyno_message_0466[43] = "Model definition requires line breaks";
char qyno_message_0467[40] = "Close the model definition";
char qyno_message_0468[60] = "The mode is r, w, a, rb, wb, or ab.";
char qyno_message_0469[34] = "Could not allocate mode";
char qyno_message_0470[43] = "Unable to keep label plan";
char qyno_message_0471[49] = "Unable to maintain runtime dependencies";
char qyno_message_0472[49] = "Unable to preserve runtime culling";
char qyno_message_0473[77] = "Cannot prove close because file's alias changes in the loop";
char qyno_message_0474[58] = "Loop nesting is too deep (up to %d levels)";
char qyno_message_0475[40] = "cannot hold expression in loop";
char qyno_message_0476[74] = "Cannot confirm close of file until next iteration of loop";
char qyno_message_0477[43] = "cannot hold loop length";
char qyno_message_0478[40] = "Requires name of loop variable";
char qyno_message_0479[70] = "Call temporary model methods inside functions.";
char qyno_message_0480[40] = "Could not allocate temporary model";
char qyno_message_0481[35] = "Could not allocate temporary value";
char qyno_message_0482[44] = "Cannot add to array with upper limit 0";
char qyno_message_0483[46] = "cannot hold immutable fields";
char qyno_message_0484[35] = "Unknown option: %s";
char qyno_message_0485[23] = "Invalid UTF-8";
char qyno_message_0486[32] = "Invalid UTF-8 string";
char qyno_message_0487[33] = "Bad UTF-8 string\n";
char qyno_message_0488[49] = "Invalid argument for random number lib.random.%s";
char qyno_message_0489[72] = "Please set the random number seed and number of characters to integers.";
char qyno_message_0490[41] = "Random number candidate is invalid UTF-8";
char qyno_message_0491[52] = "Please use str/char as the random number candidate.";
char qyno_message_0492[44] = "Could not allocate random number candidates";
char qyno_message_0493[45] = "Do not append ref to random value argument";
char qyno_message_0494[50] = "Invalid internal initialization of random numbers";
char qyno_message_0495[57] = "Random number internal functions cannot be used directly";
char qyno_message_0496[99] = "Please use module alias for random number type name API (using lib.random as random)";
char qyno_message_0497[23] = "random number argument";
char qyno_message_0498[87] = "Please use two inequality signs in the same direction for the range of random numbers.";
char qyno_message_0499[68] = "Please write the range of random numbers as min < i <= max";
char qyno_message_0500[74] = "Specify the range of random numbers using the inequality min < i <= max";
char qyno_message_0501[64] = "Random number range/candidates/number of characters are invalid";
char qyno_message_0502[130] = "The random number range/candidate/number of characters is invalid (including cases where there is no value that can be expressed)";
char qyno_message_0503[57] = "Random numbers cannot be used as initial values of const";
char qyno_message_0504[63] = "Could not allocate temporary value for random number generator";
char qyno_message_0505[53] = "Could not allocate random number generator reference";
char qyno_message_0506[39] = "Reserved words cannot be used in names";
char qyno_message_0507[11] = "Assignment";
char qyno_message_0508[31] = "Unable to get location";
char qyno_message_0509[67] = "Write positional arguments before named arguments";
char qyno_message_0510[10] = "remainder";
char qyno_message_0511[40] = "There is an extra indentation";
char qyno_message_0512[31] = "There are extra characters";
char qyno_message_0513[65] = "Working directory is not in correct Unicode";
char qyno_message_0514[73] = "Working directory is not reachable from current root";
char qyno_message_0515[52] = "Working directory path is too long";
char qyno_message_0516[52] = "Working directory operation failed";
char qyno_message_0517[71] = "Working directory operations cannot be used for const initial values";
char qyno_message_0518[119] = "Usage: qyc <input file.qy> [-o <output file>] [-S] [-I <search root>] [--target windows|linux]\n";
char qyno_message_0519[82] = "This escape cannot be used (\\n \\t \\r \\0 \\\\ \\\" \\' \\{ \\} can be used)";
char qyno_message_0520[31] = "This character cannot be used.";
char qyno_message_0521[58] = "main cannot be defined in dependent modules";
char qyno_message_0522[6] = "value";
char qyno_message_0523[34] = "No value entered";
char qyno_message_0524[72] = "Returns that return a value and returns without a value cannot be mixed";
char qyno_message_0525[81] = "Value (number, string, true, false, variable) or '(' is required";
char qyno_message_0526[61] = "The input has been completed and could not be read.";
char qyno_message_0527[43] = "Failed to read input";
char qyno_message_0528[56] = "Specify exactly one input file";
char qyno_message_0529[37] = "Could not allocate input destination";
char qyno_message_0530[134] = "Private type %s cannot be used in a public definition. Please make the definition private or convert it to a public type.";
char qyno_message_0531[65] = "Please write only one public/private public specification.";
char qyno_message_0532[66] = "Please write only one readonly after the public specification";
char qyno_message_0533[61] = "Internal error: Conversion from %s to %s not supported";
char qyno_message_0534[73] = "Internal error: 8-bit operations only support al~bl";
char qyno_message_0535[46] = "Internal error: Import ID is out of range";
char qyno_message_0536[55] = "Internal error: Import not output";
char qyno_message_0537[66] = "Internal error: import not available for Linux";
char qyno_message_0538[63] = "Internal error: movzx only supports al~bl";
char qyno_message_0539[63] = "Internal error: setcc only supports al~bl";
char qyno_message_0540[61] = "Internal error: stack depth remains %d";
char qyno_message_0541[60] = "Internal error: Data generation order does not match";
char qyno_message_0542[60] = "Internal error: Missing plan for data";
char qyno_message_0543[75] = "Internal error: attempt to write outside of buffer";
char qyno_message_0544[55] = "Internal error: Duplicate label %s";
char qyno_message_0545[67] = "Internal error: Label %s not located";
char qyno_message_0546[60] = "Internal error: Label sources do not match";
char qyno_message_0547[60] = "Internal error: Number of labels generated does not match";
char qyno_message_0548[60] = "Internal error: Label generation order does not match";
char qyno_message_0549[45] = "Internal error: invalid numeric instruction";
char qyno_message_0550[51] = "Internal error: invalid type for binary operation";
char qyno_message_0551[45] = "Internal error: type that cannot be printed";
char qyno_message_0552[48] = "Internal error: invalid type for unary -";
char qyno_message_0553[60] = "Internal error: Fixed numeric save area is inconsistent";
char qyno_message_0554[57] = "Internal error: unsupported expression for type checking";
char qyno_message_0555[55] = "Internal error: unsupported statement by type checking";
char qyno_message_0556[57] = "Internal error: invalid constant data reference";
char qyno_message_0557[66] = "Internal error: Number of constant data generated does not match";
char qyno_message_0558[69] = "Internal error: inconsistent stack of number blocks";
char qyno_message_0559[66] = "Internal error: unable to allocate numeric register";
char qyno_message_0560[52] = "Internal error: Numerical instruction not supported";
char qyno_message_0561[63] = "Internal error: Numerical instruction stack is inconsistent";
char qyno_message_0562[57] = "Internal error: invalid numeric save event";
char qyno_message_0563[68] = "Internal error: Numeric value save destination is outside the frame";
char qyno_message_0564[54] = "Internal error: Inconsistent numeric save plan";
char qyno_message_0565[60] = "Internal error: Insufficient numeric save area";
char qyno_message_0566[53] = "Internal error: unsupported xmm register";
char qyno_message_0567[39] = "Internal error: unsupported expression";
char qyno_message_0568[39] = "Internal error: unsupported statement";
char qyno_message_0569[55] = "Internal error: inconsistent machine instruction value";
char qyno_message_0570[60] = "Internal error: Invalid subscript address specification";
char qyno_message_0571[54] = "Internal error: Unsaved number";
char qyno_message_0572[51] = "Internal error: invalid type for array operation";
char qyno_message_0573[57] = "Internal error: inconsistent plan for array transfer";
char qyno_message_0574[122] = "Cannot prove close of recursive file function. Separate file operations outside of recursion";
char qyno_message_0575[40] = "Could not allocate split string";
char qyno_message_0576[14] = "Initial value";
char qyno_message_0577[49] = "Requires field name to initialize";
char qyno_message_0578[50] = "Unable to allocate syntax tree for initialization";
char qyno_message_0579[40] = "Could not allocate initialization state";
char qyno_message_0580[51] = "The result of division exceeds the range of int";
char qyno_message_0581[37] = "Unable to get monotonic clock";
char qyno_message_0582[99] = "Unary - can only be used for numbers (int, float, double) (cannot be used for %s)";
char qyno_message_0583[46] = "Reference/update path is too deep";
char qyno_message_0584[34] = "Unable to hold recipient";
char qyno_message_0585[91] = "Name %s conflicts with the name of an import or built-in module";
char qyno_message_0586[48] = "Name %s conflicts with using %s";
char qyno_message_0587[40] = "Could not allocate space for name";
char qyno_message_0588[142] = "Close of file cannot be confirmed in recovery error path (function %.*s). Please close with finally or case";
char qyno_message_0589[37] = "Unable to allocate space for type";
char qyno_message_0590[82] = "Variable %.*s declared without type name is unused (assignment only)";
char qyno_message_0591[144] = "The variable %.*s declared without the type name is unused (assignment only). Similar name to existing variable %.*s (%s:%d)";
char qyno_message_0592[31] = "Could not allocate type name";
char qyno_message_0593[65] = "Type (int, float, double, bool, str, char) is required";
char qyno_message_0594[46] = "Embedded expression is invalid";
char qyno_message_0595[55] = "This character cannot be used in embedded expressions.";
char qyno_message_0596[55] = "Embedded string not closed";
char qyno_message_0597[69] = "Please set the input destination for the radix specification to int.";
char qyno_message_0598[56] = "Radix specification can only be used in int expressions";
char qyno_message_0599[50] = "Radix specification can only be used for integers";
char qyno_message_0600[7] = "place";
char qyno_message_0601[16] = "Assign location";
char qyno_message_0602[78] = "Locations can only be compared using == and != (%s is not allowed)";
char qyno_message_0603[9] = "increase";
char qyno_message_0604[19] = "increase";
char qyno_message_0605[73] = "The direction of the increment is opposite to the direction of the range";
char qyno_message_0606[110] = "The direction of the increment is opposite to the range (%s range, so set the loop variable to %s)";
char qyno_message_0607[66] = "Please specify the value to be converted as a positional argument";
char qyno_message_0608[43] = "Variable %.*s is not declared";
char qyno_message_0609[103] = "Variable or constant %s is not declared. Check import and using";
char qyno_message_0610[14] = "variable name";
char qyno_message_0611[100] = "When declaring a loop variable with the same name as an outer variable, please specify the type.";
char qyno_message_0612[40] = "Cannot be assigned to constant %.*s";
char qyno_message_0613[50] = "Constant %.*s has no location (addr)";
char qyno_message_0614[49] = "Could not allocate plan for constant data";
char qyno_message_0615[14] = "constant name";
char qyno_message_0616[55] = "Unable to get runtime command line";
char qyno_message_0617[56] = "Runtime argument is not valid Unicode";
char qyno_message_0618[34] = "Runtime argument too long";
char qyno_message_0619[58] = "Runtime argument size out of range";
char qyno_message_0620[55] = "Number of runtime arguments exceeds range";
char qyno_message_0621[36] = "No matching try";
char qyno_message_0622[49] = "target is not a directory";
char qyno_message_0623[95] = "The decimal number is outside the range of binary128 or exceeds the upper limit of 4096 digits";
char qyno_message_0624[81] = "Alphabetic characters, _,. cannot be written immediately after a decimal number.";
char qyno_message_0625[44] = "Decimal literal is out of range %s";
char qyno_message_0626[33] = "Decimal random number boundaries";
char qyno_message_0627[37] = "Duplicate argument %.*s";
char qyno_message_0628[57] = "Argument %.*s has the same name as const on line %d";
char qyno_message_0629[31] = "requires name of argument";
char qyno_message_0630[74] = "Argument type (int, float, double, bool, str, char) is required";
char qyno_message_0631[28] = "Wait failed";
char qyno_message_0632[40] = "Wait time is out of range for int_64";
char qyno_message_0633[71] = "Circular import: %s is the same file as %s being read";
char qyno_message_0634[43] = "Circular import: Loading %s";
char qyno_message_0635[97] = "Unable to allocate required memory (size is out of range or out of memory)";
char qyno_message_0636[13] = "return value";
char qyno_message_0637[108] = "Return value types %s and %s cannot be unified. Please specify return type or conversion";
char qyno_message_0638[110] = "Return value inference also infers array dimensions. Omit the [] after the function name";
char qyno_message_0639[77] = "Dependency on return value type inference should be less than or equal to %d";
char qyno_message_0640[31] = "exponent number required";
char qyno_message_0641[21] = "Search root: %s\n";
char qyno_message_0642[62] = "Search root is not a directory: %s";
char qyno_message_0643[49] = "Could not allocate area for search root";
char qyno_message_0644[55] = "Unable to ensure dimensions of inferred return value";
char qyno_message_0645[93] = "Arguments to infer must have concrete value types (addr is not allowed)";
char qyno_message_0646[104] = "The argument to infer also infers the dimensions of the array. Please omit the [] after the name";
char qyno_message_0647[102] = "The inferred return value must have a concrete value type (addr cannot be returned as a value)";
char qyno_message_0648[78] = "Numerical conversion/operation result is outside the range of the target type";
char qyno_message_0649[55] = "Numerical conversion/arithmetic result is out of range";
char qyno_message_0650[113] = "Numerical calculation/conversion is invalid (please check division by 0, range, exponent, and shift amount)";
char qyno_message_0651[43] = "Could not allocate number block";
char qyno_message_0652[39] = "Could not allocate numeric instruction";
char qyno_message_0653[40] = "Could not allocate numeric string";
char qyno_message_0654[28] = "Invalid number format";
char qyno_message_0655[43] = "Could not allocate numerical save plan";
char qyno_message_0656[75] = "Please write the function definition in the form of a formula on one line.";
char qyno_message_0657[76] = "The integer is outside the 128 bit range or does not match the base number.";
char qyno_message_0658[54] = "Rounding result of integer exceeds the range of int";
char qyno_message_0659[96] = "Integer types %s and %s cannot be safely unified. Please convert explicitly";
char qyno_message_0660[42] = "ref cannot be used to convert integers";
char qyno_message_0661[49] = "Requires number after integer prefix";
char qyno_message_0662[43] = "Could not allocate integer string";
char qyno_message_0663[52] = "Integer random numbers must have integer boundaries";
char qyno_message_0664[37] = "Invalid base for integer conversion";
char qyno_message_0665[61] = "Result of integer conversion is out of range for target type";
char qyno_message_0666[142] = "A statement is required (please write the expression that uses the result of input in the assignment to a variable or in the condition of if)";
char qyno_message_0667[165] = "A statement is required (corresponds to return, print, if, while, for, break, continue, const, using, variable declaration and update, and function call)";
char qyno_message_0668[49] = "A newline is required at the end of the sentence";
char qyno_message_0669[73] = "Input destination grapheme boundaries change after character replacement";
char qyno_message_0670[10] = "string";
char qyno_message_0671[86] = "String is not closed (write the string on one line)";
char qyno_message_0672[87] = "Use string {expression} for print or io.file.write output";
char qyno_message_0673[46] = "Unable to allocate memory for string";
char qyno_message_0674[49] = "Unable to allocate work area for string";
char qyno_message_0675[37] = "String subscript is out of range";
char qyno_message_0676[38] = "String subscript is out of range\n";
char qyno_message_0677[83] = "The string subscript can be written only once at the end of the update destination";
char qyno_message_0678[43] = "Could not allocate string result";
char qyno_message_0679[87] = "Strings can only be concatenated with str with + (%s and %s)";
char qyno_message_0680[72] = "Cannot read string as int in specified radix";
char qyno_message_0681[64] = "Unable to read string as specified decimal type";
char qyno_message_0682[124] = "The string cannot be read as the specified numeric type (please check the range, radix, and string)";
char qyno_message_0683[54] = "ref cannot be specified for string method";
char qyno_message_0684[47] = "Invalid UTF-8 for string method";
char qyno_message_0685[48] = "Invalid UTF-8 for string method\n";
char qyno_message_0686[58] = "String method work area is too large";
char qyno_message_0687[100] = "String method search string cannot be empty or has invalid size";
char qyno_message_0688[68] = "The search string/size of the string method is invalid \n";
char qyno_message_0689[52] = "String method result too large";
char qyno_message_0690[55] = "String method cannot produce result";
char qyno_message_0691[98] = "Characters within a string cannot be passed by ref. Please specify the elements of the char array";
char qyno_message_0692[40] = "Could not allocate default mode";
char qyno_message_0693[49] = "Default arguments can only be specified for init";
char qyno_message_0694[43] = "Unable to get clock frequency";
char qyno_message_0695[31] = "can't get watch";
char qyno_message_0696[65] = "Clock acquisition, wait, and try_parse cannot be used with const";
char qyno_message_0697[37] = "Too many dimensions to update";
char qyno_message_0698[87] = "The update destination should be a variable or a subscript starting from the variable.";
char qyno_message_0699[56] = "Please specify the format as 0b / 0o / 0d / 0x";
char qyno_message_0700[40] = "Could not allocate optimization value";
char qyno_message_0701[52] = "Unable to ensure optimization control flow";
char qyno_message_0702[42] = "Could not allocate optimization statement";
char qyno_message_0703[50] = "Unfinished models cannot be copied\n";
char qyno_message_0704[86] = "Incomplete field %s can only be used for initial assignment to the whole";
char qyno_message_0705[128] = "Initialize incomplete field %s and then use the entire this and return successfully (Declaration: %s:%d)";
char qyno_message_0706[51] = "Standard module %s does not have %s";
char qyno_message_0707[35] = "Standard type %s.%s is missing";
char qyno_message_0708[49] = "Could not allocate area for machine instructions";
char qyno_message_0709[40] = "Could not allocate machine instructions";
char qyno_message_0710[84] = "Subscript and length can be used for str/char (%s is specified)";
char qyno_message_0711[47] = "Unable to create composite update of subscript";
char qyno_message_0712[40] = "Subscripts should be integer type";
char qyno_message_0713[10] = "decreased";
char qyno_message_0714[19] = "reduce";
char qyno_message_0715[42] = "Could not allocate generation destination";
char qyno_message_0716[45] = "Invalid close parsing of generation argument";
char qyno_message_0717[40] = "Could not allocate generation arguments";
char qyno_message_0718[39] = "Unable to allocate generation function";
char qyno_message_0719[43] = "Destination path cannot be empty";
char qyno_message_0720[42] = "Empty interface cannot be defined";
char qyno_message_0721[91] = "Empty array literal requires element type context, such as declaration or argument";
char qyno_message_0722[7] = "blank";
char qyno_message_0723[69] = "A range's tentative name cannot be referenced in a bounds expression";
char qyno_message_0724[118] = "Range directions do not match (use < or <= on both sides, or > or >= on both sides)";
char qyno_message_0725[74] = "Close file before entering a never-ending loop";
char qyno_message_0726[16] = "ending boundary";
char qyno_message_0727[113] = "The absolute value |x| must be a numeric value (int, float, double) (%s is specified)";
char qyno_message_0728[40] = "Close absolute value | is required";
char qyno_message_0729[44] = "The string after replacement is too large\n";
char qyno_message_0730[46] = "Imaginary number is too large or too small";
char qyno_message_0731[31] = "Too many imaginary digits";
char qyno_message_0732[70] = "Numbers, letters, and _ cannot be written after an imaginary literal.";
char qyno_message_0733[53] = "complex real/imag is read-only";
char qyno_message_0734[51] = "The components of a complex number are real / imag";
char qyno_message_0735[49] = "Compare complex numbers with == / !=";
char qyno_message_0736[27] = "Complex update of elements";
char qyno_message_0737[22] = "Assignment to element";
char qyno_message_0738[88] = "Arrays whose elements are referenced by ref cannot be added, deleted, or reassigned\n";
char qyno_message_0739[12] = "calculation";
char qyno_message_0740[52] = "Could not allocate read-only reference";
char qyno_message_0741[8] = "warning";
char qyno_message_0742[49] = "Negative 128bit integer literal is out of range";
char qyno_message_0743[43] = "Negative integer literal is out of range";
char qyno_message_0744[108] = "The result of a negative power cannot be expressed as an int (please use a decimal base like 2.0^(-1))";
char qyno_message_0745[65] = "The result of a negative power cannot be expressed as an integer";
char qyno_message_0746[46] = "Unable to get initial position of append";
char qyno_message_0747[94] = "Not a regular file or unable to retrieve file information";
char qyno_message_0748[7] = "array";
char qyno_message_0749[47] = "Array does not have method %s";
char qyno_message_0750[58] = "Space required for array exceeds int range\n";
char qyno_message_0751[43] = "Unable to create copy of array";
char qyno_message_0752[75] = "Array upper bound is negative and required space exceeds the range of int";
char qyno_message_0753[87] = "The upper limit of the array should be a non-negative int determined at compile time.";
char qyno_message_0754[76] = "Unable to check required init arguments required for missing array";
char qyno_message_0755[49] = "Array capacity exceeds int range\n";
char qyno_message_0756[58] = "Array integer element types cannot be safely unified";
char qyno_message_0757[69] = "Use append to append to the end of the array";
char qyno_message_0758[47] = "Could not allocate array dimension information";
char qyno_message_0759[49] = "Number of array dimensions exceeds upper limit";
char qyno_message_0760[71] = "The number of dimensions of the array must be less than or equal to %d";
char qyno_message_0761[34] = "Array subscript is out of range";
char qyno_message_0762[35] = "Array subscript is out of range\n";
char qyno_message_0763[18] = "elements of array";
char qyno_message_0764[69] = "Please match the element type and number of dimensions of the array.";
char qyno_message_0765[79] = "The number of elements in the array exceeds the upper limit of the destination";
char qyno_message_0766[91] = "The number of elements in the array exceeds the upper limit of the assignment destination\n";
char qyno_message_0767[64] = "Number of elements in array exceeds declared limit";
char qyno_message_0768[40] = "Unable to create diagnostic for array";
char qyno_message_0769[56] = "Adding array exceeds maximum number of elements\n";
char qyno_message_0770[93] = "Compare arrays with the same element type and number of dimensions using == or !=";
char qyno_message_0771[46] = "Unable to allocate memory for array type";
char qyno_message_0772[43] = "Could not allocate name for array type";
char qyno_message_0773[26] = "Array operation arguments";
char qyno_message_0774[46] = "Could not allocate plan for array operation";
char qyno_message_0775[46] = "Could not allocate plan for array transfer";
char qyno_message_0776[15] = "starting value";
char qyno_message_0777[90] = "For a for whose start side is an open int, the boundary and increment should also be int.";
char qyno_message_0778[132] = "Function %.*s has no return when it reaches the end (return a value every step of the way)";
char qyno_message_0779[68] = "Function %.*s has %d arguments (%d is passed)";
char qyno_message_0780[124] = "Return type inference for function %.*s is circular. Please specify the return value type of the recursive function";
char qyno_message_0781[62] = "Function %.*s is already defined on line %d";
char qyno_message_0782[57] = "Function %.*s has the same name as const on line %d";
char qyno_message_0783[88] = "Function %s is not defined. Please check \"import\" and \"using\"";
char qyno_message_0784[107] = "Function %s is not defined (please write %s.%s or declare using %s.%s)";
char qyno_message_0785[49] = "Function stack area is too large";
char qyno_message_0786[90] = "Up to 4096 functions can be instantiated in the entire program (Development source: %.*s)";
char qyno_message_0787[43] = "Unable to ensure reification of function";
char qyno_message_0788[94] = "Please write the body of the function on the line after \";\" or in the form of \"= expression\"";
char qyno_message_0789[65] = "Please indent the body of the function one line with a tab.";
char qyno_message_0790[25] = "function name required";
char qyno_message_0791[16] = "(No mold)";
char qyno_message_0792[40] = "File limit is out of range";
char qyno_message_0793[48] = "File exceeds max_bytes";
char qyno_message_0794[48] = "File exceeds max_lines";
char qyno_message_0795[80] = "write_lines newline must be LF, CRLF, or CR";
char qyno_message_0796[43] = "write_lines output is too large";
char qyno_message_0797[61] = "write_lines elements cannot contain CR or LF";
char qyno_message_0798[40] = "File operation failed";
char qyno_message_0799[127] = "Invalid file path: empty, root, dot, or trailing separator";
char qyno_message_0800[55] = "File links are unsupported";
char qyno_message_0801[52] = "Target is not a regular file";
char qyno_message_0802[55] = "Destination already exists";
char qyno_message_0803[58] = "Source is not a regular file";
char qyno_message_0804[40] = "Rename path is too long";
char qyno_message_0805[46] = "Directory operation failed";
char qyno_message_0806[52] = "Directory path cannot be empty";
char qyno_message_0807[65] = "Cannot remove a root, . or .. directory path";
char qyno_message_0808[70] = "Removing a directory link is unsupported";
char qyno_message_0809[49] = "Target is not a directory";
char qyno_message_0810[67] = "Directory entry is not valid Unicode";
char qyno_message_0811[46] = "Directory limit is out of range";
char qyno_message_0812[55] = "Directory listing exceeds its limit";
char qyno_message_0813[45] = "Malformed native directory record";
char qyno_message_0814[65] = "Directory listing exceeds max_name_bytes";
char qyno_message_0815[69] = "Metadata size exceeds int_64";
char qyno_message_0816[40] = "Malformed metadata timestamp";
char qyno_message_0817[55] = "Cannot allocate a directory default";
char qyno_message_0818[52] = "Cannot validate a directory path";
char qyno_message_0819[109] = "Invalid directory path: empty, NUL, or unsupported path format";
char qyno_message_0820[69] = "Directory limits must be nonnegative int_64 values";
char qyno_message_0821[28] = "directory limit";
char qyno_message_0822[46] = "Cannot validate a file path";
char qyno_message_0823[130] = "Invalid file path: empty, NUL, trailing separator, or unsupported format";
char qyno_message_0824[68] = "A file path cannot name a root, . or ..";
char qyno_message_0825[65] = "File text must be valid UTF-8";
char qyno_message_0826[57] = "File paths cannot contain NUL";
char qyno_message_0827[52] = "Cannot allocate a file API default";
char qyno_message_0828[61] = "write_lines final_newline must be bool";
char qyno_message_0829[63] = "File limits must be nonnegative int_64 values";
char qyno_message_0830[22] = "file limit";
char qyno_message_0831[59] = "info path must be str or char";
char qyno_message_0832[49] = "Cannot validate a metadata path";
char qyno_message_0833[96] = "Invalid info path: empty, NUL, or unsupported format";
char qyno_message_0834[53] = "info follow_links must be bool";
char qyno_message_0835[40] = "Cannot allocate a method name";
char qyno_message_0836[11] = " [path: ";
char qyno_message_0837[15] = " (OS error ";
char qyno_message_0838[49] = "io.path.info expects one or two arguments";
char qyno_message_0839[50] = "info paths do not accept ref";
char qyno_message_0840[58] = "info follow_links does not accept ref";
char qyno_message_0841[46] = "io.file.%s expects %d to %d arguments";
char qyno_message_0842[78] = "Path-based file APIs do not accept ref arguments";
char qyno_message_0843[74] = "io.file.%s requires str or char text arguments";
char qyno_message_0844[67] = "write_lines requires a one-dimensional str array";
char qyno_message_0845[44] = "io.dir.%s expects 1 to %d arguments";
char qyno_message_0846[66] = "Directory APIs do not accept ref arguments";
char qyno_message_0847[72] = "Directory paths require str or char";
char qyno_message_0848[35] = "io.file.%s expects %d arguments";
char qyno_message_0849[72] = "File path operations do not accept ref arguments";
char qyno_message_0850[66] = "File paths require str or char";
char qyno_message_0851[76] = "Moving across filesystems is unsupported";
char qyno_message_0852[16] = "seek offset";
char qyno_message_0853[58] = "A common error code cannot be converted to an integer";
char qyno_message_0854[97] = "Create a common error code from a namespaced error code";
char qyno_message_0855[78] = "case requires an io.ErrorCode or task.ErrorCode constant";
char qyno_message_0856[52] = "Common error metadata is readonly";
char qyno_message_0857[55] = "A common error code cannot be an input destination";
char qyno_message_0858[80] = "Use async with a concrete value type for a task declaration";
char qyno_message_0859[73] = "Async model and interface methods are not supported yet";
char qyno_message_0860[86] = "A task parameter requires a concrete value type and cannot use ref";
char qyno_message_0861[56] = "Tasks can only be created inside an async function";
char qyno_message_0862[58] = "Invalid number of arguments for the async operation";
char qyno_message_0863[53] = "await requires an async function";
char qyno_message_0864[49] = "await requires a task";
char qyno_message_0865[89] = "Tasks only expose the readonly state and done properties";
char qyno_message_0866[43] = "A task has no address property";
char qyno_message_0867[46] = "A task variable must be initialized";
char qyno_message_0868[94] = "Task handles must be non-ref local variables in an async function";
char qyno_message_0869[65] = "Keep the task in a variable or await it instead of discarding it";
char qyno_message_0870[75] = "A task has no value property; use await";
char qyno_message_0871[67] = "A function cannot return task handles";
char qyno_message_0872[103] = "An async result cannot contain task, ref, addr, file, or interface values";
char qyno_message_0873[92] = "An async parameter cannot contain ref, addr, file, or interface values";
char qyno_message_0874[88] = "Persistent ref, addr, and interface locals are not supported inside async functions yet";
char qyno_message_0875[59] = "Task parameters require an async function";
char qyno_message_0876[64] = "A finally block cannot create or await tasks yet";
char qyno_message_0877[74] = "Await a task before printing its result";
char qyno_message_0878[90] = "Appending a task requires an initialized task handle";
char qyno_message_0879[49] = "The task wait dependency is cyclic";
char qyno_message_0880[58] = "Async task stack limit exceeded";
char qyno_message_0881[27] = "Awaited from:\n";
char qyno_message_0882[65] = "task.group() is only valid in an async with statement";
char qyno_message_0883[52] = "Invalid arguments for the task operation";
char qyno_message_0884[60] = "with_timeout requires a task";
char qyno_message_0885[77] = "all requires tasks with the same completed value type";
char qyno_message_0886[71] = "Use cancel() without arguments on a task or group";
char qyno_message_0887[59] = "async with requires task.group()";
char qyno_message_0888[67] = "A group cannot be copied, stored, or passed as a value";
char qyno_message_0889[85] = "A task cannot escape the group where it was created";
char qyno_message_0890[43] = "The task was cancelled";
char qyno_message_0891[55] = "One or more tasks failed";
char qyno_message_0892[37] = "The task timed out";
char qyno_message_0893[18] = "Related error: ";
char qyno_message_0894[47] = "Additional related errors omitted\n";
char qyno_message_0895[64] = "Unable to initialize asynchronous file workers";
char qyno_message_0896[77] = "io.file.%s requires a one-dimensional unsigned int_8 array";
char qyno_message_0897[64] = "Function type requires explicit argument and return types";
char qyno_message_0898[64] = "This function cannot be used as a value";
char qyno_message_0899[82] = "Specify a func type to specialize this generic function";
char qyno_message_0900[37] = "Function is not set";
char qyno_message_0901[64] = "Expression is not callable";
char qyno_message_0902[41] = "Function signature does not match %s";
char qyno_message_0903[100] = "Function types cannot contain file, addr, interface, task, or group values";
char qyno_message_0904[95] = "Function call expects %d arguments, got %d";
char qyno_message_0905[67] = "Invalid or recursive function signature";
char qyno_message_0906[49] = "Function values cannot be printed";
char qyno_message_0907[53] = "lib.%.*s requires an array";
static const Message messages[] = {
    {qyno_message_0000, "\tInput | ", "\t入力 | "},
    {qyno_message_0001, "(Default is input name.exe for Windows, no extension for Linux) \n", "                       （既定は Windows 向けなら 入力名.exe、Linux 向けなら拡張子なし）\n"},
    {qyno_message_0002, "--target <OS> The OS (windows or linux) to output to. Default is running OS\n", "  --target <OS>        出力先の OS（windows または linux）。既定は実行中の OS\n"},
    {qyno_message_0003, "--version Show version \n", "  --version            バージョンを表示する\n"},
    {qyno_message_0004, "  -I <directory> Add an import search root (repeatable)\n", "  -I <ディレクトリ>    import の探索ルートを追加する（複数指定可）\n"},
    {qyno_message_0005, "-S Outputs only the assembly (.s) without creating an executable file \n", "  -S                   実行ファイルを作らず、アセンブリ（.s）だけを出力する\n"},
    {qyno_message_0006, "-o <file> Specify output file name \n", "  -o <ファイル>        出力ファイル名を指定する\n"},
    {qyno_message_0007, "interface argument %.*s: %s\n", "  interface 引数 %.*s: %s\n"},
    {qyno_message_0008, "%% can only be used between ints (%s and %s)", "%% は int 同士でのみ使えます（%s と %s）"},
    {qyno_message_0009, "%.*s is the same name as const on line %d", "%.*s は %d 行目の const と同じ名前です"},
    {qyno_message_0010, "%.*s is not a ref variable, so the location (addr) cannot be changed", "%.*s は ref の変数ではないので、場所（addr）は変えられません"},
    {qyno_message_0011, "%.*s is already declared on line %d", "%.*s はすでに %d 行目で宣言されています"},
    {qyno_message_0012, "Argument %d", "%d 番目の引数"},
    {qyno_message_0013, "Argument %d is a ref parameter. Use ref at the call site (for example, f(ref x))", "%d 番目の引数は ref の引数です。呼び出し側にも ref を書いてください（例：f(ref x)）"},
    {qyno_message_0014, "Argument %d is passed by value; remove ref", "%d 番目の引数は値渡しの引数なので、ref は付けられません"},
    {qyno_message_0015, "Argument %d: A for-loop variable cannot be passed by ref", "%d 番目の引数：for 文の繰り返しの変数は ref で渡せません"},
    {qyno_message_0016, "Argument %d: ref requires %s (%s cannot be passed; ref does not convert types)", "%d 番目の引数：ref の引数の型は %s です（%s は渡せません。ref では型を変換しません）"},
    {qyno_message_0017, "Argument %d was inferred as %s. Check ref at the call site", "%d 番目の推論する引数は %s渡しです。呼び出し側の ref を確認してください"},
    {qyno_message_0018, "%s and %s cannot be compared", "%s と %s は比べられません"},
    {qyno_message_0019, "%s and %s cannot be calculated", "%s と %s は計算できません"},
    {qyno_message_0020, "Cannot read as %s", "%s として読めません"},
    {qyno_message_0021, "%s does not have this method", "%s にはこのメソッドはありません"},
    {qyno_message_0022, "%s has no method %s", "%s にはメソッド %s はありません"},
    {qyno_message_0023, "%s does not take parentheses (since 0.0.21, replace %s() with %s)", "%s には括弧を付けません（00.21 から %s() ではなく %s と書きます）"},
    {qyno_message_0024, "%s cannot be entered directly. Please specify a variable or terminal array element", "%s には直接入力できません。変数または末端の配列要素を指定してください"},
    {qyno_message_0025, "%s does not have field %s", "%s にフィールド %s はありません"},
    {qyno_message_0026, "Unable to give execute permission to %s", "%s に実行権限を付けられません"},
    {qyno_message_0027, "Unable to write to %s", "%s に書き込めません"},
    {qyno_message_0028, "Cannot write to %s (please exit if it is running)", "%s に書き込めません（実行中の場合は終了してください）"},
    {qyno_message_0029, "%s accepts numeric values (int, float, double); received %s", "%s に渡せるのは数値（int・float・double）です（%s が渡されています）"},
    {qyno_message_0030, "%s.init has required arguments. Initialize the entire element or field", "%s の init には必須引数が必要です。要素またはフィールド全体の初期値を指定してください"},
    {qyno_message_0031, "Unable to get size of %s", "%s のサイズを取得できません"},
    {qyno_message_0032, "Initialization of %s refused\n", "%s の初期化が拒否されました\n"},
    {qyno_message_0033, "Variable locations in %s and %s cannot be compared", "%s の変数の場所と %s の変数の場所は比べられません"},
    {qyno_message_0034, "%s has %d arguments", "%s の引数は %d 個です"},
    {qyno_message_0035, "%s has 1 or 2 arguments (value, number of digits) (%d is passed)", "%s の引数は 1 個か 2 個（値, 桁数）です（%d 個渡しています）"},
    {qyno_message_0036, "%s has one argument (%d is passed)", "%s の引数は 1 個です（%d 個渡しています）"},
    {qyno_message_0037, "The argument of %s should be str / char", "%s の引数は str / char にしてください"},
    {qyno_message_0038, "Failed to write %s", "%s の書き込みに失敗しました"},
    {qyno_message_0039, "The number of digits for %s should be int (%s is passed)", "%s の桁数は int にしてください（%s が渡されています）"},
    {qyno_message_0040, "Search string for %s cannot be empty", "%s の検索文字列は空にできません"},
    {qyno_message_0041, "Out of range of %s", "%s の範囲を超えています"},
    {qyno_message_0042, "%s can only be compared with == and != (%s cannot be used)", "%s は == と != でしか比べられません（%s は使えません）"},
    {qyno_message_0043, "%s must implement interface %s public method %.*s (contract: %s:%d)", "%s は interface %s の public メソッド %.*s を実装してください（契約: %s:%d）"},
    {qyno_message_0044, "%s does not implement interface %s. Please pass the ref of the same contract", "%s は interface %s を implements していません。同じ契約の ref を渡してください"},
    {qyno_message_0045, "%s does not refer to io.%s. Check using io, selective using, and the namespace", "%s は io.%s を参照していません。using io または選択 using を宣言し、名前空間を確認してください"},
    {qyno_message_0046, "%s can only be used in a loop (for/while)", "%s はループ（for・while）の中でのみ使えます"},
    {qyno_message_0047, "%s is obsolete. Please use %s", "%s は廃止されました。%s を使用してください"},
    {qyno_message_0048, "%s is obsolete. Please use %s.%s", "%s は廃止されました。%s.%s を使用してください"},
    {qyno_message_0049, "%s is an existing constant/function/type/namespace. Cannot replace type name with omitted declaration", "%s は既存の定数・関数・型・名前空間です。型名を省略した宣言で置き換えることはできません"},
    {qyno_message_0050, "Unable to load %s", "%s を読み込めません"},
    {qyno_message_0051, "Unable to open %s: %s", "%s を開けません: %s"},
    {qyno_message_0052, "%s() accepts str or char", "%s() に渡せるのは str・char です"},
    {qyno_message_0053, "The type, argument, array constraint, and readonly of %s.%.*s do not match interface %s (contract: %s:%d, implementation: %s:%d)", "%s.%.*s の型・引数・配列制約・readonly が interface %s と一致しません（契約: %s:%d、実装: %s:%d）"},
    {qyno_message_0054, "%s.%.*s is private. Not accessible from this location (Declaration: %s:%d:%d)", "%s.%.*s は private です。この位置からはアクセスできません（宣言: %s:%d:%d）"},
    {qyno_message_0055, "%s.%.*s cannot implement interface because it is private or init (contract: %s:%d, candidate: %s:%d)", "%s.%.*s は private または init のため interface を実装できません（契約: %s:%d、候補: %s:%d）"},
    {qyno_message_0056, "%s.%s has %d arguments", "%s.%s の引数は %d 個です"},
    {qyno_message_0057, "%s.init has no argument %s", "%s.init に引数 %s はありません"},
    {qyno_message_0058, "%s.init is missing required argument %.*s", "%s.init の必須引数 %.*s がありません"},
    {qyno_message_0059, "Requires %s", "%sが必要です"},
    {qyno_message_0060, "%s subscript is out of range", "%sの添字が範囲外です"},
    {qyno_message_0061, "%s should be bool (%s is specified)", "%sは bool にしてください（%s が指定されています）"},
    {qyno_message_0062, "%s: ref %s requires a variable of type %s (a variable of type %s was supplied)", "%s：%s の ref の変数には、%s の変数の場所を入れてください（%s の変数の場所が指定されています）"},
    {qyno_message_0063, "%s: Value of %s cannot be converted to %s", "%s：%s の値は %s に変換できません"},
    {qyno_message_0064, "%s: double cannot be implicitly converted to float (literal-only expressions can be converted)", "%s：double の値は float に自動変換できません（リテラルだけでできた式なら変換できます）"},
    {qyno_message_0065, "%s: Please enter a location (x.addr or none) in the ref variable (%s is specified)", "%s：ref の変数には場所（x.addr か none）を入れてください（%s が指定されています）"},
    {qyno_message_0066, "%s: Variable %.*s of the inner block cannot be pointed to (because it disappears when the block is exited)", "%s：内側のブロックの変数 %.*s は指せません（ブロックを抜けると消えるため）"},
    {qyno_message_0067, "&& is obsolete. Use and for conjunction", "&& は廃止されました。論理積には and を使ってください"},
    {qyno_message_0068, "'%s' required", "'%s' が必要です"},
    {qyno_message_0069, "Please specify windows or linux for --target: %s", "--target には windows か linux を指定してください: %s"},
    {qyno_message_0070, "-I requires an import search root", "-I の後に探索ルートが必要です"},
    {qyno_message_0071, "Specify .addr immediately after the variable", ".addr は変数の直後に指定してください"},
    {qyno_message_0072, ".value and .addr can only be written once immediately after the variable name", ".value と .addr は変数名の直後に1回だけ書けます"},
    {qyno_message_0073, "Specify .value immediately after the variable", ".value は変数の直後に指定してください"},
    {qyno_message_0074, "Cannot compute a remainder with a zero divisor", "0 で割った余りを求めようとしました"},
    {qyno_message_0075, "Cannot divide by zero or raise zero to a negative power", "0 で割ることや0の負の累乗はできません"},
    {qyno_message_0076, "Cannot divide by zero", "0 で割ろうとしました"},
    {qyno_message_0077, "Cannot divide by complex zero", "0 の複素数では割れません"},
    {qyno_message_0078, "Cannot divide by complex zero\n", "0 の複素数では割れません\n"},
    {qyno_message_0079, "Negative powers of 0 are not defined", "0 の負の累乗は定義されていません"},
    {qyno_message_0080, "Cannot compute a remainder with a zero divisor", "0で割った余りは求められません"},
    {qyno_message_0081, "Cannot divide by zero", "0で割ることはできません"},
    {qyno_message_0082, "=, +=, -=, ++, -- is required", "=、+=、-=、++、-- のいずれかが必要です"},
    {qyno_message_0083, "Unable to get CPU time", "CPU時間を取得できません"},
    {qyno_message_0084, "Could not allocate DateTime constant", "DateTime定数を確保できません"},
    {qyno_message_0085, "Unable to get random number seed from OS", "OSから乱数シードを取得できません"},
    {qyno_message_0086, "QYNO runtime error: %s:%d:%d: ", "QYNO 実行時エラー: %s:%d:%d: "},
    {qyno_message_0087, "QYNO runtime error: %s:%d:%d: %s\n", "QYNO 実行時エラー: %s:%d:%d: %s\n"},
    {qyno_message_0088, "QYNO runtime error: %s:%d:%d: %s.%s: ", "QYNO 実行時エラー: %s:%d:%d: %s.%s: "},
    {qyno_message_0089, "QYNO runtime error: %s:%d:%d: Could not read input\n\t%d | %.*s\n", "QYNO 実行時エラー: %s:%d:%d: 入力を読み取れませんでした\n\t%d | %.*s\n"},
    {qyno_message_0090, "Could not allocate SeekOrigin", "SeekOriginを確保できません"},
    {qyno_message_0091, "The base of ^ must be a number (int, float, double) (%s is specified)", "^ の底は数値（int・float・double）にしてください（%s が指定されています）"},
    {qyno_message_0092, "Please set the exponent of ^ to int (%s exponent is not supported)", "^ の指数は int にしてください（%s の指数は未対応です）"},
    {qyno_message_0093, "and / or / xor require two bool operands or two integer operands; the types cannot be mixed", "and / or / xor は bool 同士または int 同士にしてください。型を混在できません"},
    {qyno_message_0094, "Please use append, remove_at, and clear for arrays.", "append・remove_at・clear は配列に使ってください"},
    {qyno_message_0095, "The as alias can only be specified for modules: %s", "as の別名はモジュールにだけ指定できます: %s"},
    {qyno_message_0096, "An alias is required after as", "as の後に別名が必要です"},
    {qyno_message_0097, "auto requires an initial value (e.g. auto value = 3)", "auto には初期値が必要です（例：auto value = 3）"},
    {qyno_message_0098, "The initial value of auto requires a concrete value type (addr cannot be used)", "auto の初期値には具体的な値型が必要です（addr は使用できません）"},
    {qyno_message_0099, "auto arrays infer their dimensions from the initializer (for example, auto values = [1, 2, 3])", "auto の配列は初期値から次元も推論します（例：auto values = [1, 2, 3]）"},
    {qyno_message_0100, "Please use auto to declare value variables and const (refs, function arguments, and return values are not supported)", "auto は値変数・const の宣言に使用してください（ref・関数引数・戻り値は未対応です）"},
    {qyno_message_0101, "Base should be %s2 / 8 / 10 / 16", "base は %s2 / 8 / 10 / 16 にしてください"},
    {qyno_message_0102, "base should be int", "base は int にしてください"},
    {qyno_message_0103, "Make offset from begin non-negative", "beginからのoffsetは非負にしてください"},
    {qyno_message_0104, "bnot can only be used with int", "bnot は int にだけ使用できます"},
    {qyno_message_0105, "case %s is duplicated", "case %s が重複しています"},
    {qyno_message_0106, "case is not a valid value for enum", "case がenumの有効値ではありません"},
    {qyno_message_0107, "Duplicate error code for case", "case のエラーコードが重複しています"},
    {qyno_message_0108, "Please indent the body of the case one step.", "case の本体は1段インデントしてください"},
    {qyno_message_0109, "case should be a constant of io.ErrorCode", "case は io.ErrorCode の定数にしてください"},
    {qyno_message_0110, "case should be a constant of the same enum type", "case は同じenum型の定数にしてください"},
    {qyno_message_0111, "case() should be the last case", "case() は最後の case にしてください"},
    {qyno_message_0112, "char requires a string containing exactly one grapheme cluster", "char には 書記素クラスタをちょうど1つ含む文字列が必要です"},
    {qyno_message_0113, "close failed", "close に失敗しました"},
    {qyno_message_0114, "Parsing close is too complicated. Please split the function", "close の解析が複雑すぎます。関数を分割してください"},
    {qyno_message_0115, "Too many parsing paths for close. Please split the function", "close の解析経路が多すぎます。関数を分割してください"},
    {qyno_message_0116, "Could not allocate analysis area for close", "close の解析領域を確保できません"},
    {qyno_message_0117, "real / imag of complex should be a real number", "complex の real / imag は実数にしてください"},
    {qyno_message_0118, "The named argument of complex is real / imag", "complex の名前付き引数は real / imag です"},
    {qyno_message_0119, "Duplicate arguments to complex", "complex の引数が重複しています"},
    {qyno_message_0120, "complex can have up to one value or two real / imag arguments", "complex の引数は値1個または real / imag の2個までです"},
    {qyno_message_0121, "The definition of const %.*s is circular", "const %.*s の定義が循環しています"},
    {qyno_message_0122, "Cannot be called from const/readonly %s. This method is not readonly", "const / readonly の %s から呼べません。このメソッドは readonly ではありません"},
    {qyno_message_0123, "const/readonly random number generator cannot be modified", "const / readonly の乱数生成器は変更できません"},
    {qyno_message_0124, "const/readonly values cannot be changed", "const / readonly の値は変更できません"},
    {qyno_message_0125, "const requires an initial value (e.g. const float PI = 3.141592)", "const には初期値が必要です（例：const float PI = 3.141592）"},
    {qyno_message_0126, "Keep const dependencies below %d levels", "const の依存は %d 段以下にしてください"},
    {qyno_message_0127, "For the initial value of const, write an expression using only literals and other const (variables cannot be used)", "const の初期値には、リテラルと他の const だけを使った式を書いてください（変数は使えません）"},
    {qyno_message_0128, "Methods and regular function calls cannot be used for const initial values.", "const の初期値にはメソッド・通常関数呼び出しを使えません"},
    {qyno_message_0129, "This expression cannot be used as the initial value of const.", "const の初期値に使えない式です"},
    {qyno_message_0130, "const constants, readonly values, arrays, strings and their elements cannot be changed", "const の定数・readonly の値・配列・文字列とその要素は変更できません"},
    {qyno_message_0131, "const calculation exceeds int range", "const の計算が int の範囲を超えます"},
    {qyno_message_0132, "Finding the remainder after dividing by 0 in const calculation", "const の計算で 0 で割った余りを求めています"},
    {qyno_message_0133, "Divide by 0 in const calculation", "const の計算で 0 で割っています"},
    {qyno_message_0134, "Expansion of const model/array exceeds 1 million slots", "const モデル・配列の展開が100万スロットを超えています"},
    {qyno_message_0135, "Expansion of const array exceeds 1 million elements", "const 配列の展開が100万要素を超えています"},
    {qyno_message_0136, "There is no if corresponding to else", "else に対応する if がありません"},
    {qyno_message_0137, "Please indent the body of else one step", "else の本体は1段インデントしてください"},
    {qyno_message_0138, "Please indent the body of else one line with a tab.", "else の本体はタブで1段インデントしてください"},
    {qyno_message_0139, "The definition of enum %s is circular", "enum %s の定義が循環しています"},
    {qyno_message_0140, "Cannot specify named arguments for conversion from enum", "enum からの変換に名前付き引数は指定できません"},
    {qyno_message_0141, "enum must have a member with value 0 to be the default value", "enum には既定値となる値0のメンバーが必要です"},
    {qyno_message_0142, "Numeric value not declared in enum", "enum に宣言されていない数値です"},
    {qyno_message_0143, "Duplicate enum member name %s", "enum のメンバー名 %s が重複しています"},
    {qyno_message_0144, "Enum member names must have non-reserved identifiers", "enum のメンバー名には予約語以外の識別子が必要です"},
    {qyno_message_0145, "Please indent members one level in enums.", "enum の中にはメンバーを1段インデントしてください"},
    {qyno_message_0146, "Duplicate enum value %lld", "enum の値 %lld が重複しています"},
    {qyno_message_0147, "enum value is out of int_32 range", "enum の値が int_32 の範囲外です"},
    {qyno_message_0148, "enum value is undefined", "enum の値が未定義です"},
    {qyno_message_0149, "The value of enum must be an integer constant expression within the range of int_32", "enum の値は int_32 の範囲内の整数定数式にしてください"},
    {qyno_message_0150, "Could not allocate enum value", "enum の値を確保できません"},
    {qyno_message_0151, "Each member of enum requires a newline", "enum の各メンバーには改行が必要です"},
    {qyno_message_0152, "enum name or number is undefined", "enum の名前または数値が未定義です"},
    {qyno_message_0153, "Unable to read enum name or number", "enum の名前または数値を読み取れません"},
    {qyno_message_0154, "enum type name required", "enum の型名が必要です"},
    {qyno_message_0155, "Don't add ref to enum conversion", "enum の変換に ref は付けません"},
    {qyno_message_0156, "Please specify only one value for enum conversion", "enum の変換には値を1個だけ指定してください"},
    {qyno_message_0157, "Please specify one argument for enum conversion", "enum の変換には引数を1個指定してください"},
    {qyno_message_0158, "Please use the same enum, integer, or string as the conversion source for enum.", "enum の変換元は同じenum・整数・文字列にしてください"},
    {qyno_message_0159, "Could not allocate enum constant", "enum の定数を確保できません"},
    {qyno_message_0160, "Forward or circular references to subsequent members of enum are not allowed", "enum の後続メンバーへの前方参照・循環参照はできません"},
    {qyno_message_0161, "Comparing enums", "enum の比較"},
    {qyno_message_0162, "The sequential number of enum is outside the range of int_32", "enum の連番が int_32 の範囲外です"},
    {qyno_message_0163, "Compare enums of the same type using == / !=", "enum は同じ型で == / != を使って比較してください"},
    {qyno_message_0164, "enum cannot be empty", "enum は空にできません"},
    {qyno_message_0165, "Unable to allocate enum", "enum を確保できません"},
    {qyno_message_0166, "Definition of enum member %s is circular", "enum メンバー %s の定義が循環しています"},
    {qyno_message_0167, "Could not allocate enum member", "enum メンバーを確保できません"},
    {qyno_message_0168, "Could not allocate enum member name", "enum メンバー名を確保できません"},
    {qyno_message_0169, "Could not allocate enum name", "enum 名を確保できません"},
    {qyno_message_0170, "enum type name %s conflicts with const", "enum 型名 %s がconstと衝突しています"},
    {qyno_message_0171, "enum type name %s conflicts with interface", "enum 型名 %s がinterfaceと衝突しています"},
    {qyno_message_0172, "enum type name %s conflicts with model", "enum 型名 %s がmodelと衝突しています"},
    {qyno_message_0173, "Duplicate enum type name %s", "enum 型名 %s が重複しています"},
    {qyno_message_0174, "enum type name %s conflicts with function", "enum 型名 %s が関数と衝突しています"},
    {qyno_message_0175, "enum definition requires newline", "enum 定義には改行が必要です"},
    {qyno_message_0176, "End of enum definition required", "enum 定義の終わりが必要です"},
    {qyno_message_0177, "exists failed", "exists に失敗しました"},
    {qyno_message_0178, "Unable to get file information for exists", "exists のファイル情報を取得できません"},
    {qyno_message_0179, "file is not open", "file が未オープンです"},
    {qyno_message_0180, "file cannot be used for output/string embedding", "file は出力・文字列埋め込みに使えません"},
    {qyno_message_0181, "file is closed", "file は閉じられています"},
    {qyno_message_0182, "Models containing file cannot be const", "file を含むモデルは const にできません"},
    {qyno_message_0183, "The array that generates file is too large to prove close", "file を生成する配列が大きすぎて close を証明できません"},
    {qyno_message_0184, "file is required", "fileが必要です"},
    {qyno_message_0185, "You cannot return, break, or continue outside of finally.", "finally の外へ return・break・continue できません"},
    {qyno_message_0186, "Please indent the body of finally by one level.", "finally の本体は1段インデントしてください"},
    {qyno_message_0187, "%s of for should be a finite number", "for の%sは有限の数値にしてください"},
    {qyno_message_0188, "Please set the for loop variable to int, float, or double (%s cannot be used)", "for のループ変数は int・float・double にしてください（%s は使えません）"},
    {qyno_message_0189, "Make the for loop variable a finite number", "for のループ変数は有限の数値にしてください"},
    {qyno_message_0190, "The first update result of for is not finite", "for の初回の更新結果が有限ではありません"},
    {qyno_message_0191, "for increment is 0", "for の増分が 0 です"},
    {qyno_message_0192, "for increment is 0 (loop never ends)", "for の増分が 0 です（ループが終わりません）"},
    {qyno_message_0193, "incrementing for does not advance the value", "for の増分では値が前進しません"},
    {qyno_message_0194, "increments in for do not advance the value (check the value after rounding)", "for の増分では値が前進しません（丸め後の値を確認してください）"},
    {qyno_message_0195, "The increment amount of for should be constant (literal or const) (to check direction and 0 at compile time)", "for の増分の量は定数（リテラルや const）にしてください（向きと 0 をコンパイル時に確かめるため）"},
    {qyno_message_0196, "Write the increment of for using either ++, --, += expression, or -= expression of loop variable %.*s", "for の増分はループ変数 %.*s の ++、--、+= 式、-= 式 のいずれかで書いてください"},
    {qyno_message_0197, "Make the increment of for a finite number", "for の増分は有限の数値にしてください"},
    {qyno_message_0198, "The value before updating for should be a finite number", "for の更新前の値は有限の数値にしてください"},
    {qyno_message_0199, "update result of for is not finite", "for の更新結果が有限ではありません"},
    {qyno_message_0200, "Indent the for body by one tab", "for の本体はタブで1段インデントしてください"},
    {qyno_message_0201, "Please write the range of for in the form \"start value <= variable < end value\"", "for の範囲は「開始値 <= 変数 < 終了値」の形で書いてください"},
    {qyno_message_0202, "Please write either <, <=, >, or >= at the end of for.", "for の終了側には <、<=、>、>= のいずれかを書いてください"},
    {qyno_message_0203, "Make the ending bound of for a finite number", "for の終了境界は有限の数値にしてください"},
    {qyno_message_0204, "The starting value of for should be a finite number", "for の開始値は有限の数値にしてください"},
    {qyno_message_0205, "Please indent the body of if by one tab.", "if の本体はタブで1段インデントしてください"},
    {qyno_message_0206, "if condition", "if の条件"},
    {qyno_message_0207, "implements requires type name of interface", "implements には interface の型名が必要です"},
    {qyno_message_0208, "Duplicate interface in implements", "implements の interface が重複しています"},
    {qyno_message_0209, "The target of implements should be interface.", "implements の対象は interface にしてください"},
    {qyno_message_0210, "Could not allocate implements", "implements を確保できません"},
    {qyno_message_0211, "Public specification cannot be attached to import / using", "import / using に公開指定は付けられません"},
    {qyno_message_0212, "import dependency is too deep", "import の依存が深すぎます"},
    {qyno_message_0213, "Unable to allocate space for import", "import の領域を確保できません"},
    {qyno_message_0214, "import chain: ", "import 経路: "},
    {qyno_message_0215, "Cycle init and field default generation: %s", "init とフィールド既定値の生成が循環しています: %s"},
    {qyno_message_0216, "Unable to trace file ownership responsibility of array elements by init. Please specify an initial value that indicates the number of elements.", "init による配列要素の file 所有責任を追跡できません。要素数が分かる初期値を指定してください"},
    {qyno_message_0217, "Duplicate init definition", "init の定義が重複しています"},
    {qyno_message_0218, "init arguments", "init の引数"},
    {qyno_message_0219, "Types containing file cannot be specified as arguments to init", "init の引数に file を含む型は指定できません"},
    {qyno_message_0220, "Make init arguments value arguments", "init の引数は値引数にしてください"},
    {qyno_message_0221, "Missing array cannot be generated by default due to required argument to init", "init の必須引数があるため、配列の不足分を既定生成できません"},
    {qyno_message_0222, "init returns only void/bool. cannot return array", "init の戻り値は void / bool のみです。配列は返せません"},
    {qyno_message_0223, "default arguments for init", "init の既定引数"},
    {qyno_message_0224, "The default argument of init should be a generation expression with constant inputs", "init の既定引数は定数の入力を持つ生成式にしてください"},
    {qyno_message_0225, "default generation of init is circular", "init の既定生成が循環しています"},
    {qyno_message_0226, "The body of init should be in block format", "init の本体はブロック形式にしてください"},
    {qyno_message_0227, "Please define init in a public void / bool block format.", "init は public void / bool のブロック形式で定義してください"},
    {qyno_message_0228, "Please define init as public", "init は public で定義してください"},
    {qyno_message_0229, "init is a name used only for model generation", "init はモデル生成専用の名前です"},
    {qyno_message_0230, "init is for generation only. Write {name=value} in a type context", "init は生成専用です。型の文脈で {名前=値} と書いてください"},
    {qyno_message_0231, "Models containing init cannot be const", "init を含むモデルは const にできません"},
    {qyno_message_0232, "Values containing init cannot be generated at compile time", "init を含む値はコンパイル時に生成できません"},
    {qyno_message_0233, "cannot be assigned to the entire this in init", "init 内で this 全体には代入できません"},
    {qyno_message_0234, "cannot retrieve this.addr in init", "init 内では this.addr を取り出せません"},
    {qyno_message_0235, "Duplicate init argument %s", "init 引数 %s が重複しています"},
    {qyno_message_0236, "Inside {} of input, write the variable or array element you want to read.", "input の {} の中には、読み込む先の変数または配列要素を書いてください"},
    {qyno_message_0237, "The input prompt should be int, float, double, bool, str, char.", "input のプロンプトは int・float・double・bool・str・char にしてください"},
    {qyno_message_0238, "input has one argument (multiple values are written in the form input(\"display{a}, {b}\"))", "input の引数は1つです（複数の値は input(\"表示{a}, {b}\") のように形で書きます）"},
    {qyno_message_0239, "input can only be used inside functions", "input は関数の中でだけ使えます"},
    {qyno_message_0240, "int / str requires a value to convert first", "int / str には最初に変換する値が必要です"},
    {qyno_message_0241, "The named argument of int / str is base / prefix", "int / str の名前付き引数は base / prefix です"},
    {qyno_message_0242, "int / str has too many or duplicate arguments", "int / str の引数が多すぎるか重複しています"},
    {qyno_message_0243, "int() is being passed a value outside the range of int (or NaN)", "int() に int の範囲外の値（または NaN）が渡されています"},
    {qyno_message_0244, "Duplicate interface %s", "interface %s が重複しています"},
    {qyno_message_0245, "interface %s has no contract method %.*s", "interface %s に契約メソッド %.*s はありません"},
    {qyno_message_0246, "init cannot be declared on interface", "interface に init は宣言できません"},
    {qyno_message_0247, "interface requires one or more method declarations", "interface には1つ以上のメソッド宣言が必要です"},
    {qyno_message_0248, "Duplicate methods in interface", "interface のメソッドが重複しています"},
    {qyno_message_0249, "Make interface methods public", "interface のメソッドは public にしてください"},
    {qyno_message_0250, "A newline is required at the end of a method declaration in interface", "interface のメソッド宣言の終わりには改行が必要です"},
    {qyno_message_0251, "Please specify types in interface contracts and init arguments", "interface の契約と init の引数には型を明示してください"},
    {qyno_message_0252, "Please specify the return type in the interface contract.", "interface の契約には戻り値の型を明示してください"},
    {qyno_message_0253, "No implementation method for interface", "interface の実装メソッドがありません"},
    {qyno_message_0254, "interface specialization requires a concrete model type", "interface の特殊化に具体的なモデル型が必要です"},
    {qyno_message_0255, "There are up to 4096 interface specializations in the entire program (Development source: %.*s)", "interface の特殊化はプログラム全体で4096件までです（展開元: %.*s）"},
    {qyno_message_0256, "Could not allocate specialization for interface", "interface の特殊化を確保できません"},
    {qyno_message_0257, "interface can only be used with ref / ref readonly arguments", "interface は ref / ref readonly 引数にだけ使用できます"},
    {qyno_message_0258, "interface cannot be generated", "interface は生成できません"},
    {qyno_message_0259, "Please specify the type of the argument of the method that implements the interface.", "interface を実装するメソッドの引数には型を明示してください"},
    {qyno_message_0260, "Unable to allocate interface", "interface を確保できません"},
    {qyno_message_0261, "No concrete type for interface method", "interface メソッドの具体型がありません"},
    {qyno_message_0262, "interface name duplicates const", "interface 名が const と重複しています"},
    {qyno_message_0263, "interface name duplicates model", "interface 名がモデルと重複しています"},
    {qyno_message_0264, "interface name required", "interface 名が必要です"},
    {qyno_message_0265, "interface name duplicates function", "interface 名が関数と重複しています"},
    {qyno_message_0266, "Could not allocate interface name", "interface 名を確保できません"},
    {qyno_message_0267, "Interface definition requires newline", "interface 定義には改行が必要です"},
    {qyno_message_0268, "Close interface definition", "interface 定義を閉じてください"},
    {qyno_message_0269, "The interface argument can only be used for contract method calls and ref forwarding to the same contract", "interface 引数は契約メソッドの呼び出しと同じ契約への ref 転送にだけ使用できます"},
    {qyno_message_0270, "cannot be assigned to the entire interface argument", "interface 引数全体には代入できません"},
    {qyno_message_0271, "io.file.%s requires file", "io.file.%s には file が必要です"},
    {qyno_message_0272, "Invalid number of arguments for io.file.%s (%d)", "io.file.%s の引数の数が不正です（%d 個）"},
    {qyno_message_0273, "Invalid argument for io.file.%s", "io.file.%sの引数が不正です"},
    {qyno_message_0274, "The path in io.path is invalid UTF-8", "io.path のパスが不正な UTF-8 です"},
    {qyno_message_0275, "The path in io.path is invalid UTF-8\n", "io.path のパスが不正な UTF-8 です\n"},
    {qyno_message_0276, "The io.path argument is invalid (please check NUL, extension, and path format)", "io.path の引数が不正です（NUL・拡張子・パス形式を確認してください）"},
    {qyno_message_0277, "The io.path argument is invalid (please check NUL, extension, and path format) \n", "io.path の引数が不正です（NUL・拡張子・パス形式を確認してください）\n"},
    {qyno_message_0278, "io.path results too large", "io.path の結果が大きすぎます"},
    {qyno_message_0279, "Unable to allocate space for io.path", "io.path の領域を確保できません"},
    {qyno_message_0280, "The argument of io.path.%s should be str/char", "io.path.%s の引数は str・char にしてください"},
    {qyno_message_0281, "length is read-only", "length は読み取り専用です"},
    {qyno_message_0282, "length is read-only. Please specify a subscript that can be substituted.", "length は読み取り専用です。代入できる添字を指定してください"},
    {qyno_message_0283, "Do not append a ref to calls to lib / io.path", "lib / io.path の呼び出しに ref は付けません"},
    {qyno_message_0284, "NaN or infinity cannot be specified for lib.length", "lib.length に NaN・無限大は指定できません"},
    {qyno_message_0285, "lib.length does not support length %s", "lib.length は %s の長さに対応していません"},
    {qyno_message_0286, "Unable to allocate code for lib.size", "lib.size のコードを確保できません"},
    {qyno_message_0287, "lib.size result exceeds int_64 range", "lib.size の結果が int_64 の範囲を超えています"},
    {qyno_message_0288, "lib.size result exceeds int_64 range\n", "lib.size の結果が int_64 の範囲を超えています\n"},
    {qyno_message_0289, "lib.size does not support the size of the value %s", "lib.size は %s の値のサイズに対応していません"},
    {qyno_message_0290, "main cannot be called", "main は呼び出せません"},
    {qyno_message_0291, "Missing main function (requires int main();)", "main 関数がありません（int main(); が必要です）"},
    {qyno_message_0292, "Define the main function in the form int main();, int main(void);, int main(str args[]); (the argument is a variable-length one-dimensional array of str)", "main 関数は int main();、int main(void);、int main(str args[]); の形で定義してください（引数はstrの可変長1次元配列）"},
    {qyno_message_0293, "main function cannot be private", "main 関数は private にできません"},
    {qyno_message_0294, "There is no case for %s in match. Please write all members or else", "match に %s の case がありません。全メンバーまたは else を記述してください"},
    {qyno_message_0295, "match requires line break", "match には改行が必要です"},
    {qyno_message_0296, "Please indent the case of match by one line.", "match の case は1段インデントしてください"},
    {qyno_message_0297, "Please write else of match only once at the end.", "match の else は最後に1回だけ記述してください"},
    {qyno_message_0298, "Please write case or else in match", "match の中には case または else を記述してください"},
    {qyno_message_0299, "The target of match should be enum", "match の対象は enum にしてください"},
    {qyno_message_0300, "Could not allocate inspection area for match", "match の検査領域を確保できません"},
    {qyno_message_0301, "The end of match is required", "match の終わりが必要です"},
    {qyno_message_0302, "match cannot be empty", "match は空にできません"},
    {qyno_message_0303, "Unable to expand match", "match を展開できません"},
    {qyno_message_0304, "no_error cannot be caught", "no_error は捕捉できません"},
    {qyno_message_0305, "target of not", "not の対象"},
    {qyno_message_0306, "prefix should be bool", "prefix は bool にしてください"},
    {qyno_message_0307, "Please write end=value in the second line of print (e.g. print(x, end=\"\"))", "print の2つ目には end=値 を書いてください（例：print(x, end=\"\")）"},
    {qyno_message_0308, "Use print as a statement (no return value)", "print は文として使ってください（戻り値はありません）"},
    {qyno_message_0309, "The private definition %s cannot be referenced from outside the module in which it is declared", "private の定義 %s は宣言したモジュールの外から参照できません"},
    {qyno_message_0310, "If you want to mix quadruple and complex numbers, please specify the component type and convert.", "quadruple と複素数を混ぜる場合は成分の型を明示して変換してください"},
    {qyno_message_0311, "qyc: error:", "qyc: エラー: "},
    {qyno_message_0312, "read failed", "read に失敗しました"},
    {qyno_message_0313, "count of read_bytes", "read_bytes の count"},
    {qyno_message_0314, "count of read_bytes should be non-negative", "read_bytes の count は非負にしてください"},
    {qyno_message_0315, "count of read_bytes should be a non-negative int_64", "read_bytes の count は非負の int_64 にしてください"},
    {qyno_message_0316, "Please specify ref str as the output destination of read_line", "read_line の出力先は ref str を指定してください"},
    {qyno_message_0317, "%s cannot be used for readonly file (the shared state will change)", "readonly の file に %s は使えません（共有状態が変化します）"},
    {qyno_message_0318, "Cannot copy readonly file or value containing file as a modifiable value", "readonly の file または file を含む値を変更可能な値としてコピーできません"},
    {qyno_message_0319, "Cannot retrieve addr from readonly reference", "readonly の参照から addr を取り出せません"},
    {qyno_message_0320, "Specify readonly for methods and ref arguments", "readonly はメソッドと ref 引数に指定してください"},
    {qyno_message_0321, "Cannot seek on readonly file", "readonlyのfileでseekできません"},
    {qyno_message_0322, "ref variable %.*s points to nothing", "ref の変数 %.*s が何も指していません"},
    {qyno_message_0323, "The destination of the ref argument %.*s cannot be changed (it is fixed to the caller's variable)", "ref の引数 %.*s の指す先は変えられません（呼び出し側の変数に固定されます）"},
    {qyno_message_0324, "Unable to check close due to ref cycle", "ref の循環により close を確認できません"},
    {qyno_message_0325, "ref can only be written as an argument of a function defined by yourself", "ref は、自分で定義した関数の引数にだけ書けます"},
    {qyno_message_0326, "The upper bound of the ref array does not meet the declared condition", "ref 配列の上限が宣言された条件を満たしません"},
    {qyno_message_0327, "Upper bound of ref array does not meet declared condition\n", "ref 配列の上限が宣言された条件を満たしません\n"},
    {qyno_message_0328, "return requires a value (return type of function %.*s is %s)", "return に値が必要です（関数 %.*s の戻り値の型は %s）"},
    {qyno_message_0329, "seek failed", "seekに失敗しました"},
    {qyno_message_0330, "Set seek offset to int_64", "seekのoffsetはint_64にしてください"},
    {qyno_message_0331, "seek position is outside the range of start ~ EOF or int_64", "seekの位置が先頭〜EOFまたはint_64の範囲外です"},
    {qyno_message_0332, "seek is only for binary mode", "seekはバイナリモード専用です"},
    {qyno_message_0333, "The number of seconds for sleep should be greater than or equal to 0 and less than or equal to the int_64 limit.", "sleep の秒数は0以上int_64上限以下にしてください"},
    {qyno_message_0334, "Could not allocate this", "this を確保できません"},
    {qyno_message_0335, "this, addr, value, and length cannot be used as model field names", "this・addr・value・length はモデルのフィールド名に使えません"},
    {qyno_message_0336, "Please enter a new line after throw;", "throw; の後は改行してください"},
    {qyno_message_0337, "throw; can only be used in the body of a case", "throw; は case の本体でだけ使えます"},
    {qyno_message_0338, "internal functions of time cannot be used directly", "time の内部関数は直接使用できません"},
    {qyno_message_0339, "time argument", "time の引数"},
    {qyno_message_0340, "The time argument must be a finite number or necessary integer.", "time の引数は有限の数値・必要な整数にしてください"},
    {qyno_message_0341, "The time argument must be a finite value", "time の引数は有限値にしてください"},
    {qyno_message_0342, "The string argument of time should be str/char", "time の文字列引数は str・char にしてください"},
    {qyno_message_0343, "Could not allocate default argument for time", "time の既定引数を確保できません"},
    {qyno_message_0344, "The date and time argument of time should be time.DateTime", "time の日時引数は time.DateTime にしてください"},
    {qyno_message_0345, "time.%s has %d~%d arguments", "time.%s の引数は %d〜%d 個です"},
    {qyno_message_0346, "The argument, date and time, format, and range of time are invalid.", "timeの引数・日時・形式・範囲が不正です"},
    {qyno_message_0347, "time string is too large", "timeの文字列が大きすぎます"},
    {qyno_message_0348, "Could not allocate time string", "timeの文字列を確保できません"},
    {qyno_message_0349, "try requires case or finally", "try には case または finally が必要です"},
    {qyno_message_0350, "Please indent the body of try one level.", "try の本体は1段インデントしてください"},
    {qyno_message_0351, "Try_open error should be ref io.Error", "try_open の error は ref io.Error にしてください"},
    {qyno_message_0352, "Please set the output destination of try_open to ref file", "try_open の出力先は ref file にしてください"},
    {qyno_message_0353, "try_open output destination must be unopened", "try_open の出力先は未オープンである必要があります"},
    {qyno_message_0354, "The arguments of try_open are optional: path, ref destination, ref error, mode", "try_open の引数は path, ref destination, ref error, mode省略可です"},
    {qyno_message_0355, "Output destination of try_parse should be ref quadruple", "try_parse の出力先は ref quadruple にしてください"},
    {qyno_message_0356, "Make it an integer type after unsigned", "unsigned の後は整数型にしてください"},
    {qyno_message_0357, "Please specify unsigned only once", "unsigned は1回だけ指定してください"},
    {qyno_message_0358, "using %s introduces %s which conflicts with an existing name", "using %s が導入する %s は、既存の名前と衝突しています"},
    {qyno_message_0359, "using %s with name %s collides with variable in %s:%d", "using %s の名前 %s は、%s:%d の変数と衝突しています"},
    {qyno_message_0360, "using %s name %s conflicts with using %s (%s:%d)", "using %s の名前 %s は、using %s (%s:%d) と衝突しています"},
    {qyno_message_0361, "using without parentheses", "using には括弧を付けません"},
    {qyno_message_0362, "Conflict using name %s. Please use qualified name", "using による名前 %s が衝突しています。修飾名を使ってください"},
    {qyno_message_0363, "Circular using reference: %s", "using の参照が循環しています: %s"},
    {qyno_message_0364, "There is no target of using %s. Check the required imports and names", "using の対象 %s はありません。必要な import と名前を確認してください"},
    {qyno_message_0365, "Could not allocate space for using", "using の領域を確保できません"},
    {qyno_message_0366, "Return value of void should be ()", "void の戻り値は () にしてください"},
    {qyno_message_0367, "Please indent the body of while one line with a tab.", "while の本体はタブで1段インデントしてください"},
    {qyno_message_0368, "while condition", "while の条件"},
    {qyno_message_0369, "write does not proceed", "write が進みません"},
    {qyno_message_0370, "Can be written to int, float, double, bool, str, char", "write できるのは int・float・double・bool・str・char です"},
    {qyno_message_0371, "write failed", "write に失敗しました"},
    {qyno_message_0372, "Please use int, float, double, bool, str, char for write embedding.", "write の埋め込みは int・float・double・bool・str・char にしてください"},
    {qyno_message_0373, "data of write_bytes should be a one-dimensional array of unsigned int_8", "write_bytes の data は unsigned int_8 の1次元配列にしてください"},
    {qyno_message_0374, "There is no } corresponding to { ({ itself is written as \\{)", "{ に対応する } がありません（{ そのものは \\{ と書きます）"},
    {qyno_message_0375, "A delimiter is required between {} and {} (e.g. {a}, {b})", "{} と {} の間には、区切りの文字が必要です（例：{a}, {b}）"},
    {qyno_message_0376, "Please write the formula inside {} ({ itself is written as \\{)", "{} の中に式を書いてください（{ そのものは \\{ と書きます）"},
    {qyno_message_0377, "{field=value} requires model type context", "{フィールド=値} にはモデル型の文脈が必要です"},
    {qyno_message_0378, "Embedding {expression} is currently only allowed in print", "{式} の埋め込みは現在 print の中でのみ使えます"},
    {qyno_message_0379, "|| has been deprecated. Please use or for logical sum", "|| は廃止されました。論理和には or を使ってください"},
    {qyno_message_0380, "The value entered in \"%s\" exceeds the range of %s", "「%s」に入れる値が %s の範囲を超えています"},
    {qyno_message_0381, "There is no value to put in \"%s\"", "「%s」に入れる値がありません"},
    {qyno_message_0382, "The value put in \"%s\" cannot be read as %s", "「%s」に入れる値を %s として読めません"},
    {qyno_message_0383, "\"%s\" is required here", "ここに「%s」が必要です"},
    {qyno_message_0384, "Unable to ensure that this file is explicitly closed on all paths to normal termination (function %.*s)", "この file が正常終了する全経路で明示的に close されることを確認できません（関数 %.*s）"},
    {qyno_message_0385, "This file is not for writing in the specified mode", "この file は指定モードの書き込み用ではありません"},
    {qyno_message_0386, "This file is not for reading in specified mode", "この file は指定モードの読み取り用ではありません"},
    {qyno_message_0387, "There is no opening | corresponding to this | (absolute value is written as |x|)", "この | に対応する開きの | がありません（絶対値は |x| と書きます）"},
    {qyno_message_0388, "Please use this expression in assignments, prints, conditions, etc.", "この式は代入・print・条件などで使ってください"},
    {qyno_message_0389, "This array operation has %d arguments", "この配列操作の引数は %d 個です"},
    {qyno_message_0390, "Incorrect indentation", "インデントが正しくありません"},
    {qyno_message_0391, "Indentation is too deep. Please deepen one step at a time.", "インデントが深すぎます。1段ずつ深くしてください"},
    {qyno_message_0392, "Spaces are not allowed for indentation. please use tabs", "インデントにスペースは使えません。タブを使ってください"},
    {qyno_message_0393, "Could not allocate space for inline expansion", "インライン展開の領域を確保できません"},
    {qyno_message_0394, "error", "エラー"},
    {qyno_message_0395, "Error name required", "エラーの名前が必要です"},
    {qyno_message_0396, "Reserved words cannot be used in error names", "エラーの名前に予約語は使えません"},
    {qyno_message_0397, "Shift value and amount should be int", "シフトの値と量は int にしてください"},
    {qyno_message_0398, "The shift amount should be between 0 and %d.", "シフト量は0以上%d以下にしてください"},
    {qyno_message_0399, "The shift amount should be between 0 and 63.", "シフト量は0以上63以下にしてください"},
    {qyno_message_0400, "Source file extension must be \".qy\": %s", "ソースファイルの拡張子は .qy にしてください: %s"},
    {qyno_message_0401, "The directory path is invalid (empty/NUL/please check the path format)", "ディレクトリのパスが不正です（空・NUL・パス形式を確認してください）"},
    {qyno_message_0402, "Do not append ref to directory path", "ディレクトリのパスに ref は付けません"},
    {qyno_message_0403, "Please use str/char for the directory path.", "ディレクトリのパスは str・char にしてください"},
    {qyno_message_0404, "Do not attach ref to binary I/O calls", "バイナリI/Oの呼び出しに ref は付けません"},
    {qyno_message_0405, "path too long", "パスが長すぎます"},
    {qyno_message_0406, "Path cannot be NUL", "パスに NUL は使用できません"},
    {qyno_message_0407, "Could not allocate path inspection area", "パスの検査領域を確保できません"},
    {qyno_message_0408, "Unable to convert path to UTF-16", "パスを UTF-16 に変換できません"},
    {qyno_message_0409, "Bit operations", "ビット演算"},
    {qyno_message_0410, "Please set the file path to str/char", "ファイルのパスは str・char にしてください"},
    {qyno_message_0411, "The file mode is r, w, a, rb, wb, or ab.", "ファイルのモードは r・w・a・rb・wb・ab のいずれかです"},
    {qyno_message_0412, "Please set the file mode to str/char", "ファイルのモードは str・char にしてください"},
    {qyno_message_0413, "Only const constants and function definitions can be written at the beginning of the file (functions are written in the form return value type name(argument);)", "ファイルの先頭に書けるのは const 定数と関数の定義だけです（関数は 戻り値の型 名前(引数); の形で書きます）"},
    {qyno_message_0414, "File string too large", "ファイルの文字列が大きすぎます"},
    {qyno_message_0415, "Unable to create diagnostic for file", "ファイルの診断を作成できません"},
    {qyno_message_0416, "can't open file", "ファイルを開けません"},
    {qyno_message_0417, "File size exceeds int_64 range", "ファイルサイズがint_64の範囲を超えています"},
    {qyno_message_0418, "Unable to get file information", "ファイル情報を取得できません"},
    {qyno_message_0419, "File operations cannot be used for const initial values", "ファイル操作は const の初期値に使えません"},
    {qyno_message_0420, "Duplicate field %s", "フィールド %s が重複しています"},
    {qyno_message_0421, "Field %s requires default value or parent init", "フィールド %s には既定値または親の init が必要です"},
    {qyno_message_0422, "Duplicate initialization of field %s", "フィールド %s の初期化が重複しています"},
    {qyno_message_0423, "Default value of field %s is circular", "フィールド %s の既定値が循環しています"},
    {qyno_message_0424, "Duplicate field and method name %s", "フィールドとメソッドの名前 %s が重複しています"},
    {qyno_message_0425, "Initial value of field", "フィールドの初期値"},
    {qyno_message_0426, "Could not allocate initial value for field", "フィールドの初期値を確保できません"},
    {qyno_message_0427, "Default value of field", "フィールドの既定値"},
    {qyno_message_0428, "The default value of a field should be an expression that is determined at compile time.", "フィールドの既定値はコンパイル時に確定する式にしてください"},
    {qyno_message_0429, "Could not allocate field", "フィールドを確保できません"},
    {qyno_message_0430, "field name required", "フィールド名が必要です"},
    {qyno_message_0431, "Could not allocate field name", "フィールド名を確保できません"},
    {qyno_message_0432, "A newline is required at the end of a field declaration", "フィールド宣言の終わりには改行が必要です"},
    {qyno_message_0433, "cannot hold field expression", "フィールド式を保持できません"},
    {qyno_message_0434, "Could not allocate field number", "フィールド番号を確保できません"},
    {qyno_message_0435, "Please use a new line immediately after the ';' that starts a block.", "ブロックを始める ';' の直後は改行してください"},
    {qyno_message_0436, "Could not allocate method target", "メソッドの対象を確保できません"},
    {qyno_message_0437, "Could not allocate memory", "メモリを確保できません"},
    {qyno_message_0438, "Member name required", "メンバー名が必要です"},
    {qyno_message_0439, "Module %s was not found (add an import search root with -I)", "モジュール %s が見つかりません（探索ルートを -I で追加できます）"},
    {qyno_message_0440, "Modules %s and %s refer to the same file with different names", "モジュール %s と %s は同じファイルを別の名前で参照しています"},
    {qyno_message_0441, "Unable to explore module %s: %s (%s)", "モジュール %s を探索できません: %s (%s)"},
    {qyno_message_0442, "Could not allocate space for module", "モジュールの領域を確保できません"},
    {qyno_message_0443, "Module name or identifier required", "モジュール名または識別子が必要です"},
    {qyno_message_0444, "Model %s is duplicated", "モデル %s が重複しています"},
    {qyno_message_0445, "Model %s does not have field %s", "モデル %s にフィールド %s はありません"},
    {qyno_message_0446, "Model %s does not have method %.*s", "モデル %s にメソッド %.*s はありません"},
    {qyno_message_0447, "Field definition for model %s is circular", "モデル %s のフィールド定義が循環しています"},
    {qyno_message_0448, "Please define fields or methods in your model", "モデルにはフィールドまたはメソッドを定義してください"},
    {qyno_message_0449, "Use model auto for method return values. Please specify the field type", "モデルの auto はメソッドの戻り値に使用してください。フィールドの型は明示してください"},
    {qyno_message_0450, "Could not allocate copy of model", "モデルのコピーを確保できません"},
    {qyno_message_0451, "Model field number is out of range", "モデルのフィールド番号が範囲外です"},
    {qyno_message_0452, "Please indent one line in the model using tabs.", "モデルの中はタブで1段インデントしてください"},
    {qyno_message_0453, "Models are nested too deep", "モデルの入れ子が深すぎます"},
    {qyno_message_0454, "Could not allocate initial values for model", "モデルの初期値を確保できません"},
    {qyno_message_0455, "To initialize the model, write type variable = {field=value}", "モデルの初期化は 型 変数 = {フィールド=値} と書いてください"},
    {qyno_message_0456, "Please initialize the model to the same type.", "モデルの初期化先は同じ型にしてください"},
    {qyno_message_0457, "Compare models of the same type using == or !=", "モデルは同じ型で == または != を使って比較してください"},
    {qyno_message_0458, "Could not allocate model", "モデルを確保できません"},
    {qyno_message_0459, "Model name duplicates const", "モデル名が const と重複しています"},
    {qyno_message_0460, "model name required", "モデル名が必要です"},
    {qyno_message_0461, "Model name duplicates function", "モデル名が関数と重複しています"},
    {qyno_message_0462, "Could not allocate model name", "モデル名を確保できません"},
    {qyno_message_0463, "Model type %s is not defined. Check import and using", "モデル型 %s は定義されていません。import と using を確認してください"},
    {qyno_message_0464, "Could not allocate field for model constant", "モデル定数のフィールドを確保できません"},
    {qyno_message_0465, "Could not allocate model constants", "モデル定数を確保できません"},
    {qyno_message_0466, "Model definition requires line breaks", "モデル定義には改行が必要です"},
    {qyno_message_0467, "Close the model definition", "モデル定義を閉じてください"},
    {qyno_message_0468, "The mode is r, w, a, rb, wb, or ab.", "モードは r・w・a・rb・wb・ab のいずれかです"},
    {qyno_message_0469, "Could not allocate mode", "モードを確保できません"},
    {qyno_message_0470, "Unable to keep label plan", "ラベルの計画を保持できません"},
    {qyno_message_0471, "Unable to maintain runtime dependencies", "ランタイムの依存を保持できません"},
    {qyno_message_0472, "Unable to preserve runtime culling", "ランタイムの選別を保持できません"},
    {qyno_message_0473, "Cannot prove close because file's alias changes in the loop", "ループで file の別名が変わるため close を証明できません"},
    {qyno_message_0474, "Loop nesting is too deep (up to %d levels)", "ループの入れ子が深すぎます（%d 段まで）"},
    {qyno_message_0475, "cannot hold expression in loop", "ループの式を保持できません"},
    {qyno_message_0476, "Cannot confirm close of file until next iteration of loop", "ループの次の反復までに file の close を確認できません"},
    {qyno_message_0477, "cannot hold loop length", "ループの長さを保持できません"},
    {qyno_message_0478, "Requires name of loop variable", "ループ変数の名前が必要です"},
    {qyno_message_0479, "Call temporary model methods inside functions.", "一時モデルのメソッドは関数の中で呼んでください"},
    {qyno_message_0480, "Could not allocate temporary model", "一時モデルを確保できません"},
    {qyno_message_0481, "Could not allocate temporary value", "一時値を確保できません"},
    {qyno_message_0482, "Cannot add to array with upper limit 0", "上限0の配列には追加できません"},
    {qyno_message_0483, "cannot hold immutable fields", "不変フィールドを保持できません"},
    {qyno_message_0484, "Unknown option: %s", "不明なオプションです: %s"},
    {qyno_message_0485, "Invalid UTF-8", "不正な UTF-8 です"},
    {qyno_message_0486, "Invalid UTF-8 string", "不正な UTF-8 文字列です"},
    {qyno_message_0487, "Bad UTF-8 string\n", "不正な UTF-8 文字列です\n"},
    {qyno_message_0488, "Invalid argument for random number lib.random.%s", "乱数 lib.random.%s の引数が不正です"},
    {qyno_message_0489, "Please set the random number seed and number of characters to integers.", "乱数のseed・文字数は整数にしてください"},
    {qyno_message_0490, "Random number candidate is invalid UTF-8", "乱数の候補が不正なUTF-8です"},
    {qyno_message_0491, "Please use str/char as the random number candidate.", "乱数の候補は str・char にしてください"},
    {qyno_message_0492, "Could not allocate random number candidates", "乱数の候補を確保できません"},
    {qyno_message_0493, "Do not append ref to random value argument", "乱数の値引数に ref は付けません"},
    {qyno_message_0494, "Invalid internal initialization of random numbers", "乱数の内部初期化が不正です"},
    {qyno_message_0495, "Random number internal functions cannot be used directly", "乱数の内部関数は直接使用できません"},
    {qyno_message_0496, "Please use module alias for random number type name API (using lib.random as random)", "乱数の型名APIはモジュール別名を使ってください（using lib.random as random）"},
    {qyno_message_0497, "random number argument", "乱数の引数"},
    {qyno_message_0498, "Please use two inequality signs in the same direction for the range of random numbers.", "乱数の範囲には同じ向きの不等号を2個使ってください"},
    {qyno_message_0499, "Please write the range of random numbers as min < i <= max", "乱数の範囲は min < i <= max のように書いてください"},
    {qyno_message_0500, "Specify the range of random numbers using the inequality min < i <= max", "乱数の範囲は min < i <= max の不等式で指定してください"},
    {qyno_message_0501, "Random number range/candidates/number of characters are invalid", "乱数の範囲・候補・文字数が不正です"},
    {qyno_message_0502, "The random number range/candidate/number of characters is invalid (including cases where there is no value that can be expressed)", "乱数の範囲・候補・文字数が不正です（表現できる値がない場合も含みます）"},
    {qyno_message_0503, "Random numbers cannot be used as initial values of const", "乱数は const の初期値に使えません"},
    {qyno_message_0504, "Could not allocate temporary value for random number generator", "乱数生成器の一時値を確保できません"},
    {qyno_message_0505, "Could not allocate random number generator reference", "乱数生成器の参照を確保できません"},
    {qyno_message_0506, "Reserved words cannot be used in names", "予約語は名前に使えません"},
    {qyno_message_0507, "Assignment", "代入"},
    {qyno_message_0508, "Unable to get location", "位置を取得できません"},
    {qyno_message_0509, "Write positional arguments before named arguments", "位置引数は名前付き引数より先に書いてください"},
    {qyno_message_0510, "remainder", "余り"},
    {qyno_message_0511, "There is an extra indentation", "余分なインデントがあります"},
    {qyno_message_0512, "There are extra characters", "余計な文字があります"},
    {qyno_message_0513, "Working directory is not in correct Unicode", "作業ディレクトリが正しいUnicodeではありません"},
    {qyno_message_0514, "Working directory is not reachable from current root", "作業ディレクトリが現在のルートから到達できません"},
    {qyno_message_0515, "Working directory path is too long", "作業ディレクトリのパスが長すぎます"},
    {qyno_message_0516, "Working directory operation failed", "作業ディレクトリ操作に失敗しました"},
    {qyno_message_0517, "Working directory operations cannot be used for const initial values", "作業ディレクトリ操作は const の初期値に使えません"},
    {qyno_message_0518, "Usage: qyc <input file.qy> [-o <output file>] [-S] [-I <search root>] [--target windows|linux]\n", "使い方: qyc <入力ファイル.qy> [-o <出力ファイル>] [-S] [-I <探索ルート>] [--target windows|linux]\n"},
    {qyno_message_0519, "This escape cannot be used (\\n \\t \\r \\0 \\\\ \\\" \\' \\{ \\} can be used)", "使えないエスケープです（\\n \\t \\r \\0 \\\\ \\\" \\' \\{ \\} が使えます）"},
    {qyno_message_0520, "This character cannot be used.", "使えない文字です"},
    {qyno_message_0521, "main cannot be defined in dependent modules", "依存モジュールには main を定義できません"},
    {qyno_message_0522, "value", "値"},
    {qyno_message_0523, "No value entered", "値が入力されていません"},
    {qyno_message_0524, "Returns that return a value and returns without a value cannot be mixed", "値を返す return と値のない return は混在できません"},
    {qyno_message_0525, "Value (number, string, true, false, variable) or '(' is required", "値（数値・文字列・true・false・変数）または '(' が必要です"},
    {qyno_message_0526, "The input has been completed and could not be read.", "入力が終わっていて、読み込めませんでした"},
    {qyno_message_0527, "Failed to read input", "入力の読み込みに失敗しました"},
    {qyno_message_0528, "Specify exactly one input file", "入力ファイルは1つだけ指定してください"},
    {qyno_message_0529, "Could not allocate input destination", "入力先を確保できません"},
    {qyno_message_0530, "Private type %s cannot be used in a public definition. Please make the definition private or convert it to a public type.", "公開する定義に private の型 %s を使用できません。定義を private にするか公開型へ変換してください"},
    {qyno_message_0531, "Please write only one public/private public specification.", "公開指定 public / private は1つだけ書いてください"},
    {qyno_message_0532, "Please write only one readonly after the public specification", "公開指定の後に readonly を1つだけ書いてください"},
    {qyno_message_0533, "Internal error: Conversion from %s to %s not supported", "内部エラー: %s から %s への変換は未対応です"},
    {qyno_message_0534, "Internal error: 8-bit operations only support al~bl", "内部エラー: 8ビット演算は al〜bl のみ対応しています"},
    {qyno_message_0535, "Internal error: Import ID is out of range", "内部エラー: Import ID が範囲外です"},
    {qyno_message_0536, "Internal error: Import not output", "内部エラー: Import が出力されていません"},
    {qyno_message_0537, "Internal error: import not available for Linux", "内部エラー: Linux 向けにインポートは使えません"},
    {qyno_message_0538, "Internal error: movzx only supports al~bl", "内部エラー: movzx は al〜bl のみ対応しています"},
    {qyno_message_0539, "Internal error: setcc only supports al~bl", "内部エラー: setcc は al〜bl のみ対応しています"},
    {qyno_message_0540, "Internal error: stack depth remains %d", "内部エラー: スタックの深さが %d のままです"},
    {qyno_message_0541, "Internal error: Data generation order does not match", "内部エラー: データの生成順が一致しません"},
    {qyno_message_0542, "Internal error: Missing plan for data", "内部エラー: データの計画が不足しています"},
    {qyno_message_0543, "Internal error: attempt to write outside of buffer", "内部エラー: バッファの範囲外に書き込もうとしました"},
    {qyno_message_0544, "Internal error: Duplicate label %s", "内部エラー: ラベル %s が重複しています"},
    {qyno_message_0545, "Internal error: Label %s not located", "内部エラー: ラベル %s の位置が決まっていません"},
    {qyno_message_0546, "Internal error: Label sources do not match", "内部エラー: ラベルの提供元が一致しません"},
    {qyno_message_0547, "Internal error: Number of labels generated does not match", "内部エラー: ラベルの生成数が一致しません"},
    {qyno_message_0548, "Internal error: Label generation order does not match", "内部エラー: ラベルの生成順が一致しません"},
    {qyno_message_0549, "Internal error: invalid numeric instruction", "内部エラー: 不正な数値命令です"},
    {qyno_message_0550, "Internal error: invalid type for binary operation", "内部エラー: 二項演算の型が不正です"},
    {qyno_message_0551, "Internal error: type that cannot be printed", "内部エラー: 出力できない型です"},
    {qyno_message_0552, "Internal error: invalid type for unary -", "内部エラー: 単項 - の型が不正です"},
    {qyno_message_0553, "Internal error: Fixed numeric save area is inconsistent", "内部エラー: 固定数値退避領域が不整合です"},
    {qyno_message_0554, "Internal error: unsupported expression for type checking", "内部エラー: 型検査で未対応の式です"},
    {qyno_message_0555, "Internal error: unsupported statement by type checking", "内部エラー: 型検査で未対応の文です"},
    {qyno_message_0556, "Internal error: invalid constant data reference", "内部エラー: 定数データの参照が不正です"},
    {qyno_message_0557, "Internal error: Number of constant data generated does not match", "内部エラー: 定数データの生成数が一致しません"},
    {qyno_message_0558, "Internal error: inconsistent stack of number blocks", "内部エラー: 数値ブロックのスタックが不整合です"},
    {qyno_message_0559, "Internal error: unable to allocate numeric register", "内部エラー: 数値レジスタを割り当てられません"},
    {qyno_message_0560, "Internal error: Numerical instruction not supported", "内部エラー: 数値命令が未対応です"},
    {qyno_message_0561, "Internal error: Numerical instruction stack is inconsistent", "内部エラー: 数値命令のスタックが不整合です"},
    {qyno_message_0562, "Internal error: invalid numeric save event", "内部エラー: 数値退避イベントが不正です"},
    {qyno_message_0563, "Internal error: Numeric value save destination is outside the frame", "内部エラー: 数値退避先がフレーム外です"},
    {qyno_message_0564, "Internal error: Inconsistent numeric save plan", "内部エラー: 数値退避計画が不整合です"},
    {qyno_message_0565, "Internal error: Insufficient numeric save area", "内部エラー: 数値退避領域が不足しています"},
    {qyno_message_0566, "Internal error: unsupported xmm register", "内部エラー: 未対応の xmm レジスタです"},
    {qyno_message_0567, "Internal error: unsupported expression", "内部エラー: 未対応の式です"},
    {qyno_message_0568, "Internal error: unsupported statement", "内部エラー: 未対応の文です"},
    {qyno_message_0569, "Internal error: inconsistent machine instruction value", "内部エラー: 機械命令の値が不整合です"},
    {qyno_message_0570, "Internal error: Invalid subscript address specification", "内部エラー: 添字アドレスの指定が不正です"},
    {qyno_message_0571, "Internal error: Unsaved number", "内部エラー: 退避されていない数値です"},
    {qyno_message_0572, "Internal error: invalid type for array operation", "内部エラー: 配列操作の型が不正です"},
    {qyno_message_0573, "Internal error: inconsistent plan for array transfer", "内部エラー: 配列転送の計画が不整合です"},
    {qyno_message_0574, "Cannot prove close of recursive file function. Separate file operations outside of recursion", "再帰する file 関数の close を証明できません。ファイル操作を再帰の外へ分離してください"},
    {qyno_message_0575, "Could not allocate split string", "分割文字列を確保できません"},
    {qyno_message_0576, "Initial value", "初期値"},
    {qyno_message_0577, "Requires field name to initialize", "初期化するフィールド名が必要です"},
    {qyno_message_0578, "Unable to allocate syntax tree for initialization", "初期化の構文木を確保できません"},
    {qyno_message_0579, "Could not allocate initialization state", "初期化状態を確保できません"},
    {qyno_message_0580, "The result of division exceeds the range of int", "割り算の結果が int の範囲を超えます"},
    {qyno_message_0581, "Unable to get monotonic clock", "単調時計を取得できません"},
    {qyno_message_0582, "Unary - can only be used for numbers (int, float, double) (cannot be used for %s)", "単項 - は数値（int・float・double）にしか使えません（%s には使えません）"},
    {qyno_message_0583, "Reference/update path is too deep", "参照・更新先のパスが深すぎます"},
    {qyno_message_0584, "Unable to hold recipient", "受信者を保持できません"},
    {qyno_message_0585, "Name %s conflicts with the name of an import or built-in module", "名前 %s は import または組み込みモジュールの名前と衝突しています"},
    {qyno_message_0586, "Name %s conflicts with using %s", "名前 %s は using %s と衝突しています"},
    {qyno_message_0587, "Could not allocate space for name", "名前の領域を確保できません"},
    {qyno_message_0588, "Close of file cannot be confirmed in recovery error path (function %.*s). Please close with finally or case", "回復するエラー経路で file の close を確認できません（関数 %.*s）。finally または case で close してください"},
    {qyno_message_0589, "Unable to allocate space for type", "型の領域を確保できません"},
    {qyno_message_0590, "Variable %.*s declared without type name is unused (assignment only)", "型名を省略して宣言した変数 %.*s は未使用です（代入のみ）"},
    {qyno_message_0591, "The variable %.*s declared without the type name is unused (assignment only). Similar name to existing variable %.*s (%s:%d)", "型名を省略して宣言した変数 %.*s は未使用です（代入のみ）。既存の変数 %.*s と名前が似ています（%s:%d）"},
    {qyno_message_0592, "Could not allocate type name", "型名を確保できません"},
    {qyno_message_0593, "Type (int, float, double, bool, str, char) is required", "型（int・float・double・bool・str・char）が必要です"},
    {qyno_message_0594, "Embedded expression is invalid", "埋め込みの式が正しくありません"},
    {qyno_message_0595, "This character cannot be used in embedded expressions.", "埋め込みの式に使えない文字です"},
    {qyno_message_0596, "Embedded string not closed", "埋め込みの文字列が閉じられていません"},
    {qyno_message_0597, "Please set the input destination for the radix specification to int.", "基数指定の入力先は int にしてください"},
    {qyno_message_0598, "Radix specification can only be used in int expressions", "基数指定は int の式にだけ使えます"},
    {qyno_message_0599, "Radix specification can only be used for integers", "基数指定は整数にだけ使えます"},
    {qyno_message_0600, "place", "場所"},
    {qyno_message_0601, "Assign location", "場所の代入"},
    {qyno_message_0602, "Locations can only be compared using == and != (%s is not allowed)", "場所は == と != でしか比べられません（%s は使えません）"},
    {qyno_message_0603, "increase", "増や"},
    {qyno_message_0604, "increase", "増やしていく"},
    {qyno_message_0605, "The direction of the increment is opposite to the direction of the range", "増分の向きが範囲の向きと逆です"},
    {qyno_message_0606, "The direction of the increment is opposite to the range (%s range, so set the loop variable to %s)", "増分の向きが範囲の向きと逆です（%s範囲なので、ループ変数を%sしてください）"},
    {qyno_message_0607, "Please specify the value to be converted as a positional argument", "変換する値を位置引数で指定してください"},
    {qyno_message_0608, "Variable %.*s is not declared", "変数 %.*s は宣言されていません"},
    {qyno_message_0609, "Variable or constant %s is not declared. Check import and using", "変数または定数 %s は宣言されていません。import と using を確認してください"},
    {qyno_message_0610, "variable name", "変数名"},
    {qyno_message_0611, "When declaring a loop variable with the same name as an outer variable, please specify the type.", "外側の変数と同名のループ変数を宣言する場合は、型を明示してください"},
    {qyno_message_0612, "Cannot be assigned to constant %.*s", "定数 %.*s には代入できません"},
    {qyno_message_0613, "Constant %.*s has no location (addr)", "定数 %.*s は場所（addr）を持ちません"},
    {qyno_message_0614, "Could not allocate plan for constant data", "定数データの計画を確保できません"},
    {qyno_message_0615, "constant name", "定数名"},
    {qyno_message_0616, "Unable to get runtime command line", "実行時コマンドラインを取得できません"},
    {qyno_message_0617, "Runtime argument is not valid Unicode", "実行時引数が正しいUnicodeではありません"},
    {qyno_message_0618, "Runtime argument too long", "実行時引数が長すぎます"},
    {qyno_message_0619, "Runtime argument size out of range", "実行時引数のサイズが範囲を超えています"},
    {qyno_message_0620, "Number of runtime arguments exceeds range", "実行時引数の個数が範囲を超えています"},
    {qyno_message_0621, "No matching try", "対応する try がありません"},
    {qyno_message_0622, "target is not a directory", "対象がディレクトリではありません"},
    {qyno_message_0623, "The decimal number is outside the range of binary128 or exceeds the upper limit of 4096 digits", "小数がbinary128の範囲外か桁数上限4096を超えています"},
    {qyno_message_0624, "Alphabetic characters, _,. cannot be written immediately after a decimal number.", "小数の直後に英字・_・.は書けません"},
    {qyno_message_0625, "Decimal literal is out of range %s", "小数リテラルが %s の範囲外です"},
    {qyno_message_0626, "Decimal random number boundaries", "小数乱数の境界"},
    {qyno_message_0627, "Duplicate argument %.*s", "引数 %.*s が重複しています"},
    {qyno_message_0628, "Argument %.*s has the same name as const on line %d", "引数 %.*s は %d 行目の const と同じ名前です"},
    {qyno_message_0629, "requires name of argument", "引数の名前が必要です"},
    {qyno_message_0630, "Argument type (int, float, double, bool, str, char) is required", "引数の型（int・float・double・bool・str・char）が必要です"},
    {qyno_message_0631, "Wait failed", "待機に失敗しました"},
    {qyno_message_0632, "Wait time is out of range for int_64", "待機時間がint_64の範囲外です"},
    {qyno_message_0633, "Circular import: %s is the same file as %s being read", "循環 import: %s は読み込み中の %s と同じファイルです"},
    {qyno_message_0634, "Circular import: Loading %s", "循環 import: %s を読み込み中です"},
    {qyno_message_0635, "Unable to allocate required memory (size is out of range or out of memory)", "必要なメモリを確保できません（サイズが範囲外か、メモリ不足です）"},
    {qyno_message_0636, "return value", "戻り値"},
    {qyno_message_0637, "Return value types %s and %s cannot be unified. Please specify return type or conversion", "戻り値の型 %s と %s は統一できません。戻り値型または変換を明示してください"},
    {qyno_message_0638, "Return value inference also infers array dimensions. Omit the [] after the function name", "戻り値の推論では配列の次元も推論します。関数名の後の [] は省略してください"},
    {qyno_message_0639, "Dependency on return value type inference should be less than or equal to %d", "戻り値型の推論の依存は %d 段以下にしてください"},
    {qyno_message_0640, "exponent number required", "指数の数字が必要です"},
    {qyno_message_0641, "Search root: %s\n", "探索ルート: %s\n"},
    {qyno_message_0642, "Search root is not a directory: %s", "探索ルートがディレクトリではありません: %s"},
    {qyno_message_0643, "Could not allocate area for search root", "探索ルートの領域を確保できません"},
    {qyno_message_0644, "Unable to ensure dimensions of inferred return value", "推論した戻り値の次元を確保できません"},
    {qyno_message_0645, "Arguments to infer must have concrete value types (addr is not allowed)", "推論する引数には具体的な値型が必要です（addr は使用できません）"},
    {qyno_message_0646, "The argument to infer also infers the dimensions of the array. Please omit the [] after the name", "推論する引数は配列の次元も推論します。名前の後の [] は省略してください"},
    {qyno_message_0647, "The inferred return value must have a concrete value type (addr cannot be returned as a value)", "推論する戻り値には具体的な値型が必要です（addr は値として返せません）"},
    {qyno_message_0648, "Numerical conversion/operation result is outside the range of the target type", "数値の変換・演算結果が対象型の範囲外です"},
    {qyno_message_0649, "Numerical conversion/arithmetic result is out of range", "数値の変換・演算結果が範囲外です"},
    {qyno_message_0650, "Numerical calculation/conversion is invalid (please check division by 0, range, exponent, and shift amount)", "数値の演算・変換が不正です（0除算・範囲・指数・シフト量を確認してください）"},
    {qyno_message_0651, "Could not allocate number block", "数値ブロックを確保できません"},
    {qyno_message_0652, "Could not allocate numeric instruction", "数値命令を確保できません"},
    {qyno_message_0653, "Could not allocate numeric string", "数値文字列を確保できません"},
    {qyno_message_0654, "Invalid number format", "数値書式が不正です"},
    {qyno_message_0655, "Could not allocate numerical save plan", "数値退避計画を確保できません"},
    {qyno_message_0656, "Please write the function definition in the form of a formula on one line.", "数式の形の関数定義は1行で書いてください"},
    {qyno_message_0657, "The integer is outside the 128 bit range or does not match the base number.", "整数が128bitの範囲外か基数に合わない数字です"},
    {qyno_message_0658, "Rounding result of integer exceeds the range of int", "整数の丸め結果が int の範囲を超えます"},
    {qyno_message_0659, "Integer types %s and %s cannot be safely unified. Please convert explicitly", "整数の型 %s と %s は安全に統一できません。明示的に変換してください"},
    {qyno_message_0660, "ref cannot be used to convert integers", "整数の変換に ref は使えません"},
    {qyno_message_0661, "Requires number after integer prefix", "整数の接頭辞の後に数字が必要です"},
    {qyno_message_0662, "Could not allocate integer string", "整数の文字列を確保できません"},
    {qyno_message_0663, "Integer random numbers must have integer boundaries", "整数乱数の境界は整数にしてください"},
    {qyno_message_0664, "Invalid base for integer conversion", "整数変換の base が不正です"},
    {qyno_message_0665, "Result of integer conversion is out of range for target type", "整数変換の結果が対象型の範囲外です"},
    {qyno_message_0666, "A statement is required (please write the expression that uses the result of input in the assignment to a variable or in the condition of if)", "文が必要です（input の結果を使う式は、変数への代入や if の条件に書いてください）"},
    {qyno_message_0667, "A statement is required (corresponds to return, print, if, while, for, break, continue, const, using, variable declaration and update, and function call)", "文が必要です（return・print・if・while・for・break・continue・const・using・変数の宣言と更新・関数呼び出しに対応しています）"},
    {qyno_message_0668, "A newline is required at the end of the sentence", "文の終わりには改行が必要です"},
    {qyno_message_0669, "Input destination grapheme boundaries change after character replacement", "文字の置換後に入力先の書記素境界が変わります"},
    {qyno_message_0670, "string", "文字列"},
    {qyno_message_0671, "String is not closed (write the string on one line)", "文字列が閉じられていません（文字列は1行で書いてください）"},
    {qyno_message_0672, "Use string {expression} for print or io.file.write output", "文字列の {式} は print または io.file.write の出力に使ってください"},
    {qyno_message_0673, "Unable to allocate memory for string", "文字列のメモリを確保できません"},
    {qyno_message_0674, "Unable to allocate work area for string", "文字列の作業領域を確保できません"},
    {qyno_message_0675, "String subscript is out of range", "文字列の添字が範囲外です"},
    {qyno_message_0676, "String subscript is out of range\n", "文字列の添字が範囲外です\n"},
    {qyno_message_0677, "The string subscript can be written only once at the end of the update destination", "文字列の添字は更新先の最後に1回だけ書けます"},
    {qyno_message_0678, "Could not allocate string result", "文字列の結果を確保できません"},
    {qyno_message_0679, "Strings can only be concatenated with str with + (%s and %s)", "文字列は + で str 同士を連結することしかできません（%s と %s）"},
    {qyno_message_0680, "Cannot read string as int in specified radix", "文字列を指定された基数の int として読み取れません"},
    {qyno_message_0681, "Unable to read string as specified decimal type", "文字列を指定した小数型として読み取れません"},
    {qyno_message_0682, "The string cannot be read as the specified numeric type (please check the range, radix, and string)", "文字列を指定した数値型として読み取れません（範囲・基数・文字列を確認してください）"},
    {qyno_message_0683, "ref cannot be specified for string method", "文字列メソッドに ref は指定できません"},
    {qyno_message_0684, "Invalid UTF-8 for string method", "文字列メソッドに不正な UTF-8 です"},
    {qyno_message_0685, "Invalid UTF-8 for string method\n", "文字列メソッドに不正な UTF-8 です\n"},
    {qyno_message_0686, "String method work area is too large", "文字列メソッドの作業領域が大きすぎます"},
    {qyno_message_0687, "String method search string cannot be empty or has invalid size", "文字列メソッドの検索文字列は空にできません、またはサイズが不正です"},
    {qyno_message_0688, "The search string/size of the string method is invalid \n", "文字列メソッドの検索文字列・サイズが不正です\n"},
    {qyno_message_0689, "String method result too large", "文字列メソッドの結果が大きすぎます"},
    {qyno_message_0690, "String method cannot produce result", "文字列メソッドの結果を生成できません"},
    {qyno_message_0691, "Characters within a string cannot be passed by ref. Please specify the elements of the char array", "文字列内の文字は ref で渡せません。char 配列の要素を指定してください"},
    {qyno_message_0692, "Could not allocate default mode", "既定モードを確保できません"},
    {qyno_message_0693, "Default arguments can only be specified for init", "既定引数は init にだけ指定できます"},
    {qyno_message_0694, "Unable to get clock frequency", "時計の周波数を取得できません"},
    {qyno_message_0695, "can't get watch", "時計を取得できません"},
    {qyno_message_0696, "Clock acquisition, wait, and try_parse cannot be used with const", "時計取得・待機・try_parseはconstに使えません"},
    {qyno_message_0697, "Too many dimensions to update", "更新先の次元が多すぎます"},
    {qyno_message_0698, "The update destination should be a variable or a subscript starting from the variable.", "更新先は変数、または変数から始まる添字にしてください"},
    {qyno_message_0699, "Please specify the format as 0b / 0o / 0d / 0x", "書式指定は 0b / 0o / 0d / 0x にしてください"},
    {qyno_message_0700, "Could not allocate optimization value", "最適化の値を確保できません"},
    {qyno_message_0701, "Unable to ensure optimization control flow", "最適化の制御フローを確保できません"},
    {qyno_message_0702, "Could not allocate optimization statement", "最適化の文を確保できません"},
    {qyno_message_0703, "Unfinished models cannot be copied\n", "未完了のモデルはコピーできません\n"},
    {qyno_message_0704, "Incomplete field %s can only be used for initial assignment to the whole", "未完了フィールド %s は全体への初回代入以外に使用できません"},
    {qyno_message_0705, "Initialize incomplete field %s and then use the entire this and return successfully (Declaration: %s:%d)", "未完了フィールド %s を初期化してから this 全体を使用・成功 return してください（宣言: %s:%d）"},
    {qyno_message_0706, "Standard module %s does not have %s", "標準モジュール %s に %s はありません"},
    {qyno_message_0707, "Standard type %s.%s is missing", "標準型 %s.%s がありません"},
    {qyno_message_0708, "Could not allocate area for machine instructions", "機械命令の領域を確保できません"},
    {qyno_message_0709, "Could not allocate machine instructions", "機械命令を確保できません"},
    {qyno_message_0710, "Subscript and length can be used for str/char (%s is specified)", "添字と length は str・char に使えます（%s が指定されています）"},
    {qyno_message_0711, "Unable to create composite update of subscript", "添字の複合更新を作成できません"},
    {qyno_message_0712, "Subscripts should be integer type", "添字は整数型にしてください"},
    {qyno_message_0713, "decreased", "減ら"},
    {qyno_message_0714, "reduce", "減らしていく"},
    {qyno_message_0715, "Could not allocate generation destination", "生成先を確保できません"},
    {qyno_message_0716, "Invalid close parsing of generation argument", "生成引数の close 解析が不正です"},
    {qyno_message_0717, "Could not allocate generation arguments", "生成引数を確保できません"},
    {qyno_message_0718, "Unable to allocate generation function", "生成関数を確保できません"},
    {qyno_message_0719, "Destination path cannot be empty", "移動先のパスを空にできません"},
    {qyno_message_0720, "Empty interface cannot be defined", "空の interface は定義できません"},
    {qyno_message_0721, "Empty array literal requires element type context, such as declaration or argument", "空の配列リテラルには、宣言や引数などの要素型の文脈が必要です"},
    {qyno_message_0722, "blank", "空白"},
    {qyno_message_0723, "A range's tentative name cannot be referenced in a bounds expression", "範囲の仮の名前を境界式で参照できません"},
    {qyno_message_0724, "Range directions do not match (use < or <= on both sides, or > or >= on both sides)", "範囲の向きが合っていません（左右とも <・<=、または左右とも >・>= にしてください）"},
    {qyno_message_0725, "Close file before entering a never-ending loop", "終了しないループへ入る前に file を close してください"},
    {qyno_message_0726, "ending boundary", "終了境界"},
    {qyno_message_0727, "The absolute value |x| must be a numeric value (int, float, double) (%s is specified)", "絶対値 |x| の中は数値（int・float・double）にしてください（%s が指定されています）"},
    {qyno_message_0728, "Close absolute value | is required", "絶対値を閉じる | が必要です"},
    {qyno_message_0729, "The string after replacement is too large\n", "置換後の文字列が大きすぎます\n"},
    {qyno_message_0730, "Imaginary number is too large or too small", "虚数が大きすぎるか小さすぎます"},
    {qyno_message_0731, "Too many imaginary digits", "虚数の桁が多すぎます"},
    {qyno_message_0732, "Numbers, letters, and _ cannot be written after an imaginary literal.", "虚数リテラルの後に数字・英字・_ は書けません"},
    {qyno_message_0733, "complex real/imag is read-only", "複素数の real / imag は読み取り専用です"},
    {qyno_message_0734, "The components of a complex number are real / imag", "複素数の成分は real / imag です"},
    {qyno_message_0735, "Compare complex numbers with == / !=", "複素数は == / != で比較してください"},
    {qyno_message_0736, "Complex update of elements", "要素の複合更新"},
    {qyno_message_0737, "Assignment to element", "要素への代入"},
    {qyno_message_0738, "Arrays whose elements are referenced by ref cannot be added, deleted, or reassigned\n", "要素を ref で参照している配列は追加・削除・再代入できません\n"},
    {qyno_message_0739, "calculation", "計算"},
    {qyno_message_0740, "Could not allocate read-only reference", "読み取り専用の参照を確保できません"},
    {qyno_message_0741, "warning", "警告"},
    {qyno_message_0742, "Negative 128bit integer literal is out of range", "負の128bit整数リテラルが範囲外です"},
    {qyno_message_0743, "Negative integer literal is out of range", "負の整数リテラルが範囲外です"},
    {qyno_message_0744, "The result of a negative power cannot be expressed as an int (please use a decimal base like 2.0^(-1))", "負の累乗の結果を int で表せません（2.0^(-1) のように底を小数にしてください）"},
    {qyno_message_0745, "The result of a negative power cannot be expressed as an integer", "負の累乗の結果を整数で表せません"},
    {qyno_message_0746, "Unable to get initial position of append", "追記の初期位置を取得できません"},
    {qyno_message_0747, "Not a regular file or unable to retrieve file information", "通常ファイルではありません、またはファイル情報を取得できません"},
    {qyno_message_0748, "array", "配列"},
    {qyno_message_0749, "Array does not have method %s", "配列にはメソッド %s はありません"},
    {qyno_message_0750, "Space required for array exceeds int range\n", "配列に必要な領域が int の範囲を超えます\n"},
    {qyno_message_0751, "Unable to create copy of array", "配列のコピーを作成できません"},
    {qyno_message_0752, "Array upper bound is negative and required space exceeds the range of int", "配列の上限が負か、必要な領域が int の範囲を超えます"},
    {qyno_message_0753, "The upper limit of the array should be a non-negative int determined at compile time.", "配列の上限はコンパイル時に確定する非負の int にしてください"},
    {qyno_message_0754, "Unable to check required init arguments required for missing array", "配列の不足分に必要な init の必須引数を確認できません"},
    {qyno_message_0755, "Array capacity exceeds int range\n", "配列の容量が int の範囲を超えます\n"},
    {qyno_message_0756, "Array integer element types cannot be safely unified", "配列の整数要素型を安全に統一できません"},
    {qyno_message_0757, "Use append to append to the end of the array", "配列の末尾への追加には append を使用してください"},
    {qyno_message_0758, "Could not allocate array dimension information", "配列の次元情報を確保できません"},
    {qyno_message_0759, "Number of array dimensions exceeds upper limit", "配列の次元数が上限を超えています"},
    {qyno_message_0760, "The number of dimensions of the array must be less than or equal to %d", "配列の次元数は %d 以下にしてください"},
    {qyno_message_0761, "Array subscript is out of range", "配列の添字が範囲外です"},
    {qyno_message_0762, "Array subscript is out of range\n", "配列の添字が範囲外です\n"},
    {qyno_message_0763, "elements of array", "配列の要素"},
    {qyno_message_0764, "Please match the element type and number of dimensions of the array.", "配列の要素型と次元数をそろえてください"},
    {qyno_message_0765, "The number of elements in the array exceeds the upper limit of the destination", "配列の要素数が代入先の上限を超えています"},
    {qyno_message_0766, "The number of elements in the array exceeds the upper limit of the assignment destination\n", "配列の要素数が代入先の上限を超えています\n"},
    {qyno_message_0767, "Number of elements in array exceeds declared limit", "配列の要素数が宣言された上限を超えています"},
    {qyno_message_0768, "Unable to create diagnostic for array", "配列の診断を作成できません"},
    {qyno_message_0769, "Adding array exceeds maximum number of elements\n", "配列の追加が最大要素数を超えています\n"},
    {qyno_message_0770, "Compare arrays with the same element type and number of dimensions using == or !=", "配列は同じ要素型・次元数で == または != を使って比較してください"},
    {qyno_message_0771, "Unable to allocate memory for array type", "配列型のメモリを確保できません"},
    {qyno_message_0772, "Could not allocate name for array type", "配列型の名前を確保できません"},
    {qyno_message_0773, "Array operation arguments", "配列操作の引数"},
    {qyno_message_0774, "Could not allocate plan for array operation", "配列操作の計画を確保できません"},
    {qyno_message_0775, "Could not allocate plan for array transfer", "配列転送の計画を確保できません"},
    {qyno_message_0776, "starting value", "開始値"},
    {qyno_message_0777, "For a for whose start side is an open int, the boundary and increment should also be int.", "開始側が開いた int の for は、境界・増分も int にしてください"},
    {qyno_message_0778, "Function %.*s has no return when it reaches the end (return a value every step of the way)", "関数 %.*s には、最後まで進んだときの return がありません（すべての道筋で値を返してください）"},
    {qyno_message_0779, "Function %.*s has %d arguments (%d is passed)", "関数 %.*s の引数は %d 個です（%d 個渡しています）"},
    {qyno_message_0780, "Return type inference for function %.*s is circular. Please specify the return value type of the recursive function", "関数 %.*s の戻り値型の推論が循環しています。再帰する関数の戻り値型を明示してください"},
    {qyno_message_0781, "Function %.*s is already defined on line %d", "関数 %.*s は %d 行目ですでに定義されています"},
    {qyno_message_0782, "Function %.*s has the same name as const on line %d", "関数 %.*s は %d 行目の const と同じ名前です"},
    {qyno_message_0783, "Function %s is not defined. Please check \"import\" and \"using\"", "関数 %s は定義されていません。import と using を確認してください"},
    {qyno_message_0784, "Function %s is not defined (please write %s.%s or declare using %s.%s)", "関数 %s は定義されていません（%s.%s と書くか、using %s.%s を宣言してください）"},
    {qyno_message_0785, "Function stack area is too large", "関数のスタック領域が大きすぎます"},
    {qyno_message_0786, "Up to 4096 functions can be instantiated in the entire program (Development source: %.*s)", "関数の具体化はプログラム全体で4096件までです（展開元: %.*s）"},
    {qyno_message_0787, "Unable to ensure reification of function", "関数の具体化を確保できません"},
    {qyno_message_0788, "Please write the body of the function on the line after \";\" or in the form of \"= expression\"", "関数の本体は「;」の後の行に書くか、「= 式」の形で書いてください"},
    {qyno_message_0789, "Please indent the body of the function one line with a tab.", "関数の本体はタブで1段インデントしてください"},
    {qyno_message_0790, "function name required", "関数名が必要です"},
    {qyno_message_0791, "(No mold)", "（型なし）"},
    {qyno_message_0792, "File limit is out of range", "ファイルの上限が範囲外です"},
    {qyno_message_0793, "File exceeds max_bytes", "ファイルが max_bytes を超えています"},
    {qyno_message_0794, "File exceeds max_lines", "ファイルが max_lines を超えています"},
    {qyno_message_0795, "write_lines newline must be LF, CRLF, or CR", "write_lines の newline は LF・CRLF・CR のいずれかにしてください"},
    {qyno_message_0796, "write_lines output is too large", "write_lines の出力が大きすぎます"},
    {qyno_message_0797, "write_lines elements cannot contain CR or LF", "write_lines の要素には CR・LF を含められません"},
    {qyno_message_0798, "File operation failed", "ファイル操作に失敗しました"},
    {qyno_message_0799, "Invalid file path: empty, root, dot, or trailing separator", "ファイルのパスが不正です。空のパス・ルート・ドット・末尾の区切り文字は指定できません"},
    {qyno_message_0800, "File links are unsupported", "ファイルのリンクには対応していません"},
    {qyno_message_0801, "Target is not a regular file", "対象は通常のファイルではありません"},
    {qyno_message_0802, "Destination already exists", "移動先またはコピー先は既に存在します"},
    {qyno_message_0803, "Source is not a regular file", "元のパスは通常のファイルではありません"},
    {qyno_message_0804, "Rename path is too long", "名前変更のパスが長すぎます"},
    {qyno_message_0805, "Directory operation failed", "ディレクトリ操作に失敗しました"},
    {qyno_message_0806, "Directory path cannot be empty", "ディレクトリのパスは空にできません"},
    {qyno_message_0807, "Cannot remove a root, . or .. directory path", "ルート・.・.. のディレクトリは削除できません"},
    {qyno_message_0808, "Removing a directory link is unsupported", "ディレクトリのリンクの削除には対応していません"},
    {qyno_message_0809, "Target is not a directory", "対象はディレクトリではありません"},
    {qyno_message_0810, "Directory entry is not valid Unicode", "ディレクトリ項目が正しい Unicode ではありません"},
    {qyno_message_0811, "Directory limit is out of range", "ディレクトリの上限が範囲外です"},
    {qyno_message_0812, "Directory listing exceeds its limit", "ディレクトリ一覧が上限を超えています"},
    {qyno_message_0813, "Malformed native directory record", "OSのディレクトリ情報が不正です"},
    {qyno_message_0814, "Directory listing exceeds max_name_bytes", "ディレクトリ一覧が max_name_bytes を超えています"},
    {qyno_message_0815, "Metadata size exceeds int_64", "メタデータのサイズが int_64 の範囲を超えています"},
    {qyno_message_0816, "Malformed metadata timestamp", "メタデータの時刻が不正です"},
    {qyno_message_0817, "Cannot allocate a directory default", "ディレクトリの既定値を確保できません"},
    {qyno_message_0818, "Cannot validate a directory path", "ディレクトリのパスを検証できません"},
    {qyno_message_0819, "Invalid directory path: empty, NUL, or unsupported path format", "ディレクトリのパスが不正です。空のパス・NUL・未対応の形式は指定できません"},
    {qyno_message_0820, "Directory limits must be nonnegative int_64 values", "ディレクトリの上限は非負の int_64 にしてください"},
    {qyno_message_0821, "directory limit", "ディレクトリの上限"},
    {qyno_message_0822, "Cannot validate a file path", "ファイルのパスを検証できません"},
    {qyno_message_0823, "Invalid file path: empty, NUL, trailing separator, or unsupported format", "ファイルのパスが不正です。空のパス・NUL・末尾の区切り文字・未対応の形式は指定できません"},
    {qyno_message_0824, "A file path cannot name a root, . or ..", "ファイルのパスにルート・.・.. は指定できません"},
    {qyno_message_0825, "File text must be valid UTF-8", "ファイルの文字列は正しい UTF-8 にしてください"},
    {qyno_message_0826, "File paths cannot contain NUL", "ファイルのパスには NUL を含められません"},
    {qyno_message_0827, "Cannot allocate a file API default", "ファイルAPIの既定値を確保できません"},
    {qyno_message_0828, "write_lines final_newline must be bool", "write_lines の final_newline は bool にしてください"},
    {qyno_message_0829, "File limits must be nonnegative int_64 values", "ファイルの上限は非負の int_64 にしてください"},
    {qyno_message_0830, "file limit", "ファイルの上限"},
    {qyno_message_0831, "info path must be str or char", "info の path は str または char にしてください"},
    {qyno_message_0832, "Cannot validate a metadata path", "メタデータのパスを検証できません"},
    {qyno_message_0833, "Invalid info path: empty, NUL, or unsupported format", "info のパスが不正です。空のパス・NUL・未対応の形式は指定できません"},
    {qyno_message_0834, "info follow_links must be bool", "info の follow_links は bool にしてください"},
    {qyno_message_0835, "Cannot allocate a method name", "メソッド名を確保できません"},
    {qyno_message_0836, " [path: ", " [パス: "},
    {qyno_message_0837, " (OS error ", " (OSエラー "},
    {qyno_message_0838, "io.path.info expects one or two arguments", "io.path.info の引数は1つまたは2つです"},
    {qyno_message_0839, "info paths do not accept ref", "info の path には ref を指定できません"},
    {qyno_message_0840, "info follow_links does not accept ref", "info の follow_links には ref を指定できません"},
    {qyno_message_0841, "io.file.%s expects %d to %d arguments", "io.file.%s の引数は%d個から%d個です"},
    {qyno_message_0842, "Path-based file APIs do not accept ref arguments", "パスを用いるファイルAPIの引数には ref を指定できません"},
    {qyno_message_0843, "io.file.%s requires str or char text arguments", "io.file.%s の文字列引数は str または char にしてください"},
    {qyno_message_0844, "write_lines requires a one-dimensional str array", "write_lines には1次元の str 配列を指定してください"},
    {qyno_message_0845, "io.dir.%s expects 1 to %d arguments", "io.dir.%s の引数は1個から%d個です"},
    {qyno_message_0846, "Directory APIs do not accept ref arguments", "ディレクトリAPIの引数には ref を指定できません"},
    {qyno_message_0847, "Directory paths require str or char", "ディレクトリのパスは str または char にしてください"},
    {qyno_message_0848, "io.file.%s expects %d arguments", "io.file.%s の引数は%d個です"},
    {qyno_message_0849, "File path operations do not accept ref arguments", "ファイルのパス操作の引数には ref を指定できません"},
    {qyno_message_0850, "File paths require str or char", "ファイルのパスは str または char にしてください"},
    {qyno_message_0851, "Moving across filesystems is unsupported", "異なるファイルシステムへの移動には対応していません"},
    {qyno_message_0852, "seek offset", "seek の offset"},
    {qyno_message_0853, "A common error code cannot be converted to an integer", "共通エラーコードは整数に変換できません"},
    {qyno_message_0854, "Create a common error code from a namespaced error code", "共通エラーコードは名前空間付きのエラーコードから作成してください"},
    {qyno_message_0855, "case requires an io.ErrorCode or task.ErrorCode constant", "case は io.ErrorCode または task.ErrorCode の定数にしてください"},
    {qyno_message_0856, "Common error metadata is readonly", "共通エラーの情報は読み取り専用です"},
    {qyno_message_0857, "A common error code cannot be an input destination", "共通エラーコードは入力先にできません"},
    {qyno_message_0858, "Use async with a concrete value type for a task declaration", "タスク変数は async に具体的な型を続けて宣言してください"},
    {qyno_message_0859, "Async model and interface methods are not supported yet", "async の model メソッドと interface メソッドは未対応です"},
    {qyno_message_0860, "A task parameter requires a concrete value type and cannot use ref", "タスク引数には具体的な型を指定し、ref は付けないでください"},
    {qyno_message_0861, "Tasks can only be created inside an async function", "タスクは async 関数内でのみ作成できます"},
    {qyno_message_0862, "Invalid number of arguments for the async operation", "非同期処理の引数の数が正しくありません"},
    {qyno_message_0863, "await requires an async function", "await は async 関数内で使用してください"},
    {qyno_message_0864, "await requires a task", "await の対象はタスクにしてください"},
    {qyno_message_0865, "Tasks only expose the readonly state and done properties", "タスクで使用できるプロパティは読み取り専用の state と done です"},
    {qyno_message_0866, "A task has no address property", "タスクの addr は取得できません"},
    {qyno_message_0867, "A task variable must be initialized", "タスク変数には初期値が必要です"},
    {qyno_message_0868, "Task handles must be non-ref local variables in an async function", "タスクは async 関数内の ref ではないローカル変数で保持してください"},
    {qyno_message_0869, "Keep the task in a variable or await it instead of discarding it", "タスクは変数で保持するか await してください"},
    {qyno_message_0870, "A task has no value property; use await", "タスクの結果は value ではなく await で取得してください"},
    {qyno_message_0871, "A function cannot return task handles", "タスクを関数の戻り値に含めることはできません"},
    {qyno_message_0872, "An async result cannot contain task, ref, addr, file, or interface values", "async 関数の結果にタスク、ref、addr、file、interface を含めることはできません"},
    {qyno_message_0873, "An async parameter cannot contain ref, addr, file, or interface values", "async 関数の引数に ref、addr、file、interface を含めることはできません"},
    {qyno_message_0874, "Persistent ref, addr, and interface locals are not supported inside async functions yet", "async 関数内の ref、addr、interface ローカル変数は未対応です"},
    {qyno_message_0875, "Task parameters require an async function", "タスク引数は async 関数で使用してください"},
    {qyno_message_0876, "A finally block cannot create or await tasks yet", "finally 内でのタスクの作成と await は未対応です"},
    {qyno_message_0877, "Await a task before printing its result", "タスクの結果は await で取得してから表示してください"},
    {qyno_message_0878, "Appending a task requires an initialized task handle", "タスク配列への append には初期化済みのタスクを指定してください"},
    {qyno_message_0879, "The task wait dependency is cyclic", "タスクの待機関係が循環しています"},
    {qyno_message_0880, "Async task stack limit exceeded", "非同期タスクのスタック上限を超えました"},
    {qyno_message_0881, "Awaited from:\n", "await の呼び出し元:\n"},
    {qyno_message_0882, "task.group() is only valid in an async with statement", "task.group() は async with の宣言で使用してください"},
    {qyno_message_0883, "Invalid arguments for the task operation", "タスク操作の引数が正しくありません"},
    {qyno_message_0884, "with_timeout requires a task", "with_timeout の第1引数はタスクにしてください"},
    {qyno_message_0885, "all requires tasks with the same completed value type", "all には完了値の型が一致するタスクを指定してください"},
    {qyno_message_0886, "Use cancel() without arguments on a task or group", "タスクとグループの cancel() に引数は指定できません"},
    {qyno_message_0887, "async with requires task.group()", "async with には task.group() を指定してください"},
    {qyno_message_0888, "A group cannot be copied, stored, or passed as a value", "グループのコピー、格納、引数渡しはできません"},
    {qyno_message_0889, "A task cannot escape the group where it was created", "タスクを作成元のグループの外側へ保存することはできません"},
    {qyno_message_0890, "The task was cancelled", "タスクがキャンセルされました"},
    {qyno_message_0891, "One or more tasks failed", "タスクグループでエラーが発生しました"},
    {qyno_message_0892, "The task timed out", "タスクの期限を超えました"},
    {qyno_message_0893, "Related error: ", "関連エラー: "},
    {qyno_message_0894, "Additional related errors omitted\n", "以降の関連エラーを省略しました\n"},
    {qyno_message_0895, "Unable to initialize asynchronous file workers", "非同期ファイルのワーカーを初期化できません"},
    {qyno_message_0896, "io.file.%s requires a one-dimensional unsigned int_8 array", "io.file.%s の data は unsigned int_8 の1次元配列にしてください"},
    {qyno_message_0897, "Function type requires explicit argument and return types", "関数型の引数と戻り値の型を明示してください"},
    {qyno_message_0898, "This function cannot be used as a value", "この関数を値として使用することはできません"},
    {qyno_message_0899, "Specify a func type to specialize this generic function", "この関数を値として扱う場合は func で型を指定してください"},
    {qyno_message_0900, "Function is not set", "関数が設定されていません"},
    {qyno_message_0901, "Expression is not callable", "この値を関数として呼び出すことはできません"},
    {qyno_message_0902, "Function signature does not match %s", "関数の型が %s と一致しません"},
    {qyno_message_0903, "Function types cannot contain file, addr, interface, task, or group values", "関数型に file、addr、interface、タスク、グループを含めることはできません"},
    {qyno_message_0904, "Function call expects %d arguments, got %d", "関数値の呼び出しには%d個の引数が必要です。%d個が指定されています"},
    {qyno_message_0905, "Invalid or recursive function signature", "関数型が不正か、再帰的に未確定となっています"},
    {qyno_message_0906, "Function values cannot be printed", "関数値を表示することはできません"},
    {qyno_message_0907, "lib.%.*s requires an array", "lib.%.*s の第1引数は配列にしてください"},
};
void messages_select(int japanese)
{
    size_t i;diagnostic_japanese=japanese;
    for(i=0;i<sizeof(messages)/sizeof(*messages);i++){
        const char *s=japanese?messages[i].ja:messages[i].en;
        memcpy(messages[i].text,s,strlen(s)+1);
    }
}
