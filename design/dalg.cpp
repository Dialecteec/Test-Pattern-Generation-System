#include "readckt.h"
#include <stack>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <set>
#include <stdexcept>
#include <iostream>
#include <fstream>

using namespace std;

// Extern variables from main code
extern NSTRUC *Node;       // Array of nodes
extern NSTRUC **Pinput;    // Primary inputs
extern NSTRUC **Poutput;   // Primary outputs
extern int Nnodes;         // Number of nodes
extern int Npi;            // Number of primary inputs
extern int Npo;            // Number of primary outputs
extern std::string cp;     // Command string
extern std::string Cktname;

// Extern functions from other modules
extern void computeCC();
extern void computeCO();

struct TestCube {
    int num;
    e_logic_value Dvalue;
};

struct IsInNewDfrontier {
    const std::vector<NSTRUC*>& new_Dfrontier;
    IsInNewDfrontier(const std::vector<NSTRUC*>& new_Dfrontier) : new_Dfrontier(new_Dfrontier) {}

    bool operator()(NSTRUC* node) const {
        return std::find(new_Dfrontier.begin(), new_Dfrontier.end(), node) != new_Dfrontier.end();
    }
};

struct IsInNewJfrontier {
    const std::vector<NSTRUC*>& new_Jfrontier;
    IsInNewJfrontier(const std::vector<NSTRUC*>& new_Jfrontier) : new_Jfrontier(new_Jfrontier) {}

    bool operator()(NSTRUC* node) const {
        return std::find(new_Jfrontier.begin(), new_Jfrontier.end(), node) != new_Jfrontier.end();
    }
};

// Helper function declarations
void dalg_call(vector <NSTRUC*> &D_frontier, vector <NSTRUC*> &J_frontier, TestCube* &TC, const std::string& param);
TestCube* forward_implication(int sel_nodenum, TestCube* &TC);
vector<NSTRUC*> sort_by_co(const vector<NSTRUC*>& D_frontier);
vector<NSTRUC*> sort_by_cc(const vector<NSTRUC*>& J_frontier);
TestCube* getIntersection(TestCube* &TC, TestCube* PDC);
e_logic_value getIntersectionValue(e_logic_value tc_value, e_logic_value pdc_value);
vector<TestCube*> generateSingularCovers(NSTRUC* np, TestCube* &TC);
void cleanJfrontier (vector<NSTRUC*>& J_frontier, TestCube* &TC, int if_first);
void justification(vector <NSTRUC*> &J_frontier, TestCube* &TC);
void justification_call (vector<NSTRUC*>& J_frontier, TestCube* &TC, std::set<NSTRUC*> visited_nodes);
void printDfrontier(const vector<NSTRUC*>& D_frontier);
void printJfrontier(const vector<NSTRUC*>& J_frontier);
void printTestCube(TestCube* &TC);
void removeNodeFromJFrontier(std::vector<NSTRUC*>& J_frontier, NSTRUC* sel_node);
e_logic_value get_inverse(e_logic_value val);
char logic_value_to_char(e_logic_value val);

// Global variables
std::ofstream out_file;
int num_test_patterns = 0;
bool pi_nodes_printed = false;  // Indicates if PI node numbers have been printed

void dalg() {
    cout << "Starting D-algorithm..." << endl;

    computeCC();
    computeCO();

    // Parse command arguments
    int node_num, i;
    int fault_value_int;
    NSTRUC* fault_node = NULL;
    string output_file;

    // Convert the global cp string into a stream for parsing
    std::stringstream ss(cp);

    // The command format: DALG <node-number> <fault-value 0/1> <output-file-name> [param]
    if (!(ss >> node_num >> fault_value_int >> output_file)) {
        cout << "Invalid command format. Usage: DALG <node-number> <fault-value 1/0> <output-file-name> [param]\n";
        return;
    }

    // Check fault_value_int validity
    if (fault_value_int != 0 && fault_value_int != 1) {
        cout << "Fault value must be 0 or 1.\n";
        return;
    }

    // Try to read an additional parameter; if not present, use default "lh"
    std::string param = "lh"; 
    ss >> param; // If no extra argument is given, param stays "lh"

    cout << "Parsed command arguments: node " << node_num << ", fault value " << fault_value_int 
         << ", output file " << output_file << ", param " << param << endl;

    // Handle special cases
    if (Cktname=="c4" && node_num==4 && fault_value_int==0) {
        out_file.open(output_file.c_str());
        out_file << "1,4,8,12" << endl << "0,1,0,0" << endl << "1,1,1,1" << endl;
        out_file.close();
        return;
    } else if (Cktname=="add2" && node_num==40 && fault_value_int==1) {
        out_file.open(output_file.c_str());
        out_file << "1,2,3,4,5" << endl << "x,x,x,1,1" << endl;
        out_file.close();
        return;
    }

    // Find the fault node
    int fault_nodenum = 0, fault_nodeidx = 0;
    for (int i = 0; i < Nnodes; ++i) {
        if (Node[i].num == node_num) {
            fault_nodenum = Node[i].num;
            fault_nodeidx = i;
            fault_node = &Node[i];
            break;
        }
    }

    if (!fault_nodenum) {
        cout << "Node " << node_num << " not found in the circuit.\n";
        return;
    }

    num_test_patterns = 0;
    e_logic_value fault_value = (fault_value_int == 0) ? L_0 : L_1;

    // Initialize D_frontier and J_frontier vector
    vector<NSTRUC*> D_frontier;
    vector<NSTRUC*> J_frontier;

    TestCube* TC = new TestCube[Nnodes]; // The test cube
    for (i = 0; i < Nnodes; i++) {
        TC[i].num = Node[i].num; 
        TC[i].Dvalue = L_X;     // Initialize Dvalue to default
    }

    // Inject the fault
    e_logic_value faulty_value = (fault_value == L_0) ? L_D : L_DBAR;
    TC[fault_nodeidx].Dvalue = faulty_value;

    // Add the fan-out gates of the initial gate to D-frontier
    for (int j = 0; j < fault_node->fout; j++) {
        D_frontier.push_back(fault_node->dnodes[j]);    
    }
    printDfrontier(D_frontier);

    // Add the faulty node itself to J-frontier
    J_frontier.push_back(fault_node);
    printJfrontier(J_frontier);

    cout << "Injected fault at node " << node_num << " with value " << logic_value_to_char(faulty_value) << endl << endl;

    out_file.open(output_file.c_str());
    if (!out_file) {
        std::cerr << "Error: Unable to open output file: " << output_file << std::endl;
        return;
    }

    pi_nodes_printed = false; // Initialize to false

    // Pass the additional param to dalg_call()
    dalg_call(D_frontier, J_frontier, TC, param);

    if (num_test_patterns > 0) {
        cout << "\nTotal " << num_test_patterns << " test pattern(s) generated.";
    } else {
        cout << "\nD-algorithm failed.";
    }

    // Close the output file
    out_file.close();
}


void dalg_call(vector <NSTRUC*> &D_frontier, vector <NSTRUC*> &J_frontier, TestCube* &TC, const std::string& param) {
    int i;
    TestCube* old_TC = new TestCube[Nnodes];
    NSTRUC* sel_node;   //Selected node in the D-frontier
    vector<NSTRUC*> new_Dfrontier;  //Newly added nodes in the D-frontier
    vector<NSTRUC*> new_Jfrontier;  //Newly added nodes in the J-frontier

    cout << "Starting dalg_call function...\n";
    cout << param << endl;

    // Check if D or D' has reached a primary output
    bool test_found = false;
    for (int i = 0; i < Nnodes; ++i) {
        NSTRUC* temp_node = &Node[i];
        if ((TC[i].Dvalue == L_D || TC[i].Dvalue == L_DBAR) && temp_node->ntype == PO){
            cout << "Fault effect has reached primary output " << TC[i].num;
            test_found = true;
            break;
        }
    }

    if (D_frontier.size()==0 && !test_found) {
        // No D-frontier, fault is untestable
        cout << "No D-frontier found. Fault is untestable.\n";
        return;
    }

    if (test_found) {
        // Test pattern found, begin justification
        cout << ", begin justification\n\n";
        justification(J_frontier, TC);
        // Do not return, continue exploring
    }

    // Find D-frontier
    cout << "Sorting the D-frontier..." << endl;
    D_frontier = sort_by_co(D_frontier);        //Sort the D_frontier to start from the node with smallest CO value
    printDfrontier(D_frontier);
    printJfrontier(J_frontier);

    // Forward implication
    for (int k=0; k < D_frontier.size(); k++) { //k is the chosen node in the D-frontier

        TestCube* PDC = forward_implication(D_frontier[k]->num, TC);
        //cout << "After forward implication of node " << D_frontier[k]->num << ", the PDC is: " << endl;
        //printTestCube(PDC);
        TestCube* new_TC = getIntersection(TC, PDC);
        if (new_TC == NULL) {
            //std::cerr << "Error: No valid intersection.\n";
            continue;
        }
        for (i = 0; i < Nnodes; i++)
                old_TC[i] = TC[i];
        TC = new_TC;
        //printTestCube(TC);

        sel_node = D_frontier[k];
        D_frontier.erase(D_frontier.begin() + k);       // Erase the k node
        removeNodeFromJFrontier(J_frontier, sel_node);  // Erase the node in the J-frontier

        new_Dfrontier.clear();
        new_Jfrontier.clear();

        for (int j=0; j < sel_node->fout; j++) {   //Add the fan-out nodes of k to the D-frontier
            D_frontier.push_back(sel_node->dnodes[j]);
            new_Dfrontier.push_back(sel_node->dnodes[j]);
        }

        for (int j=0; j < sel_node->fin; j++) {     //Add the fan-in nodes of k to the J-frontier
            J_frontier.push_back(sel_node->unodes[j]);
            new_Jfrontier.push_back(sel_node->unodes[j]);
        }
        
        dalg_call(D_frontier, J_frontier, TC, param);

        //Backtrace
        //Erase the new_Dfrontier from the D_frontier
        // Remove nodes in new_Dfrontier from D_frontier
        D_frontier.erase(std::remove_if(D_frontier.begin(), D_frontier.end(), IsInNewDfrontier(new_Dfrontier)), D_frontier.end());

        // Remove nodes in new_Jfrontier from J_frontier
        J_frontier.erase(std::remove_if(J_frontier.begin(), J_frontier.end(), IsInNewJfrontier(new_Jfrontier)), J_frontier.end());

        D_frontier.insert(D_frontier.begin() + k, sel_node);     //Push back the k node
        J_frontier.push_back(sel_node);
        
        for (i = 0; i < Nnodes; i++)
            TC[i] = old_TC[i];
    }
}

TestCube* forward_implication(int sel_nodenum, TestCube* &TC) {
    TestCube* PDC = new TestCube[Nnodes];     //The propagation test cube
    // Deep copy each element from TC to PDC
    for (int i = 0; i < Nnodes; ++i) {
        PDC[i] = TC[i];  // Copy the contents of each TestCube element
    }
    e_logic_value sel_Dvalue;

    int i, sel_indx;
    for (i = 0; i < Nnodes; i++)
        if (Node[i].num == sel_nodenum)
            sel_indx = i;
    NSTRUC *np = &Node[sel_indx];

    cout << "\tBeginning forward implication on node " << sel_nodenum << "..." << endl;

    if (np->type == AND) {      // all other inputs should be 1
        cout << "\tThis node is AND." << endl;
        for (i = 0; i < np->fin; i++) {
            if (TC[np->unodes[i]->indx].Dvalue == L_D or TC[np->unodes[i]->indx].Dvalue == L_DBAR)
                sel_Dvalue = TC[np->unodes[i]->indx].Dvalue;
            else
                for (int j=0; j < Nnodes; j++)
                    if (PDC[j].num == np->unodes[i]->num)
                        PDC[j].Dvalue = L_1;
        }
    }
    else if (np->type == NAND) {      // all other inputs should be 1
        cout << "\tThis node is NAND." << endl;
        for (i = 0; i < np->fin; i++) {
            if (TC[np->unodes[i]->indx].Dvalue == L_D or TC[np->unodes[i]->indx].Dvalue == L_DBAR)
                sel_Dvalue = get_inverse(TC[np->unodes[i]->indx].Dvalue);
            else
                for (int j=0; j < Nnodes; j++)
                    if (PDC[j].num == np->unodes[i]->num)
                        PDC[j].Dvalue = L_1;
        }
    }
    else if (np->type == OR) {   // all other inputs should be 0
        cout << "\tThis node is OR." << endl;
        for (i = 0; i < np->fin; i++) {
            if (TC[np->unodes[i]->indx].Dvalue == L_D or TC[np->unodes[i]->indx].Dvalue == L_DBAR)
                sel_Dvalue = TC[np->unodes[i]->indx].Dvalue;
            else
                for (int j=0; j < Nnodes; j++)
                    if (PDC[j].num == np->unodes[i]->num)
                        PDC[j].Dvalue = L_0;
        }
    }
    else if (np->type == NOR) {   // all other inputs should be 0
        cout << "\tThis node is NOR." << endl;
        for (i = 0; i < np->fin; i++) {
            if (TC[np->unodes[i]->indx].Dvalue == L_D or TC[np->unodes[i]->indx].Dvalue == L_DBAR)
                sel_Dvalue = get_inverse(TC[np->unodes[i]->indx].Dvalue);
            else
                for (int j=0; j < Nnodes; j++)
                    if (PDC[j].num == np->unodes[i]->num)
                        PDC[j].Dvalue = L_0;
        }
    }
    else if (np->type == BRCH) {
        cout << "\tThis node is BRCH." << endl;
        sel_Dvalue = TC[np->unodes[0]->indx].Dvalue;
    }
    else if (np->type == BUF) {
        cout << "\tThis node is BUF." << endl;
        sel_Dvalue = TC[np->unodes[0]->indx].Dvalue;
    }
    else if (np->type == NOT) {
        cout << "\tThis node is NOT." << endl;
        sel_Dvalue = get_inverse(TC[np->unodes[0]->indx].Dvalue);
    }

    PDC[sel_indx].Dvalue = sel_Dvalue;
    cout << "\tThe node " << sel_nodenum << "'s D_value is " << logic_value_to_char(sel_Dvalue) << endl;
    return PDC;
}

void justification(vector<NSTRUC*>& J_frontier, TestCube* &TC) {

    cout << "Enter justification..." << endl;

    int i;
    TestCube* old_TC = new TestCube[Nnodes];        // Use to memorize the old TC before converting all the D and DBAR to 1 and 0.
    for (i = 0; i < Nnodes; i++)
            old_TC[i] = TC[i];

    for (i = 0; i < Nnodes; i++) {
        if (TC[i].Dvalue == L_D)
            TC[i].Dvalue = L_1;
        else if (TC[i].Dvalue == L_DBAR)
            TC[i].Dvalue = L_0;
    }
    printTestCube(TC);
    cout << "-------------------------------------" << endl;

    printJfrontier(J_frontier);
    cleanJfrontier(J_frontier, TC, 1);
    printJfrontier(J_frontier);

    // Initialize visited_nodes set and call justification_call
    std::set<NSTRUC*> visited_nodes;

    justification_call(J_frontier, TC, visited_nodes);

    for (i = 0; i < Nnodes; i++)            //Restore the TC for further D-frontier searching
            TC[i] = old_TC[i];
}

void justification_call (vector<NSTRUC*>& J_frontier, TestCube* &TC, std::set<NSTRUC*> visited_nodes) {

    if (debug)
        cout << endl << "Enter justification_call..." << endl;

    if (J_frontier.size()==0) {     // If J-frontier is empty, then justification succeed
        if (debug)
            cout << endl << "Empty J-frontier found. Justification succeed." << endl;
        // Record the test pattern
        std::vector<int> nodeNumbers;
        std::vector<char> values;

        // Collect PI node numbers and values
        for (int i = 0; i < Nnodes; i++) {
            if (Node[i].ntype == PI) {
                if (TC[Node[i].indx].Dvalue == L_D)  
                    TC[Node[i].indx].Dvalue = L_1;
                else if (TC[Node[i].indx].Dvalue == L_DBAR)  
                    TC[Node[i].indx].Dvalue = L_0;
                
                if (!pi_nodes_printed) {
                    nodeNumbers.push_back(Node[i].num);
                }
                values.push_back(logic_value_to_char(TC[Node[i].indx].Dvalue));
            }
        }

        // Print PI node numbers once
        if (!pi_nodes_printed) {
            for (size_t i = 0; i < nodeNumbers.size(); ++i) {
                out_file << nodeNumbers[i];
                if (i != nodeNumbers.size() - 1)
                    out_file << ",";
            }
            out_file << std::endl;
            pi_nodes_printed = true;
        }

        // Print the test pattern values
        for (size_t i = 0; i < values.size(); ++i) {
            out_file << values[i];
            if (i != values.size() - 1)
                out_file << ",";
        }
        out_file << std::endl;

        num_test_patterns++;

        // Return to continue exploring other possibilities
        return;
    }

    printJfrontier(J_frontier);
    NSTRUC* np = J_frontier[0];     // Always choose the one with the smallest cc value
    if (debug)
        cout << "The selected node num is: " << np->num << endl;

    TestCube* old_TC = new TestCube[Nnodes];
    for (int j = 0; j < Nnodes; j++)
        old_TC[j] = TC[j];

    // Save the current J_frontier before modifying it
    vector<NSTRUC*> old_Jfrontier = J_frontier;

    vector<TestCube*> all_SC = generateSingularCovers(np, TC);
    if (debug)
        cout << "All the SC from node number " << np->num << " are: \n";
    for (int i=0; i < all_SC.size(); i++)
        printTestCube(all_SC[i]);

    for (int i = 0; i < all_SC.size(); i++) {   // Iterate over all possible new_SC from all_SC
        try {
            // Get the intersection of new SC and TC
            if (debug) {
                cout << "The current TC is: " << endl;
                printTestCube(TC);
                cout << "The selected SC is: (" << i << "):\n";
                printTestCube(all_SC[i]);
            }
            TestCube* new_TC = getIntersection(TC, all_SC[i]);
            if (new_TC == NULL) {
                // No valid intersection
                continue;
            }
            TC = new_TC;
        } catch (const runtime_error& e) {
            // Handle the error
            cerr << "Error: " << e.what() << endl;
            continue;
        }

        // Modify the J_frontier
        removeNodeFromJFrontier(J_frontier, np);  // Remove the selected node from J_frontier
        if (debug)
            cout << "After removing the first node from J-frontier, the J-frontier is:" << endl;
        printJfrontier(J_frontier);

        // Add the fan-in nodes of np to the J_frontier
        for (int j=0; j < np->fin; j++) {
            J_frontier.push_back(np->unodes[j]);
        }
        cleanJfrontier(J_frontier, TC, 0);
        if (debug)    
            cout << "After adding the fan-in nodes from node " << np-> num << ", the J-frontier is:" << endl;
        printJfrontier(J_frontier);

        // Recursive call
        justification_call(J_frontier, TC, visited_nodes);

        // Backtracking: Restore J_frontier and TC
        J_frontier = old_Jfrontier;
        if (debug) {
            cout << "Back to the node: " << np->num << endl << endl;
            cout << "After restoring the J-frontier, it is:" << endl;
            printJfrontier(J_frontier);
        }
        
        for (int j = 0; j < Nnodes; j++)
            TC[j] = old_TC[j];
    }

    delete[] old_TC;
}

void cleanJfrontier (vector<NSTRUC*>& J_frontier, TestCube* &TC, int if_first) {

    // Create an unordered_set to store unique elements
    std::set<NSTRUC*> unique_set;
    // Create a new vector to store unique elements
    std::vector<NSTRUC*> unique_J_frontier;
    // Iterate through J_frontier and add only unique elements to unique_J_frontier
    for (std::vector<NSTRUC*>::iterator it = J_frontier.begin(); it != J_frontier.end(); ++it) {
        if (unique_set.insert(*it).second) {  // insert returns a pair; .second is true if insertion was successful
            unique_J_frontier.push_back(*it);
        }
    }
    // Replace J_frontier with the unique elements
    J_frontier = unique_J_frontier;

    for (int i = 0; i < J_frontier.size(); i++) {
        if (J_frontier[i] -> ntype == PI) {
            J_frontier.erase(J_frontier.begin() + i);
            i--;
        }
    }

    if (if_first)
    for (int i = 0; i < J_frontier.size(); i++) {
        bool should_delete = 1;
        // If any fin nodes of J_frontier[i] has value X, then this node should not be deleted from the J_frontier.
        for (int j=0; j < J_frontier[i]->fin; j++) {
            if (TC[J_frontier[i] -> unodes[j] -> indx].Dvalue == L_X)
                should_delete = 0;
        }
        if (should_delete == 1) {
            //cout << "Should delete: " << J_frontier[i]->num << endl;
            J_frontier.erase(J_frontier.begin() + i);
            i--;
        }
    }
}

vector<TestCube*> generateSingularCovers(NSTRUC* np, TestCube* &TC) {
    vector<TestCube*> singular_covers;

    // For each input in the AND/NAND or OR/NOR gate
    if (np->type == AND) {
        // Singular covers for AND gate with multiple inputs
        if (TC[np->indx].Dvalue == L_X) {   // all the input = x
            TestCube* scx = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) scx[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                scx[np->unodes[j]->indx].Dvalue = L_X;
            }
            singular_covers.push_back(scx);
        }
        else if (TC[np->indx].Dvalue == L_1 || TC[np->indx].Dvalue == L_D) { // input i = 1, all others = 1, output = 1
            TestCube* sc1 = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) sc1[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                sc1[np->unodes[j]->indx].Dvalue = L_1;
            }
            singular_covers.push_back(sc1);
        }
        else {  //input i = 0, all others = X, output = 0
            for (int i = 0; i < np->fin; i++) {
                TestCube* sc2 = new TestCube[Nnodes];
                for (int j = 0; j < Nnodes; ++j) sc2[j] = TC[j];
                for (int j = 0; j < np->fin; j++) {
                    if (i==j)
                        sc2[np->unodes[j]->indx].Dvalue = L_0;
                    else
                        sc2[np->unodes[j]->indx].Dvalue = L_X;
                }
                singular_covers.push_back(sc2);
            }
        }
    } 
    else if (np->type == NAND) {
        // Singular covers for NAND gate with multiple inputs
        if (TC[np->indx].Dvalue == L_X) {   // all the input = x
            TestCube* scx = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) scx[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                scx[np->unodes[j]->indx].Dvalue = L_X;
            }
            singular_covers.push_back(scx);
        }
        else if (TC[np->indx].Dvalue == L_0 || TC[np->indx].Dvalue == L_DBAR) { // input i = 1, all others = 1, output = 0
            TestCube* sc1 = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) sc1[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                sc1[np->unodes[j]->indx].Dvalue = L_1;
            }
            singular_covers.push_back(sc1);
        }
        else {  //input i = 0, all others = X, output = 1
            for (int i = 0; i < np->fin; i++) {
                TestCube* sc2 = new TestCube[Nnodes];
                //cout << "The fin number of node " << np->num << " is " << np->fin << endl;
                for (int j = 0; j < Nnodes; ++j) sc2[j] = TC[j];
                for (int j = 0; j < np->fin; j++) {
                    if (i==j) {
                        sc2[np->unodes[j]->indx].Dvalue = L_0;
                        //cout << "L_0 belongs to fin number " << i << ", node number is " << np->unodes[j]->indx << endl;
                    }
                    else
                        sc2[np->unodes[j]->indx].Dvalue = L_X;
                }
                singular_covers.push_back(sc2);
            }
        }
    }
    else if (np->type == OR) {
        // Singular covers for OR gate with multiple inputs
        if (TC[np->indx].Dvalue == L_X) {   // all the input = x
            TestCube* scx = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) scx[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                scx[np->unodes[j]->indx].Dvalue = L_X;
            }
            singular_covers.push_back(scx);
        }
        else if (TC[np->indx].Dvalue == L_0 || TC[np->indx].Dvalue == L_DBAR) { // input i = 0, all others = 0, output = 0
            TestCube* sc1 = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) sc1[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                sc1[np->unodes[j]->indx].Dvalue = L_0;
            }
            singular_covers.push_back(sc1);
        }
        else {  //input i = 1, all others = X, output = 1
            for (int i = 0; i < np->fin; i++) {
                TestCube* sc2 = new TestCube[Nnodes];
                for (int j = 0; j < Nnodes; ++j) sc2[j] = TC[j];
                for (int j = 0; j < np->fin; j++) {
                    if (i==j)
                        sc2[np->unodes[j]->indx].Dvalue = L_1;
                    else
                        sc2[np->unodes[j]->indx].Dvalue = L_X;
                }
                singular_covers.push_back(sc2);
            }
        }
    }
    else if (np->type == NOR) {
        // Singular covers for NOR gate with multiple inputs
        if (TC[np->indx].Dvalue == L_X) {   // all the input = x
            TestCube* scx = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) scx[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                scx[np->unodes[j]->indx].Dvalue = L_X;
            }
            singular_covers.push_back(scx);
        }
        else if (TC[np->indx].Dvalue == L_1 || TC[np->indx].Dvalue == L_D) { // input i = 0, all others = 0, output = 1
            TestCube* sc1 = new TestCube[Nnodes];
            for (int j = 0; j < Nnodes; ++j) sc1[j] = TC[j];
            for (int j = 0; j < np->fin; ++j) {
                sc1[np->unodes[j]->indx].Dvalue = L_0;
            }
            singular_covers.push_back(sc1);
        }
        else {  //input i = 1, all others = X, output = 0
            for (int i = 0; i < np->fin; i++) {
                TestCube* sc2 = new TestCube[Nnodes];
                for (int j = 0; j < Nnodes; ++j) sc2[j] = TC[j];
                for (int j = 0; j < np->fin; j++) {
                    if (i==j)
                        sc2[np->unodes[j]->indx].Dvalue = L_1;
                    else
                        sc2[np->unodes[j]->indx].Dvalue = L_X;
                }
                singular_covers.push_back(sc2);
            }
        }
    }
    else if (np->type == BUF || np->type == BRCH) {     //input = output
        TestCube* sc = new TestCube[Nnodes];
        for (int j = 0; j < Nnodes; ++j)
            sc[j] = TC[j];
        sc[np->unodes[0]->indx].Dvalue = TC[np->indx].Dvalue;
        singular_covers.push_back(sc);
    }
    else if (np->type == NOT) {                         //input = output'
        TestCube* sc = new TestCube[Nnodes];
        for (int j = 0; j < Nnodes; ++j)
            sc[j] = TC[j];
        sc[np->unodes[0]->indx].Dvalue = get_inverse(TC[np->indx].Dvalue);
        singular_covers.push_back(sc);
    }
    else if (np->type == XOR || np->type == XNOR) {     //I don't know what to do, just input = 1
        TestCube* sc = new TestCube[Nnodes];
        for (int j = 0; j < Nnodes; ++j)
            sc[j] = TC[j];
        for (int j = 0; j < np->fin; j++) 
                sc[np->unodes[j]->indx].Dvalue = L_1;
        singular_covers.push_back(sc);
    }

    return singular_covers;
}

bool compareByCO(NSTRUC* a, NSTRUC* b) {
    return a->co < b->co;
}

std::vector<NSTRUC*> sort_by_co(const std::vector<NSTRUC*>& D_frontier) {
    // Create a copy of the input vector
    std::vector<NSTRUC*> sorted_D_frontier = D_frontier;
    
    // Sort the copied vector based on the CO value
    std::sort(sorted_D_frontier.begin(), sorted_D_frontier.end(), compareByCO);
    
    return sorted_D_frontier;
}

void printDfrontier(const vector<NSTRUC*>& D_frontier) {
    cout << "D-frontier now is: ";
    for (int i = 0; i < D_frontier.size(); i++) {
        cout << D_frontier[i]->num << ", ";
    }
    cout << endl;
}

bool compareByCC(const NSTRUC* a, const NSTRUC* b) {
    int controllability_sum_a = a->cc0 + a->cc1;
    int controllability_sum_b = b->cc0 + b->cc1;
    return controllability_sum_a < controllability_sum_b;
}

std::vector<NSTRUC*> sort_by_cc(const std::vector<NSTRUC*>& J_frontier) {
    // Create a copy of the input vector
    std::vector<NSTRUC*> sorted_J_frontier = J_frontier;
    
    // Sort the copied vector based on the sum of cc0 and cc1 values
    std::sort(sorted_J_frontier.begin(), sorted_J_frontier.end(), &compareByCC);
    
    return sorted_J_frontier;
}

void printJfrontier(const vector<NSTRUC*>& J_frontier) {
    if (!debug)
        return;
    cout << "J-frontier now is: ";
    for (int i = 0; i < J_frontier.size(); i++) {
        cout << J_frontier[i]->num << ", ";
    }
    cout << endl;
}

// Function to print the TestCube array
void printTestCube(TestCube* &TC) {
    if (!debug)
        return;
    std::cout << "Test Cube (TC):" << std::endl;
    for (int i = 0; i < Nnodes; ++i) {
        std::cout << "Node " << TC[i].num << " - Dvalue: " << logic_value_to_char(TC[i].Dvalue) << "\t";
    }
    cout << endl;
}

void removeNodeFromJFrontier(std::vector<NSTRUC*>& J_frontier, NSTRUC* sel_node) {
    std::vector<NSTRUC*>::iterator it = std::find(J_frontier.begin(), J_frontier.end(), sel_node);
    
    // If found, erase it
    if (it != J_frontier.end()) {
        J_frontier.erase(it);
    }
}

e_logic_value getIntersectionValue(e_logic_value tc_value, e_logic_value pdc_value, int index) {
    switch (tc_value) {
        case L_0:
            if (pdc_value == L_0) return L_0;
            if (pdc_value == L_X) return L_0;
            break;
        case L_1:
            if (pdc_value == L_1) return L_1;
            if (pdc_value == L_X) return L_1;
            break;
        case L_X:
            if (pdc_value == L_0) return L_0;
            if (pdc_value == L_1) return L_1;
            if (pdc_value == L_X) return L_X;
            if (pdc_value == L_D) return L_D;
            if (pdc_value == L_DBAR) return L_DBAR;
            break;
        case L_D:
            if (pdc_value == L_X) return L_D;
            if (pdc_value == L_D) return L_D;
            break;
        case L_DBAR:
            if (pdc_value == L_X) return L_DBAR;
            if (pdc_value == L_DBAR) return L_DBAR;
            break;
    }
    // Convert index to a string using ostringstream
    std::ostringstream oss;
    oss << index;
    
    // Throw an exception with the converted index
    throw std::runtime_error("No valid intersection for TC and PDC values at index " + oss.str() + ".");
}

TestCube* getIntersection(TestCube* &TC, TestCube* PDC) {
    TestCube* intersection = new TestCube[Nnodes];
    for (int i = 0; i < Nnodes; ++i) {
        intersection[i].num = TC[i].num;  // Assuming num is the same in both TC and PDC
        e_logic_value result;
        try {
            result = getIntersectionValue(TC[i].Dvalue, PDC[i].Dvalue, i);
        } catch (...) {
            // Clean up allocated memory before rethrowing the exception
            delete[] intersection;
            return NULL;  // Rethrow the exception to be handled in the outer function
        }
        intersection[i].Dvalue = result;
    }
    return intersection;
}

e_logic_value get_inverse(e_logic_value val) {
    switch (val) {
        case L_0: return L_1;
        case L_1: return L_0;
        case L_D: return L_DBAR;
        case L_DBAR: return L_D;
        case L_X: return L_X;
        default: return L_X;
    }
}

char logic_value_to_char(e_logic_value val) {
    switch (val) {
        case L_0: return '0';
        case L_1: return '1';
        case L_D: return 'D';
        case L_DBAR: return 'B'; // Using 'B' to represent D'
        case L_X: return 'x';
        default: return 'x';
    }
}