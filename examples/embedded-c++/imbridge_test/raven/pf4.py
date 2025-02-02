import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR
import pandas as pd
import time
import onnxruntime as ort

from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "pf4"

con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven_10G.db")

root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"


onnx_path = f'{root_model_path}/Flights/flights_rf_pipeline.onnx'
s1 = time.perf_counter()
ortconfig = ort.SessionOptions()
flights_onnx_session = ort.InferenceSession(onnx_path, sess_options=ortconfig)
flights_label = flights_onnx_session.get_outputs()[0]
e1 = time.perf_counter()
model_load_time = e1-s1
numerical_columns = ['slatitude', 'slongitude', 'dlatitude', 'dlongitude']
categorical_columns = ['name1', 'name2', 'name4', 'acountry', 'active', 'scity', 'scountry', 'stimezone', 'sdst',
                       'dcity', 'dcountry', 'dtimezone', 'ddst']
flights_input_columns = numerical_columns + categorical_columns
flights_type_map = {
    'int32': np.int64,
    'int64': np.int64,
    'float64': np.float32,
    'object': str,
}


def udf(slatitude, slongitude, dlatitude, dlongitude, name1, name2, name4, acountry, active,
        scity, scountry, stimezone, sdst, dcity, dcountry, dtimezone, ddst):
    def udf_wrap(*args):
        infer_batch = {
            elem: args[i].to_numpy().astype(flights_type_map[args[i].to_numpy().dtype.name]).reshape((-1, 1))
            for i, elem in enumerate(flights_input_columns)
        }
        outputs = flights_onnx_session.run([flights_label.name], infer_batch)
        return outputs[0]

    return udf_wrap(slatitude, slongitude, dlatitude, dlongitude, name1, name2, name4, acountry, active,
                    scity, scountry, stimezone, sdst, dcity, dcountry, dtimezone, ddst)


con.create_function("udf", udf,
                    [DOUBLE, DOUBLE, DOUBLE, DOUBLE, BIGINT, VARCHAR, VARCHAR, VARCHAR, VARCHAR, VARCHAR, VARCHAR,
                     BIGINT, VARCHAR, VARCHAR, VARCHAR, BIGINT, VARCHAR], BIGINT, type="arrow")
sql = '''
Explain analyze SELECT Flights_S_routes_extension.airlineid, Flights_S_routes_extension.sairportid, Flights_S_routes_extension.dairportid,
                        udf(slatitude, slongitude, dlatitude, dlongitude, name1, name2, name4, acountry, active, 
                        scity, scountry, stimezone, sdst, dcity, dcountry, dtimezone, ddst) AS codeshare 
                        FROM Flights_S_routes_extension JOIN Flights_R1_airlines ON Flights_S_routes_extension.airlineid = Flights_R1_airlines.airlineid 
                        JOIN Flights_R2_sairports ON Flights_S_routes_extension.sairportid = Flights_R2_sairports.sairportid JOIN Flights_R3_dairports 
                        ON Flights_S_routes_extension.dairportid = Flights_R3_dairports.dairportid
where slatitude > 26 and dlatitude > 30 and slatitude < 40 and dlatitude < 40
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
