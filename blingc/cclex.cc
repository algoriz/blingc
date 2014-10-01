#include "cclex.h"

#include <fstream>
#include <algorithm>

using namespace std;

inline bool is_lower(char ch) {
    return ch >= 'a' && ch <= 'z';
}

inline bool is_upper(char ch) {
    return ch >= 'A' && ch <= 'Z';
}

inline bool is_alpha(char ch) {
    return is_lower(ch) || is_upper(ch);
}

inline bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

inline bool is_whitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

inline bool is_separator(char ch) {
    return !is_lower(ch) && !is_upper(ch) && !is_digit(ch) && ch != '_';
}

inline bool is_identifier(char ch) {
    return is_lower(ch) || is_upper(ch) || ch == '_';
}

char cc_keyword_set::_keywords[128][32] =
{
    "auto", "const", "double", "float", "int", "short", "struct", "unsigned",
    "break", "continue", "else", "for", "long", "signed", "switch", "void",
    "case", "default", "enum", "goto", "register", "sizeof", "typedef",
    "volatile", "char", "do", "extern", "if", "return", "static", "union",
    "while", "asm", "dynamic_cast", "namespace", "reinterpret_cast", "try",
    "bool", "explicit", "new", "static_cast", "typeid", "catch", "false",
    "operator", "template", "typename", "class", "friend", "private", "this",
    "using", "const_cast", "inline", "public", "throw", "virtual", "delete",
    "mutable", "protected", "true", "wchar_t", 0
};

cc_keyword_set::cc_keyword_set() {
    for (size_t i = 0; i < 128 && _keywords[i][0]; ++i) {
        insert(_keywords[i]);
    }
}

char cc_class_key_set::_class_key[3][8] = { "class", "struct", "union" };

cc_class_key_set::cc_class_key_set() {
    for (int i = 0; i < 3; ++i) {
        insert(_class_key[i]);
    }

    insert("namespace");
}

char cc_base_specifier_set::_base_specifier[4][16] = {
    "public", "protected", "private", 0
};

cc_base_specifier_set::cc_base_specifier_set() {
    for (size_t i = 0; i < 3; ++i) {
        insert(_base_specifier[i]);
    }
}

char cc_access_specifier_set::_access_specifier[2][16] = {
    "const", 0
};

cc_access_specifier_set::cc_access_specifier_set() {
    for (size_t i = 0; i < 1; ++i) {
        insert(_access_specifier[i]);
    }
}

///
///  
///
std::pair<char, char> cc_stream::angle_bracket('<', '>');
std::pair<char, char> cc_stream::round_bracket('(', ')');
std::pair<char, char> cc_stream::square_bracket('[', ']');
std::pair<char, char> cc_stream::brace('{', '}');

cc_stream::cc_stream() : _content(0), _length(0), _buff_size(0){}

cc_stream::cc_stream(const char* fileName)
    : _content(0), _length(0), _buff_size(0){
    open(fileName);
}

cc_stream::cc_stream(const cc_stream& rval)
    : _content(0), _length(rval._length), _buff_size(rval._buff_size) {
    if (_length) {
        _content = new char[_length];
        memcpy(_content, rval._content, _length);
    }
}

cc_stream::~cc_stream(){
    close();
}

bool cc_stream::open(const char* fileName){
    if (_content){ return false; }

    ifstream fs(fileName, ios::in | ios::binary);
    if (!fs.is_open() || fs.fail()) {
        return false;
    }

    fs.seekg(0, ios::end);
    _length = static_cast<size_t>(fs.tellg());
    _buff_size = _length + 2;
    fs.seekg(0, ios::beg);

    _content = new char[_buff_size];
    fs.read(_content, _length);

    //  Several additional characters are appended to the buffer so that we can
    // process the file without considering different file endings
    //
    if (_content[_length - 1] != '\n') {
        _content[_length++] = '\n';
    }
    _content[_length] = _content[_buff_size - 1] = 0;
    return true;
}

bool cc_stream::close() {
    delete[] _content;
    _content = 0;
    _buff_size = _length = 0;
    return true;
}

bool cc_stream::read_name(size_t& p, std::string& name) const {
    size_t i;
    size_t beg = -1, end = -1;

    for (i = p; i < _length; ++i) {
        if (is_identifier(_content[i])) {
            beg = i;
            end = i + 1;
            break;
        }
        else if (!is_whitespace(_content[i])) {
            return false;
        }
    }

    for (; i < _length; ++i) {
        if (is_separator(_content[i])) {
            end = i;
            break;
        }
    }

    if (beg != -1 && end != -1)
    {
        p = end - 1;
        name.assign(_content + beg, end - beg);
    }
    return name.length() != 0;
}

bool cc_stream::read_complete_name(size_t& p, string_vect& sv) const{
    size_t original_size = sv.size();

    size_t dfa_state = 0;
    size_t beg, end;
    string name;

    for (size_t i = p; i < _length; ++i) {
        switch (dfa_state) {
        case 0:
            if (is_identifier(_content[i])){
                beg = i;
                dfa_state = 1;
            }
            else if (_content[i] == ':') {
                dfa_state = 2;
            }
            else if (!is_whitespace(_content[i])){
                dfa_state = -1;
            }
            break;
        case 1:
            if (is_separator(_content[i])) {
                end = i;
                p = end - 1;
                name.assign(_content + beg, end - beg);
                sv.push_back(name);

                if (is_whitespace(_content[i])){
                    dfa_state = 5;
                }
                else if (_content[i] == ':'){
                    dfa_state = 2;
                }
                else { dfa_state = -1; };
            }
            break;
        case 2:
            dfa_state = (_content[i] == ':') ? 3 : -1;
            break;
        case 3:
            if (is_identifier(_content[i])){
                beg = i;
                dfa_state = 1;
            }
            else if (is_whitespace(_content[i])){
                dfa_state = 4;
            }
            else { dfa_state = -1; }
            break;
        case 4:
            if (is_identifier(_content[i])){
                beg = i;
                dfa_state = 1;
            }
            else if (!is_whitespace(_content[i])){
                dfa_state = -1;
            }
            break;
        case 5:
            if (_content[i] == ':'){
                dfa_state = 2;
            }
            else if (!is_whitespace(_content[i])){
                dfa_state = -1;
            }
            break;
        default:
            return sv.size() != 0;
        }
    }
    return sv.size() != original_size;
}

bool cc_stream::parse_identifier(cc_name_def_list& id_list, size_t begin, size_t end) const {
    if (end > _length) {
        end = _length;
    }

    if (end <= begin || begin >= _length){
        return false;
    }

    cc_reference id_ref;
    for (size_t i = begin; i < end; ++i){
        for (; i < end && !is_identifier(_content[i]); ++i);
        id_ref.begin = i;

        for (; i < end && !is_separator(_content[i]); ++i);
        id_ref.end = i;

        if (id_ref.end > id_ref.begin){
            id_list.push_back(cc_name_def(*this, id_ref));
        }
    }
    return true;
}

size_t cc_stream::erase(size_t beg, size_t end){
    if (end > _length){
        end = _length;
    }

    size_t i;
    for (i = beg; i < end; ++i){
        if (_content[i] != '\n'){
            _content[i] = ' ';
        }
    }
    return i - beg;
}

bool cc_stream::find_pair(
    cc_reference& pref, size_t begin, size_t end, const std::pair<char, char>& ptype) const{
    if (end > _length){ end = _length; }
    if (end <= begin){ return false; }

    int depth = 0;
    for (size_t i = begin; i < end; ++i){
        if (_content[i] == ptype.first){
            ++depth;
            pref.begin = pref.end = i;
            break;
        }
    }

    if (!depth){ return false; }

    for (size_t i = pref.begin + 1; i < end; ++i){
        if (_content[i] == ptype.first){
            ++depth;
        }
        else if (_content[i] == ptype.second){
            if (--depth == 0){
                pref.end = i + 1;
                return true;
            }
        }
    }
    return false;
}

///
///
///
bool cc_symbol_base::add_file(const string& srcfile){
    if (!srcfile.length()){
        return false;
    }

    _symbol_map[srcfile];
    return true;
}

bool cc_symbol_base::remove_file(const string& srcfile){
    return _symbol_map.erase(srcfile) != 0;
}

bool cc_symbol_base::reparse_file(const string& srcfile){
    cc_stream ccs;
    cc_symbol_map::iterator it = _symbol_map.find(srcfile);
    if (it == _symbol_map.end() || !ccs.open(srcfile.data())){
        return false;
    }

    bool result;
    it->second.clear();
    result = it->second.parse_stream(ccs);
    ccs.close();
    return result;
}

bool cc_symbol_base::build(){
    cc_symbol_map::iterator it = _symbol_map.begin();
    cc_symbol_map::iterator end = _symbol_map.end();

    cc_stream ccs;
    for (; it != end; ++it){
        if (!ccs.open(it->first.data())){
            _last_failed.push_back(it->first);
        }
        it->second.parse_stream(ccs);
        ccs.close();
    }
    return _last_failed.size() == 0;
}

bool cc_symbol_base::clean(){
    cc_symbol_map::iterator it = _symbol_map.begin();
    cc_symbol_map::iterator end = _symbol_map.end();

    for (; it != end; ++it){
        it->second.clear();
    }
    _last_failed.clear();
    return true;
}

void cc_symbol_base::drop(){
    _symbol_map.clear();
    _last_failed.clear();
}

bool cc_symbol_base::save_data(const char* dbfile) const{
    // not implemented yet
    return false;
}

bool cc_symbol_base::load_data(const char* dbfile){
    // not implemented yet
    return false;
}

const cc_symbol_index* cc_symbol_base::find_index(const string& srcfile) const{
    cc_symbol_map::const_iterator it = _symbol_map.find(srcfile);
    if (it != _symbol_map.end()){
        return &(it->second);
    }
    return 0;
}

///
///
///
cc_keyword_set   cc_symbol_index::_keywords;
cc_class_key_set cc_symbol_index::_class_key;
cc_base_specifier_set cc_symbol_index::_base_specifiers;
cc_access_specifier_set cc_symbol_index::_access_specifiers;

bool cc_symbol_index::parse_stream(const cc_stream& ccs){
    clear();
    cc_stream scontext(ccs);

    // Comments, strings, characters and preprocessors should
    //be processed first
    //
    _lex_comment(scontext);
    _lex_string(scontext);
    _lex_character(scontext);
    _lex_preprocessor(scontext);

    cc_name_def_list id_list;
    scontext.parse_identifier(id_list);

    // _resolve_xxx_ref methods will remove those identifiers that have been
    //resolved from the list, thus the calling order implies the priority of 
    //each type resolution.
    //
    _resolve_macro_ref(id_list);
    _resolve_keyword_ref(id_list);

    // Handle type definitions
    _lex_enumeration(scontext);
    _lex_class(scontext);

    // Resolve user type references
    _resolve_class_ref(id_list);
    _resolve_enum_ref(id_list);
    _resolve_constant_ref(id_list); // enum constant
    _resolve_external_type_ref(scontext, id_list);
    _resolve_external_scope_ref(scontext, id_list);

    _lex_method(ccs);
    for (cc_keyword_set::iterator it = _keywords.begin(),
         end = _keywords.end(); it != end; ++it){
        _method_ref_map.erase(*it);
    }
    return true;
}

void cc_symbol_index::_lex_comment(cc_stream& scontext){
    size_t dfa_state = 0;
    size_t dfa_escape_back_state = 0;
    size_t begin = -1;

    for (size_t i = 0; i < scontext.length(); ++i){
        char input_ch = scontext.content()[i];

        switch (dfa_state){
        case 0:
            switch (input_ch){
            case '/':
                begin = i;
                dfa_state = 1;
                break;
            case '\\':
                dfa_escape_back_state = dfa_state;
                dfa_state = 10;
                break;
            case '\"':
                dfa_state = 20;
                break;
            }
            break;
        case 1:
            switch (input_ch){
            case '/': dfa_state = 2; break;
            case '*': dfa_state = 3; break;
            default: dfa_state = 0;
            }
            break;
        case 2:
            switch (input_ch){
            case '\n':
                dfa_state = 0;
                _add_comment_def(scontext, begin, i);
                break;
            case '\\':
                dfa_escape_back_state = dfa_state;
                dfa_state = 10;
                break;
            }
            break;
        case 3:
            if (input_ch == '*'){
                dfa_state = 4;
            }
            break;
        case 4:
            if (input_ch == '/'){
                dfa_state = 0;
                _add_comment_def(scontext, begin, i + 1);
            }
            else if (input_ch != '*'){
                dfa_state = 3;
            }
            break;
        case 10:
            // CRLF
            if (input_ch == '\r'){
                ++i;
            }
            dfa_state = dfa_escape_back_state;
            break;
        case 20:
            switch (input_ch){
            case '\"':
                dfa_state = 0;
                break;
            case '\\':
                dfa_escape_back_state = dfa_state;
                dfa_state = 10;
                break;
            }
            break;
        }
    }
}

void cc_symbol_index::_lex_string(cc_stream& scontext){
    size_t dfa_state = 0;
    size_t dfa_escape_back_state = 0;
    size_t begin = -1;

    for (size_t i = 0; i < scontext.length(); ++i){
        char input_ch = scontext.content()[i];

        switch (dfa_state){
        case 0:
            if (input_ch == '\"'){
                dfa_state = 1;
                begin = i;
            }
            else if (input_ch == '\\'){
                dfa_escape_back_state = dfa_state;
                dfa_state = 10;
            }
            break;
        case 1:
            if (input_ch == '\"'){
                dfa_state = 0;
                _add_string_def(scontext, begin, i + 1);
                begin = -1;
            }
            else if (input_ch == '\\'){
                dfa_escape_back_state = dfa_state;
                dfa_state = 10;
            }
            break;
        case 10:
            if (input_ch == '\r'){
                ++i;
            }
            dfa_state = dfa_escape_back_state;
            break;
        }
    }
}

void cc_symbol_index::_lex_character(cc_stream& scontext){
    size_t dfa_state = 0;
    size_t begin = 0;

    for (size_t i = 0; i < scontext.length(); ++i){
        char input_ch = scontext.content()[i];

        switch (dfa_state){
        case 0:
            if (input_ch == '\''){
                dfa_state = 1;
                begin = i;
            }
            break;
        case 1:
            if (input_ch == '\''){
                dfa_state = 0;
                _add_character_def(scontext, begin, i + 1);
            }
            else if (input_ch == '\\'){
                dfa_state = 2;
            }
            break;
        case 2:
            dfa_state = 1;
            break;
        }
    }
}

void cc_symbol_index::_lex_method(const cc_stream& ccs){
    size_t dfa_state = 0;
    size_t begin = -1, end = -1;
    string mname;

    for (size_t i = 0; i < ccs.length(); ++i){
        char input_ch = ccs.content()[i];

        switch (dfa_state){
        case 0:
            if (is_identifier(input_ch)){
                dfa_state = 1;
                begin = i;
            }
            break;
        case 1:
            if (input_ch == '('){
                dfa_state = 0;
                end = i;

                mname.assign(ccs.content() + begin, end - begin);
                _method_ref_map[mname].insert(cc_reference(begin, end));
            }
            else if (is_whitespace(input_ch)){
                dfa_state = 2;
                end = i;
            }
            else if (is_separator(input_ch)){
                dfa_state = 0;
            }
            break;
        case 2:
            if (input_ch == '('){
                dfa_state = 0;

                mname.assign(ccs.content() + begin, end - begin);
                _method_ref_map[mname].insert(cc_reference(begin, end));
            }
            else if (is_identifier(input_ch)){
                dfa_state = 1;
                begin = i;
            }
            else if (!is_whitespace(input_ch)){
                dfa_state = 0;
            }
            break;
        }
    }
}

void cc_symbol_index::_lex_enumeration(const cc_stream& ccs){
    size_t dfa_state = 1;
    size_t begin = -1;

    string val;
    cc_enum_def empty_enum;

    for (size_t i = 0; i < ccs.length(); ++i){
        char input_ch = ccs.content()[i];

        switch (dfa_state){
        case 0:
            if (is_separator(input_ch)){
                dfa_state = 1;
            }
            break;
        case 1:
            if (!is_separator(input_ch)){
                if (ccs.compare_at(i, "enum", 4) == 0){
                    dfa_state = 5;
                    i += 3;

                    _enum_def_list.push_back(empty_enum);
                    _enum_def_list.rbegin()->set_name(i);
                }
                else { dfa_state = 0; }
            }
            break;
        case 5:
            if (input_ch == '{') {
                dfa_state = 7;
            }
            else if (is_whitespace(input_ch)) {
                dfa_state = 6;
            }
            else {
                dfa_state = 0;
            }
            break;
        case 6:
            switch (input_ch) {
            case ';': dfa_state = 1; break;
            case '{':
                dfa_state = ccs.find_pair(_enum_def_list.rbegin()->body_ref, i) ? 7 : 1;
                break;
            default:
                if (ccs.read_name(i, _enum_def_list.rbegin()->name)) {
                    _enum_def_list.rbegin()->set_name_ref_end(i);
                }
            }
            break;
        case 7:
            if (input_ch == '}') {
                _enum_def_list.rbegin()->body_ref.end = i;
                dfa_state = 1;
            }
            else if (is_identifier(input_ch)) {
                dfa_state = 8; begin = i;
            }
            break;
        case 8:
            if (is_separator(input_ch)) {
                val.assign(ccs.content(), begin, i - begin);
                _enum_def_list.rbegin()->add_value(val, begin);

                switch (input_ch){
                case ',': dfa_state = 7; break;
                case '}': dfa_state = 1; break;
                default: dfa_state = 9;
                }
            }
            break;
        case 9:
            switch (input_ch) {
            case ',': dfa_state = 7; break;
            case '}': dfa_state = 1; break;
            }
            break;
        }
    }
}

/// Summary
///  Parse class declarations and definitions
///  This method uses _keyword_ref_map to find class declarations and definitions,
///thus _lex_keyword should be called prior to this method to initialize _keyword_ref_map
///
void cc_symbol_index::_lex_class(const cc_stream& ccs){
    const char* data = ccs.content();

    string_vect  cname;
    cc_class_def class_def;

    cc_class_key_set::const_iterator key_type;
    for (key_type = _class_key.begin(); key_type != _class_key.end(); ++key_type){
        class_def.key_name = *key_type;

        // Find class_key references
        cc_reference_map::const_iterator key_ref_set = _keyword_ref_map.find(*key_type);
        if (key_ref_set != _keyword_ref_map.end()){
            // Traverse references of keyword
            cc_reference_set::const_iterator key_ref;
            for (key_ref = key_ref_set->second.begin();
                 key_ref != key_ref_set->second.end(); ++key_ref){
                // Class is defined or declared right after the keyword
                for (size_t i = key_ref->end; i < ccs.length(); ++i){
                    // Look for where the class header ends
                    if (data[i] == ';' || data[i] == '{') {
                        size_t pos = key_ref->end;

                        // Give the class a default name by the position where
                        //it's defined or declared
                        class_def.set_name(pos);

                        // If the class got a name, read it
                        if (ccs.read_complete_name(pos, cname)){
                            class_def.name = *cname.rbegin();
                            cname.pop_back();
                            class_def.nested_name.swap(cname);
                            class_def.set_name_ref_end(pos);
                        }
                        else if (data[i] == ';'){
                            // An unnamed class declaration !?
                            break;
                        }

                        // If the class is defined, find out its member declaration
                        if (data[i] == '{'){
                            ccs.find_pair(class_def.body_ref, i);
                        }

                        _class_def_list.push_back(class_def);
                        break;
                    }
                }
            }
        }
    }
}

void cc_symbol_index::_lex_preprocessor(cc_stream& scontext){
    cc_preprocessor_def preprocessor_def;

    size_t dfa_state = 1;
    size_t start_pos = -1;
    for (size_t i = 0; i < scontext.length(); ++i){
        char input_ch = scontext.content()[i];

        switch (dfa_state){
        case 0:
            if (input_ch == '\\'){
                dfa_state = 2;
            }
            else if (input_ch == '\n'){
                if (start_pos != -1){
                    _add_preprocessor_def(scontext, start_pos, i);
                    start_pos = -1;
                }
                dfa_state = 1;
            }
            break;
        case 1:
            if (input_ch == '#'){
                start_pos = i;
                dfa_state = 0;
            }
            else if (!is_whitespace(input_ch)){
                dfa_state = 0;
            }
            break;
        case 2:
            if (input_ch == '\r'){
                ++i;
            }
            dfa_state = 0;
            break;
        }
    }
}

void cc_symbol_index::_lex_include(cc_preprocessor_def& pdef, cc_stream& scontext){
    if (strncmp(pdef.name.data(), "include", 7)){
        return;
    }

    cc_reference pair_pos;
    if (scontext.find_pair(
        pair_pos, pdef.name_ref.end, pdef.line_ref.end, make_pair('<', '>'))){
        _include_def_list.push_back(cc_name_def(scontext, pair_pos));
        _add_string_def(scontext, pair_pos.begin, pair_pos.end);
    }
    else{
        cc_name_def_list::const_iterator str_ref = _string_def_list.begin();
        for (; str_ref != _string_def_list.end(); ++str_ref){
            if (str_ref->name_ref.begin >= pdef.name_ref.end
                && str_ref->name_ref.end <= pdef.line_ref.end){
                _include_def_list.push_back(*str_ref);
            }
            else if (str_ref->name_ref.begin >= pdef.name_ref.end){
                break;
            }
        }
    }
}

void cc_symbol_index::_resolve_keyword_ref(cc_name_def_list& id_list){
    for (cc_name_def_list::iterator id = id_list.begin(); id != id_list.end();){
        if (_keywords.count(id->name)){
            _keyword_ref_map[id->name].insert(id->name_ref);
            id = id_list.erase(id);
        }
        else { ++id; }
    }
}

void cc_symbol_index::_resolve_constant_ref(cc_name_def_list& id_list){
    set<string> constants;
    for (cc_enum_def_list::const_iterator enum_def = _enum_def_list.begin();
         enum_def != _enum_def_list.end(); ++enum_def){
        for (cc_name_def_list::const_iterator ev = enum_def->value_def_list.begin();
             ev != enum_def->value_def_list.end(); ++ev){
            constants.insert(ev->name);
        }
    }

    for (cc_name_def_list::iterator id = id_list.begin(); id != id_list.end();){
        if (constants.count(id->name)){
            _constant_ref_map[id->name].insert(id->name_ref);
            id = id_list.erase(id);
        }
        else { ++id; }
    }
}

void cc_symbol_index::_resolve_class_ref(cc_name_def_list& id_list){
    return __resolve_type_ref(_class_def_list, id_list, _class_ref_map);
}

void cc_symbol_index::_resolve_enum_ref(cc_name_def_list& id_list){
    return __resolve_type_ref(_enum_def_list, id_list, _enum_ref_map);
}

void cc_symbol_index::_resolve_macro_ref(cc_name_def_list& id_list){
    return __resolve_type_ref(_macro_def_list, id_list, _macro_ref_map);
}

void cc_symbol_index::_resolve_external_scope_ref(cc_stream& scontext, cc_name_def_list& id_list){
    const char* content = scontext.content();
    size_t max_pos = scontext.length() - 2;

    cc_name_def_list::iterator it = id_list.begin();
    for (; it != id_list.end(); ++it){
        size_t i = it->name_ref.end;
        while (i < max_pos && is_whitespace(content[i])){
            ++i;
        }

        // Code like enum_type::enum_value is obsolete
        // Thus _enum_ref_map.count(it->name) is not checked
        //
        if (i < max_pos && content[i] == ':' && content[i + 1] == ':'
            && !_class_ref_map.count(it->name) && !_macro_ref_map.count(it->name)){
            _external_scope_ref_map[it->name].insert(it->name_ref);
            it = id_list.erase(it);
        }
    }
}

void cc_symbol_index::_resolve_external_type_ref(cc_stream& scontext, cc_name_def_list& id_list)
{
    const char* content = scontext.content();
    if (id_list.size() < 2){
        return;
    }

    cc_name_def_list::iterator it = id_list.begin();
    cc_name_def_list::iterator next = it;
    for (++next; it != id_list.end() && next != id_list.end(); it = next++){
        bool check_external_type = true;
        for (size_t i = it->name_ref.end; i < next->name_ref.begin; ++i){
            if (!is_whitespace(content[i])){
                check_external_type = false;
                break;
            }
        }
        if (!check_external_type){ continue; }

        if (!_keywords.count(it->name)){
            _external_type_ref_map[it->name].insert(it->name_ref);
            next = id_list.erase(it);
        }
        else if (!_keywords.count(next->name) && !_class_ref_map.count(next->name)
                 && (_base_specifiers.count(it->name) || _access_specifiers.count(it->name))){
            _external_type_ref_map[next->name].insert(next->name_ref);
            next = id_list.erase(next);
        }

        if (next == id_list.end()){ break; }
    }

    cc_reference_map::iterator ref_set;
    for (it = id_list.begin(); it != id_list.end(); ++it){
        ref_set = _external_type_ref_map.find(it->name);
        if (ref_set != _external_type_ref_map.end()){
            ref_set->second.insert(it->name_ref);
        }
    }
}

void cc_symbol_index::_add_string_def(cc_stream& scontext, size_t begin, size_t end){
    _string_def_list.push_back(cc_name_def(scontext, begin, end));
    scontext.erase(begin, end);
}

void cc_symbol_index::_add_comment_def(cc_stream& scontext, size_t begin, size_t end){
    _comment_def_list.push_back(cc_name_def(scontext, begin, end));
    scontext.erase(begin, end);
}

void cc_symbol_index::_add_character_def(cc_stream& scontext, size_t begin, size_t end){
    _character_def_list.push_back(cc_name_def(scontext, begin, end));
    scontext.erase(begin, end);
}

void cc_symbol_index::_add_preprocessor_def(cc_stream& scontext, size_t begin, size_t end){
    cc_preprocessor_def pdef;
    pdef.line_ref.begin = begin;
    pdef.line_ref.end = end;

    size_t p = begin + 1;
    if (scontext.read_name(p, pdef.name)){
        pdef.set_name_ref_end(p + 1);
    }
    _preprocessor_def_list.push_back(pdef);
    scontext.erase(begin, end);
}

void cc_symbol_index::clear(){
    _comment_def_list.clear();
    _string_def_list.clear();
    _character_def_list.clear();
    _include_def_list.clear();

    _enum_def_list.clear();
    _class_def_list.clear();
    _preprocessor_def_list.clear();
    _macro_def_list.clear();

    _enum_ref_map.clear();
    _constant_ref_map.clear();
    _class_ref_map.clear();
    _macro_ref_map.clear();
    _keyword_ref_map.clear();
    _method_ref_map.clear();

    _external_scope_ref_map.clear();
    _external_type_ref_map.clear();
}
