"""Nominal enums, contextual names, checked conversions and exhaustive match."""
from pathlib import Path
import subprocess
from support import QynoTestCase, program, run_suite

SEX=program('enum sex;','\tundetermined','\tmale','\tfemale=3')


class EnumTests(QynoTestCase):
    def main(self,*lines,prefix=SEX):
        return prefix+program('int main();',*('\t'+line for line in lines),'\treturn 0')

    def test_requested_users_example(self):
        source=SEX+program('using io','model User;','\tstr name','\tint age','\tsex gender',
            '\treadonly void user_info();','\t\tprint("=== USER INFO ===")',
            '\t\tprint("{this.name}\'s age: {this.age}")','\t\tprint("{this.name}\'s gender: {this.gender}")',
            'int main();','\tUser users[2]','\tusers[0].name="Ken"','\tusers[0].age=23',
            '\tusers[1].name="Allice"','\tusers[1].age=20','\tusers[1].gender=female',
            '\tusers[0].user_info()','\tusers[1].user_info()')
        self.check_program(source,"=== USER INFO ===\nKen's age: 23\nKen's gender: undetermined\n=== USER INFO ===\nAllice's age: 20\nAllice's gender: female\n")

    def test_implicit_sequence_and_numeric_assignment(self):
        prefix=program('enum sex;','\tundetermined','\tmale','\tfemale')
        self.check_program(self.main('sex a=female','sex b=2','io.print("{a} {b} {int(a)}")',prefix=prefix),'female female 2\n')
        self.compile_source(self.main('sex a=2'),error='宣言されていない')
        self.check_program(self.main('sex a=3','io.print(a)'),'female\n')

    def test_named_type_without_typedef_and_zero_defaults(self):
        self.check_program(self.main('sex a','sex values[3]=[female]','values.add()',
            'io.print(a)','io.print(values)'),'',error='最大要素数',status=70)
        self.check_program(self.main('sex a','sex values[]','values.add()',
            'io.print(a)','io.print(values)'),'undetermined\n[undetermined]\n')

    def test_negative_sparse_codes_and_zero_not_first(self):
        prefix=program('enum Code;','\terror=-3','\tnext','\tok=0','\tlast=2147483647')
        self.check_program(self.main('Code c','io.print(c)','io.print(Code.next)',
            'io.print(int(Code.last))','io.print(int(Code.error))',prefix=prefix),'ok\nnext\n2147483647\n-3\n')

    def test_invalid_declarations(self):
        for declaration,message in [
                ('enum E;\n\tx=1\n','値0'),('enum E;\n\tx\n\ty=0\n','重複'),
                ('enum E;\n\tx\n\tx=1\n','重複'),('enum E;\n\tx\n\ty=2147483648\n','範囲'),
                ('enum E;\n\tx\n\ty=2147483647\n\tz\n','範囲'),('enum E;\n\tx\n\ty=-2147483649\n','範囲'),
                ('enum E;\n\tx\n\ty=1.0\n','整数定数'),('enum E;\n\tx\n\ty=true\n','整数定数'),
                ('enum E;\n\tprivate x\n','予約語')]:
            with self.subTest(declaration=declaration):self.compile_source(self.main(prefix=declaration),error=message)
        self.compile_source(self.main(prefix='enum E;\n'),error='インデント')

    def test_enum_values_use_const_expressions_and_prior_members(self):
        prefix=program('const int STEP=2','enum E;','\tzero','\tfirst=STEP','\tsecond=int(first)+STEP',
            'const E VALUE=E.second','const int NUMBER=int(VALUE)')
        self.check_program(self.main('io.print("{VALUE} {NUMBER}")',prefix=prefix),'second 4\n')

    def test_forward_member_and_const_dependency_cycles(self):
        self.compile_source(self.main(prefix=program('enum E;','\tzero','\tfirst=int(E.later)','\tlater=2')),error='前方参照')
        self.compile_source(self.main(prefix=program('const int N=int(E.one)','enum E;','\tzero','\tone=N')),error='循環')
        self.compile_source(self.main(prefix=program('const int N=int(one)','enum E;','\tzero','\tone=N')),error='宣言')

    def test_global_names_take_priority_over_contextual_members(self):
        prefix=program('const int female=1')+SEX
        self.check_program(self.main('sex a=female','sex b=sex.female','io.print("{a} {b}")',prefix=prefix),'male female\n')
        self.compile_source(self.main('int female=3','sex a=female'),error='変換')

    def test_members_do_not_pollute_global_names_and_auto_needs_a_type(self):
        self.check_program(self.main('auto a=sex.female','b=sex.male','io.print("{a} {b}")'),'female male\n')
        self.compile_source(self.main('auto a=female'),error='宣言')
        self.compile_source(self.main('int a=female'),error='宣言')

    def test_different_enums_are_distinct(self):
        prefix=SEX+program('enum Other;','\tzero','\tone')
        for use in ['Other a=sex.male','sex a=Other.one','io.print(sex.male==Other.one)',
                    'Other a=Other(sex.male)']:
            self.compile_source(self.main(use,prefix=prefix),error='enum' if 'Other(' in use or '==' in use else '変換')
        self.check_program(self.main('Other a=Other(int(sex.male))','io.print(a)',prefix=prefix),'one\n')

    def test_equality_and_symmetric_contextual_names(self):
        self.check_program(self.main('sex a=female','io.print("{a==female} {female==a} {a!=male} {3==a} {a==3}")'),
            'true true true true true\n')
        self.compile_source(self.main('sex a=female','io.print(a==2)'),error='宣言されていない')
        self.compile_source(self.main('sex a=female','io.print(a<male)'),error='enum')

    def test_enum_arithmetic_logic_and_for_are_rejected(self):
        for expression in ['sex.male+1','sex.male-1','sex.male and sex.female','sex.male << 1','bnot sex.male','not sex.male','math.abs(sex.male)']:
            with self.subTest(expression=expression):self.compile_source(self.main('io.print('+expression+')'),error='エラー')
        self.compile_source(self.main('sex a=male','a++'),error='エラー')
        self.compile_source(self.main('for(undetermined <= sex a < female,a++);','\tio.print(a)'),error='エラー')

    def test_checked_dynamic_integer_conversion(self):
        self.check_program(self.main('int code=3','sex a=sex(code)','io.print(a)'),'female\n')
        self.check_program(self.main('int code=2','sex a=sex(code)','io.print(a)'),'',status=70,error='enum')

    def test_wide_and_unsigned_conversions_never_truncate(self):
        for decl in ['int_128 n=4294967296','unsigned int_64 n=18446744073709551615','int_64 n=4294967296']:
            self.check_program(self.main(decl,'sex a=sex(n)','io.print(a)'),'',status=70,error='範囲')
        self.check_program(self.main('int_128 n=3','sex a=sex(n)','io.print(a)','io.print(unsigned int_128(a))'),'female\n3\n')

    def test_enum_to_integer_checks_destination_range(self):
        prefix=program('enum E;','\tzero','\tlarge=256','\tnegative=-1')
        self.check_program(self.main('E a=large','io.print(int_64(a))','io.print(int_128(a))',prefix=prefix),'256\n256\n')
        self.check_program(self.main('E a=large','io.print(int_8(a))',prefix=prefix),'',status=70,error='範囲')
        self.check_program(self.main('E a=negative','io.print(unsigned int(a))',prefix=prefix),'',status=70,error='範囲')

    def test_const_conversions_display_and_lib(self):
        prefix=SEX+program('const sex VALUE=female','const int N=int(VALUE)','const str NAME=str(VALUE)',
            'const int_64 BITS=lib.size(VALUE)','const sex VALUES[3]=[male,female]',
            'const int_64 TOTAL=lib.size(VALUES)')
        self.check_program(self.main('io.print("{VALUE} {N} {NAME} {BITS} {TOTAL}")',
            'io.print(VALUES)','io.print("{int(VALUE):0x}")',prefix=prefix),'female 3 female 32 96\n[male, female, undetermined]\n0x3\n')
        self.compile_source(self.main('io.print(lib.length(sex.female))'),error='lib.length')
        self.compile_source(self.main('io.print("{sex.female:0x}")'),error='基数')

    def test_explicit_text_conversion_and_prefixes(self):
        for text in ['female','  female\t','3','0x3','0b11','0o3','0d3']:
            literal=text.replace('\t','\\t')
            self.check_program(self.main(f'str text="{literal}"','sex a=sex(text)','io.print(a)'),'female\n')
        for text in ['Female','sex.female','2','4294967296','femaleX','']:
            self.check_program(self.main(f'str text="{text}"','sex a=sex(text)','io.print(a)'),'',status=70,error='enum')
        self.compile_source(self.main('const sex BAD=sex("2")'),error='enum')

    def test_arrays_models_defaults_copy_add_and_equality(self):
        prefix=SEX+program('model Box;','\tsex values[2]','\tsex gender=female')
        self.check_program(self.main('Box a','Box b=a','b.values[0]=male','sex grid[][]=[[male],[female]]',
            'grid[0].add()','io.print(a)','io.print(b)','io.print(grid)',
            'io.print("{a==b} {grid==grid} {lib.length(grid)} {lib.size(grid)}")',prefix=prefix),
            'Box{values=[undetermined, undetermined], gender=female}\nBox{values=[male, undetermined], gender=female}\n[[male, undetermined], [female]]\nfalse true 2 96\n')

    def test_arguments_returns_ref_and_readonly(self):
        prefix=SEX+program('sex choose(sex value)=value','void change(ref sex value);','\tvalue=female',
            'sex observe(ref readonly sex value)=value')
        self.check_program(self.main('sex a','change(ref a)','io.print(choose(male))','io.print(observe(ref a))',prefix=prefix),
            'male\nfemale\n')
        self.compile_source(self.main('int n','change(ref n)',prefix=prefix),error='ref')
        self.compile_source(self.main('change(ref sex.male)',prefix=prefix),error='const')

    def test_generic_and_inferred_returns_preserve_nominal_types(self):
        prefix=SEX+program('identity(value)=value','choose(bool flag);','\tif(flag);','\t\treturn sex.male','\treturn sex.female')
        self.check_program(self.main('auto a=identity(choose(true))','io.print(a)','io.print(lib.size(a))',prefix=prefix),'male\n32\n')

    def test_direct_and_contextual_input(self):
        self.check_program(self.main('sex a','io.print(io.input(a))','io.print(a)',
            'sex b=io.input()','io.print(b)'),'true\nfemale\nmale\n',stdin='female\n1\n')
        self.check_program(self.main('sex a','io.input(a)','io.print(a)'),'',stdin='2\n',status=65,error='sex')

    def test_pattern_input_is_atomic_and_accepts_numeric_codes(self):
        self.check_program(self.main('sex a=male','int n=7','io.print(io.input("{a},{n}"))','io.print("{a} {n}")',
            'io.print(io.input("{a},{n}"))','io.print("{a} {n}")'),
            'false\nmale 7\ntrue\nfemale 42\n',stdin='2,99\n0x3,42\n')
        self.check_program(self.main('sex a=male','io.print(io.input(a))','io.print(a)'), 'false\nmale\n',stdin='Female\n')

    def test_array_and_model_input_targets(self):
        prefix=SEX+program('model Box;','\tsex gender')
        self.check_program(self.main('Box b','sex data[2]','io.input(data[1])','io.input("{b.gender}")',
            'io.print(data)','io.print(b)',prefix=prefix),'[undetermined, female]\nBox{gender=male}\n',stdin='3\nmale\n')

    def test_file_write_uses_name_and_byte_write_rejects_enum_array(self):
        target=self.directory/'text.txt'
        self.check_program(self.main(f'file f=io.file.open("{target}","w")','sex a=female',
            'io.file.write(f,"{a}")','io.file.close(f)'),'')
        self.assertEqual(target.read_bytes(),b'female')
        self.compile_source(self.main('file f','sex data[]=[male]','io.file.write_bytes(f,data)'),error='unsigned int_8')

    def test_enum_type_and_member_name_collisions(self):
        for declaration in ['enum sex;\n\tzero\n','model sex;\n\tint n\n','interface sex;\n\tint n();\n','int sex()=1\n','const int sex=1\n']:
            self.compile_source(self.main(prefix=SEX+declaration),error='enum')

    def test_module_qualified_type_member_and_using(self):
        (self.directory/'profile.qy').write_text(SEX)
        prefix=program('import profile','using profile.sex','using profile.sex.female')
        self.check_program(self.main('sex a=male','auto b=female','io.print("{a} {b} {profile.sex.undetermined}")',prefix=prefix),
            'male female undetermined\n')
        self.check_program(self.main('p.sex a=female','io.print(p.sex(a))',prefix='import profile\nusing profile as p\n'),'female\n')
        self.check_program(self.main('sex a=sex.female','io.print(a)',prefix='import profile\nusing profile\n'),'female\n')

    def test_private_enum_and_public_api_validation(self):
        (self.directory/'profile.qy').write_text(SEX.replace('enum sex','private enum sex')+'public int code()=int(sex.male)\n')
        self.compile_source(self.main('profile.sex a',prefix='import profile\n'),error='private')
        self.compile_source(self.main('io.print(profile.sex.male)',prefix='import profile\n'),error='private')
        self.check_program(self.main('io.print(profile.code())',prefix='import profile\n'),'1\n')
        for exported in ['sex read()=male\n','const sex VALUE=sex.male\n','model Box;\n\tsex gender\n']:
            self.compile_source(self.main(prefix=SEX.replace('enum sex','private enum sex')+exported),error='private')

    def test_selective_member_using_collisions_and_no_auto_member_exports(self):
        (self.directory/'a.qy').write_text(SEX)
        (self.directory/'b.qy').write_text(SEX)
        self.compile_source(self.main(prefix='import a,b\nusing a.sex.male,b.sex.male\n'),error='衝突')
        self.compile_source(self.main('auto x=male',prefix='import a\nusing a\n'),error='宣言')
        self.check_program(self.main('auto x=male','io.print(x)',prefix='import a\nusing a.sex.male,a.sex.male\n'),'male\n')

    def test_match_exhaustive_grouped_and_else(self):
        self.check_program(self.main('sex a=female','match(a);','\tcase undetermined;','\t\tio.print("zero")',
            '\tcase male,female;','\t\tio.print("set")','match(a);','\tcase male;','\t\tio.print("male")',
            '\telse;','\t\tio.print("other")'),'set\nother\n')
        self.check_program(self.main('match(sex.male);','\telse;','\t\tio.print("all")'),'all\n')

    def test_match_missing_duplicate_wrong_type_and_nonconstant_cases(self):
        for arms,error in [(['case male;','\tio.print(1)'],'case'),
                (['case male,1;','\tio.print(1)','else;','\tio.print(2)'],'重複'),
                (['case 2;','\tio.print(1)','else;','\tio.print(2)'],'宣言されていない'),
                (['case Other.one;','\tio.print(1)','else;','\tio.print(2)'],'変換'),
                (['case a;','\tio.print(1)','else;','\tio.print(2)'],'定数')]:
            self.compile_source(self.main('sex a=male','match(a);',*('\t'+line for line in arms),
                prefix=SEX+program('enum Other;','\tzero','\tone')),error=error)
        self.compile_source(self.main('match(1);','\telse;','\t\tio.print(1)'),error='enum')

    def test_match_expression_evaluated_once_and_case_scope(self):
        prefix=SEX+program('sex next(ref int n);','\tn++','\treturn female')
        self.check_program(self.main('int n','match(next(ref n));','\tcase female;','\t\tint local=1','\t\tio.print(local)',
            '\telse;','\t\tint local=2','\t\tio.print(local)','io.print(n)',prefix=prefix),'1\n1\n')
        self.compile_source(self.main('match(sex.male);','\telse;','\t\tint local=1','io.print(local)'),error='宣言')

    def test_match_return_inference_and_generic_specialization(self):
        prefix=SEX+program('name(value);','\tmatch(value);','\t\tcase undetermined;','\t\t\treturn "zero"',
            '\t\tcase male;','\t\t\treturn "male"','\t\tcase female;','\t\t\treturn "female"')
        self.check_program(self.main('io.print(name(sex.female))',prefix=prefix),'female\n')

    def test_match_break_continue_target_enclosing_loop(self):
        self.check_program(self.main('sex values[]=[undetermined,male,female]','int total','for(0 <= i < 3,i++);',
            '\tmatch(values[i]);','\t\tcase undetermined;','\t\t\tcontinue',
            '\t\tcase male;','\t\t\ttotal+=1','\t\tcase female;','\t\t\tbreak','\ttotal+=10','io.print(total)'), '11\n')

    def test_match_all_paths_close_file(self):
        target=self.directory/'file'
        self.check_program(self.main(f'file f=io.file.open("{target}","w")','sex a=female','match(a);',
            '\tcase male,undetermined;','\t\tio.file.close(f)','\tcase female;','\t\tio.file.close(f)'),'')
        self.compile_source(self.main('file f=io.file.open("in")','sex a=female','match(a);',
            '\tcase male,undetermined;','\t\tio.file.close(f)','\tcase female;','\t\tio.print(a)'),error='close')

    def test_match_initializes_required_model_fields_on_every_branch(self):
        prefix=SEX+program('model Value;','\tint n','\tvoid init(int n);','\t\tthis.n=n',
            'model Holder;','\tValue item','\tvoid init(sex gender);','\t\tmatch(gender);',
            '\t\t\tcase undetermined,male;','\t\t\t\tthis.item={n=1}',
            '\t\t\tcase female;','\t\t\t\tthis.item={n=3}')
        self.check_program(self.main('Holder h={gender=female}','io.print(h.item.n)',prefix=prefix),'3\n')
        incomplete=prefix.replace('this.item={n=3}','io.print(gender)')
        self.compile_source(self.main(prefix=incomplete),error='初期化')

    def test_model_initializers_and_const_models_keep_enum_values(self):
        prefix=SEX+program('model Record;','\tsex gender=male','const Record FIRST={gender=female}',
            'model Choice;','\tprivate sex gender',
            '\tvoid init(sex value=female);','\t\tthis.gender=value',
            '\treadonly sex get()=this.gender')
        self.check_program(self.main('Choice c={}','io.print(c.get())','io.print(FIRST)',prefix=prefix),
            'female\nRecord{gender=female}\n')

    def test_local_using_enum_member_does_not_escape_scope(self):
        self.check_program(self.main('if(true);','\tusing sex.male','\tauto a=male','\tio.print(a)'),'male\n')
        self.compile_source(self.main('if(true);','\tusing sex.male','\tauto a=male','auto b=male'),error='宣言')

    def test_readonly_enum_const_ref_and_ref_aliases(self):
        prefix=SEX+program('const sex VALUE=female','sex observe(ref readonly sex value)=value')
        self.check_program(self.main('sex a=male','ref sex alias=a.addr','alias=female',
            'io.print(a)','io.print(observe(ref VALUE))',prefix=prefix),'female\nfemale\n')
        self.compile_source(self.main(prefix=SEX+program('void bad(ref readonly sex value);','\tvalue=male')),error='readonly')

    def test_match_else_order_and_nested_scopes(self):
        self.compile_source(self.main('match(sex.male);','\telse;','\t\tio.print(1)',
            '\tcase male;','\t\tio.print(2)'),error='最後')
        self.check_program(self.main('match(sex.female);','\tcase male;','\t\tio.print(0)',
            '\telse;','\t\tmatch(sex.male);','\t\t\tcase male;','\t\t\t\tio.print(1)',
            '\t\t\telse;','\t\t\t\tio.print(2)'),'1\n')

    def test_match_all_member_cases_and_redundant_else(self):
        self.check_program(self.main('match(sex.male);','\tcase 0,1,3;','\t\tio.print("known")',
            '\telse;','\t\tio.print("fallback")'),'known\n')

    def test_invalid_enum_constructor_arguments(self):
        for use in ['sex()','sex(1,2)','sex(true)','sex(1.0)','sex(ref value)']:
            self.compile_source(self.main('sex value','io.print('+use+')'),error='エラー')

    def test_member_prefixes_and_signed_input(self):
        prefix=program('enum Code;','\tzero','\tx','\txx','\tnegative=-2147483648')
        self.check_program(self.main('Code a=io.input()','Code b=io.input()',
            'io.print("{a} {b} {int(b)}")',prefix=prefix),'xx negative -2147483648\n',stdin='xx\n-2147483648\n')

    def test_enum_conversion_keyword_names_are_validated(self):
        self.check_program(self.main('sex a=sex(3)','io.print(str(a))','io.print(int(a))'),'female\n3\n')
        for expression in ['str(base=sex.male)','int(base=sex.male)']:
            self.compile_source(self.main('io.print('+expression+')'),error='名前付き引数')
        self.compile_source(self.main('io.print(sex(value=3))'),error='必要')

    def test_nested_enum_definitions_are_rejected(self):
        self.compile_source(self.main('enum Other;','\tzero'),error='エラー')
        self.compile_source(self.main(prefix='model Container;\n\tenum Other;\n\t\tzero\n'),error='エラー')

    def test_method_parentheses_are_still_required(self):
        self.compile_source(self.main('Box b','b.show',prefix=SEX+'model Box;\n\tvoid show();\n\t\treturn\n'),error='エラー')


if __name__=='__main__':
    run_suite()
