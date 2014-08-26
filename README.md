## BLING-C ##

BLING-C is a syntax highlight tool for C/C++ source code, and it prints
highlighted code into HTML documents.

### Usage

	$>blingc <OPTIONS> <FILES>

available options:

***--stdout***<br>
	Writing output to stdout instead of files. When --stdout is specified, output content will be encoded with "Chunked" encoding, exactly one chunk for each input file.

***--css=&lt;PATH-TO-CSS&gt;***<br>
    Specifies the CSS to be used. Default value is 'style.css'. This option is ignored when --noheader is specified.

***--outdir=&lt;DIR&gt;***<br>
    Specifies the output directory. DIR should end with path separator. Output files are written to the same directory as input files by default.

***--ln=&lt;LN-SIZE&gt;***<br>
    Specifies the number of digits of line number. Default value is 0.

***--tab=&lt;TAB-SIZE&gt;***<br>
    Specifies the tab size. Default value is 4.

***--noheader***<br>
    Output HTML document without HTML header. When this option is used, --css will be ignored.

Example:

    $>blingc a.cpp
    $>blingc --css=mystyle.css --ln=5 a.cpp b.h