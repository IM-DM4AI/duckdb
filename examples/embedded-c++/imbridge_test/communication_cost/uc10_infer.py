import duckdb
import joblib
import numpy as np
import pandas as pd
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR
from sklearn.linear_model import LogisticRegression
import time
import sys
from tqdm import tqdm

# hand_type = "udf"
hand_type = "special"
name = "uc10"
scale = 10

con = duckdb.connect(
    f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf{scale}.db")

# root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"

# model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"


# model = joblib.load(model_file_name)


# def udf(business_hour_norm, amount_norm):
#     data = pd.DataFrame({
#         'business_hour_norm': business_hour_norm,
#         'amount_norm': amount_norm
#     })
#     # print(data.shape)
#     return model.predict(data)


sql1='''
select amount_norm, business_hour_norm
from uc10_used_data;
'''


sql2='''
explain analyze select amount_norm, business_hour_norm
from uc10_used_data;
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
