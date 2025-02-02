import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR
import pandas as pd
import time
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
import time
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "pf5"

con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven_10G.db")

root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"


scaler_path = f'{root_model_path}/Hospital/hospital_standard_scale_model.pkl'
enc_path = f'{root_model_path}/Hospital/hospital_one_hot_encoder.pkl'
model_path = f'{root_model_path}/Hospital/hospital_lr_model.pkl'
s1 = time.perf_counter()
with open(scaler_path, 'rb') as f:
    scaler = pickle.load(f)
with open(enc_path, 'rb') as f:
    enc = pickle.load(f)
with open(model_path, 'rb') as f:
    model = pickle.load(f)
e1 = time.perf_counter()
model_load_time = e1-s1


def udf(hematocrit, neutrophils, sodium, glucose, bloodureanitro, creatinine, bmi, pulse, respiration,
        secondarydiagnosisnonicd9,
        rcount, gender, dialysisrenalendstage, asthma, irondef, pneum, substancedependence, psychologicaldisordermajor,
        depress, psychother, fibrosisandothe, malnutrition, hemo):
    data = np.column_stack(
        [hematocrit, neutrophils, sodium, glucose, bloodureanitro, creatinine, bmi, pulse, respiration,
         secondarydiagnosisnonicd9,
         rcount, gender, dialysisrenalendstage, asthma, irondef, pneum, substancedependence,
         psychologicaldisordermajor, depress, psychother, fibrosisandothe, malnutrition, hemo])
    data = np.split(data, np.array([10]), axis=1)
    numerical = data[0]
    categorical = data[1]
    
    X = np.hstack((scaler.transform(numerical), enc.transform(categorical).toarray()))
    # print(X.shape)
    return model.predict(X)



con.create_function("udf", udf,
                    [DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, BIGINT,
                     DOUBLE, BIGINT, VARCHAR, VARCHAR,BIGINT, BIGINT,
                     BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT], BIGINT, type="arrow", null_handling=hand_type)
sql = '''
Explain analyze SELECT eid, udf(hematocrit, neutrophils, sodium, glucose, bloodureanitro, creatinine, bmi, pulse,
 respiration, secondarydiagnosisnonicd9, rcount, gender, cast(dialysisrenalendstage as INTEGER), cast(asthma as INTEGER),
  cast(irondef as INTEGER), cast(pneum as INTEGER), cast(substancedependence as INTEGER),
   cast(psychologicaldisordermajor as INTEGER), cast(depress as INTEGER), cast(psychother as INTEGER),
    cast(fibrosisandother as INTEGER), cast(malnutrition as INTEGER), cast(hemo as INTEGER)) AS lengthofstay
   FROM LengthOfStay_extension WHERE hematocrit > 10 AND neutrophils > 10 AND bloodureanitro < 20 AND pulse < 70;
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
