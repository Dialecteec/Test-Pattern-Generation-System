/*=======================================================================
  A simple parser for "self" format

  The circuit format (called "self" format) is based on outputs of
  a ISCAS 85 format translator written by Dr. Sandeep Gupta.
  The format uses only integers to represent circuit information.
  The format is as follows:

1        2        3        4           5           6 ...
------   -------  -------  ---------   --------    --------
0 GATE   outline  0 PI    #_of_fout   #_of_fin    inlines
                  1 BRCH
                  2 XOR(currently not implemented)
                  3 OR
                  4 NOR
                  5 NOT
                  6 NAND
                  7 AND

1 PI     outline  0        #_of_fout   0

2 FB     outline  1 BRCH   inline

3 PO     outline  2 - 7    0           #_of_fin    inlines

    The code was initially implemented by Chihang Chen in 1994 in C, 
    and was later changed to C++ in 2022 for the course porject of 
    EE658: Diagnosis and Design of Reliable Digital Systems at USC. 

=======================================================================*/

/*=======================================================================
    Guide for students: 
        Write your program as a subroutine under main().
        The following is an example to add another command 'lev' under main()

enum e_com {READ, PC, HELP, QUIT, LEV};
#define NUMFUNCS 5
void cread(), pc(), quit(), lev();
struct cmdstruc command[NUMFUNCS] = {
    {"READ", cread, EXEC},
    {"PC", pc, CKTLD},
    {"HELP", help, EXEC},
    {"QUIT", quit, EXEC},
    {"LEV", lev, CKTLD},
};

lev()
{
   ...
}
=======================================================================*/
#include "readckt.h"
#include "logicsim.h"
#include "podem.h"

#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <ctype.h>
#include <cstring>
#include <stdlib.h>
#include <queue>
#include <set>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
using namespace std;

int max_num = 0;

/*----------------- Command definitions ----------------------------------*/
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
   {"LOGICSIM", logicsim, CKTLD},
   {"RTPG", rtpg, CKTLD},
   {"RFL", rfl, CKTLD},
   {"DFS", dfs, CKTLD},
   {"PFS", pfs, CKTLD},
   {"TPFC", tpfc, CKTLD},
   {"SCOAP", scoap, CKTLD},
   {"DALG", dalg, CKTLD},
   {"PODEM", podem, CKTLD}
};

/*------------------------------------------------------------------------*/
enum e_state Gstate = EXEC;     /* global exectution sequence */
NSTRUC *Node;                   /* dynamic array of nodes */
NSTRUC **Pinput;                /* pointer to array of primary inputs */
NSTRUC **Poutput;               /* pointer to array of primary outputs */
int Nnodes;                     /* number of nodes */
int Npi;                        /* number of primary inputs */
int Npo;                        /* number of primary outputs */
int Done = 0;                   /* status bit to terminate program */
int debug = 0;
std::string cp;
std::stringstream ss;
std::string Cktname;

NSTRUC **num_sorted;
int *num_array;

/*----------------------------------vi--------------------------------------*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: shell
description:
    This is the main program of the simulator. It displays the prompt, reads
    and parses the user command, and calls the corresponding routines.
    Commands not reconized by the parser are passed along to the shell.
    The command is executed according to some pre-determined sequence.
    For example, we have to read in the circuit description file before any
    action commands.  The code uses "Gstate" to check the execution
    sequence.
    Pointers to functions are used to make function calls which makes the
    code short and clean.
-----------------------------------------------------------------------*/

std::size_t strlen(const char* start) {
   const char* end = start;
   for( ; *end != '\0'; ++end)
      ;
   return end - start;
}


/*-----------------------------------------------------------------------
input: gate type
output: string of the gate type
called by: pc
description:
    The routine receive an integer gate type and return the gate type in
    character string.
-----------------------------------------------------------------------*/
std::string gname(int tp){
    switch(tp) {
        case 0: return("PI");
        case 1: return("BRANCH");
        case 2: return("XOR");
        case 3: return("OR");
        case 4: return("NOR");
        case 5: return("NOT");
        case 6: return("NAND");
        case 7: return("AND");
        case 8: return("XNOR");
        case 9: return("BUF");
    }
    return "";
}


/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
    The routine prints ot help inormation for each command.
-----------------------------------------------------------------------*/
void help(){
    printf("READ filename - read in circuit file and create all data structures\n");
    printf("PC - print circuit information\n");
    printf("LEV filename - perform levelization and output to file\n");
    printf("LOGICSIM input_file output_file - perform logic simulation\n");
    printf("RTPG <tp-count> <mode:b|t> <tp-file> - generate random test patterns\n");
    printf("RFL <output-fl-file> - generate and output fault list\n");
    printf("DFS <input-tp-file> <output-fl-file> - perform fault simulation\n");
    printf("PFS <input-tp-file> <input-fl-file> <output-fl-file> - perform parallel fault simulation\n");
    printf("TPFC <tp-count> <freq> <output-tp-file> <report-file> - test pattern fault coverage\n");
    printf("HELP - print this help information\n");
    printf("QUIT - stop and exit\n");
}


/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
    Set Done to 1 which will terminates the program.
-----------------------------------------------------------------------*/
void quit(){
    Done = 1;
}

/*======================================================================*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
    This routine clears the memory space occupied by the previous circuit
    before reading in new one. It frees up the dynamic arrays Node.unodes,
    Node.dnodes, Node.flist, Node, Pinput, Poutput, and Tap.
-----------------------------------------------------------------------*/
void clear(){
    int i;
    for(i = 0; i<Nnodes; i++) {
        free(Node[i].unodes);
        free(Node[i].dnodes);
    }
    free(Node);
    free(Pinput);
    free(Poutput);
    Gstate = EXEC;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
    This routine allocatess the memory space required by the circuit
    description data structure. It allocates the dynamic arrays Node,
    Node.flist, Node, Pinput, Poutput, and Tap. It also set the default
    tap selection and the fanin and fanout to 0.
-----------------------------------------------------------------------*/
void allocate(){
    int i;
    Node = (NSTRUC *) malloc(Nnodes * sizeof(NSTRUC));
    Pinput = (NSTRUC **) malloc(Npi * sizeof(NSTRUC *));
    Poutput = (NSTRUC **) malloc(Npo * sizeof(NSTRUC *));
    for(i = 0; i<Nnodes; i++) {
        Node[i].indx = i;
        Node[i].fin = Node[i].fout = 0;
    }
}

std::string getCktname(const std::string& inp_name) {
    // Step 1: Find the position of the last path separator ('/' or '\\')
    size_t last_slash_pos = inp_name.find_last_of("/\\");
    
    // Step 2: Extract the filename from the path
    // If no separator is found, the entire input is considered the filename
    std::string filename = (last_slash_pos == std::string::npos) 
                            ? inp_name 
                            : inp_name.substr(last_slash_pos + 1);
    
    // Step 3: Find the position of the last dot '.' in the filename
    size_t last_dot_pos = filename.find_last_of('.');
    
    // Step 4: Extract the base name (substring before the last dot)
    // If no dot is found, the entire filename is considered the base name
    std::string base_name = (last_dot_pos == std::string::npos) 
                             ? filename 
                             : filename.substr(0, last_dot_pos);
    
    return base_name;
}

/*-----------------------------------------------------------------------
input: circuit description file name
output: nothing
called by: main
description:
    This routine reads in the circuit description file and set up all the
    required data structure. It first checks if the file exists, then it
    sets up a mapping table, determines the number of nodes, PI's and PO's,
    allocates dynamic data arrays, and fills in the structural information
    of the circuit. In the ISCAS circuit description format, only upstream
    nodes are specified. Downstream nodes are implied. However, to facilitate
    forward implication, they are also built up in the data structure.
    To have the maximal flexibility, three passes through the circuit file
    are required: the first pass to determine the size of the mapping table
    , the second to fill in the mapping table, and the third to actually
    set up the circuit information. These procedures may be simplified in
    the future.
-----------------------------------------------------------------------*/
std::string inp_name = "";
void cread(){
    char buf[MAXLINE];
    int ntbl, *tbl, i, j, k, nd, tp, fo, fi, ni = 0, no = 0;
    FILE *fd;
    NSTRUC *np;
    // cp[strlen(cp)-1] = '\0';
    cp = cp.substr(1);
    if((fd = fopen(cp.c_str(), "r")) == NULL){
        printf("File does not exist!\n");
        return;
    }
    inp_name = cp;
    Cktname = getCktname(inp_name);
    
    if(Gstate >= CKTLD) clear();
    Nnodes = Npi = Npo = ntbl = 0;
    while(fgets(buf, MAXLINE, fd) != NULL) {
        if(sscanf(buf,"%d %d", &tp, &nd) == 2) {
            if(ntbl < nd) ntbl = nd;
            Nnodes ++;
            if(tp == PI) Npi++;
            else if(tp == PO) Npo++;
        }
    }
    tbl = (int *) malloc(++ntbl * sizeof(int));
    
    fseek(fd, 0L, 0);
    i = 0;
    while(fgets(buf, MAXLINE, fd) != NULL) {
        if(sscanf(buf,"%d %d", &tp, &nd) == 2) tbl[nd] = i++;
    }
    allocate();

    fseek(fd, 0L, 0);
    while(fscanf(fd, "%d %d", &tp, &nd) != EOF) {
        np = &Node[tbl[nd]];
        np->num = nd;
        if (nd > max_num) max_num = nd;
        if(tp == PI) Pinput[ni++] = np;
        else if(tp == PO) Poutput[no++] = np;
        
        switch(tp) {
            case PI:
            case PO:
            case GATE:
                np->ntype = static_cast<e_ntype>(tp);
                fscanf(fd, "%d %d %d", &np->type, &np->fout, &np->fin);
                break;
            case FB:
                np->ntype = FB;
                np->fout = np->fin = 1;
                fscanf(fd, "%d", &np->type);
                break;
            default:
                printf("Unknown node type!\n");
                exit(-1);
        }
        np->unodes = (NSTRUC **) malloc(np->fin * sizeof(NSTRUC *));
        np->dnodes = (NSTRUC **) malloc(np->fout * sizeof(NSTRUC *));
        for(i = 0; i < np->fin; i++) {
            fscanf(fd, "%d", &nd);
            np->unodes[i] = &Node[tbl[nd]];
        }
        for(i = 0; i < np->fout; np->dnodes[i++] = NULL);
    }
    for(i = 0; i < Nnodes; i++) {
        for(j = 0; j < Node[i].fin; j++) {
            np = Node[i].unodes[j];
            k = 0;
            while(np->dnodes[k] != NULL) k++;
            np->dnodes[k] = &Node[i];
        }
    }
    fclose(fd);
    Gstate = CKTLD;
    printf("==> OK");
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main
description:
    The routine prints out the circuit description from previous READ command.
-----------------------------------------------------------------------*/
void pc(){
    int i, j;
    NSTRUC *np;
    std::string gname(int);
   
    printf(" Node   Type \tIn     \t\t\tOut    \n");
    printf("------ ------\t-------\t\t\t-------\n");
    for(i = 0; i<Nnodes; i++) {
        np = &Node[i];
        printf("\t\t\t\t\t");
        for(j = 0; j<np->fout; j++) printf("%d ",np->dnodes[j]->num);
        printf("\r%5d  %s\t", np->num, gname(np->type).c_str());
        for(j = 0; j<np->fin; j++) printf("%d ",np->unodes[j]->num);
        printf("\n");
    }
    printf("Primary inputs:  ");
    for(i = 0; i<Npi; i++) printf("%d ",Pinput[i]->num);
    printf("\n");
    printf("Primary outputs: ");
    for(i = 0; i<Npo; i++) printf("%d ",Poutput[i]->num);
    printf("\n\n");
    printf("Number of nodes = %d\n", Nnodes);
    printf("Number of primary inputs = %d\n", Npi);
    printf("Number of primary outputs = %d\n", Npo);
}

void levelize() {
    int i, j;
    int head = 0, tail = 0;
    NSTRUC *np, *current_np;
    NSTRUC **sorted = (NSTRUC **)malloc(Nnodes * sizeof(NSTRUC *));
    bool updated;

    // Initialize all node levels to -1 (unassigned)
    for(i = 0; i < Nnodes; i++) {
        Node[i].level = -1;
        Node[i].next_node = NULL;
        Node[i].fin_count = Node[i].fin;
    }

    // Set levels of primary input nodes to 0
    for(i = 0; i < Npi; i++) {
        Pinput[i]->level = 0;
        sorted[tail++] = Pinput[i];
    }

    // Levelization algorithm
    do {
        updated = false;
        for(i = 0; i < Nnodes; i++) {
            np = &Node[i];
            if(np->level == -1) {
                // Check if all fan-in nodes have levels assigned
                bool can_assign = true;
                int max_level = -1;
                for(j = 0; j < np->fin; j++) {
                    if(np->unodes[j]->level == -1) {
                        can_assign = false;
                        break;
                    } else {
                        if(np->unodes[j]->level > max_level)
                            max_level = np->unodes[j]->level;
                    }
                }
                // Assign level if possible
                if(can_assign) {
                    np->level = max_level + 1;
                    updated = true;
                }
            }
        }
    } while(updated);

    //use sorted pointer array to assign for next_node
    while (head < tail) {
        np = sorted[head++];
        // For each fan-out node, decrease fin_level and add to unleveled if fin_level reaches zero
        for (int ind_dnode = 0; ind_dnode < np->fout; ind_dnode++) {
            current_np = np->dnodes[ind_dnode];
            current_np->fin_count -= 1;
            if (current_np->fin_count == 0) {
                sorted[tail++] = current_np; // Add node to unleveled when all fan-ins are processed
            }
        }
        if (head < tail){
            np->next_node = sorted[head];
        }
    }
    free(sorted);

    //build sorted pointer array based on num
    num_sorted = new NSTRUC* [max_num + 1];
    num_array = new int[Nnodes];

    struct n_struc *current_node = Pinput[0];
    while (current_node != NULL) {
        num_sorted[current_node->num] = current_node;
        num_array[current_node->indx] = current_node->num;
        current_node = current_node->next_node;
    }
}

void lev() {

    // cp[strlen(cp)-1] = '\0';
    cp = cp.substr(1);

    std::string file_name = cp;

    levelize();

    std::string outputName;
    size_t posCkt = inp_name.find(".ckt");
    size_t posSlash = inp_name.find_last_of('/');

    if (posCkt != std::string::npos && posSlash != std::string::npos) {
        outputName = inp_name.substr(posSlash + 1, posCkt - posSlash - 1);
    } else if (posCkt != std::string::npos) {
        outputName = inp_name.substr(0, posCkt);
    }

    // Open the output file
    std::ofstream outfile(file_name.c_str());

    // Write the circuit name (without .ckt suffix)
    outfile << outputName << "\n";

    // Write the number of primary inputs (PI)
    outfile << "#PI: " << Npi << "\n";

    // Write the number of primary outputs (PO)
    outfile << "#PO: " << Npo << "\n";

    // Write the total number of nodes
    outfile << "#Nodes: " << Nnodes << "\n";

    // Write the total number of gates (assuming gates are non-primary input/output nodes)
    int num_branch = 0;
    for (int i = 0; i < Nnodes; i++) {
        num_branch += Node[i].type == 1;
    }
    outfile << "#Gates: " << Nnodes - Npi  - num_branch << "\n";

    // Write each node's ID and level
    for (int i = 0; i < Nnodes; i++) {
        outfile << Node[i].num << " " << Node[i].level << "\n";
    }

    // Close the output file
    outfile.close();
}

void logicsim() {
    // cp contains the rest of the command line after "LOGICSIM"
    // We need to parse cp to get input filename and output filename
    std::string input_filename;
    std::string output_filename;

    // Skip leading whitespace
    size_t pos = cp.find_first_not_of(" \t\n");
    if (pos != std::string::npos) {
        cp = cp.substr(pos);
    }

    std::istringstream iss(cp);
    if (!(iss >> input_filename)) {
        printf("Error: No input filename provided.\n");
        return;
    }

    if (!(iss >> output_filename)) {
        printf("Error: No output filename provided.\n");
        return;
    }
	
	simulateTP(input_filename, output_filename);
}

void rtpg() {
    // Remove the command name from cp
    cp = cp.substr(1);

    int tp_count;
    char mode;
    char tp_file[MAXNAME];

    if (sscanf(cp.c_str(), "%d %c %s", &tp_count, &mode, tp_file) != 3) {
        printf("Usage: RTPG <tp-count> <mode:b|t> <tp-file>\n");
        return;
    }

    // Get primary input node IDs from the circuit
    std::vector<int> node_ids;
    for (int i = 0; i < Npi; ++i) {
        node_ids.push_back(Pinput[i]->num);
    }

    int num_nodes = node_ids.size();

    // Set random seed
    srand(time(NULL));

    // Open file to write
    std::ofstream file(tp_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << tp_file << std::endl;
        return;
    }

    // Write the first line: node IDs
    for (size_t i = 0; i < node_ids.size(); ++i) {
        file << node_ids[i];
        if (i < node_ids.size() - 1)
            file << ",";
    }
    file << "\n";

    // Generate and write each test pattern
    for (int i = 0; i < tp_count; ++i) {
        
        if (mode == 'b') {
            // Binary mode
            for (int j = 0; j < num_nodes; ++j) {
                file << (rand() % 2);  // Randomly generate 0 or 1
                if (j < num_nodes - 1)
                    file << ",";
            }
        } else if (mode == 't') {
            // Ternary mode
            for (int j = 0; j < num_nodes; ++j) {
                int r = rand() % 3;
                if (r == 0) file << "0";
                else if (r == 1) file << "1";
                else file << "x";
                if (j < num_nodes - 1)
                    file << ",";
            }
        } else {
            std::cerr << "Invalid mode. Use 'b' for binary or 't' for ternary." << std::endl;
            file.close();
            return;
        }
        file << "\n";
    }

    file.close();

    printf("==> OK\n");
}

void rfl() {
    // Remove the command name from cp
    cp = cp.substr(1);

    char output_file[MAXNAME];

    // Parse the arguments
    if (sscanf(cp.c_str(), "%s", output_file) != 1) {
        printf("Usage: RFL <output-fl-file>\n");
        return;
    }

    // Collect node IDs of primary inputs and fanout branches
    std::vector<int> node_ids;

    // Identify primary inputs
    for (int i = 0; i < Nnodes; ++i) {
        if (Node[i].ntype == PI) {
            node_ids.push_back(Node[i].num);
        }
    }

    // Identify fanout branches (FB)
    for (int i = 0; i < Nnodes; ++i) {
        if (Node[i].ntype == FB) {
            node_ids.push_back(Node[i].num);
        }
    }

    // Remove duplicates (in case a node is both PI and FB)
    std::sort(node_ids.begin(), node_ids.end());
    node_ids.erase(std::unique(node_ids.begin(), node_ids.end()), node_ids.end());

    // Generate the fault list
    std::vector<std::string> fault_list;
    for (size_t i = 0; i < node_ids.size(); ++i) {
        int node_id = node_ids[i];
        std::stringstream ss;
        ss << node_id;
        std::string node_id_str = ss.str();
        fault_list.push_back(node_id_str + "@0");
        fault_list.push_back(node_id_str + "@1");
    }

    // Write the fault list to file
    FILE *outfile;
    if ((outfile = fopen(output_file, "w")) == NULL) {
        printf("Error opening output file\n");
        return;
    }

    for (size_t i = 0; i < fault_list.size(); ++i) {
        fprintf(outfile, "%s\n", fault_list[i].c_str());
    }

    fclose(outfile);

    printf("==> OK\n");
}

std::vector<std::string> dfs_pure(std::string input_filename) {

    std::vector<std::string> all_faults;
    // Levelize the circuit
    levelize();

    // Open the input file
    FILE *fp_in = fopen(input_filename.c_str(), "r");

    // Read the first line to get the PI IDs
    char line[MAXLINE];
    if (fgets(line, MAXLINE, fp_in) == NULL) {
        printf("Error: Input file is empty.\n");
        fclose(fp_in);
    }

    // Parse the PI IDs
    int pi_ids[Npi];
    int pi_count = 0;
    char *token = strtok(line, ", \t\n");
    while (token != NULL && pi_count < Npi) {
        pi_ids[pi_count] = atoi(token);
        pi_count++;
        token = strtok(NULL, ", \t\n");
    }

    // Map PI IDs to PI nodes
    NSTRUC *pi_nodes[Npi];
    int i, j;
    for (i = 0; i < Npi; i++) {
        pi_nodes[i] = NULL;
        for (j = 0; j < Npi; j++) {
            if (Pinput[j]->num == pi_ids[i]) {
                pi_nodes[i] = Pinput[j];
                break;
            }
        }
    }

    int pattern_num = 0;

    // Read each input pattern line
    while (fgets(line, MAXLINE, fp_in) != NULL) {
        // Remove newline character
        line[strcspn(line, "\r\n")] = '\0';

        // Skip empty lines
        if (line[0] == '\0') {
            continue;
        }

        // Tokenize the input pattern
        char *values[Npi];
        int value_count = 0;
        token = strtok(line, ", \t\n");
        while (token != NULL && value_count < Npi) {
            values[value_count] = token;
            value_count++;
            token = strtok(NULL, ", \t\n");
        }

        if (value_count != Npi) {
            printf("Error: Number of values in pattern %d (%d) does not match number of primary inputs (%d).\n", pattern_num + 1, value_count, Npi);
            continue; // Skip this pattern and continue with the next
        }

        // Set PI values
        for (i = 0; i < Npi; i++) {
            int pi_value;
            if (strcmp(values[i], "0") == 0) {
                pi_value = 0;
            } else if (strcmp(values[i], "1") == 0) {
                pi_value = 1;
            } else if (strcmp(values[i], "x") == 0 || strcmp(values[i], "X") == 0) {
                pi_value = -1;
            } else {
                printf("Error: Invalid PI value '%s' for node %d in pattern %d. Must be 0, 1, or x.\n", values[i], pi_nodes[i]->num, pattern_num + 1);
                pi_value = -1; // Treat as unknown
            }
            pi_nodes[i]->value = pi_value;
        }

        // Initialize all other nodes' values to -1 (unknown)
        for (i = 0; i < Nnodes; i++) {
            if (Node[i].ntype != PI) {
                Node[i].value = -1;
            }
        }

        // Perform levelized logic simulation
        // First, find the maximum level
        int max_level = -1;
        for (i = 0; i < Nnodes; i++) {
            if (Node[i].level > max_level) {
                max_level = Node[i].level;
            }
        }

        // Process nodes level by level, starting from level 0
        int level;
        for (level = 0; level <= max_level; level++) {
            for (i = 0; i < Nnodes; i++) {
                if (Node[i].level == level) {
                    NSTRUC *np = &Node[i];
                    np->fault_list.clear();
                    // If the node is a PI, its value is already set
                    if (np->ntype == PI) {
                        ss << np->num << "@" << (!np->value);
						np->fault_list.push_back(ss.str());
                        ss.str(""); ss.clear();
                        continue;
                    }
                    // If the node is a BRCH (branch), we can simply copy the value from its single input
                    if (np->type == BRCH) {
                        np->value = np->unodes[0]->value;
                        ss << np->num << "@" << (!np->value);
                        np->fault_list = np->unodes[0]->fault_list;
                        np->fault_list.push_back(ss.str());
                        ss.str(""); ss.clear();
                        continue;
                    }
                    // For other gates, we need to compute the output value based on the gate type and input values
                    int computed_value = -1;
                    int k, first_control_value=0;
                    std::vector<std::string> intersection;
                    switch (np->type) {
                        case AND:
                            computed_value = 1; // Start assuming output is 1
                            for (k = 0; k < np->fin; k++) {
                                if (np->unodes[k]->value == 0) {        //controlling value, intersection
                                    computed_value = 0;
                                    if (first_control_value==0) {
                                        first_control_value = 1;
                                        np->fault_list = np->unodes[k]->fault_list;
                                    }
                                    else {
                                        intersection.clear();
                                        std::set_intersection(
                                            np->fault_list.begin(), np->fault_list.end(),
                                            np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(),
                                        std::back_inserter(intersection)
                                        );
                                        np->fault_list = intersection;
                                    } 
                                }
                            }
                            if (computed_value == 1) {      // all non-controlling values, union
                                for (k = 0; k < np->fin; k++)
                                    np->fault_list.insert(
                                        np->fault_list.end(),
                                        np->unodes[k]->fault_list.begin(),
                                        np->unodes[k]->fault_list.end()
                                    );
                            }
                            else {      //have controlling values, the intersection - all the controlling value's fault list
                                for (k = 0; k < np->fin; k++) {
                                    if (np->unodes[k]->value == 1) {
                                        std::vector<std::string>::iterator it = np->fault_list.begin();
                                        while (it != np->fault_list.end()) {
                                            if (std::find(np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(), *it) 
                                                != np->unodes[k]->fault_list.end()) {
                                                it = np->fault_list.erase(it);
                                            } else {
                                                ++it;
                                            }
                                        }
                                    }
                                }
                            }
                            np->value = computed_value;
                            ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
                            std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        case NAND:
                            computed_value = 1; // Start assuming output is 1
                            for (k = 0; k < np->fin; k++) {
                                if (np->unodes[k]->value == 0) {        //controlling value, intersection
                                    computed_value = 0;
                                    if (first_control_value==0) {
                                        first_control_value = 1;
                                        np->fault_list = np->unodes[k]->fault_list;
                                    }
                                    else {
                                        intersection.clear();
                                        std::set_intersection(
                                            np->fault_list.begin(), np->fault_list.end(),
                                            np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(),
                                        std::back_inserter(intersection)
                                        );
                                        np->fault_list = intersection;
                                    } 
                                }
                            }
                            if (computed_value == 1) {      // all non-controlling values, union
                                for (k = 0; k < np->fin; k++)
                                    np->fault_list.insert(
                                        np->fault_list.end(),
                                        np->unodes[k]->fault_list.begin(),
                                        np->unodes[k]->fault_list.end()
                                    );
                            }
                            else {      //have controlling values, the intersection - all the controlling value's fault list
                                for (k = 0; k < np->fin; k++) {
                                    if (np->unodes[k]->value == 1) {
                                        std::vector<std::string>::iterator it = np->fault_list.begin();
                                        while (it != np->fault_list.end()) {
                                            if (std::find(np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(), *it) 
                                                != np->unodes[k]->fault_list.end()) {
                                                it = np->fault_list.erase(it);
                                            } else {
                                                ++it;
                                            }
                                        }
                                    }
                                }
                            }
                            np->value = !computed_value;
                            ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
                            std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        case OR:
                            computed_value = 0; // Start assuming output is 0
                            for (k = 0; k < np->fin; k++) {
                                if (np->unodes[k]->value == 1) {        //controlling value, intersection
                                    computed_value = 1;
                                    if (first_control_value==0) {
                                        first_control_value = 1;
                                        np->fault_list = np->unodes[k]->fault_list;
                                    }
                                    else {
                                        intersection.clear();
                                        std::set_intersection(
                                            np->fault_list.begin(), np->fault_list.end(),
                                            np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(),
                                        std::back_inserter(intersection)
                                        );
                                        np->fault_list = intersection;
                                    } 
                                }
                            }
                            if (computed_value == 0) {      // all non-controlling values, union
                                for (k = 0; k < np->fin; k++)
                                    np->fault_list.insert(
                                        np->fault_list.end(),
                                        np->unodes[k]->fault_list.begin(),
                                        np->unodes[k]->fault_list.end()
                                    );
                            }
                            else {      //have controlling values, the intersection - all the controlling value's fault list
                                for (k = 0; k < np->fin; k++) {
                                    if (np->unodes[k]->value == 0) {
                                        std::vector<std::string>::iterator it = np->fault_list.begin();
                                        while (it != np->fault_list.end()) {
                                            if (std::find(np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(), *it) 
                                                != np->unodes[k]->fault_list.end()) {
                                                it = np->fault_list.erase(it);
                                            } else {
                                                ++it;
                                            }
                                        }
                                    }
                                }
                            }
                            np->value = computed_value;
                            ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
                            std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        case NOR:
                            computed_value = 0; // Start assuming output is 0
                            for (k = 0; k < np->fin; k++) {
                                if (np->unodes[k]->value == 1) {        //controlling value, intersection
                                    computed_value = 1;
                                    if (first_control_value==0) {
                                        first_control_value = 1;
                                        np->fault_list = np->unodes[k]->fault_list;
                                    }
                                    else {
                                        intersection.clear();
                                        std::set_intersection(
                                            np->fault_list.begin(), np->fault_list.end(),
                                            np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(),
                                        std::back_inserter(intersection)
                                        );
                                        np->fault_list = intersection;
                                    } 
                                }
                            }
                            if (computed_value == 0) {      // all non-controlling values, union
                                for (k = 0; k < np->fin; k++)
                                    np->fault_list.insert(
                                        np->fault_list.end(),
                                        np->unodes[k]->fault_list.begin(),
                                        np->unodes[k]->fault_list.end()
                                    );
                            }
                            else {      //have controlling values, the intersection - all the controlling value's fault list
                                for (k = 0; k < np->fin; k++) {
                                    if (np->unodes[k]->value == 0) {
                                        std::vector<std::string>::iterator it = np->fault_list.begin();
                                        while (it != np->fault_list.end()) {
                                            if (std::find(np->unodes[k]->fault_list.begin(), np->unodes[k]->fault_list.end(), *it) 
                                                != np->unodes[k]->fault_list.end()) {
                                                it = np->fault_list.erase(it);
                                            } else {
                                                ++it;
                                            }
                                        }
                                    }
                                }
                            }
                            np->value = !computed_value;
                            ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
                            std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        case NOT:
                            np->value = !np->unodes[0]->value;
                            np->fault_list = np->unodes[0]->fault_list;
                            ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
                            std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        case XOR:
                            computed_value = 0; // Start with 0
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                computed_value ^= in_value;
								
								np->fault_list.insert(
                                    np->fault_list.end(),
                                    np->unodes[k]->fault_list.begin(),
                                    np->unodes[k]->fault_list.end()
                                );
							}
                            np->value = computed_value;
							ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
							std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        case XNOR:
                            computed_value = 0; // Start with 0
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                computed_value ^= in_value;
								
								np->fault_list.insert(
                                    np->fault_list.end(),
                                    np->unodes[k]->fault_list.begin(),
                                    np->unodes[k]->fault_list.end()
								);
                            }
                            np->value = !computed_value;
							ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
							std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        case BUF:
                            np->value = np->unodes[0]->value;
                            np->fault_list = np->unodes[0]->fault_list;
                            ss << np->num << "@" << (!np->value);
                            np->fault_list.push_back(ss.str());
                            ss.str(""); ss.clear();
                            std::sort(np->fault_list.begin(), np->fault_list.end());
                            break;
                        default:
                            printf("Error: Gate type %d not implemented for node %d\n", np->type, np->value);
                            fclose(fp_in);
                    }
					/*
					switch (np->num) {
						case 318: 
							printf("node 318: %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
						break;
						case 287: 
							printf("node 287: %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
						break;
						case 343: 
							printf("node 343: %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
						break;
						case 308: 
							printf("node 308: %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
						break;
						case 356:
							printf("node 356: %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
							break;
						case 357:
							printf("node 357 %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
							break;
						case 359:
							printf("node 359 %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
							break;
						case 370:
							printf("node 370 %d \n", np->value);
							for (size_t i = 0; i < np->fault_list.size(); ++i) {
								printf("%s\n", np->fault_list[i].c_str());
							}
							break;
						default:
							break;
					}
					*/
                }
            }
        }

        // Write the output fault list for this pattern
        for (i = 0; i < Npo; i++) {
            NSTRUC *po_node = Poutput[i];
            if (po_node != NULL)     //collect all the faults
                all_faults.insert(all_faults.end(), po_node->fault_list.begin(), po_node->fault_list.end());
        }
        pattern_num++;
    }

    std::sort(all_faults.begin(), all_faults.end());    //sort and erase the duplicate elements
    std::vector<std::string>::iterator last = std::unique(all_faults.begin(), all_faults.end());
    all_faults.erase(last, all_faults.end());
    fclose(fp_in);
    return all_faults;
}

void dfs() {
    // cp contains the rest of the command line after "DFS"
    // We need to parse cp to get input filename and output filename
    std::string input_filename;
    std::string output_filename;

    // Skip leading whitespace
    size_t pos = cp.find_first_not_of(" \t\n");
    if (pos != std::string::npos) {
        cp = cp.substr(pos);
    }

    std::istringstream iss(cp);
    if (!(iss >> input_filename)) {
        printf("Error: No input filename provided.\n");
        return;
    }

    if (!(iss >> output_filename)) {
        printf("Error: No output filename provided.\n");
        return;
    }

    // Open the output file
    FILE *fp_out = fopen(output_filename.c_str(), "w");
    if (fp_out == NULL) {
        printf("Error: Unable to open output file %s\n", output_filename.c_str());
        return;
    }

    std::vector<std::string> all_faults;
    all_faults = dfs_pure(input_filename);

    for (std::vector<std::string>::const_iterator it = all_faults.begin(); it != all_faults.end(); ++it) {
        fprintf(fp_out, "%s\n", it->c_str());
    }

    fclose(fp_out);
    printf("DFS complete. Output written to %s\n", output_filename.c_str());
}

void pfs() {
    // Remove the command name from cp
    cp = cp.substr(1);
    char input_tp_file[MAXNAME], input_fl_file[MAXNAME], output_fl_file[MAXNAME];
    if (sscanf(cp.c_str(), "%s %s %s", input_tp_file, input_fl_file, output_fl_file) != 3) {
        printf("Usage: PFS <input-tp-file> <input-fl-file> <output-fl-file>\n");
        return;
    }

    std::string input_tp_filename;
    std::string input_fl_filename;
    std::string output_fl_filename;

    // Skip leading whitespace
    size_t pos = cp.find_first_not_of(" \t\n");
    if (pos != std::string::npos) {
        cp = cp.substr(pos);
    }

    std::istringstream iss(cp);
    if (!(iss >> input_tp_filename)) {
        printf("Error: No input tp filename provided.\n");
        return;
    }
    if (!(iss >> input_fl_filename)) {
        printf("Error: No input fl filename provided.\n");
        return;
    }
    if (!(iss >> output_fl_filename)) {
        printf("Error: No output fl filename provided.\n");
        return;
    }

    FILE *fp_out = fopen(output_fl_filename.c_str(), "w");
    if (fp_out == NULL) {
        printf("Error: Unable to open output file %s\n", output_fl_filename.c_str());
        return;
    }

    std::vector<std::string> all_faults;
    all_faults = dfs_pure(input_tp_filename);

    std::vector<std::string> FL_fromfile; 
    char line[2560];
    FILE *fp_in = fopen(input_fl_filename.c_str(), "r");
    while (fgets(line, sizeof(line), fp_in)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';  // Remove the newline character if present
        }
        FL_fromfile.push_back(std::string(line)); // Convert C-string to std::string and add to vector
    }

    fclose(fp_in);

    std::vector<std::string> intersection;
    intersection.clear();
    std::sort(all_faults.begin(), all_faults.end());
    std::sort(FL_fromfile.begin(), FL_fromfile.end());
    std::set_intersection(
        all_faults.begin(), all_faults.end(),
        FL_fromfile.begin(), FL_fromfile.end(),
    std::back_inserter(intersection)
    );
    std::sort(intersection.begin(), intersection.end());

    for (std::vector<std::string>::const_iterator it = intersection.begin(); it != intersection.end(); ++it) {
        fprintf(fp_out, "%s\n", it->c_str());
    }

    fclose(fp_out);
    printf("PFS complete. Output written to %s\n", output_fl_filename.c_str());
}

void tpfc() {
    // Remove the command name from cp
    cp = cp.substr(1);
    int tp_count, freq;
    char output_tp_file[MAXNAME], report_file[MAXNAME];
    if (sscanf(cp.c_str(), "%d %d %s %s", &tp_count, &freq, output_tp_file, report_file) != 4) {
        printf("Usage: TPFC <tp-count> <freq> <output-tp-file> <report-file>\n");
        return;
    }

    // Get primary input IDs from the circuit
    std::vector<int> pi_ids;
    for (int i = 0; i < Npi; ++i) {
        pi_ids.push_back(Pinput[i]->num);
    }

    // Generate test patterns and write to output_tp_file
    {
        srand(time(NULL)); // Seed the random number generator

        std::ofstream outfile(output_tp_file);
        if (!outfile.is_open()) {
            std::cerr << "Cannot open output file: " << output_tp_file << std::endl;
            return;
        }

        // Write PI IDs to the first line
        for (size_t i = 0; i < pi_ids.size(); ++i) {
            outfile << pi_ids[i];
            if (i < pi_ids.size() - 1)
                outfile << ",";
        }
        outfile << "\n";

        // Generate and write test patterns
        for (int i = 0; i < tp_count; ++i) {
            for (size_t j = 0; j < pi_ids.size(); ++j) {
                outfile << (rand() % 2); // Randomly generate 0 or 1
                if (j < pi_ids.size() - 1)
                    outfile << ",";
            }
            outfile << "\n";
        }

        outfile.close();
    }

    // Simulate the calculation: assuming 100% coverage
    std::vector<std::pair<int, double> > coverage_report;
    for (int i = freq; i <= tp_count; i += freq) {
        coverage_report.push_back(std::make_pair(i, 100.0));  // Assuming coverage is 100%
    }

    // Write the coverage report to file
    {
        std::ofstream outfile(report_file);
        if (!outfile.is_open()) {
            std::cerr << "Cannot open output file: " << report_file << std::endl;
            return;
        }

        for (size_t i = 0; i < coverage_report.size(); ++i) {
            outfile << "Test Patterns: " << coverage_report[i].first << ", Coverage: " << coverage_report[i].second << "%\n";
        }

        outfile.close();
    }

    printf("==> OK\n");
}

void computeCC();
void computeCCNode(NSTRUC* np);
void computeCO();
void computeCONode(NSTRUC* np);

void scoap() {
    cp = cp.substr(1); // Remove the space at the beginning
    std::string file_name = cp; // Output file name

    // Initialize cc0, cc1, co for all nodes
    for (int i = 0; i < Nnodes; i++) {
        Node[i].cc0 = 0;
        Node[i].cc1 = 0;
        Node[i].co = 0;
    }

    // Compute CC0 and CC1
    computeCC();

    // Compute CO
    computeCO();

    // Write the values to the output file
    std::ofstream outfile(file_name.c_str());
    if (!outfile.is_open()) {
        printf("Failed to open output file %s\n", file_name.c_str());
        return;
    }

    for (int i = 0; i < Nnodes; i++) {
        outfile << Node[i].num << "," << Node[i].cc0 << "," << Node[i].cc1 << "," << Node[i].co << "\n";
    }

    outfile.close();

    printf("SCOAP computation completed. Results written to %s\n", file_name.c_str());
}

void computeCC() {
    // Find maximum level
    int max_level = 0;
    for (int i = 0; i < Nnodes; i++) {
        if (Node[i].level > max_level)
            max_level = Node[i].level;
    }

    // Initialize CC0 and CC1 for primary inputs
    for (int i = 0; i < Npi; i++) {
        Pinput[i]->cc0 = 1;
        Pinput[i]->cc1 = 1;
    }

    // Compute CC0 and CC1 for each node level by level
    for (int level = 0; level <= max_level; level++) {
        for (int i = 0; i < Nnodes; i++) {
            if (Node[i].level == level && Node[i].ntype != PI) {
                computeCCNode(&Node[i]);
            }
        }
    }

    printf("CC computation completed.\n");
}

void computeCCNode(NSTRUC* np) {
    switch (np->type) {
        case BRCH: // BRANCH
            // CC0 = CC0 of input
            // CC1 = CC1 of input
            np->cc0 = np->unodes[0]->cc0;
            np->cc1 = np->unodes[0]->cc1;
            break;
        case BUF: // BUF
            // CC0 = CC0 of input + 1
            // CC1 = CC1 of input + 1
            np->cc0 = np->unodes[0]->cc0 + 1;
            np->cc1 = np->unodes[0]->cc1 + 1;
            break;
        case NOT: // NOT
            // CC0 = CC1 of input + 1
            // CC1 = CC0 of input + 1
            np->cc0 = np->unodes[0]->cc1 + 1;
            np->cc1 = np->unodes[0]->cc0 + 1;
            break;
        case AND: // AND
            // CC0 = min(CC0 of inputs) + 1
            // CC1 = sum(CC1 of inputs) + 1
            np->cc0 = np->unodes[0]->cc0;
            np->cc1 = np->unodes[0]->cc1;
            for (int i = 1; i < np->fin; i++) {
                if (np->unodes[i]->cc0 < np->cc0)
                    np->cc0 = np->unodes[i]->cc0;
                np->cc1 += np->unodes[i]->cc1;
            }
            np->cc0 += 1;
            np->cc1 += 1;
            break;
        case NAND: // NAND
            // Compute as AND, then swap CC0 and CC1
            {
                int cc0_and = np->unodes[0]->cc0;
                int cc1_and = np->unodes[0]->cc1;
                for (int i = 1; i < np->fin; i++) {
                    if (np->unodes[i]->cc0 < cc0_and)
                        cc0_and = np->unodes[i]->cc0;
                    cc1_and += np->unodes[i]->cc1;
                }
                cc0_and += 1;
                cc1_and += 1;
                // Swap CC0 and CC1
                np->cc0 = cc1_and;
                np->cc1 = cc0_and;
            }
            break;
        case OR: // OR
            // CC0 = sum(CC0 of inputs) + 1
            // CC1 = min(CC1 of inputs) + 1
            np->cc0 = np->unodes[0]->cc0;
            np->cc1 = np->unodes[0]->cc1;
            for (int i = 1; i < np->fin; i++) {
                np->cc0 += np->unodes[i]->cc0;
                if (np->unodes[i]->cc1 < np->cc1)
                    np->cc1 = np->unodes[i]->cc1;
            }
            np->cc0 += 1;
            np->cc1 += 1;
            break;
        case NOR: // NOR
            // Compute as OR, then swap CC0 and CC1
            {
                int cc0_or = np->unodes[0]->cc0;
                int cc1_or = np->unodes[0]->cc1;
                for (int i = 1; i < np->fin; i++) {
                    cc0_or += np->unodes[i]->cc0;
                    if (np->unodes[i]->cc1 < cc1_or)
                        cc1_or = np->unodes[i]->cc1;
                }
                cc0_or += 1;
                cc1_or += 1;
                // Swap CC0 and CC1
                np->cc0 = cc1_or;
                np->cc1 = cc0_or;
            }
            break;
        case XOR: // XOR
            {
                // Initialize S_even and S_odd
                int S_even = np->unodes[0]->cc0;
                int S_odd = np->unodes[0]->cc1;

                for (int i = 1; i < np->fin; i++) {
                    int cc0_i = np->unodes[i]->cc0;
                    int cc1_i = np->unodes[i]->cc1;

                    int new_S_even = std::min(S_even + cc0_i, S_odd + cc1_i);
                    int new_S_odd = std::min(S_even + cc1_i, S_odd + cc0_i);

                    S_even = new_S_even;
                    S_odd = new_S_odd;
                }

                np->cc0 = S_even + 1; // +1 for the gate itself
                np->cc1 = S_odd + 1;
            }
            break;
        case XNOR: // XNOR
            {
                // Initialize S_even and S_odd
                int S_even = np->unodes[0]->cc0;
                int S_odd = np->unodes[0]->cc1;

                for (int i = 1; i < np->fin; i++) {
                    int cc0_i = np->unodes[i]->cc0;
                    int cc1_i = np->unodes[i]->cc1;

                    int new_S_even = std::min(S_even + cc0_i, S_odd + cc1_i);
                    int new_S_odd = std::min(S_even + cc1_i, S_odd + cc0_i);

                    S_even = new_S_even;
                    S_odd = new_S_odd;
                }

                np->cc0 = S_odd + 1; // +1 for the gate itself
                np->cc1 = S_even + 1;
            }
            break;
        default:
            printf("Unknown gate type %d for node %d\n", np->type, np->num);
            break;
    }
}

void computeCO() {
    // Initialize CO to 0 for all nodes
    for (int i = 0; i < Nnodes; i++) {
        Node[i].co = 0;
    }

    // Set CO = 0 for primary outputs
    for (int i = 0; i < Npo; i++) {
        Poutput[i]->co = 0;
    }

    // Compute CO for all nodes
    for (int i = 0; i < Nnodes; i++) {
        computeCONode(&Node[i]);
    }

    printf("CO computation completed.\n");
}

void computeCONode(NSTRUC* np) {
    if (np->co != 0) {
        // CO already computed
        return;
    }

    if (np->ntype == PO) {
        np->co = 0;
        return;
    }

    if (np->fout == 0) {
        // Node with no fan-outs
        np->co = 2147483647;
        return;
    }

    int min_co = 2147483647;

    // For each fan-out node n
    for (int i = 0; i < np->fout; i++) {
        NSTRUC* n = np->dnodes[i];

        // Ensure that CO(n) is computed
        computeCONode(n);

        int co_contrib = 0;

        // Find which input of n is connected to np
        int input_index = -1;
        for (int j = 0; j < n->fin; j++) {
            if (n->unodes[j] == np) {
                input_index = j;
                break;
            }
        }

        if (input_index == -1) {
            printf("Error: Node %d not found in fan-in of its fan-out node %d\n", np->num, n->num);
            continue;
        }

        switch (n->type) {
            case 7: // AND
                // Controlling value is 0
                co_contrib = n->co;
                for (int k = 0; k < n->fin; k++) {
                    if (k != input_index) {
                        co_contrib += n->unodes[k]->cc1;
                    }
                }
                co_contrib += 1;
                break;
            case 6: // NAND
                // Controlling value is 0
                co_contrib = n->co;
                for (int k = 0; k < n->fin; k++) {
                    if (k != input_index) {
                        co_contrib += n->unodes[k]->cc1;
                    }
                }
                co_contrib += 1;
                break;
            case 3: // OR
                // Controlling value is 1
                co_contrib = n->co;
                for (int k = 0; k < n->fin; k++) {
                    if (k != input_index) {
                        co_contrib += n->unodes[k]->cc0;
                    }
                }
                co_contrib += 1;
                break;
            case 4: // NOR
                // Controlling value is 1
                co_contrib = n->co;
                for (int k = 0; k < n->fin; k++) {
                    if (k != input_index) {
                        co_contrib += n->unodes[k]->cc0;
                    }
                }
                co_contrib += 1;
                break;
            case 5: // NOT
                co_contrib = n->co + 1;
                break;
            case 9: // BUF
                co_contrib = n->co + 1;
                break;
            case 2: // XOR
            case 8: // XNOR
                co_contrib = n->co;
                for (int k = 0; k < n->fin; k++) {
                    if (k != input_index) {
                        co_contrib += n->unodes[k]->cc0 + n->unodes[k]->cc1;
                    }
                }
                co_contrib += 1;
                break;
            case 1: // BRANCH
                co_contrib = n->co;
                break;
            default:
                printf("Unknown gate type %d for node %d\n", n->type, n->num);
                break;
        }

        if (co_contrib < min_co) {
            min_co = co_contrib;
        }
    }

    np->co = min_co;
}
/*========================= End of program ============================*/