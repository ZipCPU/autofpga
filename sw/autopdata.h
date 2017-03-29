#ifndef	AUTOPDATA_H
#define	AUTOPDATA_H

typedef	struct	REGINFO_S {
	int		offset;
	const char	*defname;
	const char	*namelist;
} REGINFO;

typedef	struct	AUTOPDATA_S {
	const char	*prefix;
	int	naddr;
	const char	*access;
	const char	*ext_ports, *ext_decls;
	const char	*main_defns;
	const char	*dbg_defns;
	const char	*main_insert;
	const char	*alt_insert;
	const char	*dbg_insert;
	REGINFO		*pregs;
	const char	*cstruct, *ioname;
} AUTOPDATA;

#endif
