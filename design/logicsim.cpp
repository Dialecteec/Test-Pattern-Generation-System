#include "readckt.h"
#include "logicsim.h"

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

#ifndef MAXLINE
#define MAXLINE 4096
#endif

void simulateTP(std::string input_filename, std::string output_filename) {

    // Levelize the circuit
    levelize();
    // Proceed with logic simulation

    // Open the input file
    FILE *fp_in = fopen(input_filename.c_str(), "r");
    if (fp_in == NULL) {
        printf("Error: Unable to open input file %s\n", input_filename.c_str());
        return;
    }

    // Read the first line to get the PI IDs
    char line[MAXLINE];
    if (fgets(line, MAXLINE, fp_in) == NULL) {
        printf("Error: Input file is empty.\n");
        fclose(fp_in);
        return;
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

    if (pi_count != Npi) {
        printf("Error: Number of PI IDs in the first line (%d) does not match number of primary inputs (%d).\n", pi_count, Npi);
        fclose(fp_in);
        return;
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
        if (pi_nodes[i] == NULL) {
            printf("Error: PI node with ID %d not found.\n", pi_ids[i]);
            fclose(fp_in);
            return;
        }
    }

    // Open the output file
    FILE *fp_out = fopen(output_filename.c_str(), "w");
    if (fp_out == NULL) {
        printf("Error: Unable to open output file %s\n", output_filename.c_str());
        fclose(fp_in);
        return;
    }

    // Write the header line for outputs (PO IDs)
    for (i = 0; i < Npo; i++) {
        if (i == 0) fprintf(fp_out, "%d", Poutput[i]->num);
        else fprintf(fp_out, ",%d", Poutput[i]->num);
    }
    fprintf(fp_out, "\n");

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
            if (Node[i].type != IPT) {
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
                    // If the node is a PI, its value is already set
                    if (np->type == IPT) {
                        continue;
                    }
                    // If the node is a BRCH (branch), we can simply copy the value from its single input
                    if (np->type == BRCH) {
                        np->value = np->unodes[0]->value;
                        continue;
                    }
                    // For other gates, we need to compute the output value based on the gate type and input values
                    int computed_value = -1;
                    int k;
                    switch (np->type) {
                        case AND:
                            computed_value = 1; // Start assuming output is 1
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                if (in_value == 0) {
                                    computed_value = 0;
                                    break;
                                } else if (in_value == -1) {
                                    computed_value = -1;
                                    // Do not break; need to check for inputs being 0
                                }
                            }
                            np->value = computed_value;
                            break;
                        case NAND:
                            computed_value = 1; // Start assuming output is 1
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                if (in_value == 0) {
                                    computed_value = 0;
                                    break;
                                } else if (in_value == -1) {
                                    computed_value = -1;
                                    // Do not break
                                }
                            }
                            // Now invert the computed_value
                            if (computed_value == -1)
                                np->value = -1;
                            else
                                np->value = !computed_value;
                            break;
                        case OR:
                            computed_value = 0; // Start assuming output is 0
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                if (in_value == 1) {
                                    computed_value = 1;
                                    break;
                                } else if (in_value == -1) {
                                    computed_value = -1;
                                    // Do not break; need to check for inputs being 1
                                }
                            }
                            np->value = computed_value;
                            break;
                        case NOR:
                            computed_value = 0; // Start assuming output is 0
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                if (in_value == 1) {
                                    computed_value = 1;
                                    break;
                                } else if (in_value == -1) {
                                    computed_value = -1;
                                    // Do not break
                                }
                            }
                            // Now invert the computed_value
                            if (computed_value == -1)
                                np->value = -1;
                            else
                                np->value = !computed_value;
                            break;
                        case NOT:
                            if (np->unodes[0]->value == -1)
                                np->value = -1;
                            else
                                np->value = !np->unodes[0]->value;
                            break;
                        case XOR:
                            computed_value = 0; // Start with 0
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                if (in_value == -1) {
                                    computed_value = -1; // Unknown
                                    break;
                                } else {
                                    computed_value ^= in_value;
                                }
                            }
                            np->value = computed_value;
                            break;
                        case XNOR:
                            computed_value = 0; // Start with 0
                            for (k = 0; k < np->fin; k++) {
                                int in_value = np->unodes[k]->value;
                                if (in_value == -1) {
                                    computed_value = -1; // Unknown
                                    break;
                                } else {
                                    computed_value ^= in_value;
                                }
                            }
                            if (computed_value != -1)
                                np->value = !computed_value;
                            else
                                np->value = -1;
                            break;
                        case BUF:
                            np->value = np->unodes[0]->value;
                            break;
                        default:
                            printf("Error: Gate type %d not implemented for node %d\n", np->type, np->value);
                            fclose(fp_in);
                            fclose(fp_out);
                            return;
                    }
                }
            }
        }

        // Write the output values for this pattern
        for (i = 0; i < Npo; i++) {
            NSTRUC *po_node = Poutput[i];

            // Handle the first element separately (no leading comma)
            if (i == 0) {
                if (po_node->value == -1) {
                    fprintf(fp_out, "x");  // no comma before the first element
                } else {
                    fprintf(fp_out, "%d", po_node->value);  // no comma before the first element
                }
            } else {
                // For subsequent elements, include the leading comma
                if (po_node->value == -1) {
                    fprintf(fp_out, ",x");
                } else {
                    fprintf(fp_out, ",%d", po_node->value);
                }
            }
        }
        fprintf(fp_out, "\n");

        pattern_num++;
    }

    fclose(fp_in);
    fclose(fp_out);

    printf("Logic simulation complete. Output written to %s\n", output_filename.c_str());
}