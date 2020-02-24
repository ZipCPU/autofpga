/*******************************************************************************
**
** Filename: 	expr.l
**
** Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
**
** Purpose:	The lexical analyzer for any variable evaluations.  This
**		converts data description input into tokens, to feed the parser.
**	In general, the data description input is a single line, although it may
**	be split across multiple if the computation to be accomplished is
**	complex enough.
**
**
** Creator:	Dan Gisselquist, Ph.D.
**		Gisselquist Technology, LLC
**
********************************************************************************
**
** Copyright (C) 2017-2020, Gisselquist Technology, LLC
**
** This program is free software (firmware): you can redistribute it and/or
** modify it under the terms of  the GNU General Public License as published
** by the Free Software Foundation, either version 3 of the License, or (at
** your option) any later version.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
** for more details.
**
** You should have received a copy of the GNU General Public License along
** with this program.  (It's in the $(ROOT)/doc directory.  Run make with no
** target there if the PDF file isn't present.)  If not, see
** <http://www.gnu.org/licenses/> for a copy.
**
** License:	GPL, v3, as defined and found on www.gnu.org,
**		http://www.gnu.org/licenses/gpl.html
**
**
********************************************************************************
**
**/

%{
#include <stdio.h>
#include "parser.h"
#include "expr.tab.h"

extern "C" int yylex();
extern "C" int	yywrap() { return 1;}
%}

%%
[@]?[$]?[/.^+]?[_A-Za-z][._a-zA-Z0-9]*  {
		if (yytext[0] == '@') {
			if (yytext[1]=='$')
				yylval.u_id = strdup(yytext+2);
			else
				yylval.u_id = strdup(yytext+1);
		} else
			yylval.u_id  = strdup(yytext);
		return IDENTIFIER;
	}
@[$]?\([/.^+]?[_A-Za-z][._a-zA-Z0-9]*\)  {
		if (yytext[1]=='$')
			yylval.u_id = strdup(yytext+3);
		else
			yylval.u_id = strdup(yytext+2);
		// Remove the trailing parenthesis
		yylval.u_id[strlen(yylval.u_id)-1]='\0';
		return IDENTIFIER;
	}
0[xX][0-9A-Fa-f]+   { yylval.u_ival = strtoul(yytext,NULL,16);return INT;}
0[0-7]+		{ yylval.u_ival = strtoul(yytext,NULL, 8);return INT;}
[1-9][0-9]*	  	{ yylval.u_ival = strtoul(yytext,NULL,10);return INT;}
"0"		{ yylval.u_ival = 0;return INT;}
"+"		{ return PLUS; }
"-"		{ return MINUS; }
"*"		{ return TIMES; }
"/"		{ return DIVIDE; }
"<<"		{ return UPSHIFT; }
">>"		{ return DOWNSHIFT; }
"!="		{ return LOGICAL_NEQ; }
"<="		{ return LOGICAL_LTE; }
">="		{ return LOGICAL_GTE; }
"<"		{ return LOGICAL_LT; }
">"		{ return LOGICAL_GT; }
"&&"		{ return BOOLEANAND; }
"||"		{ return BOOLEANOR; }
"=="		{ return BOOLEANEQ; }
"!"		{ return BOOLEANNOT; }
"|"		{ return BITWISEOR; }
"&"		{ return BITWISEAND; }
"^"		{ return BITWISEXOR; }
"~"		{ return BITWISENOT; }
"%"		{ return MODULO; }
":"		{ return COLON; }
"?"		{ return QUESTION; }
[ \t]+		{ }
[(]		{ return '('; }
[)]		{ return ')'; }

%%
