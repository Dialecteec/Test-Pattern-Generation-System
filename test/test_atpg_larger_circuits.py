import os
import argparse
import pandas as pd
import subprocess
import time


TEST_DIR = "auto-tests-phase3-atpg-large"
CIRCUIT_DIR = "./ckts_all"
FAULT_DIR = "./larger_circuits_faults.csv"
CMD_DIR = f"{TEST_DIR}/cmds"
OUTPUT_DIR = f"{TEST_DIR}/outputs"

DALG_CMD_TESTS_DIR  = f"{CMD_DIR}/dalg" 
DALG_OUTPUT_DIR  = f"{OUTPUT_DIR}/dalg" 
PODEM_CMD_TESTS_DIR  = f"{CMD_DIR}/podem" 
PODEM_OUTPUT_DIR  = f"{OUTPUT_DIR}/podem" 


ATPG_CMD_DIR = {"dalg": DALG_CMD_TESTS_DIR, "podem": PODEM_CMD_TESTS_DIR}
ATPG_OUTPUT_DIR = {"dalg": DALG_OUTPUT_DIR, "podem": PODEM_OUTPUT_DIR}


class bcolors:
    GRN = '\033[92m'
    RED = '\033[91m'
    ORG = '\033[93m'
    ENDC = '\033[0m'
    BGD  = '\033[44m'
    BOLD = '\033[1m'


def get_args():
    parser = argparse.ArgumentParser(description ='Which function to test')
    parser.add_argument("-tlim", type=float, default=30,
            help="Time limit in seconds")
    parser.add_argument("-alg", type=str, 
            help="ATPG algorithm (DALG/PODEM)")

    return parser.parse_args()


def path_cmd(ckt, alg, fault_node, fault_val):
    cmd_dir = ATPG_CMD_DIR[alg]
    cmd_fname = f"{cmd_dir}/{ckt}_{alg}_{fault_node}_{fault_val}.cmd"
    return cmd_fname


def path_output(ckt, alg, fault_node, fault_val):
    output_dir = ATPG_OUTPUT_DIR[alg]
    output_fname = f"{output_dir}/{ckt}_{alg}_{fault_node}_{fault_val}.out"
    return output_fname 
    

def test_cmd(cmd, alg, tlim=1):
    cmd = cmd.split("/")[-1].replace(".cmd", "")
    ckt, _, fault_node, fault_val = cmd.split("_")
    cmd_path = path_cmd(ckt, alg, fault_node, fault_val)
    output_path = path_output(ckt, alg, fault_node, fault_val)

    msg = f"cd build/; ./simulator < ../{cmd_path} > /dev/null"
    process = subprocess.Popen(msg, shell=True)
    
    current_time=time.time()
    while time.time() < current_time + tlim and process.poll() is None:
        pass
    
    time_exceeded = False
    if process.poll() is None:
        process.kill()
        # process.terminate()
        print(f"{bcolors.ORG}[TLIM] {cmd} exceeded ({tlim} s){bcolors.ENDC}")
        return "TLIM"
    
    ed_time = f"{time.time() - current_time:.2f} s"
    if correct_answer(output_path, detectable):
        print(f"{bcolors.GRN}[PASS] '{cmd}'\t({ed_time}) {bcolors.ENDC}")
        return "PASS"
    
    err_message = 'The fault is detectable but no tp is found' if detectable else 'The fault is undetectable but tp is found'
    
    print(f"{bcolors.RED}[FAIL]: '{cmd}'\t({ed_time}) {err_message} {bcolors.ENDC}")
    return "FAIL"


def gen_cmd_atpg_single(ckt, alg, fault_node, fault_val):
    cmd_dir = ATPG_CMD_DIR[alg]
    cmd_fname = path_cmd(ckt, alg, fault_node, fault_val) 
    output_fname = path_output(ckt, alg, fault_node, fault_val) 
    with open(cmd_fname, 'w') as outfile:
        outfile.write(f"READ ../{CIRCUIT_DIR}/{ckt}.ckt\n")
        outfile.write(f"{alg.upper()} {fault_node} {fault_val} ../{output_fname}\n")
        outfile.write("QUIT")
    return cmd_fname


def correct_answer(output_fname, detectable):
    """Using fd_csv"""
    ckt, _, fault_node, fault_val = output_fname.split("/")[-1].strip(".out").split("_")
    fault = f"{fault_node}@{fault_val}"

    answer_result = open(output_fname,'r').readlines()
    

    if len(answer_result)>1:
        # test pattern found
        passed = True if detectable else False
    else:
        # not test pattern found
        passed = False if detectable else True
    return passed


if __name__ == '__main__':
    tlim = get_args().tlim
    alg = get_args().alg.lower()


    os.system("rm -rf build; mkdir build && cd build; cmake ..; make")
    if not os.path.exists('./build/simulator'):
        print('No simulator could be found')
        exit(1)
        
    os.system(f"rm -rf {TEST_DIR}")
    os.system(f"mkdir {TEST_DIR}")
    os.system(f"mkdir {CMD_DIR}")
    os.system(f"mkdir {ATPG_CMD_DIR[alg]}")
    os.system(f"mkdir {OUTPUT_DIR}")
    os.system(f"mkdir {ATPG_OUTPUT_DIR[alg]}")


    if alg not in ["dalg", "podem"]:
        raise ValueError(f"Algorithm {alg} is not accepted. ")
        
    fault_df = pd.read_csv(FAULT_DIR)
    cmds = []
    ckt_res = {"PASS":0, "FAIL":0, "TLIM":0, "GFAIL":0}
    for index, row in fault_df.iterrows():
        circuit_name = row['CKT']
        fault = row['Fault']
        detectable = row['Detectable']
        fault_node, fault_val = fault.split("@")
        # print(f"Testing Circuit: {circuit_name}, Fault: {fault}, Detectable: {detectable}")
        cmd = gen_cmd_atpg_single(circuit_name, alg, fault_node, fault_val)

        try:
            res = test_cmd(cmd, alg, tlim)
            ckt_res[res] += 1
        except Exception as e:
            print(f"{bcolors.RED}\n[ERROR] {cmd} {bcolors.ENDC}")
            print(e)
            ckt_res["FAIL"] += 1
            print('-'*50)
    
    os.system(f'rm -rf {TEST_DIR}')