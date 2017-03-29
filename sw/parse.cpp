#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <string>
#include <unordered_map>

#define	MAPT_INT	0
#define	MAPT_STRING	1
#define	MAPT_MAP	2

typedef	std::string *STRINGP, STRING;
typedef std::unordered_map<STRING,struct MAPT_S> FILEMAP;

typedef	struct MAPT_S {
	int	m_typ;
	union {
		int		m_v;
		STRINGP		m_s;
		FILEMAP		*m_m;
	} u;
} MAPT;


std::string	*rawline(FILE *fp) {
	char		linebuf[512];
	std::string	*r = new STRING;
	char		 *rv, *nl;

	do {
		rv = fgets(linebuf, sizeof(linebuf), fp);
		nl = strchr(linebuf, '\n');
		if (rv)
			r->append(linebuf);
	} while((rv)&&(!nl));

	// If we didn't get anything, such as on an end of file, return NULL
	// that way we can differentiate an EOF or error from an empty line.
	if ((!rv)&&(r->length()==0)) {
		delete r;
		printf("EOF!!\n");
		return NULL;
	}

	return r;
}

std::string	*getline(FILE *fp) {
	std::string	*r;

	do {
		r = rawline(fp);
	} while((r)&&(r->c_str()[0] == '#'));

	return r;
}

bool	iskey(STRING &s) {
	return (s[0] == '@');
}

STRING	*trim(STRING &s) {
	const char	*a, *b;
	a = s.c_str();
	b = s.c_str() + s.length()-1;

	for(; (*a)&&(a<b); a++)
		if (!isspace(*a))
			break;
	for(; (b>a); b--)
		if (!isspace(*b))
			break;
	return new STRING(s.substr(a-s.c_str(), b-a+1));
}

void	addtomap(FILEMAP &fm, STRING ky, STRING vl) {
	size_t	pos;

	if (((pos=ky.find('.')) != STRING::npos)&&(pos != 0)
			&&(pos < ky.length()-1)) {
		STRING	mkey = ky.substr(0,pos),
			subky = ky.substr(pos+1,ky.length()-pos+1);
		printf("KY = %s, MKEY = %s, SUBKY = %s\n", ky.c_str(),
			mkey.c_str(), subky.c_str());
		assert(subky.length() > 0);
		MAPT	*subfm;
		FILEMAP::iterator	subloc = fm.find(mkey);
printf("KEY = %s = %s . %s\n", ky.c_str(), mkey.c_str(), subky.c_str());
		if (subloc == fm.end()) {
printf("No map found\n");
			subfm = new MAPT;
			subfm->m_typ = MAPT_MAP;
			subfm->u.m_m = new FILEMAP;
			fm.insert(std::pair<STRING,MAPT>(mkey, *subfm ) );
		} else {
printf("Map found\n");
			subfm = &(*subloc).second;
			if ((!subfm)||(subfm->m_typ != MAPT_MAP)) {
				fprintf(stderr, "MAP[%s] isnt a map\n", mkey.c_str());
				return;
			}
		}
		addtomap(*subfm->u.m_m, subky, vl);
		return;
	}

	printf("KEY is %s\n", ky.c_str());
	MAPT	*elm = new MAPT;
	elm->m_typ = MAPT_STRING;
	elm->u.m_s = new STRING(vl);
	fm.insert(std::pair<STRING,MAPT>(ky, *elm ) );
}

FILEMAP	*parsefile(FILE *fp) {
	std::string	key, value, *ln, pfkey, prefix;
	FILEMAP		*fm = new FILEMAP;
	size_t		pos;

	pfkey = "@PREFIX";
	key = ""; value = "";
	while(ln = getline(fp)) {
		if (iskey(*ln)) {

printf("LN = %s is a KEY\n", ln->c_str());

			if (key.length()>0) {
				if (key == pfkey)
					prefix = value;
				addtomap(*fm, key, value);
			}

			if ((pos = ln->find("="))
				&&(pos != STRING::npos)) {
				key   = ln->substr(1, pos-1);
				value = ln->substr(pos+1, ln->length()-pos-1);
			} else {
				key = ln->substr(1, ln->length()-1);
				value = "";
			}

			STRING	*s = trim(key);
			key = *s;
		} else if (ln->c_str()[0]) {
			printf("Adding %s to our string\n", (*ln).c_str());
			value = value + (*ln);
		}
	} if (key.length()>0) {
		addtomap(*fm, key, value);
	}

	return fm;
}

FILEMAP	*parsefile(const char *fname) {
	FILE	*fp = fopen(fname, "r");
	if (fp == NULL) {
		fprintf(stderr, "ERR: Could not open %s\n", fname);
		return NULL;
	} FILEMAP *map = parsefile(fp);
	fclose(fp);

	return map;
}

FILEMAP	*parsefile(const std::string &fname) {
	return parsefile((const char *)fname.c_str());
}

void	mapdump_aux(FILEMAP &fm, int offset) {
	FILEMAP::iterator	kvpair;

	for(kvpair = fm.begin(); kvpair != fm.end(); kvpair++) {
		printf("%*s%s: ", offset, "", (*kvpair).first.c_str());
		if ((*kvpair).second.m_typ == MAPT_MAP) {
			printf("\n");
			mapdump_aux(*(*kvpair).second.u.m_m, offset+1);
		} else if ((*kvpair).second.m_typ == MAPT_INT) {
			printf("%d\n", (*kvpair).second.u.m_v);
		} else { // if ((*kvpair).second.m_typ == MAPT_STRING) {
			STRINGP	s = (*kvpair).second.u.m_s;
			size_t	pos;
			if ((pos=s->find("\n")) == STRING::npos) {
				printf("%s\n", s->c_str());
			} else if (pos == s->length()-1) {
				printf("%s", s->c_str());
			} else	{
				printf("<Multi-line-String>\n");
				printf("\n\n----------------\n%s\n", s->c_str());
			}
		}
	}
}

void	mapdump(FILEMAP &fm) {
	printf("\n\nDUMPING!!\n\n");
	mapdump_aux(fm, 0);
}

int main(int argc, char **argv) {
	FILEMAP	*fm;

	fm = parsefile("wbuart.txt");
	if (fm)
		mapdump(*fm);
	else printf("No file to dump\n");
}

