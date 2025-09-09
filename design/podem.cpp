//PODEM algerithem

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>

#include "readckt.h"
#include "logicsim.h"

using namespace std;

int Dfrontier_flag;
int* Dfrontier_array;
std::vector<int*> Dfrontier_vector;

std::vector<int> obj_node;
std::vector<unsigned int> obj_val;

int backtraced_node;
unsigned int backtraced_val;

int podem_fail_count;

std::string logicValueToString(e_logic_value value) {
    switch (value) {
        case L_0: return "L_0";
        case L_1: return "L_1";
        case L_D: return "L_D";
        case L_DBAR: return "L_DBAR";
        case L_X: return "L_X";
        default: return "Unknown";
    }
}

void podem()
{ 
  levelize();

  // Get the faults from user input
  int fault_node, fault_type;
  int seed_podem, seed_podem_temp;
  podem_fail_count = 0;

  Dfrontier_array = new int[Nnodes];

  std::string output_file_name;
  std::istringstream iss(cp);

  iss >> fault_node >> fault_type >> output_file_name;

  std::cout << "Full command: " << cp << std::endl;
  std::cout << "Fault Node: " << fault_node << std::endl;
  std::cout << "Fault Type: " << fault_type << std::endl;
  std::cout << "Output File Name: " << output_file_name << std::endl;

  Dfrontier_vector.clear();
  obj_node.clear();
  obj_val.clear();
  struct n_struc *current_node;
  current_node = Pinput[0];

  while(1){
    //printf("current_node->index:%d\n",current_node->indx);
    Dfrontier_array[current_node->indx] = 0;
    if (current_node->next_node == NULL){
      current_node = Pinput[0];
      //printf("clearing Dfrontier_array done\n");
      //printf("current_node->index:%d\n",current_node->indx);
      break;
    }
    current_node = current_node->next_node;
  }

  Dfrontier_vector.push_back(Dfrontier_array);

  // Initialize value for each node
  //e_logic_value last_value;
  while(1){
    current_node->Dvalue_vector.push_back(L_X); // Initialize for PODEM
    //last_value = current_node->Dvalue_vector.back();
    //printf("current_node->index:%d, ",current_node->indx);
    //std::cout << "current_node->Dvalue_vector.back(): " << logicValueToString(last_value) << std::endl;
    if (current_node->next_node == NULL){
      break;
    }
    current_node = current_node->next_node;
  }

  // Call execute_podem
  int podem_result;
  podem_result = execute_podem(fault_node, fault_type);

  FILE * podem_report;
  podem_report = fopen(output_file_name.c_str(), "w");
  
  for (int i = 0; i < Npi; i++){
    if (i == Npi-1){
      fprintf(podem_report, "%d\n", Pinput[i]->num);
      break;
    }
    fprintf(podem_report, "%d,", Pinput[i]->num);
  }

  if (podem_result){
    printf("PODEM success!\n");
    // Print the result to file
    char fault_node_str[20], fault_type_str[20];

    for (int i = 0; i < Npi; i++){
      if (Pinput[i]->Dvalue_vector.back() == L_1){
        fprintf(podem_report, "1");
        if (i < Npi-1){
          fprintf(podem_report, ",");
        }
      } else if (Pinput[i]->Dvalue_vector.back() == L_0){
        fprintf(podem_report, "0");
        if (i < Npi-1){
          fprintf(podem_report, ",");
        }
      } else if (Pinput[i]->Dvalue_vector.back() == L_D){
          fprintf(podem_report, "1");
          if (i < Npi-1){
          fprintf(podem_report, ",");
        }
      } else if (Pinput[i]->Dvalue_vector.back() == L_DBAR){
        fprintf(podem_report, "0");
        if (i < Npi-1){
          fprintf(podem_report, ",");
        }
      } else{
        fprintf(podem_report, "x");
        if (i < Npi-1){
          fprintf(podem_report, ",");
        }
      }
    }
    fclose(podem_report);
  } else{
    printf("PODEM failed!\n");
  }
}

int execute_podem(int fault_node, int fault_type) {
  std::vector<int> backtraced_node_vector;
  std::vector<unsigned int> backtraced_val_vector;

  int count;
  int obj_flag;   //1 means objective success, 0 means fail
  int error_at_PO; //1 means D/DBAR at objective output
  int i;
  error_at_PO = 0;
  struct n_struc *current_node;

  printf("-------------execute_podem called--------------\n");

  podem_fail_count = podem_fail_count + 1;
  printf("podem_fail_count: %d\n", podem_fail_count);
  if (podem_fail_count > 5000) {
    return 0;
  }

  printf("current_node->num | current_node->Dvalue\n");
  current_node = Pinput[0];
  while(1){
    printf("          %d          |           %u\n", current_node->num, current_node->Dvalue_vector.back());
    if (current_node->next_node == NULL){
      break;
    }
    current_node = current_node->next_node;
  }

  for (i = 0; i < Npo; i++){
    if ((Poutput[i]->Dvalue_vector.back() == L_D) || (Poutput[i]->Dvalue_vector.back() == L_DBAR)){
       error_at_PO = 1;
      return 1;  // If error at PO, success. 
    }
  }

  // Call objective
  obj_flag = objective(fault_node, fault_type);
  if (obj_flag == 0){
    return 0;
    printf("objective failed\n");
  }
  printf("-----------------end objective----------------\n");

  // Call backtrace
  backtrace();
  backtraced_node_vector.push_back(backtraced_node);
  backtraced_val_vector.push_back(backtraced_val);
  
  current_node = num_sorted[backtraced_node_vector.back()];
  current_node->Dvalue_vector.push_back(static_cast<e_logic_value>(backtraced_val));

  //For debug 
  printf("current number %d, ", current_node->num);
  e_logic_value last_value;
  last_value = current_node->Dvalue_vector.back();
  std::cout << "value: " << logicValueToString(last_value) << std::endl;

  // Call imply
  imply(fault_node,  fault_type);

  int podem_success;
  podem_success = execute_podem(fault_node, fault_type);
  printf("-----------------Second podem called!-------------------\ncurrent node num: %d\n", backtraced_node_vector.back());
  if (podem_success){
    return 1;
  }

  current_node = num_sorted[backtraced_node_vector.back()];
  if (backtraced_val_vector.back() == 0){
    current_node->Dvalue_vector.push_back(L_1);
  } else if (backtraced_val_vector.back() == 1){
    current_node->Dvalue_vector.push_back(L_0);
  }

  imply(fault_node,  fault_type);

  podem_success = execute_podem(fault_node, fault_type);
  printf("------------------Third podem called------------------\ncurrent node number %d\n", backtraced_node_vector.back());
  if (podem_success){
    return 1;
  }

  current_node = num_sorted[backtraced_node_vector.back()];
  current_node->Dvalue_vector.push_back(L_X);

  imply(fault_node, fault_type);

  obj_node.pop_back();
  obj_val.pop_back();
  return 0;
}

int objective(int fault_node, int fault_type){
  int i, j;

  printf("-------------begin objective---------------\n");
  // if fault node value is L_X
  if (num_sorted[fault_node]->Dvalue_vector.back() == L_X){       
    obj_node.push_back(fault_node);
    if (fault_type){
      obj_val.push_back(L_0);
    } else {
      obj_val.push_back(L_1);
    }
    return 1;
  }

  // fault node wasnt excited
  if (fault_type) {
    if (num_sorted[fault_node]->Dvalue_vector.back() != L_DBAR){
      printf("Falut node %d does not excite\n", num_sorted[fault_node]->num);
      return 0;
    }
  } else {
    if (num_sorted[fault_node]->Dvalue_vector.back() != L_D){
      printf("Falut node %d does not excite\n", num_sorted[fault_node]->num);
      return 0;
    }
  }

  for (i = 0; i < Nnodes; i++){
    printf("d_frontier:%d valid:%d\n", i, Dfrontier_vector.back()[i]);
    if (Dfrontier_vector.back()[i] == 1){
      break;
    }
    else if (i == Nnodes-1){
      if (Dfrontier_vector.back()[i] == 0) {
        printf("no d frontier found!");
        return 0;
      }
    }
  }

  for (j = 0; j < num_sorted[num_array[i]]->fin; j++){
    if (num_sorted[num_array[i]]->unodes[j]->Dvalue_vector.back() == L_X) {
      break;
    }
    else if (j == num_sorted[num_array[i]]->fin - 1) {
      if (num_sorted[num_array[i]]->unodes[num_sorted[num_array[i]]->fin - 1]->Dvalue_vector.back() != L_X) {
        return 0;
      }
    }
  }

  obj_node.push_back(num_sorted[num_array[i]]->unodes[j]->num);

  if ((num_sorted[num_array[i]]->type == AND) || (num_sorted[num_array[i]]->type == NAND) || (num_sorted[num_array[i]]->type == XOR)){
    obj_val.push_back(L_1);
  } else if ((num_sorted[num_array[i]]->type == OR) || (num_sorted[num_array[i]]->type == NOR)){
    obj_val.push_back(L_0);
  } 

  return 1;
}

int backtrace(void){
    unsigned int v;
    int k, j;
    unsigned int i;
    int counter_1 = 0;
    int counter_0 = 0;

    v = obj_val.back();
    k = obj_node.back();

    printf("-----------------begin backtrace----------------\n");
    printf("backtrace k:%d\n", k);

    while (num_sorted[k]->type != IPT){
        switch (num_sorted[k]->type) {
            case AND:
            case OR:
                i = L_0;
                for (j = 0; j < num_sorted[k]->fin; j++){
                    if (num_sorted[k]->unodes[j]->Dvalue_vector.back() == L_X){
                        break;
                    }
                }
                v ^= i;
                k = num_sorted[k]->unodes[j]->num;
                break;

            case NAND:
            case NOR:
            case NOT:
                i = L_1;
                for (j = 0; j < num_sorted[k]->fin; j++){
                    if (num_sorted[k]->unodes[j]->Dvalue_vector.back() == L_X){
                        break;
                    }
                }
                v ^= i;
                k = num_sorted[k]->unodes[j]->num;
                break;

            case XOR:
                counter_1 = 0;
                counter_0 = 0;
                for (j = 0; j < num_sorted[k]->fin; j++){
                    unsigned int dvalue = num_sorted[k]->unodes[j]->Dvalue_vector.back();
                    if (dvalue == L_1){
                        counter_1++;
                    } else if (dvalue == L_0){
                        counter_0++;
                    }
                }
                {
                    unsigned int new_v;
                    if ((v == L_1 && counter_1 % 2 == 0) || (v == L_0 && counter_1 % 2 == 1)){
                        new_v = L_1;
                    } else {
                        new_v = L_0;
                    }
                    for (j = 0; j < num_sorted[k]->fin; j++){
                        if (num_sorted[k]->unodes[j]->Dvalue_vector.back() == L_X){
                            v = new_v;
                            k = num_sorted[k]->unodes[j]->num;
                            break;
                        }
                    }
                }
                break;

            case BRCH:
                i = L_0;
                v ^= i;
                k = num_sorted[k]->unodes[0]->num;
                break;

            default:
                printf("Unknown gate type %d\n", num_sorted[k]->type);
                return -1;
        }
    }

    backtraced_val = v;
    backtraced_node = k;
    printf("Backtraced to PI %d value: %u\n", backtraced_node, backtraced_val);
    printf("-----------------end backtrace----------------\n");
    return 0;
}

int imply(int fault_node,int fault_type){

  Dfrontier_array = new int[Nnodes];

  struct n_struc *current_node = Pinput[0];
  int i, j;
  int faultNode_flag=0;
  int unknown_flag = 0;
  e_logic_value temp_faultType;
  e_logic_value temp_Dvalue;
  //int has_D, has_L_DBAR, has_L_1_or_L_0;  // Used in some forward implication cases

  printf("-----------------begin imply----------------\n");

  // initialize Dfrontier_array
  while(1){
    Dfrontier_array[current_node->indx] = 0;
    if (current_node->next_node == NULL){
      current_node = Pinput[0];
      break;
    }
    current_node = current_node->next_node;
  }
  
  while (1){
    if (current_node->num == fault_node) {
      faultNode_flag = 1; //found D/L_DBAR
      printf("Forward propagate to fault node num: %d value: %u\n", current_node->num, current_node->Dvalue_vector.back());
    }

    switch (current_node->type){
      case BRCH:{
        current_node->Dvalue_vector.push_back(current_node->unodes[0]->Dvalue_vector.back()); 
        break;
      }

      case XOR:{
        for (i = 0; i < current_node->fin; i++){
          if (current_node->unodes[i]->Dvalue_vector.back() == L_X){
            unknown_flag = 1;
            for (i = 0; i < current_node->fin; i++){
              if (current_node->unodes[i]->Dvalue_vector.back() == L_D || current_node->unodes[i]->Dvalue_vector.back() == L_DBAR){
                Dfrontier_array[current_node->indx] = 1;
                printf("d_frontier node num: %d index: %d value: %u\n", current_node->num, current_node->indx, current_node->Dvalue_vector.back());
                break;
              }
            }
            break;
          } else {
            if (i == 0){
              temp_Dvalue = current_node->unodes[i]->Dvalue_vector.back();
            }
            else{
              temp_Dvalue = logic_imply(XOR, temp_Dvalue, current_node->unodes[i]->Dvalue_vector.back());
            }
          }
        }

        if (unknown_flag == 1){
          temp_Dvalue = L_X;
          unknown_flag = 0;
        }
        current_node->Dvalue_vector.push_back(temp_Dvalue) ;
      
        printf("XOR node num: %d value: %u\n", current_node->num, current_node->Dvalue_vector.back());
        break;
      }

      case OR:{
        for (i = 0; i < current_node->fin; i++){
          if (current_node->unodes[i]->Dvalue_vector.back() == L_X){
            unknown_flag = 1;
            for (i = 0; i < current_node->fin; i++){
              if (current_node->unodes[i]->Dvalue_vector.back() == L_D || current_node->unodes[i]->Dvalue_vector.back() == L_DBAR){
                Dfrontier_array[current_node->indx] = 1;
                printf("d_frontier node num: %d index: %d value: %u\n", current_node->num, current_node->indx, current_node->Dvalue_vector.back());
                break;
              }
            }
            break;
          } else {
            if (i == 0){
              temp_Dvalue = current_node->unodes[i]->Dvalue_vector.back();
            }
            else{
              temp_Dvalue = logic_imply(OR, temp_Dvalue, current_node->unodes[i]->Dvalue_vector.back());
            }
          }
        }

        if (unknown_flag == 1){
          temp_Dvalue = L_X;
          unknown_flag = 0;
        }
        current_node->Dvalue_vector.push_back(temp_Dvalue) ;
        
        printf("OR node num: %d value: %u\n", current_node->num, current_node->Dvalue_vector.back());
        break;
      }

      case NOR:{
        for (i = 0; i < current_node->fin; i++){
          if (current_node->unodes[i]->Dvalue_vector.back() == L_X){
            unknown_flag = 1;
            for (i = 0; i < current_node->fin; i++){
              if (current_node->unodes[i]->Dvalue_vector.back() == L_D || current_node->unodes[i]->Dvalue_vector.back() == L_DBAR){
                Dfrontier_array[current_node->indx] = 1;
                printf("d_frontier node num: %d index: %d value: %u\n", current_node->num, current_node->indx, current_node->Dvalue_vector.back());
                break;
              }
            }
            break;
          } else {
            if (i == 0){
              temp_Dvalue = current_node->unodes[i]->Dvalue_vector.back();
            }
            else{
              temp_Dvalue = logic_imply(OR, temp_Dvalue, current_node->unodes[i]->Dvalue_vector.back());
            }
          }
        }

        temp_Dvalue = logic_imply(NOT, temp_Dvalue, L_X);

        if (unknown_flag == 1){
          temp_Dvalue = L_X;
          unknown_flag = 0;
        }
        current_node->Dvalue_vector.push_back(temp_Dvalue) ;
        
        printf("NOR node num: %d value: %u\n", current_node->num, current_node->Dvalue_vector.back());
        break;
      }

      case NOT:{
        for (i = 0; i < current_node->fin; i++){
          if (current_node->unodes[i]->Dvalue_vector.back() == L_X){
            unknown_flag = 1;
            for (i = 0; i < current_node->fin; i++){
              if (current_node->unodes[i]->Dvalue_vector.back() == L_D || current_node->unodes[i]->Dvalue_vector.back() == L_DBAR){
                Dfrontier_array[current_node->indx] = 1;
                printf("d_frontier node num: %d index: %d value: %u\n", current_node->num, current_node->indx, current_node->Dvalue_vector.back());
                break;
              }
            }
            break;
          } else {
              temp_Dvalue = logic_imply(NOT, current_node->unodes[i]->Dvalue_vector.back(), L_X);
          }
        }

        if (unknown_flag == 1){
          temp_Dvalue = L_X;
          unknown_flag = 0;
        }
        current_node->Dvalue_vector.push_back(temp_Dvalue) ;
        
        printf("NOT node num: %d, value: %u\n", current_node->num, current_node->Dvalue_vector.back());
        break;
      }

      case NAND:{
        for (i = 0; i < current_node->fin; i++){
          if (current_node->unodes[i]->Dvalue_vector.back() == L_X){
            unknown_flag = 1;
            for (i = 0; i < current_node->fin; i++){
              if (current_node->unodes[i]->Dvalue_vector.back() == L_D || current_node->unodes[i]->Dvalue_vector.back() == L_DBAR){
                Dfrontier_array[current_node->indx] = 1;
                printf("d_frontier node num: %d index: %d value: %u\n", current_node->num, current_node->indx, current_node->Dvalue_vector.back());
                break;
              }
            }
            break;
          } else {
            if (i == 0){
              temp_Dvalue = current_node->unodes[i]->Dvalue_vector.back();
            }
            else{
              temp_Dvalue = logic_imply(AND, temp_Dvalue, current_node->unodes[i]->Dvalue_vector.back());
            }
          }
        }
       
        temp_Dvalue = logic_imply(NOT, temp_Dvalue, L_X);

        if (unknown_flag == 1){
          temp_Dvalue = L_X;
          unknown_flag = 0;
        }
        current_node->Dvalue_vector.push_back(temp_Dvalue);

        printf("NAND node num: %d value: %u\n", current_node->num, current_node->Dvalue_vector.back());
        break;
      }

      case AND:{
        for (i = 0; i < current_node->fin; i++){
          if (current_node->unodes[i]->Dvalue_vector.back() == L_X){
            unknown_flag = 1;
            for (i = 0; i < current_node->fin; i++){
              if (current_node->unodes[i]->Dvalue_vector.back() == L_D || current_node->unodes[i]->Dvalue_vector.back() == L_DBAR){
                Dfrontier_array[current_node->indx] = 1;
                printf("d_frontier node num: %d index: %d value: %u\n", current_node->num, current_node->indx, current_node->Dvalue_vector.back());
                break;
              }
            }
            break;
          } else {
            if (i == 0){
              temp_Dvalue = current_node->unodes[i]->Dvalue_vector.back();
            }
            else{
              temp_Dvalue = logic_imply(AND, temp_Dvalue, current_node->unodes[i]->Dvalue_vector.back());
            }
          }
        }

        if (unknown_flag == 1){
          temp_Dvalue = L_X;
          unknown_flag = 0;
        }

        current_node->Dvalue_vector.push_back(temp_Dvalue) ;
        
        printf("AND node num: %d value: %u\n", current_node->num, current_node->Dvalue_vector.back());
        break;
        }
      default:;
    }

    if (faultNode_flag) { //change 1,0 to D/L_DBAR
      if (fault_type){
        temp_faultType = L_1;
      }else {
        temp_faultType = L_0;
      }
      if(fault_node==current_node->num){
        if((temp_faultType == L_0 && current_node->Dvalue_vector.back() == L_1) || (temp_faultType == L_1 && current_node->Dvalue_vector.back() == L_0)){
          printf("enter second flag\n");
          if (current_node->Dvalue_vector.back()==L_1){
            current_node->Dvalue_vector.pop_back();
            current_node->Dvalue_vector.push_back(L_D);
            printf("change 1 to D\n");
          } else if (current_node->Dvalue_vector.back()==L_0){
            current_node->Dvalue_vector.pop_back();
            current_node->Dvalue_vector.push_back(L_DBAR);
            printf("change 0 to DBAR\n");
          }   
        }
      }
    }
    printf("Implied node num: %d, value:%u \n", current_node->num, current_node->Dvalue_vector.back());
    
    if (current_node->next_node == NULL)
    { 
      Dfrontier_flag = 0;
      for (j = 0; j < Nnodes; j++)
      {
        if (Dfrontier_array[j] == 1)
        {
         Dfrontier_flag = 1;
        }
      }
      Dfrontier_vector.push_back(Dfrontier_array); //save d_frontier to d_fontier_stack
      break;
    }
    current_node = current_node->next_node; 
  }
  printf("-----------------end imply----------------\n");
  return 1;
}

// Helper function for NOT gate (since NOT only has one input)
e_logic_value not_gate(e_logic_value a) {
    switch (a) {
        case L_0: return L_1;
        case L_1: return L_0;
        case L_D: return L_DBAR;
        case L_DBAR: return L_D;
        default: return L_X;
    }
}

// Unified logic gate function
e_logic_value logic_imply(e_gtype gate, e_logic_value a, e_logic_value b = L_X) {
    switch (gate) {
        case AND:
            if (a == L_0 || b == L_0) return L_0;
            if (a == L_1) return b;
            if (b == L_1) return a;
            if (a == L_D && b == L_D) return L_D;
            if (a == L_DBAR && b == L_DBAR) return L_DBAR;
            return L_X;

        case OR:
            if (a == L_1 || b == L_1) return L_1;
            if (a == L_0) return b;
            if (b == L_0) return a;
            if (a == L_D && b == L_D) return L_D;
            if (a == L_DBAR && b == L_DBAR) return L_DBAR;
            return L_X;

        case XOR:
            if (a == L_X || b == L_X) return L_X;
            if (a == L_0) return b;
            if (b == L_0) return a;
            if (a == L_1) return (b == L_1 ? L_0 : L_1);
            if (b == L_1) return (a == L_1 ? L_0 : L_1);
            if ((a == L_D && b == L_DBAR) || (a == L_DBAR && b == L_D)) return L_1;
            if ((a == L_D && b == L_D) || (a == L_DBAR && b == L_DBAR)) return L_0;
            return L_X;

        case NOT:
            return not_gate(a);
        
        default:
            return L_X;
    }
}