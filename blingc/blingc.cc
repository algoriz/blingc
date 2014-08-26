#include "cclex.h"

#include <fstream>
#include <string>
#include <iostream>

enum style_class
{
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

struct style_map : public std::map < style_class, std::string >
{
    style_map()
    {
        style_map& self = *this;
        self[style_line_number] = "line-number";
        self[style_identifier] = "identifier";
        self[style_keyword] = "keyword";
        self[style_user_type] = "user-type";
        self[style_method] = "method";
        self[style_macro] = "macro";
        self[style_string] = "string";
        self[style_comment] = "comment";
        self[style_character] = "character";
        self[style_external_type] = "external-type";
        self[style_external_scope] = "external-scope";
        self[style_preprocessor] = "preprocessor";
        self[style_enum_constant] = "enum-constant";
    }
};

struct style_index : public cc_reference
{
    style_index() : style(style_identifier){}
    style_index(const cc_reference& r, style_class s) :cc_reference(r), style(s){}
    style_index(size_t b, size_t e, style_class s) : cc_reference(b, e), style(s){}
    style_class   style;
};

typedef std::set<style_index, cc_reference::less> style_index_set;

struct html_ctl
{
    std::string     title;
    std::string     style;
    size_t  lno_size;
    size_t  tab_size;
    size_t  no_header;
};

void sort_symbols(style_index_set& iset, cc_symbol_index& symbols);
void sort_name_def_list(style_index_set& iset, const cc_name_def_list& ref_set, style_class c);
void sort_reference_map(style_index_set& iset, const cc_reference_map& ref_map, style_class c);
void source_to_html(cc_stream& src, std::ofstream& html, html_ctl& ctl, style_index_set& iset);
void sort_preprocessor_list(style_index_set& iset,
                            const cc_preprocessor_def_list& proc_list, style_class style);

enum config_key
{
    ck_html_style,
    ck_output_dir,
    ck_lno_size,
    ck_tab_size,
    ck_no_header
};

int parse_arg(int argc, char* argv[], std::vector<std::string>& flist,
              std::map<config_key, std::string>& arglist)
{
    arglist[ck_html_style] = "style.css";
    arglist[ck_tab_size] = "4";
    arglist[ck_lno_size] = "0";
    arglist[ck_no_header] = "0";

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (!strnicmp(argv[i], "-css:", 5))
            {
                arglist[ck_html_style] = argv[i] + 5;
            }
            else if (!strnicmp(argv[i], "-outdir:", 8))
            {
                arglist[ck_output_dir] = argv[i] + 8;
            }
            else if (!strnicmp(argv[i], "-lnosize:", 9))
            {
                if (argv[i][9] >= '1' && argv[i][9] <= '9')
                {
                    arglist[ck_lno_size] = argv[i] + 9;
                }
                else return i;
            }
            else if (!strnicmp(argv[i], "-noheader", 9))
            {
                arglist[ck_no_header] = "1";
            }
            else if (!strnicmp(argv[i], "-tabsize:", 9))
            {
                if (argv[i][9] >= '1' && argv[i][9] <= '9')
                {
                    arglist[ck_tab_size] = argv[i] + 9;
                }
                else return i;
            }
            else{ return i; }
        }
        else{ flist.push_back(argv[i]); }
    }
    return 0;
}

std::string source_name(const std::string& fname)
{
    size_t linux_p = fname.find_last_of('/');
    size_t win_p = fname.find_last_of('\\');
    if (linux_p == std::string::npos && win_p == std::string::npos)
    {
        return fname;
    }

    size_t pos;
    if ((win_p != std::string::npos) && (linux_p != std::string::npos))
    {
        pos = linux_p > win_p ? linux_p : win_p;
    }
    else
    {
        pos = linux_p > win_p ? win_p : linux_p;
    }
    return fname.substr(pos + 1);
}

int print_manual()
{
    std::cout << "                     BLING-C\n\n";
    std::cout << "A C/C++ source syntax highlight tool.\n";
    std::cout << "Usage: blingc <options> <file-list>\n";
    std::cout << "    Order of options and file list is not cared.\n";
    std::cout << "Available options:\n";
    std::cout << "    -css:<file-path>    Color scheme. By default style.css is used.\n";
    std::cout << "    -outdir:<dir-path>  Output all files to the given existing directory.\n";
    std::cout << "    -lnosize:<number>   Length of line number. Default value is 0.\n";
    std::cout << "    -tabsize:<number>   Tab size. Default value is 4.\n";
    std::cout << "    -noheader           Output HTML document without HTML header, /css\n";
    std::cout << "                        option will be ignored.\n";
    std::cout << "Example:\n";
    std::cout << "    blingc a.cpp\n";
    std::cout << "    blingc -css:mystyle.css a.cpp b.h -lnosize:5\n";
    return 0;
}

int main(int argc, char* argv[])
{
    std::vector<std::string> flist;
    std::map<config_key, std::string> arglist;

    if (argc <= 1)
    {
        return print_manual();
    }

    if (int rtn = parse_arg(argc, argv, flist, arglist))
    {
        std::cerr << "Bad option: " << argv[rtn] << '\n';
        return 0;
    }

    if (flist.size() == 0)
    {
        std::cout << "No file to process.\n";
        return 0;
    }

    cc_stream sinput;
    std::ofstream soutput;
    cc_symbol_index symbols;
    style_index_set iset;

    for (std::vector<std::string>::iterator fpath = flist.begin();
         fpath != flist.end(); ++fpath)
    {
        if (!sinput.open(fpath->data()))
        {
            std::cerr << "Failed to read input file: " << *fpath << '\n';
            continue;
        }

        std::string fname;
        if (arglist.count(ck_output_dir))
        {
            fname = arglist[ck_output_dir];
            fname += source_name(*fpath);
            fname += ".html";
        }
        else
        {
            fname = *fpath;
            fname += ".html";
        }

        soutput.open(fname.data(), std::ios::out | std::ios::trunc);
        if (soutput.fail() || soutput.bad())
        {
            std::cerr << "Failed to write output file: " << fname << '\n';
            sinput.close();
            continue;
        }

        // do parsing
        if (!symbols.parse_stream(sinput))
        {
            std::cerr << "[FAILED] " << *fpath << '\n';
            continue;
        }

        // sort symbols
        sort_symbols(iset, symbols);

        html_ctl ctl;
        ctl.style = arglist[ck_html_style];
        ctl.title = source_name(*fpath);
        ctl.lno_size = atoi(arglist[ck_lno_size].data());
        ctl.tab_size = atoi(arglist[ck_tab_size].data());
        ctl.no_header = atoi(arglist[ck_no_header].data());

        // construct html document based on the index
        source_to_html(sinput, soutput, ctl, iset);

        sinput.close();
        soutput.close();
        symbols.clear();
        iset.clear();
    }
    return 0;
}

void sort_symbols(style_index_set& iset, cc_symbol_index& symbols)
{
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

void sort_name_def_list(style_index_set& iset, const cc_name_def_list& def_list, style_class style)
{
    cc_name_def_list::const_iterator it;
    for (it = def_list.begin(); it != def_list.end(); ++it)
    {
        iset.insert(style_index(it->name_ref, style));
    }
}

void sort_reference_map(style_index_set& iset, const cc_reference_map& ref_map, style_class style)
{
    cc_reference_map::const_iterator it_map;
    for (it_map = ref_map.begin(); it_map != ref_map.end(); ++it_map)
    {
        cc_reference_set::const_iterator it_set;
        for (it_set = it_map->second.begin(); it_set != it_map->second.end(); ++it_set)
        {
            iset.insert(style_index(*it_set, style));
        }
    }
}

void sort_preprocessor_list(style_index_set& iset,
                            const cc_preprocessor_def_list& proc_list, style_class style)
{
    cc_preprocessor_def_list::const_iterator it;
    for (it = proc_list.begin(); it != proc_list.end(); ++it)
    {
        iset.insert(style_index(it->line_ref.begin, it->name_ref.end, style));
    }
}

void begin_label(std::string& buff, style_class idx)
{
    static style_map label_class;

    buff += "<label class=\"";
    buff += label_class[idx];
    buff += "\">";
}

void close_label(std::string& buff)
{
    buff += "</label>";
}

void source_to_html(cc_stream& src, std::ofstream& html, html_ctl& ctl, style_index_set& iset)
{
    size_t line = 1;
    char lno_format[16] = { 0 };
    char lno[16];
    bool add_line_num = (ctl.lno_size != 0);
    if (add_line_num)
    {
        sprintf(lno_format, "%%0%dd", ctl.lno_size);
    }

    const char* data = src.content();
    std::string body;
    style_index_set::iterator it_label = iset.begin();
    style_index_set::iterator it_label_end = iset.end();
    size_t tab_col = 0;

    for (size_t gp = 0; gp < src.length(); ++gp)
    {
        if (add_line_num)
        {
            sprintf(lno, lno_format, line);
            begin_label(body, style_line_number);
            body += lno;
            close_label(body);
            add_line_num = false;
            tab_col = 0;
        }

        if ((it_label != it_label_end) && (gp == it_label->end))
        {
            close_label(body);
            ++it_label;
        }

        if ((it_label != it_label_end) && (gp == it_label->begin))
        {
            begin_label(body, it_label->style);
        }

        switch (data[gp])
        {
        case '\r': break;
        case '<': body += "&lt;";   ++tab_col; break;
        case '>': body += "&gt;";   ++tab_col; break;
        case ' ': body += "&nbsp;"; ++tab_col; break;
        case '&': body += "&amp";   ++tab_col; break;
        case '\t':
        {
            body.append(ctl.tab_size - (tab_col % 4), ' ');
            tab_col = 0;
        }
            break;
        case '\n':
        {
            body += "<br/>";
            ++line;
            if (ctl.lno_size){ add_line_num = true; }
        }
            break;
        default: body.append(1, data[gp]);
        }
    }

    if (!ctl.no_header)
    {
        html << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
            << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n";
        html << "<!--This document is generated by BLING-C -->\n";
        html << "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
        html << "<head>\n";
        html << "<title>" << ctl.title << "</title>\n";
        html << "<link rel=\"stylesheet\" href=\"" << ctl.style << "\" type=\"text/css\"/>\n";
        html << "</head>\n" << "<body>\n";
    }

    html << body;

    if (!ctl.no_header)
    {
        html << "\n</body>\n</html>\n";
    }
}
