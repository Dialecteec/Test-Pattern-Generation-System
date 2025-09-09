void podem();
int execute_podem(int fault_node, int fault_type);
int objective(int fault_node, int fault_type);
int backtrace(void);
int imply(int fault_node,int fault_type);
e_logic_value not_gate(e_logic_value a);
e_logic_value logic_imply(e_gtype gate, e_logic_value a, e_logic_value b);