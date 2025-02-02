import duckdb
import joblib
import numpy as np
import pandas as pd
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR
import joblib
from surprise import SVD
from surprise import Dataset
from surprise.reader import Reader
import time
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "uc07"
scale = 40


con = duckdb.connect(
    f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf{scale}.db")

root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"


model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"
model_load_time = 0

s1 = time.perf_counter()
model = joblib.load(model_file_name)
e1 = time.perf_counter()

model_load_time = e1-s1



def udf(user_id, item_id):
    ratings = []

    for i in range(len(user_id)):
        rating = model.predict(user_id[i], item_id[i]).est
        ratings.append(rating)
    return np.array(ratings)


con.create_function("udf", udf, [BIGINT, BIGINT],
                    DOUBLE, type="arrow", null_handling=hand_type)


sql = '''
explain analyze select userID, productID, r, score  from (select userID, productID, score, rank() OVER (PARTITION BY userID ORDER BY score) as r  from (select userID, productID, udf(userID, productID) score  from (select userID, productID  from Product_Rating group by userID, productID))) where r <=10;
'''
times = 5
min1 = 0
max1 = 0
res = 0
flag = True
for i in tqdm(range(times)):
    s = time.perf_counter()
    con.sql(sql)
    e = time.perf_counter()
    t = e-s+model_load_time
    print(f"{i+1} : {t}")
    res = res + t
    if flag:
        flag = False
        min1 = t
        max1 = t
    else:
        min1 = t if min1 > t else min1
        max1 = t if max1 < t else max1
print(f"min : {min1}")
print(f"max : {max1}")
res = res - min1 - max1
times = times - 2
print(f"{name}, {res/times}s ")
