import pandas as pd
import numpy as np
from tqdm import tqdm
import matplotlib.pyplot as plt


def handle_dict(path):
    sys_core = 0
    workload = ""
    records = {}
    with open(path, "r") as f:
        lines = f.readlines()
        for line in tqdm(lines):
            line = line.strip()
            if "sys" in line:
                sys_core = int(line.split()[-1])
                records[sys_core] = {}
            elif "workload" in line:
                workload = line.split()[-1]
                records[sys_core][workload] = {}
            elif "db" in line:
                data = line.split()
                db = data[2].replace(",","")
                ml = data[5].replace(",","")
                bs = data[7].replace(",","")
                time = data[-2]
                key = (db, ml, bs)
                records[sys_core][workload][key] = time
            else:
                continue
    return records

def handle2execl(path, out_path):
    sys_core = 0
    workload = ""
    result_list = []
    with open(path, "r") as f:
        lines = f.readlines()
        for line in tqdm(lines):
            line = line.strip()
            if "sys" in line:
                sys_core = int(line.split()[-1])
            elif "workload" in line:
                workload = line.split()[-1]
            elif "db" in line:
                data = line.split()
                db = data[2].replace(",","")
                ml = data[5].replace(",","")
                bs = data[7].replace(",","")
                time = data[-2]
                result_list.append([int(sys_core), workload, int(db), int(ml), int(bs), float(time)])
            else:
                continue
    df = pd.DataFrame(result_list, columns=["sys_core", "workload", "db", "ml", "bs", "time"])
    def han(l, x):
        l = []
        l.append
    res = df.loc[:, ["db" ,"time"]].groupby("db").agg({'time': lambda x: list(x)})
    print(res)
    res =  [np.array(t[0]) for t in res.values]
    x = np.arange(1, 17)
    plt.boxplot(res, positions=x)
    # 设置 x 轴标签
    plt.xlabel('db threads')
    # 设置 y 轴标签
    plt.ylabel('time (s)')
    plt.yscale('log')
    plt.savefig(f"/root/workspace/duckdb/.vscode/tmp3.png",
                bbox_inches="tight", pad_inches=0, dpi=600)
    
    # df.to_excel(out_path, index=False)
    return df



if __name__ == "__main__":
    path = "/root/workspace/duckdb/.vscode/permutation_pf3.log"
    out_path = "/root/workspace/duckdb/.vscode/permutation_pf5.xlsx"
    # handle_dict(path)
    handle2execl(path, out_path)
