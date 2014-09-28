#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <list>

class  cc_stream;
struct cc_reference;
struct cc_name_def;
typedef std::list<cc_name_def> cc_name_def_list;

typedef std::vector<std::string>    string_vect;
typedef std::set<std::string>       string_set;

struct cc_keyword_set: public string_set{
    cc_keyword_set();

private:
    static char _keywords[128][32];
};

struct cc_class_key_set: public string_set{
    cc_class_key_set();

private:
    static char _class_key[3][8];
};

struct cc_base_specifier_set: public string_set{
    cc_base_specifier_set();

private:
    static char _base_specifier[4][16];
};

struct cc_access_specifier_set: public string_set{
    cc_access_specifier_set();

private:
    static char _access_specifier[2][16];
};
/// Summary
///  Structure that marks a reference
///
struct cc_reference{
    struct less {
        bool operator()(const cc_reference& l, const cc_reference& r){
            return l.end <= r.begin;
        }
    };

    cc_reference(): begin(0), end(0) {}

    cc_reference(size_t b, size_t e): begin(b), end(e){}

    size_t length() const { return end - begin; }

    size_t begin;
    size_t end;     // end character is not included
};

typedef std::list<cc_reference>                     cc_reference_list;
typedef std::set<cc_reference, cc_reference::less>  cc_reference_set;
typedef std::map<std::string, cc_reference_set>     cc_reference_map;

/// Summary
///  C++ source stream, an encapsulation of C++ source buffer
///  Support source files encoded with ANSI or UTF-8
///
class cc_stream {
public:
    cc_stream();
    cc_stream(const cc_stream& rval);
    explicit cc_stream(const char* fileName);
    virtual ~cc_stream();

    /// Create stream from a C++ source file
    bool open(const char* fileName);

    /// Close the stream object
    bool close();

    /// Summary
    ///  Erase the stream content from <beg> to <end> with <ch>
    ///  LFs will not be touched
    ///
    size_t erase(size_t beg, size_t end);

    /// Summary
    ///  Read a identifier name from the given position <p>
    ///  Heading spaces are ignored
    ///
    /// Returns
    ///  <p> returns the position of the last character of identifier name
    ///  <name> returns the identifier name
    ///  This method returns true if <p> points to a valid identifier name,
    /// otherwise this method returns false
    ///
    bool read_name(size_t& p, std::string& name) const;

    /// Summary
    ///  Read complete name from the given position <p>
    ///  Complete-Name = (Nested-Name-Specifier)Identifier
    ///  Nested-Name-Specifier = (::)Identifier(::Identifier)*::
    ///
    ///  Leading spaces are skipped
    ///
    /// Returns
    ///  <p> returns the position of the last character of name
    ///  <sv> returns the splitted complete name
    ///  This method returns true if p points to a valid identifier name,
    ///otherwise returns false
    ///
    bool read_complete_name(size_t& p, string_vect& sv) const;

    /// Summary
    ///  Parse C++ identifiers from <begin> to <end>
    ///
    bool parse_identifier(cc_name_def_list& id_list, size_t begin = 0, size_t end = -1) const;

    /// Summary
    ///  Search paired characters recursively.
    ///
    /// Parameters
    ///  <begin> <end>, content range to search
    ///  <pair_pos>, output, stores the find result
    ///  <ptype>, paired characters to search
    ///
    /// Returns
    ///  This method returns true if the pair is found, otherwise returns false
    ///  <pair_pos> returns the position of the first pair that found
    ///  
    bool find_pair(cc_reference& pair_pos, size_t begin, size_t end = -1,
        const std::pair<char, char>& ptype = cc_stream::brace) const;

    /// Summary
    ///  Read the referred content pointed by <wdref> from stream and append it to <wd>
    ///  This method does NOT check the boundary
    ///
    bool read(const cc_reference& wdref, std::string& wd) const {
        wd.assign(_content+wdref.begin, wdref.length());
        return true;
    }

    /// Summary
    ///  Locate next line from <line.end>
    ///
    /// Returns
    ///  false if <line.end> has reached the end of stream, otherwise this
    /// method returns true 
    ///
    bool getline(cc_reference& line) const;

    const char* content() const { return _content; }

    size_t length() const { return _length; }

    bool is_open() const { return _content != 0; }

    int compare_at(size_t p, const char* dst, size_t cch) const {
        return strncmp(_content+p, dst, cch);
    }

public:
    static std::pair<char, char>    round_bracket;
    static std::pair<char, char>    angle_bracket;
    static std::pair<char, char>    square_bracket;
    static std::pair<char, char>    brace;

private:
    char*   _content;
    size_t  _length;
    size_t  _buff_size;
};

struct cc_name_def {
    std::string     name;
    cc_reference    name_ref;

    cc_name_def(){}

    cc_name_def(const std::string& n, size_t pos)
        :name(n), name_ref(pos, pos+name.length()){}

    cc_name_def(const std::string& n, const cc_reference& r)
        :name(n), name_ref(r) {}

    cc_name_def(const cc_stream& s, const cc_reference& r)
        :name(s.content()+r.begin, r.length()), name_ref(r) {}

    cc_name_def(const cc_stream& s, size_t begin, size_t end)
        :name(s.content()+begin, end-begin), name_ref(begin, end) {}

    void set_name(const std::string& n, size_t pos){
        name = n;
        set_name_ref_begin(pos);
    }

    void set_name_ref_begin(size_t begin){
        name_ref.begin = begin;
        name_ref.end = begin + name.length();
    }

    void set_name_ref_end(size_t end){
        name_ref.end = end;
        name_ref.begin = end - name.length();
    }
};

typedef std::list<cc_name_def> cc_name_def_list;
/// Summary
///  Describes a preprocessor reference
///
struct cc_preprocessor_def: public cc_name_def{
    cc_reference    line_ref;   // Address of preprocessor line
};

typedef std::list<cc_preprocessor_def>  cc_preprocessor_def_list;

struct cc_entity_def: public cc_name_def {
    string_vect  nested_name;
    cc_reference body_ref;
};

/// Summary
///  Describes a enumeration definition
///
struct cc_enum_def: public cc_entity_def {
    cc_name_def_list value_def_list;
    
    bool empty() const {
        return name.size() || value_def_list.size();
    }

    void clear() {
        nested_name.clear();
        name.clear();
        value_def_list.clear();
    }

    void set_name(size_t pos) {
        char temp[32];
        name.assign(temp, sprintf(temp, "unnamed_enum_%p", pos));
        name_ref.begin = name_ref.end = pos;
    }

    void add_value(const std::string& val, size_t pos) {
        value_def_list.push_back(cc_name_def(val, pos));
    }
};

typedef std::list<cc_enum_def> cc_enum_def_list;

/// Summary
///  Describes a class definition
///
struct cc_class_def: public cc_entity_def {
    void set_name(size_t pos) {
        char temp[32];
        name.assign(temp, sprintf(temp, "unnamed_class_%p", pos));
        name_ref.begin = name_ref.end = pos;
    }

    std::string     key_name;
};

typedef std::list<cc_class_def> cc_class_def_list;

class cc_symbol_index {
public:
    /// Summary
    ///  Parse C++ source stream
    ///  C/C++ comments, strings and characters will be replaced with spaces 
    /// after parsing
    ///
    /// Returns
    ///  true for success, false for failure
    ///
    bool parse_stream(const cc_stream& ccs);

    // Clear all symbol index information
    void clear();

    const cc_enum_def_list& enum_def_list() const {
        return _enum_def_list;
    }

    const cc_class_def_list& class_def_list() const {
        return _class_def_list;
    }

    const cc_preprocessor_def_list& preprocessor_def_list() const {
        return _preprocessor_def_list;
    }

    const cc_name_def_list& comment_def_list() const {
        return _comment_def_list;
    }

    const cc_name_def_list& string_def_list() const {
        return _string_def_list;
    }

    const cc_name_def_list& character_def_list() const {
        return _character_def_list;
    }

    const cc_name_def_list& include_def_list() const {
        return _include_def_list;
    }

    const cc_name_def_list& macro_def_list() const {
        return _macro_def_list;
    }

    const cc_reference_map& keyword_ref_map() const {
        return _keyword_ref_map;
    }

    const cc_reference_map& method_ref_map() const {
        return _method_ref_map;
    }

    const cc_reference_map& class_ref_map() const {
        return _class_ref_map;
    }

    const cc_reference_map& enum_ref_map() const {
        return _enum_ref_map;
    }

    const cc_reference_map& macro_ref_map() const {
        return _macro_ref_map;
    }

    const cc_reference_map& constant_ref_map() const {
        return _constant_ref_map;
    }

    const cc_reference_map& external_type_ref_map() const {
        return _external_type_ref_map;
    }

    const cc_reference_map& external_scope_ref_map() const {
        return _external_scope_ref_map;
    }

private:
    void _lex_comment(cc_stream& scontext);
    void _lex_string (cc_stream& scontext);
    void _lex_character(cc_stream& scontext);
    void _lex_preprocessor(cc_stream& scontext);
    void _lex_include(cc_preprocessor_def& pdef, cc_stream& ccs);

    void _lex_method(const cc_stream& ccs);
    void _lex_enumeration(const cc_stream& ccs);
    void _lex_class(const cc_stream& ccs);
    
    void _resolve_constant_ref(cc_name_def_list& id_list);
    void _resolve_keyword_ref(cc_name_def_list& id_list);
    void _resolve_class_ref(cc_name_def_list& id_list);
    void _resolve_enum_ref(cc_name_def_list& id_list);
    void _resolve_macro_ref(cc_name_def_list& id_list);
    void _resolve_external_scope_ref(cc_stream& scontext, cc_name_def_list& id_list);
    void _resolve_external_type_ref(cc_stream& scontext, cc_name_def_list& id_list);

    void _add_string_def(cc_stream& scontext, size_t begin, size_t end);
    void _add_comment_def(cc_stream& scontext, size_t begin, size_t end);
    void _add_character_def(cc_stream& scontext, size_t begin, size_t end);
    void _add_preprocessor_def(cc_stream& scontext, size_t begin, size_t end);

private:
    template<typename _def_list> void __resolve_type_ref(
        const _def_list& dl, cc_name_def_list& id_list, cc_reference_map& ref_map) {
        string_set name_set;
        for(_def_list::const_iterator it = dl.begin(); it != dl.end(); ++it) {
            name_set.insert(it->name);
        }

        for(cc_name_def_list::iterator id = id_list.begin(); id != id_list.end();) {
            if( name_set.count(id->name) ) {
                ref_map[id->name].insert(id->name_ref);
                id = id_list.erase(id);
            }
            else { ++id; }
        }
    }

protected:
    cc_name_def_list    _include_def_list;      // included files
    cc_name_def_list    _comment_def_list;      // comment blocks
    cc_name_def_list    _string_def_list;       // set of strings
    cc_name_def_list    _character_def_list;    // set of characters

    cc_enum_def_list    _enum_def_list;         // list of enumeration definition
    cc_class_def_list   _class_def_list;        // list of class definition
    cc_name_def_list    _macro_def_list;        // list of macro definition

    cc_reference_map    _keyword_ref_map;       // references of keywords
    cc_reference_map    _method_ref_map;        // references of methods
    cc_reference_map    _class_ref_map;         // references of user classes
    cc_reference_map    _enum_ref_map;          // references of user enumerations
    cc_reference_map    _constant_ref_map;      // references of enum constants
    cc_reference_map    _macro_ref_map;         // references of macros
    
    cc_reference_map    _external_type_ref_map; // types that neither defined nor declared
    cc_reference_map    _external_scope_ref_map;// scopes that neither defined nor declared

    cc_preprocessor_def_list _preprocessor_def_list;

    static cc_keyword_set   _keywords;
    static cc_class_key_set _class_key;
    static cc_access_specifier_set  _access_specifiers;
    static cc_base_specifier_set    _base_specifiers;
};

typedef std::map<std::string, cc_symbol_index>    cc_symbol_map;

/// Summary
///  Symbol index management
///
///
class cc_symbol_base {
public:
    bool add_file(const std::string& srcfile);
    bool remove_file(const std::string& srcfile);
    bool reparse_file(const std::string& srcfile);

    bool build();
    bool clean();
    void drop();

    bool save_data(const char* dbfile) const;
    bool load_data(const char* dbfile);

    const cc_symbol_index* find_index(const std::string& srcfile) const;

private:
    cc_symbol_map   _symbol_map;
    string_vect     _last_failed;
};
