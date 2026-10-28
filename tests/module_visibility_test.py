"""Module-private declarations, public APIs, using scopes and inference."""
from support import QynoTestCase, program, run_suite

LIB = program('private const LIMIT=3', 'private double twice(double n)=n*2',
    'public double calculate(double n)=twice(n)+LIMIT', 'const LABEL="ready"',
    'public model Point;', '\tdouble x', '\tdouble y',
    'private model Internal;', '\tint n',
    'private interface Hidden;', '\tint get();',
    'public interface Shape;', '\treadonly double area();')


class ModuleVisibilityTests(QynoTestCase):
    def library(self, source=LIB, name='library.qy'):
        (self.directory/name).write_text(source)

    def test_implicit_and_explicit_public_exports(self):
        self.library()
        self.check_program(program('import library','int main();','\tprint(library.calculate(4.0))',
            '\tlibrary.Point p={x=1.5,y=2.5}','\tprint(p)','\tprint(library.LABEL)'), '11.0\nPoint{x=1.5, y=2.5}\nready\n')

    def test_private_functions_and_constants_work_with_forward_references(self):
        self.library(program('public read()=helper(BASE)','private helper(x)=x*2','private const BASE=4'))
        self.check_program(program('import library','int main();','\tprint(library.read())'), '8\n')

    def test_direct_qualified_private_access_is_rejected(self):
        self.library()
        for use in ['print(library.LIMIT)','print(library.twice(2.0))','library.Internal x',
                    'library.Hidden identity(ref library.Hidden x)=x']:
            source=program('import library','int main();','\t'+use) if 'identity' not in use else program('import library',use,'int main();','\treturn 0')
            self.compile_source(source,error='private')

    def test_module_alias_does_not_bypass_private(self):
        self.library()
        for use in ['print(lib.LIMIT)','print(lib.twice(2.0))','lib.Internal x']:
            self.compile_source(program('import library','using library as lib','int main();','\t'+use),error='private')

    def test_selective_using_rejects_private_definitions(self):
        self.library()
        for name in ['LIMIT','twice','Internal','Hidden']:
            self.compile_source(program('import library',f'using library.{name}','int main();','\treturn 0'),error='private')

    def test_wildcard_using_only_introduces_public_names(self):
        self.library()
        self.check_program(program('import library','using library','int main();','\tint LIMIT=7',
            '\tprint(calculate(1.0))','\tprint(LIMIT)'), '5.0\n7\n')
        self.compile_source(program('import library','using library','int main();','\tprint(twice(1.0))'),error='エラー')

    def test_private_names_do_not_cause_using_collisions(self):
        self.library(program('private const VALUE=1','private helper()=1','public read()=helper()+VALUE'))
        self.library(program('public const VALUE=5','public helper()=7'),'other.qy')
        self.check_program(program('import library, other','using library, other','int main();',
            '\tprint("{read()} {VALUE} {helper()}")'), '2 5 7\n')

    def test_local_private_using_remains_available_in_defining_module(self):
        self.library(program('private helper(x)=x+1','using helper','private const BASE=5','using BASE','public read()=helper(BASE)'))
        self.check_program(program('import library','int main();','\tprint(library.read())'), '6\n')

    def test_transitive_import_and_using_do_not_reexport_names(self):
        self.library()
        self.library(program('import library','using library','public read()=calculate(2.0)'),'facade.qy')
        self.check_program(program('import facade','int main();','\tprint(facade.read())'), '7.0\n')
        self.compile_source(program('import facade','int main();','\tprint(facade.calculate(2.0))'),error='エラー')

    def test_private_model_can_back_a_public_model_private_field(self):
        self.library(program('private model State;','\tint n=5','public model Counter;',
            '\tprivate State state','\tpublic readonly int get()=this.state.n'))
        self.check_program(program('import library','int main();','\tlibrary.Counter c','\tprint(c.get())'), '5\n')

    def test_private_interface_can_be_an_implementation_detail(self):
        self.library(program('private interface Internal;','\treadonly int get();',
            'public model Counter implements Internal;','\treadonly int get()=5',
            'private int read(ref readonly Internal c)=c.get()', 'public int run();','\tCounter c','\treturn read(ref c)'))
        self.check_program(program('import library','int main();','\tprint(library.run())'), '5\n')

    def test_public_function_cannot_expose_private_parameter_or_return_type(self):
        for header in ['public Secret make() = {}','Secret make() = {}','public void use(Secret x);\n\treturn',
                       'public Secret make()[] = []','public void use(ref Secret x[]);\n\treturn']:
            self.library(program('private model Secret;','\tint n',header))
            self.compile_source(program('import library','int main();','\treturn 0'),error='private',error_source='library.qy')

    def test_inferred_return_cannot_expose_private_nominal_type(self):
        self.library(program('private model Secret;','\tint n','public create();','\tSecret s','\treturn s'))
        self.compile_source(program('import library','int main();','\treturn 0'),error='private',error_source='library.qy')

    def test_public_generic_instances_are_checked_after_inference(self):
        self.library(program('private model Secret;','\tint n','public create(x);','\tSecret s','\treturn s'))
        self.compile_source(program('import library','int main();','\tresult=library.create(1)','\tprint(result)'),error='private',error_source='library.qy')
        self.library(program('private model Secret;','\tint n=2','private create(x);','\tSecret s','\treturn s',
            'public read()=create(1).n'))
        self.check_program(program('import library','int main();','\tprint(library.read())'), '2\n')

    def test_public_constant_and_field_cannot_expose_private_type(self):
        for declaration in ['public const Secret INSTANCE={}', 'public const INSTANCE=Secret()',
                            'public model Visible;\n\tSecret state', 'model Visible;\n\tpublic Secret states[]']:
            self.library(program('private model Secret;','\tint n',declaration))
            self.compile_source(program('import library','int main();','\treturn 0'),error='private',error_source='library.qy')

    def test_public_methods_and_interfaces_cannot_expose_private_types(self):
        for declaration in ['public model Visible;\n\tSecret create() = {}',
                            'public model Visible;\n\tvoid init(Secret n);\n\t\treturn',
                            'public interface Contract;\n\tSecret get();']:
            self.library(program('private model Secret;','\tint n',declaration))
            self.compile_source(program('import library','int main();','\treturn 0'),error='private',error_source='library.qy')

    def test_private_models_methods_and_generic_helpers_can_use_private_types(self):
        self.library(program('private model Hidden;','\tint n=3','\tHidden copy()=this',
            'private identity(x)=x','private Hidden seed()={}', 'public read()=identity(seed()).copy().n'))
        self.check_program(program('import library','int main();','\tprint(library.read())'), '3\n')

    def test_private_interface_type_cannot_appear_in_public_ref_signature(self):
        self.library(program('private interface Secret;','\tint get();','public int use(ref Secret n)=n.get()'))
        self.compile_source(program('import library','int main();','\treturn 0'),error='private',error_source='library.qy')

    def test_visibility_modifiers_cannot_be_repeated_or_attached_to_import_using(self):
        for declaration in ['public private int f()=1','private public const N=1','public import library','private using io']:
            self.library()
            self.compile_source(program(declaration,'int main();','\treturn 0'),error='公開指定')

    def test_main_cannot_be_private(self):
        self.compile_source(program('private int main();','\treturn 0'),error='private')
        self.check_program(program('public int main();','\treturn 0'), '')

    def test_private_primitive_wide_grapheme_unit_values_preserve_behavior(self):
        self.library(program('private const int_128 BIG=18446744073709551616','private const char LETTER="é"',
            'private const Void UNIT=()','public int_128 number()=BIG','public char letter()=LETTER','public Void unit()=UNIT'))
        self.check_program(program('import library','int main();','\tprint(library.number())',
            '\tprint(library.letter().length)','\tprint(library.unit())'), '18446744073709551616\n1\n()\n')

    def test_explicit_public_const_model_interface_and_function_can_be_selected(self):
        self.library()
        self.check_program(program('import library','using library.calculate, library.LABEL, library.Point, library.Shape',
            'int main();','\tPoint p','\tprint("{LABEL} {p.x} {calculate(2.0)}")'), 'ready 0.0 7.0\n')


if __name__=='__main__':run_suite()
