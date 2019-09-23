/*********************************************************************
*
* File      : scantree.c
*
* Author    : Barry Kimelman
*
* Created   : September 21, 2019
*
* Purpose   : Recursively scan a directory tree for files
*
*********************************************************************/

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<dirent.h>
#include	<string.h>
#include	<time.h>
#include	<string.h>
#include	<stdarg.h>
#include	<getopt.h>
#include	<regex.h>

#define	EQ(s1,s2)	(strcmp(s1,s2)==0)
#define	NE(s1,s2)	(strcmp(s1,s2)!=0)
#define	GT(s1,s2)	(strcmp(s1,s2)>0)
#define	LT(s1,s2)	(strcmp(s1,s2)<0)
#define	LE(s1,s2)	(strcmp(s1,s2)<=0)

typedef struct name_tag {
	char	*name;
	struct name_tag	*next_name;
} NAME;
typedef struct nameslist_tag {
	NAME	*first_name;
	NAME	*last_name;
	int		num_names;
} NAMESLIST;

static	char	ftypes[] = {
 '.' , 'p' , 'c' , '?' , 'd' , '?' , 'b' , '?' , '-' , '?' , 'l' , '?' , 's' , '?' , '?' , '?'
};

static char	*perms[] = {
	"---" , "--x" , "-w-" , "-wx" , "r--" , "r-x" , "rw-" , "rwx"
};

static char	*months[12] = { "Jan" , "Feb" , "Mar" , "Apr" , "May" , "Jun" ,
				"Jul" , "Aug" , "Sep" , "Oct" , "Nov" , "Dec" } ;

static	int		opt_d = 0 , opt_h = 0 , opt_l = 0;
static	int		num_args;
static	char	*startdir , *pattern;

static	_regex_t	expression;
static	_regmatch_t	pmatch[2];

static	time_t	start_time;
static	time_t	end_time;
static	time_t	elapsed_time;

extern	int		optind , optopt , opterr;

extern	void	system_error() , quit() , die();
extern	void	format_number_with_commas();

/*********************************************************************
*
* Function  : debug_print
*
* Purpose   : Display an optional debugging message.
*
* Inputs    : char *format - the format string (ala printf)
*             ... - the data values for the format string
*
* Output    : the debugging message
*
* Returns   : nothing
*
* Example   : debug_print("The answer is %s\n",answer);
*
* Notes     : (none)
*
*********************************************************************/

void debug_print(char *format,...)
{
	va_list ap;

	if ( opt_d ) {
		va_start(ap,format);
		vfprintf(stdout, format, ap);
		fflush(stdout);
		va_end(ap);
	} /* IF debug mode is on */

	return;
} /* end of debug_print */

/*********************************************************************
*
* Function  : usage
*
* Purpose   : Display a program usage message
*
* Inputs    : char *pgm - name of program
*
* Output    : the usage message
*
* Returns   : nothing
*
* Example   : usage("The answer is %s\n",answer);
*
* Notes     : (none)
*
*********************************************************************/

void usage(char *pgm)
{
	fprintf(stderr,"Usage : %s [-dhl] dirname pattern\n\n",pgm);
	fprintf(stderr,"d - invoke debugging mode\n");
	fprintf(stderr,"h - produce this summary\n");
	fprintf(stderr,"l - list file infornation in long format\n");

	return;
} /* end of usage */

/*********************************************************************
*
* Function  : format_mode
*
* Purpose   : Format binary permission bits into a printable ASCII string
*
* Inputs    : unsigned short file_mode - mode bits from stat()
*             char *mode_bits - buffer to receive formatted info
*
* Output    : (none)
*
* Returns   : formatted mode info
*
* Example   : format_mode(filestat->st_mode,mode_info);
*
* Notes     : (none)
*
*********************************************************************/

void format_mode(unsigned short file_mode, char *mode_info)
{
	unsigned short setids;
	char *permstrs[3] , ftype , *ptr;

	setids = (file_mode & 07000) >> 9;
	permstrs[0] = perms[ (file_mode & 0700) >> 6 ];
	permstrs[1] = perms[ (file_mode & 0070) >> 3 ];
	permstrs[2] = perms[ file_mode & 0007 ];
	ftype = ftypes[ (file_mode & 0170000) >> 12 ];
	if ( setids ) {
		if ( setids & 01 ) { // sticky bit
			ptr = permstrs[2];
			if ( ptr[2] == 'x' ) {
				ptr[2] = 't';
			}
			else {
				ptr[2] = 'T';
			}
		}
		if ( setids & 04 ) { // setuid bit
			ptr = permstrs[0];
			if ( ptr[2] == 'x' ) {
				ptr[2] = 's';
			}
			else {
				ptr[2] = 'S';
			}
		}
		if ( setids & 02 ) { // setgid bit
			ptr = permstrs[1];
			if ( ptr[2] == 'x' ) {
				ptr[2] = 's';
			}
			else {
				ptr[2] = 'S';
			}
		}
	} // IF setids
	sprintf(mode_info,"%c%3.3s%3.3s%3.3s",ftype,permstrs[0],permstrs[1],permstrs[2]);

	return;
} /* end of format_mode */

/*********************************************************************
*
* Function  : display_file_info
*
* Purpose   : Display information for one file
*
* Inputs    : char *filename - name of file
*
* Output    : file information
*
* Returns   : nothing
*
* Example   : display_file_info("foo.txt");
*
* Notes     : (none)
*
*********************************************************************/

void display_file_info(char *filepath)
{
	struct _stat	filestats;
	unsigned short	filemode;
	char	mode_info[1024] , file_date[256] , size[100];
	struct tm	*filetime;

	if ( _stat(filepath,&filestats) == 0 ) {
		filemode = filestats.st_mode & _S_IFMT;
		format_mode(filestats.st_mode,mode_info);
		filetime = localtime(&filestats.st_mtime);
		sprintf(file_date,"%3.3s %2d, %d %02d:%02d:%02d",
			months[filetime->tm_mon],
			filetime->tm_mday,1900+filetime->tm_year,filetime->tm_hour,filetime->tm_min,
			filetime->tm_sec);
		format_number_with_commas(filestats.st_size,size);
		printf("%s %4d %12s %s %s\n",mode_info,filestats.st_nlink,size,file_date,filepath);
	}

	return;
} /* end of display_file_info */

/*********************************************************************
*
* Function  : scan_tree
*
* Purpose   : List the files under a directory.
*
* Inputs    : char *dirname - name of directory
*
* Output    : (none)
*
* Returns   : (nothing)
*
* Example   : scan_tree(dirname);
*
* Notes     : (none)
*
*********************************************************************/

int scan_tree(char *dirpath)
{
	_DIR	*dirptr;
	struct _dirent	*entry;
	struct _stat	filestats;
	char	*name , filename[1024];
	int		current_directory , errcode;
	NAMESLIST	subdirs;
	unsigned short	filemode;
	NAME	*dir;

	debug_print("scan_tree(%s)\n",dirpath);

	subdirs.num_names = 0;
	subdirs.first_name = NULL;
	subdirs.last_name = NULL;

	dirptr = _opendir(dirpath);
	if ( dirptr == NULL ) {
		quit(1,"_opendir failed for \"%s\"",dirpath);
	}

	current_directory = EQ(dirpath,".");
	entry = _readdir(dirptr);
	for ( ; entry != NULL ; entry = _readdir(dirptr) ) {
		name = entry->d_name;
		if ( current_directory )
			strcpy(filename,name);
		else
			sprintf(filename,"%s/%s",dirpath,name);
		if ( _stat(filename,&filestats) < 0 ) {
			system_error("stat() failed for \"%s\"",filename);
		} /* IF */
		else {
			filemode = filestats.st_mode & _S_IFMT;
			if ( _S_ISDIR(filemode) ) {
				if ( NE(name,".") && NE(name,"..") ) {
					subdirs.num_names += 1;
					dir = (NAME *)calloc(1,sizeof(NAME));
					if ( dir == NULL ) {
						quit(1,"calloc failed for NAME");
					}
					dir->name = _strdup(filename);
					if ( dir->name == NULL ) {
						quit(1,"strdup failed for dir.name");
					}
					if ( subdirs.num_names == 1 ) {
						subdirs.first_name = dir;
					}
					else {
						subdirs.last_name->next_name = dir;
					}
					subdirs.last_name = dir;
				} /* IF not '.' or '..' */
			} /* IF a directory */
			errcode = _regexec(&expression, name, (size_t)1, pmatch, 0);
			if ( errcode == 0 ) {
				if ( opt_l ) {
					display_file_info(filename);
				}
				else {
					printf("%s\n",filename);
				}
			} /* IF */
		} /* ELSE */
	} /* FOR */
	_closedir(dirptr);

	dir = subdirs.first_name;
	for ( ; dir != NULL ; dir = dir->next_name ) {
		scan_tree(dir->name);
	}

	return(0);
} /* end of scan_tree */

/*********************************************************************
*
* Function  : main
*
* Purpose   : program entry point
*
* Inputs    : argc - number of parameters
*             argv - list of parameters
*
* Output    : (none)
*
* Returns   : (nothing)
*
* Example   : scantree dirname pattern
*
* Notes     : (none)
*
*********************************************************************/

int main(int argc, char *argv[])
{
	int		errflag , c , num_args , errcode;
	char	*filename;
	char	errmsg[256];
	struct _stat	filestats;
	unsigned short	filemode;

	errflag = 0;
	while ( (c = _getopt(argc,argv,":dhl")) != -1 ) {
		switch (c) {
		case 'h':
			opt_h = 1;
			break;
		case 'd':
			opt_d = 1;
			break;
		case 'l':
			opt_l = 1;
			break;
		case '?':
			printf("Unknown option '%c'\n",optopt);
			errflag += 1;
			break;
		case ':':
			printf("Missing value for option '%c'\n",optopt);
			errflag += 1;
			break;
		default:
			printf("Unexpected value from getopt() '%c'\n",c);
		} /* SWITCH */
	} /* WHILE */
	if ( errflag ) {
		usage(argv[0]);
		die(1,"\nAborted due to parameter errors\n");
	} /* IF */
	if ( opt_h ) {
		usage(argv[0]);
		exit(0);
	} /* IF */

	num_args = argc - optind;
	if ( num_args < 2 ) {
		usage(argv[0]);
		die(1,"\nAborted due to parameter errors\n");
	}
	startdir = argv[optind];
	pattern = argv[optind+1];
	errcode = _regcomp(&expression, pattern, _REG_ICASE | _REG_EXTENDED);
	if ( errcode != 0 ) {
		_regerror(errcode,&expression,errmsg,sizeof(errmsg));
		fprintf(stderr,"Bad data pattern : %s\n",errmsg);
		die(1,"Bad data pattern : %s\n",errmsg);
	} /* IF */

	scan_tree(startdir);

	exit(0);
} /* end of main */
