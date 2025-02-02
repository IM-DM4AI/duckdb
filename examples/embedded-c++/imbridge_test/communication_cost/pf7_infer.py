import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR
import pandas as pd
import time
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
import lightgbm as lgb
import sys
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "pf7"


con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven.db")

root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"


# scaler_path = f'{root_model_path}/Credit_Card/creditcard_standard_scale_model.pkl'
# model_path = f'{root_model_path}/Credit_Card/creditcard_lgb_model.txt'
# with open(scaler_path, 'rb') as f:
#     scaler = pickle.load(f)
# model = lgb.Booster(model_file=model_path)


# def udf(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount):
#     data = np.column_stack(
#         [V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount])
#     numerical = np.column_stack(data.T)
    
#     X = scaler.transform(numerical)
#     return model.predict(X)



sql1 = '''
SELECT V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15,
 V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount FROM pf7_used_data;
'''

sql2 = '''
explain analyze SELECT V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15,
 V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount FROM pf7_used_data;
'''

if sys.argv[1] == "udf":
    s = time.perf_counter()
    res_data = con.sql(sql1).fetch_arrow_table()
    # udf(*res_data)
    e = time.perf_counter()
    t = e-s
    print(f"udf : {t}")
else:
    s = time.perf_counter()
    res_data = con.sql(sql2)
    # udf(*res_data)
    e = time.perf_counter()
    t = e-s
    print(f"origin : {t}")
