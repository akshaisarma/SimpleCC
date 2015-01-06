#include <setjmp.h>

//Definitions for chartype array below. Used in Scanner.
#define ENDOFFILE 0
#define NUMBER 48
#define LETTER 65
#define OPERATOR 42
#define QUOTE 34
#define SPACELIKE 9
#define HASH 35
#define AMBIG 36
#define BADCHAR 2

//Defining all globals here

//Structs
struct keywordnode {
	char* name;
	int tokentype;
	struct keywordnode* next;
};

struct filedetails { 
	int curtoken;
	union {
		int intliteral;
		double fltliteral;
	} curtokenvalue;
	char* strliteral;
	char* curname;
	char* curfile;
	int curline;
	int curcol;
	char* scanp;
	char* filecontents;
};

struct filestack {
	struct filedetails* record;
	struct filestack* next;
};

struct func_details {
	char body_seen;
	int* call_list;
	int numcalls;
};

struct symtabrecord {
	char* name;
	int tokentype;// var, arr or func
	int vartype; //if var/arr
	int addr;
	int size;
	struct func_details* funcdet; // if func
	struct symtabrecord* next; 
};

struct symtabstack {
	struct symtabrecord* symtab[256];
	struct symtabstack* nextsymtab;
};

//Globals for the virtual stack machine

//"Registers"
int ip; //Instruction Pointer
int dp; //Data Pointer
int stp;//String Pointer
int sp; //Stack Pointer
int bp; //Base Pointer - To be Implemented later
int hp; //Heap Pointer - To be Implemented later

//Segment sizes
int cssize;
int dssize;
int sssize;

//Segments
char* cs; //Code Segment
char* ds; //Data Segment
char* ss; //String Segment

char compile_error;
jmp_buf a;
struct filestack* fstack;
char chartype[256];
int hextable[256];
struct keywordnode* keywordtable[32];
struct symtabstack* sstack;

//Functions
void gettoken();
void printtoken();
int lookupKeyword(char* kword);
void mystrncpy (char* dest, char* source, int size);
int createfilerecord(char* filename); 
int hashvalue(char* str, int n);
void compile();