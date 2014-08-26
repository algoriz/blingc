#include "cclex.h"

#include <fstream>
#include <string>
#include <iostream>

enum style_class {
    style_line_number,

    style_identifier,
    style_keyword,
    style_user_type,    // style for user classes and enum types
    style_external_type,
    style_external_scope,
    style_method,
    style_macro,
    style_enum_constant,
    style_comment,
    style_string,
    style_character,
    style_preprocessor
};

struct style_map : public std::map < style_class, std::string >{
    style_map(){
        style_map& self = *this;
        self[style_line_number] = "ln";
        self[style_identifier] = "id";
        self[style_keyword] = "kw";
        self[style_user_type] = "ut";
        self[style_method] = "fn";
        self[style_macro] = "m";
        self[style_string] = "s";
        self[style_comment] = "c";
        self[style_character] = "ch";
        self[style_external_type] = "et";
        self[style_external_scope] = "es";
        self[style_preprocessor] = "p";
        self[style_enum_constant] = "k";
    }
};

struct style_index : public cc_reference{
    style_index() : style(style_identifier){}
    style_index(const cc_reference& r, style_class s) :cc_reference(r), style(s){}
    style_index(size_t b, size_t e, style_class s) : cc_reference(b, e), style(s){}
    style_class   style;
};

typedef std::set<style_index, cc_reference::less> style_index_set;

struct html_ctl{
    std::string title;
    std::string style;
    int lno_size;
    int tab_size;
    int no_header;
    int std_chunk;
};

void sort_symbols(style_index_set& iset, cc_symbol_index& symbols);
void sort_name_def_list(style_index_set& iset, const cc_name_def_list& ref_set, style_class c);
void sort_reference_map(style_index_set& iset, const cc_reference_map& ref_map, style_class c);
void source_to_html(cc_stream& src, std::ostream& html, html_ctl& ctl, style_index_set& iset);
void sort_preprocessor_list(style_index_set& iset,
                            const cc_preprocessor_def_list& proc_list, style_class style);

enum config_key
{
    ck_html_style,
    ck_output_dir,
    ck_lno_size,
    ck_tab_size,
    ck_no_header,
    ck_std_chunk
};

int parse_arg(int argc, char* argv[], std::vector<std::string>& flist,
              std::map<config_key, std::string>& arglist){
    arglist[ck_html_style] = "style.css";
    arglist[ck_tab_size] = "4";
    arglist[ck_lno_size] = "0";
    arglist[ck_no_header] = "0";
    arglist[ck_std_chunk] = "0";

    for (int i = 1; i < argc; ++i){
        if (argv[i][0] != '-') {
            flist.push_back(argv[i]);
            continue;
        }
        
        if (!strnicmp(argv[i], "--css=", 6)) {
            arglist[ck_html_style] = argv[i] + 6;
        }
        else if (!strnicmp(argv[i], "--outdir=", 9)) {
            arglist[ck_output_dir] = argv[i] + 9;
        }
        else if (!strnicmp(argv[i], "--ln=", 5)) {
            if (argv[i][5] >= '1' && argv[i][5] <= '9') {
                arglist[ck_lno_size] = argv[i] + 5;
            }
            else return i;
        }
        else if (!strnicmp(argv[i], "--noheader", 10)) {
            arglist[ck_no_header] = "1";
        }
        else if (!strnicmp(argv[i], "--tab=", 6)) {
            if (argv[i][6] >= '1' && argv[i][6] <= '9') {
                arglist[ck_tab_size] = argv[i] + 6;
            }
            else return i;
        }
        else if (!strnicmp(argv[i], "--stdout", 8)) {
            arglist[ck_std_chunk] = "1";
        }
        else{ return i; }
    }
    return 0;
}

std::string source_name(const std::string& fname) {
    size_t linux_p = fname.find_last_of('/');
    size_t win_p = fname.find_last_of('\\');
    if (linux_p == std::string::npos && win_p == std::string::npos) {
        return fname;
    }

    size_t pos;
    if ((win_p != std::string::npos) && (linux_p != std::string::npos)) {
        pos = linux_p > win_p ? linux_p : win_p;
    }
    else {
        pos = linux_p > win_p ? win_p : linux_p;
    }
    return fname.substr(pos + 1);
}

int print_manual() {
    std::cout <<
        "                     BLING-C\n\n"
        "BLING-C is a syntax highlight tool for C/C++ source code, and it prints\n"
        "highlighted code into HTML documents.\n\n"
        "Usage: blingc <OPTIONS> <FILES>\n"
        "Available options:\n"
        "  --stdout\n"
        "    Writing output to stdout instead of files. When --stdout is specified,\n"
        "    output content will be encoded with \"Chunked\" encoding, exactly \n"
        "    one chunk for each input file.\n\n"
        "  --css=<PATH-TO-CSS>\n"
        "    Specifies the CSS to be used. Default value is 'style.css'\n"
        "    This option is ignored when --noheader is specified.\n\n"
        "  --outdir=<DIR>\n"
        "    Specifies the output directory. DIR should end with path separator.\n"
        "    Output files are written to the same directory as input files by default.\n\n"
        "  --ln=<LN-SIZE>\n"
        "    Specifies the number of digits of line number. Default value is 0.\n\n"
        "  --tab=<TAB-SIZE>\n"
        "    Specifies the tab size. Default value is 4.\n\n"
        "  --noheader\n"
        "    Output HTML document without HTML header.\n"
        "    When this option is used, --css will be ignored.\n\n"
        "Example:\n"
        "    blingc a.cpp\n"
        "    blingc --css=mystyle.css a.cpp b.h --ln=5\n";
    return 0;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> flist;
    std::map<config_key, std::string> arglist;

    if (argc <= 1) {
        return print_manual();
    }

    if (int rtn = parse_arg(argc, argv, flist, arglist)) {
        std::cerr << "Bad option: " << argv[rtn] << '\n';
        return 0;
    }

    if (flist.size() == 0) {
        std::cout << "No file to process.\n";
        return 0;
    }

    cc_stream input;
    cc_symbol_index symbols;
    style_index_set iset;

    html_ctl ctl;
    ctl.style = arglist[ck_html_style];
    ctl.lno_size = atoi(arglist[ck_lno_size].c_str());
    ctl.tab_size = atoi(arglist[ck_tab_size].c_str());
    ctl.no_header = atoi(arglist[ck_no_header].c_str());
    ctl.std_chunk = atoi(arglist[ck_std_chunk].c_str());

    for (std::vector<std::string>::iterator fpath = flist.begin();
         fpath != flist.end(); ++fpath){
        if (!input.open(fpath->data())) {
            std::cerr << "Failed to read input file: " << *fpath << '\n';
            continue;
        }

        std::string fname;
        if (arglist.count(ck_output_dir)) {
            fname = arglist[ck_output_dir];
            fname += source_name(*fpath);
            fname += ".html";
        }
        else {
            fname = *fpath;
            fname += ".html";
        }

        std::ofstream outf;
        std::ostream* output = &std::cout;
        if (!ctl.std_chunk){
            outf.open(fname.data(), std::ios::out | std::ios::trunc);
            if (!outf) {
                std::cerr << "Failed to write output file: " << fname << '\n';
                input.close();
                continue;
            }
            output = &outf;
        }

        // do parsing
        if (!symbols.parse_stream(input)) {
            std::cerr << "Failed to parse file: " << *fpath << '\n';
            continue;
        }

        // sort symbols
        sort_symbols(iset, symbols);

        // construct html document based on the index
        ctl.title = source_name(*fpath);
        source_to_html(input, *output, ctl, iset);

        input.close();
        outf.close();
        symbols.clear();
        iset.clear();
    }
    return 0;
}

void sort_symbols(style_index_set& iset, cc_symbol_index& symbols){
    sort_preprocessor_list(iset, symbols.preprocessor_def_list(), style_preprocessor);
    sort_name_def_list(iset, symbols.comment_def_list(), style_comment);
    sort_name_def_list(iset, symbols.string_def_list(), style_string);
    sort_name_def_list(iset, symbols.character_def_list(), style_character);
    sort_name_def_list(iset, symbols.include_def_list(), style_string);
    sort_reference_map(iset, symbols.external_type_ref_map(), style_external_type);
    sort_reference_map(iset, symbols.external_scope_ref_map(), style_external_scope);
    sort_reference_map(iset, symbols.keyword_ref_map(), style_keyword);
    sort_reference_map(iset, symbols.class_ref_map(), style_user_type);
    sort_reference_map(iset, symbols.enum_ref_map(), style_user_type);
    sort_reference_map(iset, symbols.macro_ref_map(), style_macro);
    sort_reference_map(iset, symbols.constant_ref_map(), style_enum_constant);
    sort_reference_map(iset, symbols.method_ref_map(), style_method);
}

void sort_name_def_list(style_index_set& iset,
                        const cc_name_def_list& def_list, style_class style){
    cc_name_def_list::const_iterator it;
    for (it = def_list.begin(); it != def_list.end(); ++it){
        iset.insert(style_index(it->name_ref, style));
    }
}

void sort_reference_map(style_index_set& iset,
                        const cc_reference_map& ref_map, style_class style){
    cc_reference_map::const_iterator it_map;
    for (it_map = ref_map.begin(); it_map != ref_map.end(); ++it_map){
        cc_reference_set::const_iterator it_set;
        for (it_set = it_map->second.begin(); it_set != it_map->second.end(); ++it_set){
            iset.insert(style_index(*it_set, style));
        }
    }
}

void sort_preprocessor_list(style_index_set& iset,
                            const cc_preprocessor_def_list& proc_list, style_class style){
    cc_preprocessor_def_list::const_iterator it;
    for (it = proc_list.begin(); it != proc_list.end(); ++it){
        iset.insert(style_index(it->line_ref.begin, it->name_ref.end, style));
    }
}

void begin_label(std::string& buff, style_class idx){
    static style_map label_class;

    buff += "<label class=\"";
    buff += label_class[idx];
    buff += "\">";
}

void close_label(std::string& buff){
    buff += "</label>";
}

void source_to_html(cc_stream& src, std::ostream& output,
                    html_ctl& ctl, style_index_set& iset){
    size_t line = 1;
    char lno_format[16] = { 0 };
    char lno[16];
    bool add_line_num = (ctl.lno_size != 0);
    if (add_line_num){
        sprintf(lno_format, "%%0%dd", ctl.lno_size);
    }

    const char* data = src.content();
    style_index_set::iterator it_label = iset.begin();
    style_index_set::iterator it_label_end = iset.end();
    size_t tab_col = 0;

    // output HTML contents
    std::string html =
        "<!--This document is generated by BLING-C https://github.com/algoriz/blingc -->\n";
    for (size_t gp = 0; gp < src.length(); ++gp){
        if (add_line_num) {
            sprintf(lno, lno_format, line);
            begin_label(html, style_line_number);
            html += lno;
            close_label(html);
            add_line_num = false;
            tab_col = 0;
        }

        if ((it_label != it_label_end) && (gp == it_label->end)) {
            close_label(html);
            ++it_label;
        }

        if ((it_label != it_label_end) && (gp == it_label->begin)) {
            begin_label(html, it_label->style);
        }

        switch (data[gp]) {
        case '\r':
            break;
        case '<':
            html += "&lt;";
            ++tab_col;
            break;
        case '>':
            html += "&gt;";
            ++tab_col;
            break;
        case ' ':
            html += "&nbsp;";
            ++tab_col;
            break;
        case '&':
            html += "&amp";
            ++tab_col;
            break;
        case '\t':
            html.append(ctl.tab_size - (tab_col % 4), ' ');
            tab_col = 0;
            break;
        case '\n':
            html += "<br/>";
            ++line;
            if (ctl.lno_size){
                add_line_num = true;
            }
            break;
        default:
            html.append(1, data[gp]);
        }
    }

    if (!ctl.no_header){
        std::string header = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
            "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n<head>\n<title>";
        header += ctl.title;
        header += "</title>\n<link rel=\"stylesheet\" href=\"";
        header += ctl.style;
        header += "\" type=\"text/css\"/>\n</head>\n<body>\n";
        header += html;
        html.swap(header);
    }

    if (!ctl.no_header){
        html += "\n</body>\n</html>\n";
    }

    if (ctl.std_chunk){
        output << html.size() << "\r\n";
    }
    output << html;
}
