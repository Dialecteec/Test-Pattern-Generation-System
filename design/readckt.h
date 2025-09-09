// readckt.h

#ifndef READCKT_H
#define READCKT_H

#include <string>
#include <set>
#include <vector>

#define MAXLINE 4096               /* Input buffer size */
#define MAXNAME 4096               /* File name size */

#define Upcase(x) ((isalpha(x) && islower(x))? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x))? tolower(x) : (x))

enum e_com {READ, PC, HELP, QUIT, LEV, LOGICSIM, RTPG, RFL, DFS, PFS, TPFC, SCOAP, DALG, PODEM};
enum e_state {EXEC, CKTLD};         /* Gstate values */
enum e_ntype {GATE, PI, FB, PO};    /* column 1 of circuit format */
enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND, XNOR, BUF};  /* gate types */
enum e_logic_value {L_0, L_1, L_D, L_DBAR, L_X};

extern int max_num;

struct cmdstruc {
    char name[MAXNAME];        /* command syntax */
    void (*fptr)();            /* function pointer of the commands */
    enum e_state state;        /* execution state sequence */
};

typedef struct n_struc {
    unsigned indx;             /* node index(from 0 to NumOfLine - 1 */
    unsigned num;              /* line number(May be different from indx */
    enum e_ntype ntype;
    enum e_gtype type;         /* gate type */
    unsigned fin;              /* number of fanins */
    unsigned fout;             /* number of fanouts */
    struct n_struc **unodes;   /* pointer to array of up nodes */
    struct n_struc **dnodes;   /* pointer to array of down nodes */
    int level;                 /* level of the gate output */
    int value;                 /* logic value of the node*/
    std::vector<std::string> fault_list;
    int cc0; // SCOAP CC0 value
    int cc1; // SCOAP CC1 value
    int co;  // SCOAP CO value
    e_logic_value Dvalue;  /* logic value of the node */

    //Ziqi added for PODEM
    struct n_struc *next_node; /* pointer to next nodes */
    unsigned fin_count;        /* for **sorted */
    std::vector<e_logic_value> Dvalue_vector; 
} NSTRUC; 

#define NUMFUNCS 14

std::string gname(int tp);
void clear();
void allocate();
void cread();
void pc();
void help();
void quit();
void levelize();
void lev();
void logicsim();
void rtpg();
void rfl();
void dfs();
void pfs();
void tpfc();
void scoap();
void dalg();
void podem();

extern struct cmdstruc command[NUMFUNCS];
extern enum e_state Gstate;     /* global execution sequence */
extern NSTRUC *Node;            /* dynamic array of nodes */
extern NSTRUC **Pinput;         /* pointer to array of primary inputs */
extern NSTRUC **Poutput;        /* pointer to array of primary outputs */
extern int Nnodes;              /* number of nodes */
extern int Npi;                 /* number of primary inputs */
extern int Npo;                 /* number of primary outputs */
extern int Done;                /* status bit to terminate program */
extern std::string cp;          /* command string */

extern NSTRUC **num_sorted;
extern int *num_array;

#endif // READCK
